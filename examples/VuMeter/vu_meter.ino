// Example: AudioVUMeter / SerialControl — a VU meter fed by external input.
// Build/upload with:  pio run -e vu_meter -t upload -t monitor
//
//*********************************************************** */
//***********        What this example does                   */
//*********************************************************** */
// Turns the 6x12 panel into a vertical VU meter. A level (0..100%) fills the
// panel from the bottom up, colored green at the bottom, yellow in the middle
// and red at the top, with a white "peak hold" marker that floats at the
// highest recent level and slowly falls back down.
//
// The level comes from an EXTERNAL source. Two are built in:
//   - Serial control (default): type a number 0..100 + ENTER to set the level.
//   - Analog audio/pot: set USE_ANALOG_INPUT to 1 to read a microphone or
//     potentiometer on AUDIO_PIN instead.
//
//*********************************************************** */
//***********        Purpose / what you will learn            */
//*********************************************************** */
// Every other example generates its own data. This one REACTS to the outside
// world in real time, which is the basis of audio visualizers, sensor displays
// and remote-controlled signage.
//
// You will practice:
//   - reading live input each frame (Serial line or analogRead()).
//   - smoothing that input: instant attack, slow release (classic VU ballistics).
//   - a peak-hold marker that decays over time.
//   - mapping a 0..100 value to a colored bar with setPixel()/show().

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

// ── Input source ──────────────────────────────────────────
#define USE_ANALOG_INPUT 0    // 0 = Serial control, 1 = read AUDIO_PIN
#define AUDIO_PIN        34   // ADC pin for a mic/pot when USE_ANALOG_INPUT = 1
#define AUDIO_FULLSCALE  800  // Amplitude (counts) that maps to 100% (tune it)

// ── Ballistics / animation ────────────────────────────────
#define FRAME_MS      40   // Milliseconds between frames.
#define RELEASE_STEP  4    // % the bar falls per frame (slow release).
#define PEAK_HOLD_MS  400  // How long the peak marker holds before falling.
#define PEAK_FALL     3    // % the peak marker falls per frame after holding.

// Instantiate the object (uses the default SPI / VSPI bus).
AW20216S ledMatrix(HEIGHT_LED_MATIX, WIDTH_LED_MATRIX, CS_PIN, SPI);

// Smoothed display level and peak marker, both in percent (0..100).
float dispPct = 0;
float peakPct = 0;
uint32_t peakHeldSince = 0; // millis() when the peak was last refreshed.

//*********************************************************** */
//***********        Input helpers                            */
//*********************************************************** */

#if USE_ANALOG_INPUT
// Read the analog input and turn it into a 0..100 level. A slow running average
// tracks the DC bias so an AC audio signal is measured by its deviation.
static int readLevelPct()
{
  static float bias = 2048.0f; // mid-scale for a 12-bit ADC
  const int raw = analogRead(AUDIO_PIN);
  bias += (raw - bias) * 0.02f; // slow follow

  const float amp = fabs(raw - bias);
  int pct = (int)(amp * 100.0f / AUDIO_FULLSCALE);
  if (pct > 100) pct = 100;
  return pct;
}
#else
// Read a 0..100 level typed over Serial. Works with ANY Serial Monitor line
// ending: the number is accepted when a terminator (ENTER / space) arrives, or
// after a short idle gap if the monitor sends no terminator at all.
static void commitLevel(char *buf, uint8_t &len, int &target)
{
  buf[len] = '\0';
  target = constrain(atoi(buf), 0, 100);
  len = 0;
  Serial.print("Level set to ");
  Serial.print(target);
  Serial.println("%");
}

static int readLevelPct()
{
  static int      target = 0; // last accepted level
  static char     buf[8];
  static uint8_t  len = 0;
  static uint32_t lastCharMs = 0;

  while (Serial.available())
  {
    const char c = (char)Serial.read();
    if (c >= '0' && c <= '9')
    {
      if (len < sizeof(buf) - 1)
        buf[len++] = c;
      lastCharMs = millis();
    }
    else if (len > 0)
    {
      // Any non-digit (ENTER, space, etc.) ends the number.
      commitLevel(buf, len, target);
    }
    // ignore other characters
  }

  // No terminator sent ("No line ending" mode): accept after a brief pause.
  if (len > 0 && (millis() - lastCharMs) > 40)
    commitLevel(buf, len, target);

  return target;
}
#endif

//*********************************************************** */
//***********        Color helper                             */
//*********************************************************** */

// Color for a bar cell at a given height fraction (0 = bottom, 1 = top):
// green low, yellow mid, red high — the classic VU coloring.
static void barColor(float frac, uint8_t &r, uint8_t &g, uint8_t &b)
{
  if (frac < 0.6f)       { r = 0;   g = 255; b = 0; }   // green
  else if (frac < 0.85f) { r = 255; g = 180; b = 0; }   // yellow/amber
  else                   { r = 255; g = 0;   b = 0; }   // red
}

//*********************************************************** */
//***********        Setup Function                           */
//*********************************************************** */

void setup()
{
  Serial.begin(115200);
  Serial.println("Starting AW20216S VU Meter...");
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
#if USE_ANALOG_INPUT
  Serial.println("Source: analog input on AUDIO_PIN.");
#else
  Serial.println("Source: Serial. Type a number 0..100 + ENTER to set the level.");
#endif

  ledMatrix.setGlobalCurrent(0x40);
  ledMatrix.setScaling(0xFF, 0xFF, 0xFF);
  ledMatrix.clearScreen();
  ledMatrix.show();
}

//*********************************************************** */
//***********        Main Loop Function                       */
//*********************************************************** */

void loop()
{
  static uint32_t lastMs = 0; // Timestamp of the last frame.

  // Always service the input so Serial commands are read promptly.
  const int targetPct = readLevelPct();

  // Non-blocking frame timing.
  const uint32_t now = millis();
  if ((now - lastMs) < FRAME_MS)
    return;
  lastMs = now;

  // 1. VU ballistics: jump up instantly (attack), ease down slowly (release).
  if (targetPct >= dispPct)
    dispPct = targetPct;
  else
    dispPct = max((float)targetPct, dispPct - RELEASE_STEP);

  // 2. Peak hold: refresh the peak when the bar reaches it, otherwise let it
  //    hold for a moment and then fall.
  if (dispPct >= peakPct)
  {
    peakPct = dispPct;
    peakHeldSince = now;
  }
  else if (now - peakHeldSince > PEAK_HOLD_MS)
  {
    peakPct = max(dispPct, peakPct - PEAK_FALL);
  }

  // 3. Convert percentages to row counts (0..HEIGHT).
  const int barRows  = (int)(dispPct * HEIGHT_LED_MATIX / 100.0f + 0.5f);
  const int peakRow  = (int)(peakPct * HEIGHT_LED_MATIX / 100.0f + 0.5f);

  // 4. Render: fill columns from the bottom up, plus the peak marker.
  ledMatrix.clearScreen();

  for (int i = 0; i < barRows; i++)
  {
    const int   y    = HEIGHT_LED_MATIX - 1 - i;        // bottom-up
    const float frac = (float)(i + 1) / HEIGHT_LED_MATIX;
    uint8_t r, g, b;
    barColor(frac, r, g, b);
    for (uint8_t x = 0; x < WIDTH_LED_MATRIX; x++)
      ledMatrix.setPixel(x, y, r, g, b);
  }

  // White peak marker (only when it sits above the solid bar).
  if (peakRow > 0 && peakRow >= barRows)
  {
    const int y = HEIGHT_LED_MATIX - peakRow;
    for (uint8_t x = 0; x < WIDTH_LED_MATRIX; x++)
      ledMatrix.setPixel(x, y, 255, 255, 255);
  }

  ledMatrix.show();
}
