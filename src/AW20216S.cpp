#include "AW20216S.h"

// Definitions of times (Datasheet Page 7 & 9)
#define AW_SPI_SPEED      10000000 // 10MHz Max SPI Speed [cite: 455]
#define AW_RESET_DELAY    2        // 2ms delay after reset [cite: 524]

//******************************************************** */
AW20216S::AW20216S(uint8_t csPin, SPIClass &spiPort) {
    _csPin = csPin;
    _spiPort = &spiPort;
    _currentPage = 0xFF; // Invalid value to force update
}

//******************************************************** */

bool AW20216S::begin() {
    pinMode(_csPin, OUTPUT);
    digitalWrite(_csPin, HIGH);

    _spiPort->begin();

    // 1. Reset the chip via software to ensure a clean state [cite: 522]
    writeRegister(AW20216S_PAGE0, AW_REG_RSTN, AW_RST_CMD);
    delay(AW_RESET_DELAY); // Esperar tiempo de carga OTP [cite: 507]

    // 2. Chip Enable [cite: 530]
    // GCR register (0x00), Bit 0 (CHIPEN) = 1
    // Bit 4-7 (SWSEL) The default is 1011 (12 active rows), so we'll leave it like that.
    writeRegister(AW20216S_PAGE0, AW_REG_GCR, 0xB1); // 1011 0001

    // 3. Set global current to the maximum by default (or a safe value) [cite: 9]
    setGlobalCurrent(0x80); // 128/255 (~50% global current)

    // Simple verification: Read the GCR register to see if it saved the value
    uint8_t gcr = readRegister(AW20216S_PAGE0, AW_REG_GCR);
    return (gcr == 0xB1);
}

//******************************************************** */

void AW20216S::reset() {
    writeRegister(AW20216S_PAGE0, AW_REG_RSTN, AW_RST_CMD);
    delay(AW_RESET_DELAY);
}

//******************************************************** */

void AW20216S::setGlobalCurrent(uint8_t current) {
    // Configure the overall current for all LEDs [cite: 31]
    writeRegister(AW20216S_PAGE0, AW_REG_GCCR, current);
}

//******************************************************** */

void AW20216S::setPixel(uint8_t x, uint8_t y, uint8_t r, uint8_t g, uint8_t b) {
    if (x >= AW_WIDTH_RGB || y >= AW_HEIGHT) return;

    uint8_t rIdx, gIdx, bIdx;
    _getLedIndices(x, y, rIdx, gIdx, bIdx);

    // We write on PAGE 1 (PWM Registers) [cite: 598]
    // We could use PAGE 4 to write PWM+Scaling together, 
    // But for standard setPixel we use Page 1.
    
    // Note: Writing byte by byte is inefficient if the entire screen is being updated.
    // But for individual setPixel it is correct.
    writeRegister(AW20216S_PAGE1, rIdx, r);
    writeRegister(AW20216S_PAGE1, gIdx, g);
    writeRegister(AW20216S_PAGE1, bIdx, b);
}

//******************************************************** */

void AW20216S::setScaling(uint8_t r_scale, uint8_t g_scale, uint8_t b_scale) {
    // Configure the mix current (Page 2) for all pixels.
    // This is useful for overall white balance.
    // We go through all the LEDs.
    
    for (uint8_t y = 0; y < AW_HEIGHT; y++) {
        for (uint8_t x = 0; x < AW_WIDTH_RGB; x++) {
            uint8_t rIdx, gIdx, bIdx;
            _getLedIndices(x, y, rIdx, gIdx, bIdx);
            
            writeRegister(AW20216S_PAGE2, rIdx, r_scale);
            writeRegister(AW20216S_PAGE2, gIdx, g_scale);
            writeRegister(AW20216S_PAGE2, bIdx, b_scale);
        }
    }
}

//******************************************************** */

//* --- Private and Low-Level Methods ---

void AW20216S::_getLedIndices(uint8_t x, uint8_t y, uint8_t &rIdx, uint8_t &gIdx, uint8_t &bIdx) {
    // The chip has 18 physical columns (CS1-CS18) and 12 rows (SW1-SW12).
    // Logical mapping RGB 6x12 to Physical 18x12:
    // Pixel(0,0) -> SW1 + CS1(R), CS2(G), CS3(B)
    // The PWM register index is linear: (Row * 18) + Physical_Column
    
    uint8_t baseIndex = (y * 18) + (x * 3);
    
    // Assuming standard connection: CS1=R, CS2=G, CS3=B for the first pixel
    rIdx = baseIndex + 0;
    gIdx = baseIndex + 1;
    bIdx = baseIndex + 2;
}

//******************************************************** */

void AW20216S::writeRegister(uint8_t page, uint8_t reg, uint8_t value) {
    // SPI Command Byte Structure [cite: 541, 543]
    // Bit 7-4: ID (1010)
    // Bit 3-1: Page ID (0-4)
    // Bit 0:   W/R (0 = Write)
    
    uint8_t commandByte = AW_CHIPID_SPI | ((page & 0x07) << 1) | 0x00;

    _spiPort->beginTransaction(SPISettings(AW_SPI_SPEED, MSBFIRST, SPI_MODE0));
    
    digitalWrite(_csPin, LOW);
    _spiPort->transfer(commandByte); // 1. Send command (ID + Page + Write)
    _spiPort->transfer(reg);         // 2. Send registration address
    _spiPort->transfer(value);       // 3. Send data
    digitalWrite(_csPin, HIGH);
    
    _spiPort->endTransaction();
}

//******************************************************** */

uint8_t AW20216S::readRegister(uint8_t page, uint8_t reg) {
    // Structure for Reading [cite: 555]
    // Bit 0:   W/R (1 = Read)
    
    uint8_t commandByte = AW_CHIPID_SPI | ((page & 0x07) << 1) | 0x01;
    uint8_t result = 0;

    _spiPort->beginTransaction(SPISettings(AW_SPI_SPEED, MSBFIRST, SPI_MODE0));
    
    digitalWrite(_csPin, LOW);
    _spiPort->transfer(commandByte); // 1. Read Command
    _spiPort->transfer(reg);         // 2. Direction
    result = _spiPort->transfer(0x00); // 3. Read data (sends dummy 0x00)
    digitalWrite(_csPin, HIGH);
    
    _spiPort->endTransaction();
    
    return result;
}

//******************************************************** */