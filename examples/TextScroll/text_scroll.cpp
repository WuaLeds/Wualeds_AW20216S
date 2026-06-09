// Example: TextScroll / Font3x5 — horizontal scrolling marquee text.
// Build/upload with:  pio run -e text_scroll -t upload -t monitor
//
//*********************************************************** */
//***********        What this example does                   */
//*********************************************************** */
// A text message scrolls horizontally across the 6x12 panel like a marquee,
// using a tiny built-in 3x5 pixel font. Because the panel is only 6 columns
// wide, roughly two characters are visible at a time while the rest scrolls
// through. When the message ends it wraps around and repeats forever.
//
//*********************************************************** */
//***********        Purpose / what you will learn            */
//*********************************************************** */
// This is the classic "LED matrix" project: rendering shapes from a font and
// animating them by shifting a window across a virtual, wider canvas. Unlike
// fillScreen() examples, here you build the image pixel by pixel every frame.
//
// You will practice:
//   - clearScreen()        : wipe the framebuffer before drawing each frame.
//   - setPixel(x, y, r,g,b) : light individual pixels to form glyphs.
//   - show()               : push the finished frame to the chip in one burst.
//   - non-blocking timing with millis() to control the scroll speed.
//
// Key idea: the text is treated as a virtual strip that is much WIDER than the
// 6-pixel panel. A scroll offset slides a 6-column window across that strip;
// for every on-screen column we look up which font column it maps to.

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

// ── Font / layout ─────────────────────────────────────────
#define FONT_WIDTH    3  // Pixels wide per glyph.
#define FONT_HEIGHT   5  // Pixels tall per glyph.
#define CHAR_SPACING  1  // Blank columns between glyphs.
#define CHAR_ADVANCE  (FONT_WIDTH + CHAR_SPACING) // Columns one glyph occupies.

// Vertically center the 5-pixel-tall text inside the 12-row panel.
#define Y_OFFSET      ((HEIGHT_LED_MATIX - FONT_HEIGHT) / 2)

// ── Animation ─────────────────────────────────────────────
#define SCROLL_MS  120 // Milliseconds between 1-column scroll steps.

// Text color (R, G, B).
#define TEXT_R 0
#define TEXT_G 180
#define TEXT_B 255

// The message to scroll. Only A-Z, 0-9 and space are drawable; any other
// character is rendered as a blank space.
const char MESSAGE[] = "HELLO WUALABS ";

// Instantiate the object (uses the default SPI / VSPI bus).
AW20216S ledMatrix(HEIGHT_LED_MATIX, WIDTH_LED_MATRIX, CS_PIN, SPI);

//*********************************************************** */
//***********        3x5 Font Table                           */
//*********************************************************** */
// Each glyph is 5 rows (top to bottom). Only the low 3 bits of each row byte
// are used and bit 2 is the LEFT-most pixel:
//
//     bit2 bit1 bit0
//      X    .    X     ->  0b101
//
// Index 0 = space, 1..26 = 'A'..'Z', 27..36 = '0'..'9'.

const uint8_t FONT3X5[][FONT_HEIGHT] = {
  {0b000, 0b000, 0b000, 0b000, 0b000}, // (space)
  {0b010, 0b101, 0b111, 0b101, 0b101}, // A
  {0b110, 0b101, 0b110, 0b101, 0b110}, // B
  {0b011, 0b100, 0b100, 0b100, 0b011}, // C
  {0b110, 0b101, 0b101, 0b101, 0b110}, // D
  {0b111, 0b100, 0b110, 0b100, 0b111}, // E
  {0b111, 0b100, 0b110, 0b100, 0b100}, // F
  {0b011, 0b100, 0b101, 0b101, 0b011}, // G
  {0b101, 0b101, 0b111, 0b101, 0b101}, // H
  {0b111, 0b010, 0b010, 0b010, 0b111}, // I
  {0b001, 0b001, 0b001, 0b101, 0b010}, // J
  {0b101, 0b110, 0b100, 0b110, 0b101}, // K
  {0b100, 0b100, 0b100, 0b100, 0b111}, // L
  {0b101, 0b111, 0b111, 0b101, 0b101}, // M
  {0b101, 0b111, 0b111, 0b111, 0b101}, // N
  {0b010, 0b101, 0b101, 0b101, 0b010}, // O
  {0b110, 0b101, 0b110, 0b100, 0b100}, // P
  {0b010, 0b101, 0b101, 0b110, 0b011}, // Q
  {0b110, 0b101, 0b110, 0b101, 0b101}, // R
  {0b011, 0b100, 0b010, 0b001, 0b110}, // S
  {0b111, 0b010, 0b010, 0b010, 0b010}, // T
  {0b101, 0b101, 0b101, 0b101, 0b111}, // U
  {0b101, 0b101, 0b101, 0b101, 0b010}, // V
  {0b101, 0b101, 0b111, 0b111, 0b101}, // W
  {0b101, 0b101, 0b010, 0b101, 0b101}, // X
  {0b101, 0b101, 0b010, 0b010, 0b010}, // Y
  {0b111, 0b001, 0b010, 0b100, 0b111}, // Z
  {0b010, 0b101, 0b101, 0b101, 0b010}, // 0
  {0b010, 0b110, 0b010, 0b010, 0b111}, // 1
  {0b110, 0b001, 0b010, 0b100, 0b111}, // 2
  {0b110, 0b001, 0b010, 0b001, 0b110}, // 3
  {0b101, 0b101, 0b111, 0b001, 0b001}, // 4
  {0b111, 0b100, 0b110, 0b001, 0b110}, // 5
  {0b011, 0b100, 0b110, 0b101, 0b010}, // 6
  {0b111, 0b001, 0b010, 0b010, 0b010}, // 7
  {0b010, 0b101, 0b010, 0b101, 0b010}, // 8
  {0b010, 0b101, 0b011, 0b001, 0b110}, // 9
};

//*********************************************************** */
//***********        Font helpers                             */
//*********************************************************** */

// Map an ASCII character to its glyph index in FONT3X5. Unknown characters
// fall back to the blank space glyph (index 0).
static uint8_t charToGlyph(char c)
{
  if (c >= 'A' && c <= 'Z') return 1  + (uint8_t)(c - 'A');
  if (c >= 'a' && c <= 'z') return 1  + (uint8_t)(c - 'a');
  if (c >= '0' && c <= '9') return 27 + (uint8_t)(c - '0');
  return 0; // space / anything else
}

// Return the vertical bit pattern of one glyph column (0 = left, 1, 2).
// Bit r of the result is set when row r of that column is lit, so it can be
// drawn top-to-bottom directly. Out-of-range columns return 0 (blank spacer).
static uint8_t glyphColumnBits(uint8_t glyph, uint8_t col)
{
  if (col >= FONT_WIDTH)
    return 0;

  const uint8_t mask = (uint8_t)(1u << (FONT_WIDTH - 1 - col)); // pick this column
  uint8_t bits = 0;
  for (uint8_t row = 0; row < FONT_HEIGHT; row++)
  {
    if (FONT3X5[glyph][row] & mask)
      bits |= (uint8_t)(1u << row);
  }
  return bits;
}

//*********************************************************** */
//***********        Setup Function                           */
//*********************************************************** */

void setup()
{
  Serial.begin(115200);
  Serial.println("Starting AW20216S TextScroll...");
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

  ledMatrix.clearScreen();
  ledMatrix.show();
}

//*********************************************************** */
//***********        Main Loop Function                       */
//*********************************************************** */

void loop()
{
  static uint16_t scrollPos = 0;  // Left-most virtual column shown on screen.
  static uint32_t lastMs    = 0;  // Timestamp of the last scroll step.

  // Total width of the virtual text strip, in columns.
  const uint16_t msgLen   = (uint16_t)(sizeof(MESSAGE) - 1); // drop '\0'
  const uint16_t totalCols = msgLen * CHAR_ADVANCE;

  // Non-blocking timing: only advance one column every SCROLL_MS.
  const uint32_t now = millis();
  if ((now - lastMs) < SCROLL_MS)
    return;
  lastMs = now;

  // 1. Clear the previous frame.
  ledMatrix.clearScreen();

  // 2. For each on-screen column, find which virtual text column it maps to,
  //    decode that column's glyph bits and draw the lit pixels.
  for (uint8_t x = 0; x < WIDTH_LED_MATRIX; x++)
  {
    const uint16_t vcol      = (uint16_t)(scrollPos + x) % totalCols;
    const uint16_t charIndex = vcol / CHAR_ADVANCE;     // which character
    const uint8_t  colInChar = (uint8_t)(vcol % CHAR_ADVANCE); // column inside it

    // Columns >= FONT_WIDTH are the blank spacing between characters.
    if (colInChar >= FONT_WIDTH)
      continue;

    const uint8_t glyph = charToGlyph(MESSAGE[charIndex]);
    const uint8_t bits  = glyphColumnBits(glyph, colInChar);

    for (uint8_t row = 0; row < FONT_HEIGHT; row++)
    {
      if (bits & (1u << row))
        ledMatrix.setPixel(x, Y_OFFSET + row, TEXT_R, TEXT_G, TEXT_B);
    }
  }

  // 3. Push the finished frame and advance the scroll position.
  ledMatrix.show();
  scrollPos = (uint16_t)(scrollPos + 1) % totalCols;
}
