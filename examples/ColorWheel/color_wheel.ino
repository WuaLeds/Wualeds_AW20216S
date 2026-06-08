// Example: ColorWheel / RainbowFill — cycle the whole panel through the rainbow.
// Build/upload with:  pio run -e color_wheel -t upload -t monitor
//
//*********************************************************** */
//***********        What this example does                   */
//*********************************************************** */
// The whole 6x12 matrix is filled with a single, solid color that slowly
// sweeps around the color wheel (red -> green -> blue -> back to red),
// producing a smooth "breathing rainbow" of the entire panel.
//
//*********************************************************** */
//***********        Purpose / what you will learn            */
//*********************************************************** */
// Unlike SpatialSine (which uses a precomputed sine table) this sketch shows
// how to GENERATE color on the fly, with no lookup tables, using a classic
// HSV -> RGB conversion at full saturation/value. You will practice:
//
//   - fillScreen(r, g, b) : paint the entire framebuffer with one color.
//   - show()              : push the framebuffer to the chip in one burst.
//   - setGlobalCurrent()  : set the master brightness (current) once at start.
//   - setScaling()        : set the white balance once at start.
//
// It is also the simplest place to SEE the difference between the three
// brightness stages of the chip: global current (master), per-channel scaling
// (white balance) and the PWM value carried by each pixel. Here PWM comes from
// the rainbow color, while current and scaling stay fixed.

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

// How fast the rainbow advances and how often we redraw a frame.
#define HUE_STEP    1   // Hue increment per frame (1..255). Higher = faster.
#define FRAME_MS    30  // Milliseconds between frames.

// Instantiate the object (uses the default SPI / VSPI bus).
AW20216S ledMatrix(HEIGHT_LED_MATIX, WIDTH_LED_MATRIX, CS_PIN, SPI);

//*********************************************************** */
//***********        Helper: HSV -> RGB                       */
//*********************************************************** */
// Convert a hue (0..255 around the wheel) into an RGB triplet at full
// saturation and full value. This is a compact, integer-only version of the
// usual HSV->RGB formula, fast enough to call every frame on an MCU.
//
//   hue: 0   = red, 85  = green, 170 = blue, 255 wraps back to red.
//   r/g/b: outputs, 0..255 each.

static void hueToRgb(uint8_t hue, uint8_t &r, uint8_t &g, uint8_t &b)
{
  // Split the 0..255 wheel into three 85-wide sectors (R->G, G->B, B->R).
  const uint8_t sector = hue / 85;       // 0, 1 or 2
  const uint8_t pos    = (hue % 85) * 3; // ramp 0..255 within the sector

  switch (sector)
  {
    case 0: // Red -> Green
      r = 255 - pos;
      g = pos;
      b = 0;
      break;
    case 1: // Green -> Blue
      r = 0;
      g = 255 - pos;
      b = pos;
      break;
    default: // Blue -> Red
      r = pos;
      g = 0;
      b = 255 - pos;
      break;
  }
}

//*********************************************************** */
//***********        Setup Function                           */
//*********************************************************** */

void setup()
{
  Serial.begin(115200);
  Serial.println("Starting AW20216S ColorWheel / RainbowFill...");
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

  // 3. Set Scaling (White Balance) to maximum
  // This ensures that when you set PWM to maximum, the LED will shine at full brightness.
  ledMatrix.setScaling(0xFF, 0xFF, 0xFF);
}

//*********************************************************** */
//***********        Main Loop Function                       */
//*********************************************************** */

void loop()
{
  static uint8_t  hue    = 0; // Current position on the color wheel (0..255).
  static uint32_t lastMs = 0; // Timestamp of the last drawn frame.

  // Non-blocking frame timing: only redraw every FRAME_MS milliseconds.
  const uint32_t now = millis();
  if ((now - lastMs) < FRAME_MS)
    return;
  lastMs = now;

  // 1. Turn the current hue into an RGB color.
  uint8_t r, g, b;
  hueToRgb(hue, r, g, b);

  // 2. Paint the whole panel with it and push to the chip.
  ledMatrix.fillScreen(r, g, b);
  ledMatrix.show();

  // 3. Advance around the wheel (wraps automatically at 255 -> 0).
  hue += HUE_STEP;
}
