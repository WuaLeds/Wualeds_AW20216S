// Example: PwmFrequencySweep — cycle through every PWM frequency of the chip.
// Build/upload with:  pio run -e pwm_frequency_sweep -t upload -t monitor
//
//*********************************************************** */
//***********        What this example does                   */
//*********************************************************** */
// The whole panel is lit at a steady mid-brightness gray and the PWM frequency
// is changed every few seconds, stepping from the highest (62.5 kHz) down to
// the lowest (488 Hz) and looping. The current frequency is printed over Serial
// as it changes.
//
// To your eyes the panel looks the same at every step. The difference shows up
// on a CAMERA: film the panel with a phone and you will see the low frequencies
// produce visible flicker / rolling bands, while the high frequencies look
// clean. That is exactly why you raise the PWM frequency when recording LEDs.
//
//*********************************************************** */
//***********        Purpose / what you will learn            */
//*********************************************************** */
// This is the only example focused on setPwmFrequency(). The PWM (Page 1) value
// of a pixel sets its DUTY CYCLE; the PWM frequency sets how fast that duty
// cycle is switched on and off. A 50% gray is used here because a half-on duty
// cycle makes the flicker easiest to capture on camera.
//
// You will practice:
//   - setPwmFrequency(freq, phase) : pick the PWM switching frequency.
//   - the AwPwmFreq enum (High/62.5 kHz ... Low/488 Hz).
//   - fillScreen()/show() to hold a constant test image while only the
//     frequency register changes.

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

// Time spent at each frequency before moving to the next one.
#define CHANGE_MS 2500

// Test image brightness (50% duty makes flicker easiest to film).
#define TEST_LEVEL 128

// Instantiate the object (uses the default SPI / VSPI bus).
AW20216S ledMatrix(HEIGHT_LED_MATIX, WIDTH_LED_MATRIX, CS_PIN, SPI);

//*********************************************************** */
//***********        Frequency table                          */
//*********************************************************** */
// Every PWM frequency the AW20216S supports, from highest to lowest, with a
// label to print over Serial.

struct FreqOption
{
  AwPwmFreq   freq;
  const char *name;
};

const FreqOption FREQS[] = {
  { AwPwmFreq::Hz62500, "62.5 kHz (High)" },
  { AwPwmFreq::Hz32250, "32.25 kHz"       },
  { AwPwmFreq::Hz15600, "15.6 kHz"        },
  { AwPwmFreq::Hz7800,  "7.8 kHz"         },
  { AwPwmFreq::Hz3900,  "3.9 kHz"         },
  { AwPwmFreq::Hz1950,  "1.95 kHz"        },
  { AwPwmFreq::Hz977,   "977 Hz"          },
  { AwPwmFreq::Hz488,   "488 Hz (Low)"    },
};

const uint8_t FREQ_COUNT = sizeof(FREQS) / sizeof(FREQS[0]);

//*********************************************************** */
//***********        Helper functions                         */
//*********************************************************** */

// Apply one frequency from the table and report it over Serial.
static void applyFrequency(uint8_t index)
{
  ledMatrix.setPwmFrequency(FREQS[index].freq, AwPwmPhase::PhaseDelay);
  Serial.print("PWM frequency -> ");
  Serial.println(FREQS[index].name);
}

//*********************************************************** */
//***********        Setup Function                           */
//*********************************************************** */

void setup()
{
  Serial.begin(115200);
  Serial.println("Starting AW20216S PwmFrequencySweep...");
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
  Serial.println("Film the panel with a camera to see the flicker change.");

  // 2. Configure global current (Master brightness) and full white balance.
  ledMatrix.setGlobalCurrent(0x40);
  ledMatrix.setScaling(0xFF, 0xFF, 0xFF);

  // 3. Hold a constant 50% gray test image; only the frequency will change.
  ledMatrix.fillScreen(TEST_LEVEL, TEST_LEVEL, TEST_LEVEL);
  ledMatrix.show();

  // 4. Start at the highest frequency.
  applyFrequency(0);
}

//*********************************************************** */
//***********        Main Loop Function                       */
//*********************************************************** */

void loop()
{
  static uint8_t  index  = 0; // Current entry in the frequency table.
  static uint32_t lastMs = 0; // Timestamp of the last change.

  // Non-blocking timing: only switch frequency every CHANGE_MS.
  const uint32_t now = millis();
  if ((now - lastMs) < CHANGE_MS)
    return;
  lastMs = now;

  // Step to the next frequency (wraps back to the highest after the lowest).
  index = (uint8_t)((index + 1) % FREQ_COUNT);
  applyFrequency(index);
}
