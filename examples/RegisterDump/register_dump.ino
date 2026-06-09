// Example: RegisterDump / Diagnostics — read the chip's registers over Serial.
// Build/upload with:  pio run -e register_dump -t upload -t monitor
//
//*********************************************************** */
//***********        What this example does                   */
//*********************************************************** */
// A hardware diagnostic tool. It does NOT animate anything: instead it lights a
// dim white test image and prints a report over Serial that:
//   1. Checks communication (reads GCR and compares it to the expected value).
//   2. Dumps the key Page 0 configuration registers in hex and binary.
//   3. Dumps the Open/Short detection registers (OSR, 0x03..0x26) and flags any
//      non-zero byte as a possible open/short fault.
//   4. Runs a write+read round-trip test to confirm both directions work.
// The report repeats every few seconds, so you can wiggle wiring or swap the
// panel and watch the values change live.
//
//*********************************************************** */
//***********        Purpose / what you will learn            */
//*********************************************************** */
// Every other example WRITES to the chip; this one shows the READ side and how
// to use the low-level register API to debug a board that "doesn't light up".
//
// You will practice:
//   - readRegister(page, reg)  : read any register on any page.
//   - writeRegister(page, reg) : write any register (used for the round-trip).
//   - interpreting GCR / GCCR / PCCR and the OSR fault registers.
//
// Honest notes about this chip:
//   - There is no readable "chip ID" register. The reliable "is it alive?" test
//     is reading GCR: begin() leaves it at 0xB1, so a correct read-back proves
//     the SPI link (MOSI, MISO, SCK, CS) is wired right.
//   - The exact LED <-> bit mapping of the OSR registers is datasheet-specific
//     and not decoded by this library, so we report the raw bytes and which
//     register holds a fault, not an (x, y) pixel. Open/short detection may also
//     need to be enabled/triggered per the datasheet to populate these bytes.

#include <Arduino.h>
#include <SPI.h>
#include "AW20216S.h"

//*********************************************************** */
//***********        Definitions                              */
//*********************************************************** */
// ── Pins ─────────────────────────────────────────────────
#define PIN_SCK  18
#define PIN_MISO 19
#define PIN_MOSI 23

// Chip Select (CS) pin. On ESP32 the VSPI default CS is GPIO 5.
#define CS_PIN 5

// Row and Column definitions for the 6x12 RGB matrix
#define WIDTH_LED_MATRIX 6
#define HEIGHT_LED_MATIX 12

// Value begin() leaves in GCR (CHIPEN + 12 active rows). Used as the link test.
#define GCR_EXPECTED 0xB1

// Open/Short register block on Page 0 (inclusive range).
#define OSR_START 0x03
#define OSR_END   0x26

// How often the report is reprinted.
#define REPORT_MS 5000

// Instantiate the object (uses the default SPI / VSPI bus).
AW20216S ledMatrix(HEIGHT_LED_MATIX, WIDTH_LED_MATRIX, CS_PIN, SPI);

//*********************************************************** */
//***********        Register table                           */
//*********************************************************** */
// The Page 0 configuration registers worth inspecting, with readable names.

struct RegInfo
{
  uint8_t     reg;
  const char *name;
};

const RegInfo PAGE0_REGS[] = {
  { AW_REG_GCR,  "GCR  (global control)" },
  { AW_REG_GCCR, "GCCR (global current)" },
  { AW_REG_DGCR, "DGCR (de-ghost)"       },
  { AW_REG_OTCR, "OTCR (over-temp)"      },
  { AW_REG_PCCR, "PCCR (PWM clock)"      },
  { AW_REG_UVCR, "UVCR (UVLO)"           },
  { AW_REG_SRCR, "SRCR (slew rate)"      },
};

const uint8_t PAGE0_REG_COUNT = sizeof(PAGE0_REGS) / sizeof(PAGE0_REGS[0]);

//*********************************************************** */
//***********        Print helpers                            */
//*********************************************************** */

// Print a byte as two hex digits (e.g. 0x0B), always padded.
static void printHex8(uint8_t v)
{
  Serial.print("0x");
  if (v < 0x10) Serial.print('0');
  Serial.print(v, HEX);
}

// Print a byte as 8 binary digits, always padded.
static void printBin8(uint8_t v)
{
  Serial.print("0b");
  for (int8_t bit = 7; bit >= 0; bit--)
    Serial.print((v >> bit) & 0x01);
}

//*********************************************************** */
//***********        Diagnostic routines                      */
//*********************************************************** */

// 1) Communication check: read GCR and compare it to the expected value.
static void checkLink()
{
  const uint8_t gcr = ledMatrix.readRegister(AW20216S_PAGE0, AW_REG_GCR);
  Serial.print("Link check: GCR = ");
  printHex8(gcr);
  if (gcr == GCR_EXPECTED)
  {
    Serial.println("  -> OK (chip responding, wiring looks good)");
  }
  else
  {
    Serial.print("  -> MISMATCH (expected ");
    printHex8(GCR_EXPECTED);
    Serial.println("). Check MISO/MOSI/SCK/CS and common GND.");
  }
}

// 2) Dump the named Page 0 configuration registers.
static void dumpConfig()
{
  Serial.println("Page 0 configuration registers:");
  for (uint8_t i = 0; i < PAGE0_REG_COUNT; i++)
  {
    const uint8_t v = ledMatrix.readRegister(AW20216S_PAGE0, PAGE0_REGS[i].reg);
    Serial.print("  ");
    printHex8(PAGE0_REGS[i].reg);
    Serial.print("  ");
    printHex8(v);
    Serial.print("  ");
    printBin8(v);
    Serial.print("  ");
    Serial.println(PAGE0_REGS[i].name);
  }
}

// 3) Dump the Open/Short registers and flag any non-zero byte.
static void dumpOpenShort()
{
  Serial.println("Open/Short registers (0x03..0x26):");
  uint8_t faults = 0;

  for (uint8_t reg = OSR_START; reg <= OSR_END; reg++)
  {
    const uint8_t v = ledMatrix.readRegister(AW20216S_PAGE0, reg);
    if (v != 0x00)
    {
      faults++;
      Serial.print("  FAULT at ");
      printHex8(reg);
      Serial.print(" = ");
      printBin8(v);
      Serial.println("  (a set bit = an open/short LED; see datasheet for mapping)");
    }
  }

  if (faults == 0)
    Serial.println("  All zero -> no open/short reported.");
}

// 4) Write+read round-trip on GCCR to prove both directions work, then restore.
static void roundTripTest()
{
  const uint8_t original = ledMatrix.readRegister(AW20216S_PAGE0, AW_REG_GCCR);
  const uint8_t probe    = 0x55;

  ledMatrix.writeRegister(AW20216S_PAGE0, AW_REG_GCCR, probe);
  const uint8_t readBack = ledMatrix.readRegister(AW20216S_PAGE0, AW_REG_GCCR);

  Serial.print("Round-trip (GCCR): wrote ");
  printHex8(probe);
  Serial.print(", read ");
  printHex8(readBack);
  Serial.println(readBack == probe ? "  -> PASS" : "  -> FAIL");

  // Restore the original global current so the test image stays as configured.
  ledMatrix.writeRegister(AW20216S_PAGE0, AW_REG_GCCR, original);
}

// Print the whole report.
static void printReport()
{
  Serial.println();
  Serial.println("========== AW20216S diagnostics ==========");
  checkLink();
  Serial.println();
  dumpConfig();
  Serial.println();
  dumpOpenShort();
  Serial.println();
  roundTripTest();
  Serial.println("==========================================");
}

//*********************************************************** */
//***********        Setup Function                           */
//*********************************************************** */

void setup()
{
  Serial.begin(115200);
  Serial.println("Starting AW20216S RegisterDump / Diagnostics...");
  delay(500);
  SPI.begin(PIN_SCK, PIN_MISO, PIN_MOSI, CS_PIN);
  delay(50);

  // 1. Initialize the chip
  if (!ledMatrix.begin())
  {
    // Even on failure we keep going: the report below helps diagnose WHY.
    Serial.println("Warning: begin() reported a mismatch. Running diagnostics anyway...");
  }
  else
  {
    Serial.println("Chip started correctly.");
  }

  // 2. Light a dim white test image so the LEDs are actually driven.
  ledMatrix.setGlobalCurrent(0x40);
  ledMatrix.setScaling(0xFF, 0xFF, 0xFF);
  ledMatrix.fillScreen(0x10, 0x10, 0x10);
  ledMatrix.show();

  // 3. Print the first report immediately.
  printReport();
}

//*********************************************************** */
//***********        Main Loop Function                       */
//*********************************************************** */

void loop()
{
  static uint32_t lastMs = 0; // Timestamp of the last report.

  // Non-blocking timing: reprint the report every REPORT_MS.
  const uint32_t now = millis();
  if ((now - lastMs) < REPORT_MS)
    return;
  lastMs = now;

  printReport();
}
