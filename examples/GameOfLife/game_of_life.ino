// Example: GameOfLife — Conway's Game of Life on the 6x12 matrix.
// Build/upload with:  pio run -e game_of_life -t upload -t monitor
//
//*********************************************************** */
//***********        What this example does                   */
//*********************************************************** */
// Runs Conway's Game of Life, a classic cellular automaton, on the 6x12 panel.
// Each LED is a cell that is alive (lit) or dead (off). Every generation the
// whole grid is recomputed from four simple rules and redrawn. When the colony
// dies out or freezes (no change), the grid is reseeded with a new random soup.
//
// Rules (each cell looks at its 8 neighbors):
//   - A live cell with 2 or 3 live neighbors survives.
//   - A dead cell with exactly 3 live neighbors becomes alive (birth).
//   - Every other cell dies or stays dead.
//
// The edges wrap around (toroidal grid) so patterns that run off one side come
// back on the other; on such a small panel this keeps the simulation lively.
//
//*********************************************************** */
//***********        Purpose / what you will learn            */
//*********************************************************** */
// This example is autonomous (no input needed) and shows how to drive the
// panel from a SIMULATION held in your own memory rather than from a formula
// or a stored bitmap. The key pattern is DOUBLE BUFFERING: the next generation
// is computed into a second array while reading the current one, then the two
// are swapped.
//
// You will practice:
//   - keeping a 2D grid state in RAM and updating it each frame.
//   - reading neighbors with wrap-around (modulo) indexing.
//   - clearScreen() + setPixel() + show() to render the grid.
//   - non-blocking timing with millis() to pace the generations.

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

// ── Simulation ────────────────────────────────────────────
#define GEN_MS         180 // Milliseconds between generations.
#define SEED_PERCENT   35  // Approx. % of cells alive when reseeding (0..100).
#define MAX_GEN        200 // Reseed after this many generations (avoids loops).

// Color of a live cell.
#define CELL_R 0
#define CELL_G 255
#define CELL_B 80

// Pin used only to gather analog noise for the random seed (leave unconnected).
#define SEED_NOISE_PIN 34

// Instantiate the object (uses the default SPI / VSPI bus).
AW20216S ledMatrix(HEIGHT_LED_MATIX, WIDTH_LED_MATRIX, CS_PIN, SPI);

// Two grids: the current generation and the one being computed (double buffer).
uint8_t gridCur[HEIGHT_LED_MATIX][WIDTH_LED_MATRIX];
uint8_t gridNext[HEIGHT_LED_MATIX][WIDTH_LED_MATRIX];

//*********************************************************** */
//***********        Simulation helpers                       */
//*********************************************************** */

// Fill the current grid with a fresh random pattern.
static void seedGrid()
{
  for (uint8_t y = 0; y < HEIGHT_LED_MATIX; y++)
    for (uint8_t x = 0; x < WIDTH_LED_MATRIX; x++)
      gridCur[y][x] = (random(100) < SEED_PERCENT) ? 1 : 0;
}

// Count the live neighbors of cell (x, y), wrapping around the edges.
static uint8_t countNeighbors(uint8_t x, uint8_t y)
{
  uint8_t count = 0;
  for (int8_t dy = -1; dy <= 1; dy++)
  {
    for (int8_t dx = -1; dx <= 1; dx++)
    {
      if (dx == 0 && dy == 0)
        continue; // skip the cell itself

      const uint8_t nx = (uint8_t)((x + dx + WIDTH_LED_MATRIX) % WIDTH_LED_MATRIX);
      const uint8_t ny = (uint8_t)((y + dy + HEIGHT_LED_MATIX) % HEIGHT_LED_MATIX);
      count += gridCur[ny][nx];
    }
  }
  return count;
}

// Compute the next generation into gridNext, then swap it into gridCur.
// Returns true if the grid changed (false means it froze and should reseed).
static bool stepGeneration()
{
  bool changed = false;

  for (uint8_t y = 0; y < HEIGHT_LED_MATIX; y++)
  {
    for (uint8_t x = 0; x < WIDTH_LED_MATRIX; x++)
    {
      const uint8_t n     = countNeighbors(x, y);
      const uint8_t alive = gridCur[y][x];

      // Apply Conway's rules.
      const uint8_t next = alive ? (n == 2 || n == 3) : (n == 3);

      gridNext[y][x] = next;
      if (next != alive)
        changed = true;
    }
  }

  // Swap buffers: copy the freshly computed generation into the current one.
  memcpy(gridCur, gridNext, sizeof(gridCur));
  return changed;
}

// Total number of live cells (used to detect an empty grid).
static uint16_t population()
{
  uint16_t total = 0;
  for (uint8_t y = 0; y < HEIGHT_LED_MATIX; y++)
    for (uint8_t x = 0; x < WIDTH_LED_MATRIX; x++)
      total += gridCur[y][x];
  return total;
}

// Draw the current grid onto the panel.
static void renderGrid()
{
  ledMatrix.clearScreen();
  for (uint8_t y = 0; y < HEIGHT_LED_MATIX; y++)
    for (uint8_t x = 0; x < WIDTH_LED_MATRIX; x++)
      if (gridCur[y][x])
        ledMatrix.setPixel(x, y, CELL_R, CELL_G, CELL_B);
  ledMatrix.show();
}

//*********************************************************** */
//***********        Setup Function                           */
//*********************************************************** */

void setup()
{
  Serial.begin(115200);
  Serial.println("Starting AW20216S GameOfLife...");
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

  // 4. Seed the random generator from analog noise, then create the first soup.
  randomSeed((uint32_t)analogRead(SEED_NOISE_PIN) ^ micros());
  seedGrid();
  renderGrid();
}

//*********************************************************** */
//***********        Main Loop Function                       */
//*********************************************************** */

void loop()
{
  static uint32_t lastMs = 0; // Timestamp of the last generation.
  static uint16_t gen    = 0; // Generation counter since the last reseed.

  // Non-blocking timing: only advance one generation every GEN_MS.
  const uint32_t now = millis();
  if ((now - lastMs) < GEN_MS)
    return;
  lastMs = now;

  // 1. Advance the simulation and draw the new generation.
  const bool changed = stepGeneration();
  gen++;
  renderGrid();

  // 2. Reseed when the colony freezes, dies out, or runs too long.
  if (!changed || population() == 0 || gen >= MAX_GEN)
  {
    Serial.print("Reseeding after ");
    Serial.print(gen);
    Serial.println(" generations.");
    seedGrid();
    renderGrid();
    gen = 0;
  }
}
