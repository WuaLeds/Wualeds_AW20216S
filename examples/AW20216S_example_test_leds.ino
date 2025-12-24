#include <SPI.h>
#include "AW20216S.h"

//*********************************************************** */
//***********        Definitions                              */
//*********************************************************** */

// Chip Select (CS) pin definition
// On Arduino UNO it is usually 10. On ESP32 it can be 5 or 15.
#define CS_PIN 10 

// Row and Column definitions for the 6x12 RGB matrix
#define WIDTH_LED_MATRIX 6
#define HEIGHT_LED_MATIX 12

// Instantiate the object.
// If you are using a microcontroller with multiple SPIs, you can pass &SPI1, etc.
AW20216S ledMatrix(CS_PIN);


//*********************************************************** */
//***********        Setup Function                           */
//*********************************************************** */

void setup() {
  Serial.begin(115200);
  Serial.println("Starting AW20216S...");

  // 1. Initialize the chip
  if (!ledMatrix.begin()) {
    Serial.println("Error: AW20216S chip not detected.");
    while (1); // Stop execution if it fails
  }
  
  Serial.println("Chip started correctly.");

  // 2. Configure global current (Master brightness)
  // Range 0-255. 0x80 is approximately 50% power.
  ledMatrix.setGlobalCurrent(0x40); // Adjust according to desired consumption

  // 3. Set Scaling (White Balance) to maximum
  // This ensures that when you set PWM to maximum, the LED will shine at full brightness.
  ledMatrix.setScaling(0xFF, 0xFF, 0xFF);
  
  // Clean the screen at the beginning (everything is turned off)
  clearScreen();
}

//*********************************************************** */
//***********        Main Loop Function                       */
//*********************************************************** */

void loop() {
  // --- DEMO 1: Pixel Red Runner ---
  Serial.println("Demo: Red Pixel traveling...");
  
  for (int y = 0; y < HEIGHT_LED_MATIX; y++) {       // 12 Rows
    for (int x = 0; x < WIDTH_LED_MATRIX; x++) {      // 6 Colums
      
      // Turn current pixel red
      // setPixel(x, y, R, G, B)
      ledMatrix.setPixel(x, y, 255, 0, 0); 
      
      delay(50); // Movement speed
      
      // Turn off current pixel before moving to the next one
      ledMatrix.setPixel(x, y, 0, 0, 0);
    }
  }

  // --- DEMO 2: Blue Flicker ---
  Serial.println("Demo: Blue Flash");
  
  // Fill everything in blue
  fillScreen(0, 0, 255);
  delay(500);
  
  // Turn off everything
  clearScreen();
  delay(500);
}

//*********************************************************** */

//* --- Auxiliary functions for the example ---

void clearScreen() {
  // Scan the entire matrix and turn off the LEDs
  for (int y = 0; y < HEIGHT_LED_MATIX; y++) {
    for (int x = 0; x < WIDTH_LED_MATRIX; x++) {
      ledMatrix.setPixel(x, y, 0, 0, 0);
    }
  }
}

void fillScreen(uint8_t r, uint8_t g, uint8_t b) {
  // It scans the entire matrix and sets a fixed color.
  for (int y = 0; y < HEIGHT_LED_MATIX; y++) {
    for (int x = 0; x < WIDTH_LED_MATRIX; x++) {
      ledMatrix.setPixel(x, y, r, g, b);
    }
  }
}