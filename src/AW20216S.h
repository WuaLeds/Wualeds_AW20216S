#ifndef AW20216S_H
#define AW20216S_H

#include <Arduino.h>
#include <SPI.h>

/**
 * Registers definitions del AW20216S
 * Datasheet Page 22 - Register List
 */

// --- PAGE 0: Function Registers ---
#define AW20216S_PAGE0          0x00
#define AW_REG_GCR              0x00 // Global Control Register (Enable, SW selection)
#define AW_REG_GCCR             0x01 // Global Current Control (Global Brightness)
#define AW_REG_DGCR             0x02 // De-ghost Control
#define AW_REG_OSR_BASE         0x03 // Open/Short Register Base (until 0x26)
#define AW_REG_OTCR             0x27 // Over Temperature Control
#define AW_REG_SSCR             0x28 // Spread Spectrum Control
#define AW_REG_PCCR             0x29 // PWM Clock Control (Frequency)
#define AW_REG_UVCR             0x2A // UVLO Control
#define AW_REG_SRCR             0x2B // Slew Rate Control
#define AW_REG_RSTN             0x2F // Soft Reset (Write 0xAE)
#define AW_REG_MIXCR            0x46 // Mix Function (Enable Page 4)
#define AW_REG_SDCR             0x4D // SW Drive Capability

// Automatic Breathing Registers  (Breath Pattern) - Page 0
#define AW_REG_PWMH0            0x30 // PWMH0-PWMH2 Maximum Brightness for Auto Breath (until 0x32)
#define AW_REG_PWML0            0x33 // PWML0-PWML2 Minimum Brightness for Auto Breath (until 0x35)
#define AW_REG_PAT0T0           0x36 // PAT0T0-PAT2T0 Pattern Timer 0 (0x36 0x3A 0x2E)
#define AW_REG_PAT0CFG          0x42 // PAT0CFG-PAT2CFG Configure Register (until 0x44)
#define AW_REG_PATGO            0x45 // PATGO Start Control Register

// --- PAGE 1: PWM Registers (Brightness) ---
// Controls individual brightness (0-255). Directions 0x00 to 0xD7 (216 LEDs)
#define AW20216S_PAGE1          0x01
#define AW_REG_PWM_BASE         0x00 

// --- PAGE 2: Scaling Registers (Current) ---
// Controls individual current (Color Mixing). Directions 0x00 to 0xD7
#define AW20216S_PAGE2          0x02
#define AW_REG_SL_BASE          0x00

// --- PAGE 3: Pattern Choice ---
// Assign each LED to a pattern driver.
#define AW20216S_PAGE3          0x03
#define AW_REG_PATG_BASE        0x00

// --- PAGE 4: Virtual Page (PWM + Scaling) ---
// It allows writing PWM and SL in a single transaction.
#define AW20216S_PAGE4          0x04
#define AW_REG_PWM_SL_BASE      0x00

// --- Constants ---
#define AW_CHIPID_SPI           0xA0 // Fixed part of the first SPI byte (1010xxxx) [cite: 542]
#define AW_RST_CMD              0xAE // Command to reset the chip [cite: 716]
#define AW_GLOBAL_ENABLE        0x01 // Bit CHIPEN in GCR [cite: 700]
#define AW_MAX_LEDS             216


// #define AW_WIDTH_RGB            6    // LMX2 configuration: 6 RGB columns
// #define AW_HEIGHT               12   // LMX2 configuration: 12 rows (SW)


//* AW20216S Class Definition */

class AW20216S {
public:
    /**
     * Constructor
     * @param rows number of rows
     * @param cols number of columns
     * @param csPin Arduino Chip Select Pin
     * @param spiPort SPI port to use (default SPI)
     */
    AW20216S(uint8_t rows, uint8_t cols, uint8_t csPin, SPIClass &spiPort = SPI);

    /**
     * Initialize the chip, configure SPI, and reset registers.
     * @return true if the initialization was successful (successful communication).
     */
    bool begin();

    /**
     * Software reset (returns registers to default).
     */
    void reset();

    /**
     * Scan the entire matrix and turn off the LEDs
     */
    void clearScreen();

    /**
     * It scans the entire matrix and sets a fixed color.
     * @param r red color component value
     * @param g green color component value
     * @param b blue color component value
     */
    void fillScreen(uint8_t r, uint8_t g, uint8_t b);

    /**
     * Configure the global current (Master Brightness).
     * @param current Value 0-255.
     */
    void setGlobalCurrent(uint8_t current);

    /**
     * Configure a color in the RGB matrix (X, Y coordinates).
     * @param x Column (0-5 for your 6x12 RGB matrix)
     * @param y Row (0-11)
     * @param r Red Value (0-255)
     * @param g Green Value (0-255)
     * @param b Blue Value (0-255)
     * Note: Writes directly to the chip's PWM register.
     */
    void setPixel(uint8_t x, uint8_t y, uint8_t r, uint8_t g, uint8_t b);

    /**
     * Configure the mixing stream (Scaling) for white balance.
     * @param r_scale Scale for Red channel (0-255)
     * @param g_scale Scale for Green channel (0-255)
     * @param b_scale Scale for Blue channel (0-255)
     */
    void setScaling(uint8_t r_scale, uint8_t g_scale, uint8_t b_scale);

    /**
     * Advanced function to directly write a RAW record.
     */
    void writeRegister(uint8_t page, uint8_t reg, uint8_t value);
    uint8_t readRegister(uint8_t page, uint8_t reg);

private:
    uint8_t _csPin;
    SPIClass *_spiPort;
    uint8_t _currentPage; // To optimize and avoid sending page commands if we are already there
    uint8_t _rows; // number of rows
    uint8_t _cols; // number of columns

    /**
     * Change the internal page of the chip if necessary.
     * The SPI command byte includes the page ID.
     */
    void _setPage(uint8_t page);
    
    /**
     * Converts RGB X,Y coordinates to physical LED indices (0-215).
     * Assumes a standard R-G-B order in CS sinks.
     */
    void _getLedIndices(uint8_t x, uint8_t y, uint8_t &rIdx, uint8_t &gIdx, uint8_t &bIdx);
};

#endif