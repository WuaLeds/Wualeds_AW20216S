// Example: Plasma / FirePalette — palette-mapped fire effect that rises up.
// Build/upload with:  pio run -e fire_palette -t upload -t monitor
//
//*********************************************************** */
//***********        What this example does                   */
//*********************************************************** */
// Simulates a flickering flame that rises from the bottom of the 6x12 panel.
// A "heat" value is kept for every pixel: the bottom row is constantly reheated
// with random embers, and each frame the heat spreads upward while cooling
// down, so the flame fades from white-hot at the base to dark red at the top.
//
// The tall 12-row panel is ideal for this: the fire has plenty of vertical room
// to climb and cool.
//
//*********************************************************** */
//***********        Purpose / what you will learn            */
//*********************************************************** */
// SpatialSine builds color directly from RGB sine waves. This example shows the
// other common technique: keep a single scalar field (here "heat", 0..255) and
// turn it into color through a COLOR PALETTE. Changing only the palette gives a
// completely different look (fire, ice, plasma...) from the same simulation.
//
// You will practice:
//   - holding a per-pixel scalar field in RAM and evolving it each frame.
//   - a fire algorithm: reheat the base, then average-and-cool moving upward.
//   - mapping a scalar to RGB through a palette function (heatToColor()).
//   - setPixel() + show() to render, paced with millis().

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

// ── Fire tuning ───────────────────────────────────────────
#define FRAME_MS    50  // Milliseconds between frames.
#define COOLING     45  // Max heat lost per row as the flame rises (higher = shorter flames).
#define EMBER_MIN   180 // Lowest heat injected at the base each frame.
#define EMBER_MAX   255 // Highest heat injected at the base each frame.

// Pin used only to gather analog noise for the random seed (leave unconnected).
#define SEED_NOISE_PIN 34

// Instantiate the object (uses the default SPI / VSPI bus).
AW20216S ledMatrix(HEIGHT_LED_MATIX, WIDTH_LED_MATRIX, CS_PIN, SPI);

// Per-pixel heat field. y = 0 is the top row, y = HEIGHT-1 is the hot base.
uint8_t heat[HEIGHT_LED_MATIX][WIDTH_LED_MATRIX];

//*********************************************************** */
//***********        Fire palette                             */
//*********************************************************** */
// Map a heat value (0..255) to a fire color: black -> red -> orange/yellow ->
// white. Swap this function for a different ramp to get ice or plasma instead.

static void heatToColor(uint8_t h, uint8_t &r, uint8_t &g, uint8_t &b)
{
  if (h < 85)
  {
    // Black -> red.
    r = (uint8_t)(h * 3);
    g = 0;
    b = 0;
  }
  else if (h < 170)
  {
    // Red -> yellow (ramp up green).
    r = 255;
    g = (uint8_t)((h - 85) * 3);
    b = 0;
  }
  else
  {
    // Yellow -> white (ramp up blue).
    r = 255;
    g = 255;
    b = (uint8_t)((h - 170) * 3);
  }
}

//*********************************************************** */
//***********        Fire simulation                          */
//*********************************************************** */

// Advance the flame by one frame: reheat the base row, then spread heat upward
// while cooling, and finally render the heat field through the palette.
static void stepFire()
{
  // 1. Reheat the bottom row with random embers (the fuel source).
  const uint8_t base = HEIGHT_LED_MATIX - 1;
  for (uint8_t x = 0; x < WIDTH_LED_MATRIX; x++)
    heat[base][x] = (uint8_t)random(EMBER_MIN, EMBER_MAX + 1);

  // 2. Propagate heat upward (rows above the base read the rows below them).
  //    Each cell becomes the average of the cells below it, minus some random
  //    cooling. Columns wrap horizontally so flames can lean side to side.
  for (int y = 0; y < (int)base; y++)
  {
    for (uint8_t x = 0; x < WIDTH_LED_MATRIX; x++)
    {
      const uint8_t xl = (uint8_t)((x + WIDTH_LED_MATRIX - 1) % WIDTH_LED_MATRIX);
      const uint8_t xr = (uint8_t)((x + 1) % WIDTH_LED_MATRIX);

      const uint8_t below  = heat[y + 1][x];
      const uint8_t belowL = heat[y + 1][xl];
      const uint8_t belowR = heat[y + 1][xr];
      const uint8_t below2 = (y + 2 < HEIGHT_LED_MATIX) ? heat[y + 2][x] : below;

      const uint16_t avg = ((uint16_t)below + belowL + belowR + below2) / 4;
      const int16_t cooled = (int16_t)avg - (int16_t)random(0, COOLING);

      heat[y][x] = (cooled < 0) ? 0 : (uint8_t)cooled;
    }
  }

  // 3. Render the heat field through the fire palette.
  for (uint8_t y = 0; y < HEIGHT_LED_MATIX; y++)
  {
    for (uint8_t x = 0; x < WIDTH_LED_MATRIX; x++)
    {
      uint8_t r, g, b;
      heatToColor(heat[y][x], r, g, b);
      ledMatrix.setPixel(x, y, r, g, b);
    }
  }
  ledMatrix.show();
}

//*********************************************************** */
//***********        Setup Function                           */
//*********************************************************** */

void setup()
{
  Serial.begin(115200);
  Serial.println("Starting AW20216S FirePalette...");
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
  ledMatrix.setGlobalCurrent(0x40);

  // 3. Set Scaling (White Balance) to maximum.
  ledMatrix.setScaling(0xFF, 0xFF, 0xFF);

  // 4. Seed the random generator and start with a cold (black) field.
  randomSeed((uint32_t)analogRead(SEED_NOISE_PIN) ^ micros());
  memset(heat, 0, sizeof(heat));

  ledMatrix.clearScreen();
  ledMatrix.show();
}

//*********************************************************** */
//***********        Main Loop Function                       */
//*********************************************************** */

void loop()
{
  static uint32_t lastMs = 0; // Timestamp of the last frame.

  // Non-blocking timing: only advance the fire every FRAME_MS.
  const uint32_t now = millis();
  if ((now - lastMs) < FRAME_MS)
    return;
  lastMs = now;

  stepFire();
}
