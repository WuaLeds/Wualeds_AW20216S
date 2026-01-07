#include "AW20216S.h"

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
    uint8_t *p = &_frameBuffer[AW_BASE_INDEX(x, y)];

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

void AW20216S::setPwmClock(uint8_t pccr)
{
    // Keep reserved bits [4:2] at 0
    pccr &= 0b11100011;
    writeRegister(AW20216S_PAGE0, AW_REG_PCCR, pccr);
}

void AW20216S::setPwmFrequency(AwPwmFreq freq, AwPwmPhase phase)
{
    const uint8_t pccr =
        (uint8_t)(((uint8_t)freq & 0x07) << 5) |
        (uint8_t)((uint8_t)phase & 0x03);

    setPwmClock(pccr);
}

//******************************************************** */

void AW20216S::setChannelPattern(uint8_t x, uint8_t y, AwChannel ch, AwPattern pat)
{
    if (x >= _cols || y >= _rows)
        return;

    const uint8_t base = AW_BASE_INDEX(x, y);          // channel base (R)
    const uint8_t led = (uint8_t)(base + (uint8_t)ch); // 0..215 (R/G/B channel)

    const uint8_t reg = (uint8_t)(AW_REG_PATG_BASE + (led / 3u));
    const uint8_t shift = (uint8_t)((led % 3u) * 2u);

    const uint8_t pat2 = ((uint8_t)pat) & 0x03u;

    uint8_t v = readRegister(AW20216S_PAGE3, reg);
    v &= (uint8_t)~(0x03u << shift);
    v |= (uint8_t)(pat2 << shift);
    writeRegister(AW20216S_PAGE3, reg, v);
}

void AW20216S::setPixelPatternRGB(uint8_t x, uint8_t y, AwPattern rPat, AwPattern gPat, AwPattern bPat)
{
    setChannelPattern(x, y, AwChannel::R, rPat);
    setChannelPattern(x, y, AwChannel::G, gPat);
    setChannelPattern(x, y, AwChannel::B, bPat);
}

void AW20216S::configureBreathing(
    AwPattern pat,
    uint8_t t0,
    uint8_t t1,
    uint8_t t2,
    uint8_t t3,
    bool logarithmic)
{
    if (pat == AwPattern::PWM)
        return;

    const uint8_t idx = AW_PAT_INDEX(pat);
    const uint8_t tBase = AW_PAT_T_BASE(idx);
    const uint8_t cfgAddr = AW_PAT_CFG_ADDR(idx);

    // T0–T3
    writeRegister(AW20216S_PAGE0, tBase + 0, t0);
    writeRegister(AW20216S_PAGE0, tBase + 1, t1);
    writeRegister(AW20216S_PAGE0, tBase + 2, t2);
    writeRegister(AW20216S_PAGE0, tBase + 3, t3);

    // Read-modify-write PATxCFG
    uint8_t cfg = readRegister(AW20216S_PAGE0, cfgAddr);

    // Clear controllable bits
    cfg &= ~(AW_PATCFG_PATEN |
             AW_PATCFG_LOGEN |
             AW_PATCFG_PATMD);

    // Enable breathing, autonomous mode
    cfg |= AW_PATCFG_PATEN | AW_PATCFG_PATMD;

    if (logarithmic)
    {
        cfg |= AW_PATCFG_LOGEN;
    }

    writeRegister(AW20216S_PAGE0, cfgAddr, cfg);
}

void AW20216S::setBreathingBrightness(AwPattern pat, uint8_t minV, uint8_t maxV)
{
    if (pat == AwPattern::PWM)
        return;
    const uint8_t idx = AW_PAT_INDEX(pat); // PAT0->0, PAT1->1, PAT2->2

    writeRegister(AW20216S_PAGE0, (uint8_t)(AW_REG_PWMH0 + idx), maxV);
    writeRegister(AW20216S_PAGE0, (uint8_t)(AW_REG_PWML0 + idx), minV);
}

void AW20216S::enableBreathing(AwPattern pat, bool enable)
{
    uint8_t addr = AW_REG_PAT0CFG + (uint8_t)pat;
    uint8_t cfg = enable ? 0x01 : 0x00;

    writeRegister(AW20216S_PAGE0, addr, cfg);
}

void AW20216S::startBreathing(AwPattern pat)
{
    if (pat == AwPattern::PWM)
        return;

    const uint8_t idx = AW_PAT_INDEX(pat);
    writeRegister(AW20216S_PAGE0, AW_REG_PATGO, (uint8_t)(1u << idx));
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