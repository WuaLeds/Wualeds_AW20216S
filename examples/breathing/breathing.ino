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
AW20216S ledMatrix(HEIGHT_LED_MATIX, WIDTH_LED_MATRIX, CS_PIN);

//*********************************************************** */
//***********        Setup Function                           */
//*********************************************************** */

void setup()
{
    Serial.begin(115200);
    Serial.println("Starting AW20216S...");

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

    // 4. Configure breathing patterns
    // t0 rise, t1 on, t2 fall, t3 off
    ledMatrix.configureBreathing(AwPattern::PAT0, 80, 10, 80, 10, true);
    ledMatrix.configureBreathing(AwPattern::PAT1, 100, 10, 100, 10, true);
    ledMatrix.configureBreathing(AwPattern::PAT2, 120, 10, 120, 10, true);
    for (uint8_t y = 0; y < 12; y++)
    {
        for (uint8_t x = 0; x < 6; x++)
        {
            ledMatrix.setPixelPatternRGB(x, y,
                                         AwPattern::PAT0,
                                         AwPattern::PAT1,
                                         AwPattern::PAT2);
        }
    }
    ledMatrix.setBreathingBrightness(AwPattern::PAT0, 0x20, 0xE0); // R
    ledMatrix.setBreathingBrightness(AwPattern::PAT1, 0x20, 0xC8); // G
    ledMatrix.setBreathingBrightness(AwPattern::PAT2, 0x20, 0xB0); // B
    ledMatrix.startBreathing(AwPattern::PAT0);
    ledMatrix.startBreathing(AwPattern::PAT1);
    ledMatrix.startBreathing(AwPattern::PAT2);
}

//*********************************************************** */
//***********        Main Loop Function                       */
//*********************************************************** */

void loop()
{
}

//*********************************************************** */