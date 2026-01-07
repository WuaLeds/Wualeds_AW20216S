#include "AW20216S.h"

#define CS_PIN 10
#define WIDTH_LED_MATRIX  6
#define HEIGHT_LED_MATRIX 12

AW20216S ledMatrix(HEIGHT_LED_MATRIX, WIDTH_LED_MATRIX, CS_PIN);

// -------------------- Sine LUT (quarter-wave, 64 samples) --------------------
// Values are sin(theta) scaled to 0..255 for theta in [0..pi/2).
// We reconstruct full 0..2pi using symmetry and sign, then map to 0..255 brightness.

#if defined(ARDUINO_ARCH_AVR)
  #include <avr/pgmspace.h>
  #define AW_READ_U8(addr) pgm_read_byte(addr)
  const uint8_t kSinQ64[64] PROGMEM = {
#else
  #define AW_READ_U8(addr) (*(const uint8_t*)(addr))
  const uint8_t kSinQ64[64] = {
#endif
    0, 6, 13, 19, 25, 31, 38, 44,
    50, 56, 63, 69, 75, 81, 87, 94,
    100,106,112,118,124,130,136,142,
    148,154,160,166,171,177,183,188,
    194,199,204,210,215,220,225,230,
    235,239,244,248,252,255,255,255,
    255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255
  };

static inline uint8_t sin8_brightness(uint8_t phase)
{
  // phase: 0..255 maps to 0..2pi
  const uint8_t quadrant = (phase >> 6) & 0x03; // 0..3
  const uint8_t idx = phase & 0x3F;             // 0..63

  uint8_t s = AW_READ_U8(&kSinQ64[idx]);
  uint8_t s_mirror = AW_READ_U8(&kSinQ64[63 - idx]);

  // Build signed sine in range [-255..255] as int16
  int16_t signedSin;
  switch (quadrant) {
    case 0: signedSin = (int16_t)s;         break; // 0..+255
    case 1: signedSin = (int16_t)s_mirror;  break; // +255..0
    case 2: signedSin = -(int16_t)s;        break; // 0..-255
    default: signedSin = -(int16_t)s_mirror;break; // -255..0
  }

  // Map [-255..255] -> [0..255]
  int16_t v = (signedSin + 255) >> 1;
  if (v < 0) v = 0;
  if (v > 255) v = 255;
  return (uint8_t)v;
}

static inline uint8_t add8(uint8_t a, uint8_t b) { return (uint8_t)(a + b); }

// -------------------- Spatial animation params --------------------
static const uint8_t kDx = 18;    // phase step per column
static const uint8_t kDy = 11;    // phase step per row
static const uint8_t kSpeed = 1;  // phase increment per frame

// Per-channel phase offsets (spread colors)
static const uint8_t kOffR = 0;
static const uint8_t kOffG = 85;   // ~120 degrees
static const uint8_t kOffB = 170;  // ~240 degrees

void setup()
{
  Serial.begin(115200);
  Serial.println("Starting AW20216S spatial sine RGB...");

  if (!ledMatrix.begin()) {
    Serial.println("Error: AW20216S chip not detected.");
    while (1) {}
  }

  ledMatrix.setGlobalCurrent(0x40);
  ledMatrix.setScaling(0xFF, 0xFF, 0xFF);
  ledMatrix.clearScreen();
  ledMatrix.show();
}

void loop()
{
  static uint32_t lastMs = 0;
  static uint8_t t = 0;

  const uint32_t now = millis();
  if ((now - lastMs) < 20) return; 
  lastMs = now;

  t = (uint8_t)(t + kSpeed);

  // Render spatial wave:
  // phase(x,y) = t + x*dx + y*dy
  // Each channel uses a different phase offset.
  for (uint8_t y = 0; y < HEIGHT_LED_MATRIX; y++) {
    const uint8_t py = (uint8_t)(y * kDy);
    for (uint8_t x = 0; x < WIDTH_LED_MATRIX; x++) {
      const uint8_t phase = (uint8_t)(t + (uint8_t)(x * kDx) + py);

      uint8_t r = sin8_brightness(add8(phase, kOffR));
      uint8_t g = sin8_brightness(add8(phase, kOffG));
      uint8_t b = sin8_brightness(add8(phase, kOffB));

      ledMatrix.setPixel(x, y, r, g, b);
    }
  }

  ledMatrix.show();
}