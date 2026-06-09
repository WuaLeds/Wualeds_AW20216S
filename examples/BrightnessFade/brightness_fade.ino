// Example: BrightnessFade — fade a fixed color in and out with global current.
// Build/upload with:  pio run -e brightness_fade -t upload -t monitor
//
//*********************************************************** */
//***********        What this example does                   */
//*********************************************************** */
// The whole 6x12 matrix is painted ONCE with a single fixed color and never
// redrawn. Instead, the panel smoothly fades from off to full brightness and
// back, over and over, by sweeping ONLY the global current (master current)
// up and down. The framebuffer (the per-pixel PWM values) never changes.
//
//*********************************************************** */
//***********        Purpose / what you will learn            */
//*********************************************************** */
// This sketch isolates the master brightness stage so you can clearly see what
// setGlobalCurrent() does on its own. It is the best place to understand the
// THREE independent brightness stages of the AW20216S, each of which scales the
// final LED output:
//
//   1. Global current  (setGlobalCurrent) : master, shared by every LED.
//   2. Per-channel scaling (setScaling)    : white-balance trim per R/G/B.
//   3. Per-pixel PWM    (setPixel/fillScreen) : the color of each pixel.
//
//   Final brightness  ≈  global current  x  scaling  x  PWM
//
// You will practice:
//   - fillScreen(r, g, b) : paint the framebuffer once with a fixed color.
//   - show()              : push that framebuffer to the chip just once.
//   - setGlobalCurrent()  : animate the master brightness every frame WITHOUT
//                           touching the framebuffer or calling show() again.

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

// Fixed color of the panel while it fades (feel free to change it).
#define COLOR_R 255
#define COLOR_G 40
#define COLOR_B 0

// How fast the brightness moves and how often we update a frame.
#define FADE_STEP   2   // Global-current change per frame (1..255). Higher = faster.
#define FRAME_MS    20  // Milliseconds between brightness updates.

// Instantiate the object (uses the default SPI / VSPI bus).
AW20216S ledMatrix(HEIGHT_LED_MATIX, WIDTH_LED_MATRIX, CS_PIN, SPI);

//*********************************************************** */
//***********        Setup Function                           */
//*********************************************************** */

void setup()
{
  Serial.begin(115200);
  Serial.println("Starting AW20216S BrightnessFade...");
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

  // 2. Start with the master current at 0 (panel off). We will animate it later.
  ledMatrix.setGlobalCurrent(0x00);

  // 3. Set Scaling (White Balance) to maximum so PWM maps to full output.
  ledMatrix.setScaling(0xFF, 0xFF, 0xFF);

  // 4. Paint the fixed color into the framebuffer ONCE and push it to the chip.
  //    From now on the per-pixel PWM values stay constant; only the global
  //    current changes, so we never need to call show() again.
  ledMatrix.fillScreen(COLOR_R, COLOR_G, COLOR_B);
  ledMatrix.show();
}

//*********************************************************** */
//***********        Main Loop Function                       */
//*********************************************************** */

void loop()
{
  static uint8_t  level     = 0; // Current master brightness (0..255).
  static int8_t   direction = +1; // +1 = getting brighter, -1 = getting dimmer.
  static uint32_t lastMs    = 0;  // Timestamp of the last brightness update.

  // Non-blocking frame timing: only update every FRAME_MS milliseconds.
  const uint32_t now = millis();
  if ((now - lastMs) < FRAME_MS)
    return;
  lastMs = now;

  // 1. Advance the brightness in the current direction, clamping at the ends.
  const int16_t next = (int16_t)level + (int16_t)direction * FADE_STEP;

  if (next >= 255)
  {
    level = 255;
    direction = -1; // Reached full brightness: start fading out.
  }
  else if (next <= 0)
  {
    level = 0;
    direction = +1; // Reached off: start fading in again.
  }
  else
  {
    level = (uint8_t)next;
  }

  // 2. Apply the new master brightness. Note: NO fillScreen() and NO show()
  //    here. We only touch the global current register; the framebuffer and
  //    the color of every pixel stay exactly as set in setup().
  ledMatrix.setGlobalCurrent(level);
}
