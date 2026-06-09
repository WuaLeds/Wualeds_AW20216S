// Example: Bitmap / IconViewer — show several color icons, one after another.
// Build/upload with:  pio run -e icon_viewer -t upload -t monitor
//
//*********************************************************** */
//***********        What this example does                   */
//*********************************************************** */
// A small gallery of multi-color icons (heart, up arrow, check, cross,
// battery, exclamation) is drawn on the 6x12 panel. Every few seconds the
// current icon is replaced by the next one, looping forever.
//
//*********************************************************** */
//***********        Purpose / what you will learn            */
//*********************************************************** */
// This example shows how to store and draw STATIC images (bitmaps / sprites)
// instead of computing pixels with a formula. Each icon is written as plain
// "ASCII art": one character per pixel, where the character selects a color
// from a small palette ('.' = off). That makes the icons trivial to read and
// edit by hand.
//
// You will practice:
//   - storing bitmaps as a table and indexing into them.
//   - clearScreen() + setPixel() + show() to render one full frame.
//   - mapping data (a character) to a color through a palette function.
//   - swapping the displayed image on a timer with millis().

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

// How long each icon stays on screen before switching to the next one.
#define DISPLAY_MS 1500

// Instantiate the object (uses the default SPI / VSPI bus).
AW20216S ledMatrix(HEIGHT_LED_MATIX, WIDTH_LED_MATRIX, CS_PIN, SPI);

//*********************************************************** */
//***********        Icon Bitmaps (6 wide x 12 tall)          */
//*********************************************************** */
// Each icon is 12 rows of exactly 6 characters. One character = one pixel:
//   '.' off    R red    G green    B blue
//   Y yellow   C cyan   M magenta  W white   O orange
// Add your own icons here; just keep them 6 wide and 12 tall.

const char ICONS[][HEIGHT_LED_MATIX][WIDTH_LED_MATRIX + 1] = {
  { // 0: Heart
    "......",
    ".R..R.",
    "RRRRRR",
    "RRRRRR",
    "RRRRRR",
    ".RRRR.",
    ".RRRR.",
    "..RR..",
    "..RR..",
    "......",
    "......",
    "......",
  },
  { // 1: Arrow up
    "......",
    "..GG..",
    ".GGGG.",
    "GGGGGG",
    "GGGGGG",
    "..GG..",
    "..GG..",
    "..GG..",
    "..GG..",
    "..GG..",
    "..GG..",
    "......",
  },
  { // 2: Check mark
    "......",
    "......",
    ".....G",
    "....GG",
    "...GG.",
    "G..GG.",
    "GG.GG.",
    ".GGGG.",
    ".GGG..",
    "..G...",
    "......",
    "......",
  },
  { // 3: Cross
    "......",
    "R....R",
    "RR..RR",
    ".RRRR.",
    "..RR..",
    "..RR..",
    "..RR..",
    ".RRRR.",
    "RR..RR",
    "R....R",
    "......",
    "......",
  },
  { // 4: Battery
    "..WW..",
    "GGGGGG",
    "G....G",
    "G.GG.G",
    "G.GG.G",
    "G.GG.G",
    "G.GG.G",
    "G.GG.G",
    "G.GG.G",
    "GGGGGG",
    "......",
    "......",
  },
  { // 5: Exclamation
    "..YY..",
    "..YY..",
    "..YY..",
    "..YY..",
    "..YY..",
    "..YY..",
    "..YY..",
    "..YY..",
    "......",
    "..YY..",
    "..YY..",
    "......",
  },
};

// Number of icons in the gallery (computed automatically).
const uint8_t ICON_COUNT = sizeof(ICONS) / sizeof(ICONS[0]);

// Human-readable names, printed over Serial as each icon appears.
const char *const ICON_NAMES[] = {
  "Heart", "Arrow up", "Check", "Cross", "Battery", "Exclamation"
};

//*********************************************************** */
//***********        Helper functions                         */
//*********************************************************** */

// Translate one bitmap character into an RGB color from the palette.
static void charColor(char c, uint8_t &r, uint8_t &g, uint8_t &b)
{
  switch (c)
  {
    case 'R': r = 255; g = 0;   b = 0;   break; // red
    case 'G': r = 0;   g = 255; b = 0;   break; // green
    case 'B': r = 0;   g = 0;   b = 255; break; // blue
    case 'Y': r = 255; g = 200; b = 0;   break; // yellow
    case 'C': r = 0;   g = 255; b = 255; break; // cyan
    case 'M': r = 255; g = 0;   b = 255; break; // magenta
    case 'W': r = 255; g = 255; b = 255; break; // white
    case 'O': r = 255; g = 90;  b = 0;   break; // orange
    default:  r = 0;   g = 0;   b = 0;   break; // '.' or unknown = off
  }
}

// Render one icon to the panel: clear, plot every pixel, then push the frame.
static void drawIcon(uint8_t index)
{
  ledMatrix.clearScreen();

  for (uint8_t y = 0; y < HEIGHT_LED_MATIX; y++)
  {
    for (uint8_t x = 0; x < WIDTH_LED_MATRIX; x++)
    {
      uint8_t r, g, b;
      charColor(ICONS[index][y][x], r, g, b);
      ledMatrix.setPixel(x, y, r, g, b);
    }
  }

  ledMatrix.show();

  Serial.print("Showing icon: ");
  Serial.println(ICON_NAMES[index]);
}

//*********************************************************** */
//***********        Setup Function                           */
//*********************************************************** */

void setup()
{
  Serial.begin(115200);
  Serial.println("Starting AW20216S IconViewer...");
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

  // 4. Draw the first icon right away.
  drawIcon(0);
}

//*********************************************************** */
//***********        Main Loop Function                       */
//*********************************************************** */

void loop()
{
  static uint8_t  index  = 0; // Currently displayed icon.
  static uint32_t lastMs = 0; // Timestamp of the last icon switch.

  // Non-blocking timing: only advance to the next icon every DISPLAY_MS.
  const uint32_t now = millis();
  if ((now - lastMs) < DISPLAY_MS)
    return;
  lastMs = now;

  // Move to the next icon (wraps back to 0 after the last one) and draw it.
  index = (uint8_t)((index + 1) % ICON_COUNT);
  drawIcon(index);
}
