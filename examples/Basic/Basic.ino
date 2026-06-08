// Example: Basic — moving red pixel + blue flash.
// Build/upload with:  pio run -e basic -t upload -t monitor

#include <Arduino.h>
#include <SPI.h>
#include "AW20216S.h"

//*********************************************************** */
//***********        Definitions                              */
//*********************************************************** */
// ── Pines ─────────────────────────────────────────────────
#define PIN_SCK  18
#define PIN_MISO 19
#define PIN_MOSI 23

// Chip Select (CS) pin. On ESP32 the VSPI default CS is GPIO 5.
#define CS_PIN 5

// Row and Column definitions for the 6x12 RGB matrix
#define WIDTH_LED_MATRIX 6
#define HEIGHT_LED_MATIX 12

// Instantiate the object (uses the default SPI / VSPI bus).
AW20216S ledMatrix(HEIGHT_LED_MATIX, WIDTH_LED_MATRIX, CS_PIN, SPI);

//*********************************************************** */
//***********        Setup Function                           */
//*********************************************************** */

void setup()
{
  Serial.begin(115200);
  Serial.println("Starting AW20216S...");
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

  // ledMatrix.setPwmFrequency(AwPwmFreq::High); // Set PWM frequency to 62.5 kHz to avoid flicker (internal default is already High)
}

//*********************************************************** */
//***********        Main Loop Function                       */
//*********************************************************** */

void loop()
{
  // --- DEMO 1: Pixel Red Runner ---
  Serial.println("Demo: Red Pixel traveling...");

  for (int y = 0; y < HEIGHT_LED_MATIX; y++)
  { // 12 Rows
    for (int x = 0; x < WIDTH_LED_MATRIX; x++)
    { // 6 Colums

      // Turn current pixel red
      // setPixel(x, y, R, G, B)
      ledMatrix.setPixel(x, y, 255, 0, 0);
      ledMatrix.show(); // Update the chip with the new framebuffer

      delay(50); // Movement speed

      // Turn off current pixel before moving to the next one
      ledMatrix.setPixel(x, y, 0, 0, 0);
      ledMatrix.show(); // Update the chip with the new framebuffer
    }
  }

  // --- DEMO 2: Blue Flicker ---
  Serial.println("Demo: Blue Flash");

  // Fill everything in blue
  ledMatrix.fillScreen(0, 0, 255);
  ledMatrix.show(); // Update the chip with the new framebuffer
  delay(500);

  // Turn off everything
  ledMatrix.clearScreen();
  ledMatrix.show(); // Update the chip with the new framebuffer
  delay(500);
}
* */