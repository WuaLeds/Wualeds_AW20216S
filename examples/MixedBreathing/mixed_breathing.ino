// Example: MixedBreathing — hardware breathing and direct PWM on one panel.
// Build/upload with:  pio run -e mixed_breathing -t upload -t monitor
//
//*********************************************************** */
//***********        What this example does                   */
//*********************************************************** */
// The panel is split into two bands:
//   - Top band (rows 0..5):  breathes a color ALL BY ITSELF using the chip's
//     autonomous breathing engines (PAT0/PAT1/PAT2). The MCU never touches it
//     after setup.
//   - Bottom band (rows 6..11): a scrolling rainbow driven by DIRECT PWM, i.e.
//     the MCU writes pixels every frame with setPixel()/show().
//
// Both run at the same time, proving that hardware-driven and software-driven
// pixels can coexist on the same matrix.
//
//*********************************************************** */
//***********        Purpose / what you will learn            */
//*********************************************************** */
// The existing "breathing" example puts the WHOLE panel on the breathing
// engines. This one shows the more interesting case: routing only SOME channels
// to the engines while leaving the rest under direct PWM control.
//
// You will practice:
//   - setChannelPattern(x, y, ch, pat) : route one color channel of one pixel
//     to a breathing engine (PATx) or to direct PWM. (setPixelPatternRGB() is
//     just the shortcut that routes all three channels at once.)
//   - configureBreathing() + setBreathingBrightness() + startBreathing() to set
//     up and launch the autonomous engines.
//   - the key idea: PAT-routed channels follow the engine with ZERO MCU load,
//     while PWM-routed channels follow whatever setPixel()/show() writes.

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

// First row of the bottom (direct-PWM) band. Rows above it breathe.
#define SPLIT_ROW 6

// Direct-PWM rainbow animation speed / refresh.
#define HUE_STEP  2   // Hue advance per frame.
#define FRAME_MS  30  // Milliseconds between frames.

// Instantiate the object (uses the default SPI / VSPI bus).
AW20216S ledMatrix(HEIGHT_LED_MATIX, WIDTH_LED_MATRIX, CS_PIN, SPI);

//*********************************************************** */
//***********        Helper: HSV -> RGB                       */
//*********************************************************** */
// Compact integer hue (0..255 around the wheel) to RGB at full saturation.

static void hueToRgb(uint8_t hue, uint8_t &r, uint8_t &g, uint8_t &b)
{
  const uint8_t sector = hue / 85;       // 0, 1 or 2
  const uint8_t pos    = (hue % 85) * 3; // ramp 0..255 within the sector

  switch (sector)
  {
    case 0:  r = 255 - pos; g = pos;       b = 0;         break; // red -> green
    case 1:  r = 0;         g = 255 - pos; b = pos;       break; // green -> blue
    default: r = pos;       g = 0;         b = 255 - pos; break; // blue -> red
  }
}

//*********************************************************** */
//***********        Setup Function                           */
//*********************************************************** */

void setup()
{
  Serial.begin(115200);
  Serial.println("Starting AW20216S MixedBreathing...");
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

  // 2. Configure global current (Master brightness) and full white balance.
  ledMatrix.setGlobalCurrent(0x40);
  ledMatrix.setScaling(0xFF, 0xFF, 0xFF);

  // 3. Configure the three breathing engines with the SAME timing so the top
  //    band breathes in sync (rise -> hold -> fall -> off). One engine per
  //    color channel lets us shape the band's color with their max values.
  ledMatrix.configureBreathing(AwPattern::PAT0, 80, 10, 80, 10, true); // R
  ledMatrix.configureBreathing(AwPattern::PAT1, 80, 10, 80, 10, true); // G
  ledMatrix.configureBreathing(AwPattern::PAT2, 80, 10, 80, 10, true); // B

  // Brightness envelope per engine. Different maxima -> the breath has a color
  // (here a warm orange: strong red, some green, little blue).
  ledMatrix.setBreathingBrightness(AwPattern::PAT0, 0x00, 0xE0); // R
  ledMatrix.setBreathingBrightness(AwPattern::PAT1, 0x00, 0x60); // G
  ledMatrix.setBreathingBrightness(AwPattern::PAT2, 0x00, 0x10); // B

  // 4. Route the channels of every pixel.
  for (uint8_t y = 0; y < HEIGHT_LED_MATIX; y++)
  {
    for (uint8_t x = 0; x < WIDTH_LED_MATRIX; x++)
    {
      if (y < SPLIT_ROW)
      {
        // Top band: bind each color channel to its breathing engine.
        // (This is exactly what setPixelPatternRGB(x, y, PAT0, PAT1, PAT2)
        //  does; we use setChannelPattern here to show the per-channel call.)
        ledMatrix.setChannelPattern(x, y, AwChannel::R, AwPattern::PAT0);
        ledMatrix.setChannelPattern(x, y, AwChannel::G, AwPattern::PAT1);
        ledMatrix.setChannelPattern(x, y, AwChannel::B, AwPattern::PAT2);
      }
      else
      {
        // Bottom band: keep all channels under direct PWM control so the MCU
        // can animate them with setPixel()/show().
        ledMatrix.setPixelPatternRGB(x, y, AwPattern::PWM, AwPattern::PWM, AwPattern::PWM);
      }
    }
  }

  // 5. Start the autonomous breathing engines. From now on the top band runs
  //    with no MCU involvement at all.
  ledMatrix.startBreathing(AwPattern::PAT0);
  ledMatrix.startBreathing(AwPattern::PAT1);
  ledMatrix.startBreathing(AwPattern::PAT2);

  // Clear the framebuffer (only affects the direct-PWM channels).
  ledMatrix.clearScreen();
  ledMatrix.show();
}

//*********************************************************** */
//***********        Main Loop Function                       */
//*********************************************************** */

void loop()
{
  static uint8_t  hue    = 0; // Current hue of the rainbow band.
  static uint32_t lastMs = 0; // Timestamp of the last frame.

  // Non-blocking timing: only redraw the PWM band every FRAME_MS.
  const uint32_t now = millis();
  if ((now - lastMs) < FRAME_MS)
    return;
  lastMs = now;

  // Animate ONLY the bottom band with direct PWM. The top band keeps breathing
  // by itself, so we never write to those rows here.
  for (uint8_t y = SPLIT_ROW; y < HEIGHT_LED_MATIX; y++)
  {
    for (uint8_t x = 0; x < WIDTH_LED_MATRIX; x++)
    {
      // Spread the hue across columns and rows for a diagonal rainbow.
      const uint8_t phase = (uint8_t)(hue + x * 30 + (y - SPLIT_ROW) * 20);
      uint8_t r, g, b;
      hueToRgb(phase, r, g, b);
      ledMatrix.setPixel(x, y, r, g, b);
    }
  }

  ledMatrix.show();
  hue += HUE_STEP;
}
