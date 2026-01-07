#include "AW20216S.h"
#include <string.h>

// Definitions of times (Datasheet Page 7 & 9)
#define AW_RESET_DELAY 2 // 2ms delay after reset [cite: 524]

//******************************************************** */
AW20216S::AW20216S(uint8_t rows, uint8_t cols, uint8_t csPin, SPIClass &spiPort)
{
    _csPin = csPin;
    _rows = rows;
    _cols = cols;
    _spiPort = &spiPort;
    _currentPage = 0xFF; // Invalid value to force update
     _clearFrameBuffer();
}

//******************************************************** */

bool AW20216S::begin()
{
    pinMode(_csPin, OUTPUT);
    digitalWrite(_csPin, HIGH);
    delay(20); // Small delay to ensure proper startup
    _spiPort->begin();

    // 1. Reset the chip via software to ensure a clean state [cite: 522]
    this->reset();
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

void AW20216S::reset()
{
    writeRegister(AW20216S_PAGE0, AW_REG_RSTN, AW_RST_CMD);
    delay(AW_RESET_DELAY); // Wait for OTP loading time [cite: 507]
}

//******************************************************** */

void AW20216S::clearScreen()
{
     _clearFrameBuffer();
}

//******************************************************** */

void AW20216S::fillScreen(uint8_t r, uint8_t g, uint8_t b)
{
    for (uint16_t i = 0; i < AW_MAX_LEDS; i += 3)
    {
        _frameBuffer[i + 0] = r;
        _frameBuffer[i + 1] = g;
        _frameBuffer[i + 2] = b;
    }
}

//******************************************************** */

void AW20216S::setGlobalCurrent(uint8_t current)
{
    // Configure the overall current for all LEDs [cite: 31]
    writeRegister(AW20216S_PAGE0, AW_REG_GCCR, current);
}

//******************************************************** */

void AW20216S::setPixel(uint8_t x, uint8_t y, uint8_t r, uint8_t g, uint8_t b)
{
    if (x >= _cols || y >= _rows)
        return;

    // Physical layout: 18 channels per row, RGB packed
    uint8_t *p = &_frameBuffer[AW_BASE_INDEX(x, y) ];

    *p++ = r;
    *p++ = g;
    *p = b;
}

//******************************************************** */

void AW20216S::show()
{
    _writePageBurst(AW20216S_PAGE1, _frameBuffer, AW_MAX_LEDS);
}

//******************************************************** */

void AW20216S::setScaling(uint8_t r_scale, uint8_t g_scale, uint8_t b_scale)
{
    // Configure the mix current (Page 2) for all pixels.
    // This is useful for overall white balance.
    // We go through all the LEDs.

    // Burst write all 216 scaling bytes (Page 2)
    _spiPort->beginTransaction(SPISettings(AW_SPI_SPEED, MSBFIRST, SPI_MODE0));
    digitalWrite(_csPin, LOW);

    const uint8_t commandByte = AW_CMD_WRITE_PAGE(AW20216S_PAGE2);

    _spiPort->transfer(commandByte);
    _spiPort->transfer(0x00); // Start address

    // Fill Page2 scaling registers in the same linear order as PWM:
    uint16_t i = 0;
    while (i < AW_MAX_LEDS)
    {
        _spiPort->transfer(r_scale);
        _spiPort->transfer(g_scale);
        _spiPort->transfer(b_scale);
        i += 3;
    }

    digitalWrite(_csPin, HIGH);
    _spiPort->endTransaction();
}

//******************************************************** */

void AW20216S::writeRegister(uint8_t page, uint8_t reg, uint8_t value)
{
    // SPI Command Byte Structure [cite: 541, 543]
    // Bit 7-4: ID (1010)
    // Bit 3-1: Page ID (0-4)
    // Bit 0:   W/R (0 = Write)

    uint8_t commandByte = AW_CMD_WRITE_PAGE(page);

    _spiPort->beginTransaction(SPISettings(AW_SPI_SPEED, MSBFIRST, SPI_MODE0));

    digitalWrite(_csPin, LOW);
    _spiPort->transfer(commandByte); // 1. Send command (ID + Page + Write)
    _spiPort->transfer(reg);         // 2. Send registration address
    _spiPort->transfer(value);       // 3. Send data
    digitalWrite(_csPin, HIGH);

    _spiPort->endTransaction();
}

//******************************************************** */

uint8_t AW20216S::readRegister(uint8_t page, uint8_t reg)
{
    // Structure for Reading [cite: 555]

    uint8_t commandByte = AW_CMD_READ_PAGE(page);
    uint8_t result = 0;

    _spiPort->beginTransaction(SPISettings(AW_SPI_SPEED, MSBFIRST, SPI_MODE0));

    digitalWrite(_csPin, LOW);
    _spiPort->transfer(commandByte);   // 1. Read Command
    _spiPort->transfer(reg);           // 2. Direction
    result = _spiPort->transfer(0x00); // 3. Read data (sends dummy 0x00)
    digitalWrite(_csPin, HIGH);

    _spiPort->endTransaction();

    return result;
}

//******************************************************** */

void AW20216S::_writePageBurst(uint8_t page, const uint8_t *data, uint16_t len)
{
    const uint8_t commandByte = AW_CMD_WRITE_PAGE(page);

    _spiPort->beginTransaction(SPISettings(AW_SPI_SPEED, MSBFIRST, SPI_MODE0));
    digitalWrite(_csPin, LOW);

    _spiPort->transfer(commandByte);
    _spiPort->transfer(0x00); // start address

#if AW_HAS_SPI_BULK_TRANSFER
    // Protect original buffer (SPI is full-duplex)
    memcpy(_spiScratch, data, len);
    _spiPort->transfer((void *)_spiScratch, (size_t)len);
#else
    // no bulk transfer so byte-wise transfer
    const uint8_t *p = data;
    while (len--)
    {
        _spiPort->transfer(*p++);
    }
#endif

    digitalWrite(_csPin, HIGH);
    _spiPort->endTransaction();
}