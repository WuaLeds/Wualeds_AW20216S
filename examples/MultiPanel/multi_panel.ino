// Example: MultiPanel — two AW20216S chips as one wide canvas.
// Build/upload with:  pio run -e multi_panel -t upload -t monitor
//
//*********************************************************** */
//***********        What this example does                   */
//*********************************************************** */
// Drives TWO 6x12 panels from the same SPI bus and stitches them into a single
// logical canvas that is 12 wide x 12 tall. A horizontal rainbow scrolls across
// the whole canvas so you can confirm the seam between the two panels is
// continuous (the color flows smoothly from panel A into panel B).
//
//*********************************************************** */
//***********        Purpose / what you will learn            */
//*********************************************************** */
// The AW20216S only needs a dedicated Chip Select line; SCK/MOSI/MISO are
// shared. So you can hang several chips on ONE SPI bus and pick which one to
// talk to with its CS pin. This example shows how to:
//
//   - create more than one AW20216S object on the same `SPI` bus, each with its
//     own CS pin.
//   - wrap them behind world-coordinate helpers (setPixelWorld / showAll) so
//     the rest of your drawing code ignores where the panel boundary is.
//
// You will practice:
//   - instantiating multiple drivers and calling begin() on each.
//   - mapping a wide logical coordinate to the right panel + local coordinate.
//   - pushing both framebuffers each frame.
//
// Wiring: panel A and panel B share SCK (18), MOSI (23) and MISO (19). Panel A
// CS -> GPIO 5, panel B CS -> GPIO 15. Both share GND with the ESP32; power the
// LEDs from an external supply.

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

// One Chip Select per panel (any free GPIOs). The SPI bus is shared.
#define CS_PIN_A 5
#define CS_PIN_B 15

// Single-panel geometry and how the panels are laid out.
#define PANEL_W   6   // Columns per panel.
#define PANEL_H   12  // Rows per panel.
#define PANELS_X  2   // Panels placed side by side (along X).

// Combined logical canvas size.
#define CANVAS_W  (PANEL_W * PANELS_X) // 12
#define CANVAS_H  (PANEL_H)            // 12

// Rainbow animation speed / refresh.
#define HUE_STEP  2   // Hue advance per frame.
#define HUE_SPAN  18  // Hue change per canvas column (spreads the rainbow).
#define FRAME_MS  30  // Milliseconds between frames.

// Two drivers on the SAME SPI bus, each with its own CS pin.
AW20216S panelA(PANEL_H, PANEL_W, CS_PIN_A, SPI);
AW20216S panelB(PANEL_H, PANEL_W, CS_PIN_B, SPI);

//*********************************************************** */
//***********        Multi-panel helpers                      */
//*********************************************************** */
// These hide the panel boundary so the animation can use canvas coordinates
// (wx = 0..CANVAS_W-1, wy = 0..CANVAS_H-1) without caring which chip is which.

// Plot a pixel in world coordinates, routing it to the correct panel.
static void setPixelWorld(uint8_t wx, uint8_t wy, uint8_t r, uint8_t g, uint8_t b)
{
  if (wx < PANEL_W)
    panelA.setPixel(wx, wy, r, g, b);            // left panel
  else
    panelB.setPixel(wx - PANEL_W, wy, r, g, b);  // right panel (local x)
}

// Clear both framebuffers.
static void clearAll()
{
  panelA.clearScreen();
  panelB.clearScreen();
}

// Push both framebuffers to their chips.
static void showAll()
{
  panelA.show();
  panelB.show();
}

//*********************************************************** */
//***********        Helper: HSV -> RGB                       */
//*********************************************************** */

static void hueToRgb(uint8_t hue, uint8_t &r, uint8_t &g, uint8_t &b)
{
  const uint8_t sector = hue / 85;
  const uint8_t pos    = (hue % 85) * 3;
  switch (sector)
  {
    case 0:  r = 255 - pos; g = pos;       b = 0;         break;
    case 1:  r = 0;         g = 255 - pos; b = pos;       break;
    default: r = pos;       g = 0;         b = 255 - pos; break;
  }
}

//*********************************************************** */
//***********        Setup Function                           */
//*********************************************************** */

void setup()
{
  Serial.begin(115200);
  Serial.println("Starting AW20216S MultiPanel...");
  delay(500);
  SPI.begin(PIN_SCK, PIN_MISO, PIN_MOSI, CS_PIN_A);
  delay(50);

  // Initialize both panels. They share the bus but answer on different CS pins.
  const bool okA = panelA.begin();
  const bool okB = panelB.begin();
  Serial.print("Panel A (CS ");
  Serial.print(CS_PIN_A);
  Serial.println(okA ? "): OK" : "): NOT detected");
  Serial.print("Panel B (CS ");
  Serial.print(CS_PIN_B);
  Serial.println(okB ? "): OK" : "): NOT detected");

  // Same brightness/white-balance settings on both for a uniform canvas.
  panelA.setGlobalCurrent(0x40);
  panelB.setGlobalCurrent(0x40);
  panelA.setScaling(0xFF, 0xFF, 0xFF);
  panelB.setScaling(0xFF, 0xFF, 0xFF);

  clearAll();
  showAll();
}

//*********************************************************** */
//***********        Main Loop Function                       */
//*********************************************************** */

void loop()
{
  static uint8_t  hue    = 0; // Current base hue.
  static uint32_t lastMs = 0; // Timestamp of the last frame.

  // Non-blocking timing: only redraw every FRAME_MS.
  const uint32_t now = millis();
  if ((now - lastMs) < FRAME_MS)
    return;
  lastMs = now;

  // Draw a vertical rainbow gradient across the FULL 12-wide canvas. Because we
  // use world coordinates, the gradient spans both panels seamlessly.
  for (uint8_t wx = 0; wx < CANVAS_W; wx++)
  {
    uint8_t r, g, b;
    hueToRgb((uint8_t)(hue + wx * HUE_SPAN), r, g, b);
    for (uint8_t wy = 0; wy < CANVAS_H; wy++)
      setPixelWorld(wx, wy, r, g, b);
  }

  showAll();
  hue += HUE_STEP;
}
