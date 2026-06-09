// Example: WhiteBalance — calibrate the panel white point over Serial.
// Build/upload with:  pio run -e white_balance -t upload -t monitor
//
//*********************************************************** */
//***********        What this example does                   */
//*********************************************************** */
// The whole 6x12 matrix is filled with full white (255, 255, 255) and stays
// there. While it is lit you can interactively trim the Red, Green and Blue
// channels over the Serial Monitor until the panel looks like a clean, neutral
// white instead of a tinted one. The trim is applied with setScaling().
//
// Serial commands (type a letter, then ENTER):
//   r / R : decrease / increase the Red   scaling
//   g / G : decrease / increase the Green scaling
//   b / B : decrease / increase the Blue  scaling
//   p     : print the current R/G/B scaling values
//   f     : reset all channels back to full (0xFF, 0xFF, 0xFF)
//
//*********************************************************** */
//***********        Purpose / what you will learn            */
//*********************************************************** */
// This is the only example focused purely on setScaling(), the per-channel
// "white balance" stage of the chip. LEDs of different colors rarely have the
// same efficiency, so full white usually looks slightly tinted. Scaling lets
// you attenuate the too-strong channels to correct the white point WITHOUT
// lowering the PWM color or the global current.
//
// Recall the three independent brightness stages:
//   Final brightness  ≈  global current  x  scaling  x  PWM
//
// You will practice:
//   - fillScreen(r, g, b) : paint a fixed full-white image once.
//   - setScaling(r, g, b) : trim each channel live to fix the white point.
//   - reading the Serial port to drive the calibration interactively.
//
// Note: setScaling() writes the chip immediately and does NOT use the
// framebuffer, so changing it takes effect without calling show() again.

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

// How much each key press changes a channel's scaling.
#define TRIM_STEP 4

// Instantiate the object (uses the default SPI / VSPI bus).
AW20216S ledMatrix(HEIGHT_LED_MATIX, WIDTH_LED_MATRIX, CS_PIN, SPI);

// Current per-channel scaling values (start at full = no attenuation).
uint8_t scaleR = 0xFF;
uint8_t scaleG = 0xFF;
uint8_t scaleB = 0xFF;

//*********************************************************** */
//***********        Helper functions                         */
//*********************************************************** */

// Clamp-and-add a signed step to a channel value, keeping it inside 0..255.
static uint8_t trim(uint8_t value, int16_t delta)
{
  const int16_t next = (int16_t)value + delta;
  if (next < 0)   return 0;
  if (next > 255) return 255;
  return (uint8_t)next;
}

// Push the current scaling to the chip and report it over Serial.
static void applyScaling()
{
  ledMatrix.setScaling(scaleR, scaleG, scaleB);
  Serial.print("Scaling -> R: ");
  Serial.print(scaleR);
  Serial.print("  G: ");
  Serial.print(scaleG);
  Serial.print("  B: ");
  Serial.println(scaleB);
}

//*********************************************************** */
//***********        Setup Function                           */
//*********************************************************** */

void setup()
{
  Serial.begin(115200);
  Serial.println("Starting AW20216S WhiteBalance...");
  delay(500);
  SPI.begin(PIN_SCK, PIN_MISO, PIN_MOSI, CS_PIN);
  delay(50);

  // 1. Initialize the chip
  if (!ledMatrix.begin())
  {
    Serial.println("Error: AW20216S chip not detected.");
    while (1)
      ; // Stop execution if it fails
  }

  Serial.println("Chip started correctly.");

  // 2. Configure global current (Master brightness)
  // Range 0-255. 0x80 is approximately 50% power.
  ledMatrix.setGlobalCurrent(0x40); // Adjust according to desired consumption

  // 3. Start with scaling at maximum, then paint full white once.
  applyScaling();
  ledMatrix.fillScreen(255, 255, 255);
  ledMatrix.show();

  // 4. Print the interactive command help.
  Serial.println();
  Serial.println("White balance calibration. Send a letter + ENTER:");
  Serial.println("  r/R : Red   down/up");
  Serial.println("  g/G : Green down/up");
  Serial.println("  b/B : Blue  down/up");
  Serial.println("  p   : print current scaling");
  Serial.println("  f   : reset all to full (0xFF)");
}

//*********************************************************** */
//***********        Main Loop Function                       */
//*********************************************************** */

void loop()
{
  // Nothing animates here. We only react to single-character Serial commands
  // and re-apply the scaling whenever a channel changes.
  if (!Serial.available())
    return;

  const char cmd = (char)Serial.read();

  switch (cmd)
  {
    case 'r': scaleR = trim(scaleR, -TRIM_STEP); applyScaling(); break;
    case 'R': scaleR = trim(scaleR, +TRIM_STEP); applyScaling(); break;
    case 'g': scaleG = trim(scaleG, -TRIM_STEP); applyScaling(); break;
    case 'G': scaleG = trim(scaleG, +TRIM_STEP); applyScaling(); break;
    case 'b': scaleB = trim(scaleB, -TRIM_STEP); applyScaling(); break;
    case 'B': scaleB = trim(scaleB, +TRIM_STEP); applyScaling(); break;

    case 'p': // Print current values without changing anything.
      applyScaling();
      break;

    case 'f': // Reset all channels back to full output.
      scaleR = scaleG = scaleB = 0xFF;
      applyScaling();
      break;

    default:
      // Ignore everything else (including the newline/carriage-return chars).
      break;
  }
}
