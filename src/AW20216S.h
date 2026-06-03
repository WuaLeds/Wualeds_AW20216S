#ifndef AW20216S_H
#define AW20216S_H

#include <Arduino.h>
#include <SPI.h>
#include <string.h>

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

// --- General Enumerations ---

// Color channel inside an RGB pixel. The value is also the byte offset of the
// channel within the pixel triplet in the framebuffer (R first, then G, B).
enum class AwChannel : uint8_t {
    R = 0, // Red   channel (offset 0)
    G = 1, // Green channel (offset 1)
    B = 2  // Blue  channel (offset 2)
};

// --- PAGE 0: Enumerations ---

// PWM Frequency Options (PCCR Register) [page: 27]
enum class AwPwmFreq : uint8_t {
    High   = 0b000, // 62.5 kHz (max PWM freq)
    Hz62500 = 0b000,

    Hz32250 = 0b001,
    Hz15600 = 0b010,
    Hz7800  = 0b011,
    Hz3900  = 0b100,
    Hz1950  = 0b101,
    Hz977   = 0b110,

    Low    = 0b111, // 488 Hz (min PWM freq)
    Hz488  = 0b111
};

// PWM Phase Options (PCCR Register bits [1:0]). Controls how channel switching
// is staggered to spread current peaks and shape EMI.
enum class AwPwmPhase : uint8_t {
    PhaseDelay  = 0b00, // Default: phase-delayed switching (lowers peak current)
    PhaseInvert = 0b01, // Inverted phase
    ThreePhase2 = 0b10, // 3-phase mode, variant 2
    ThreePhase3 = 0b11  // 3-phase mode, variant 3
};

// --- PAGE 3: Breathing enums ---

// Breathing pattern assignment for a channel. PWM means the channel follows
// the PWM register directly; PAT0..PAT2 bind it to one of the three
// autonomous breathing engines configured via configureBreathing().
enum class AwPattern : uint8_t {
    PWM  = 0b00, // No breathing, direct PWM control
    PAT0 = 0b01, // Breathing engine 0
    PAT1 = 0b10, // Breathing engine 1
    PAT2 = 0b11  // Breathing engine 2
};

//******************************************************** */

// Supported cores with SPI bulk transfer
#if defined(ARDUINO_ARCH_SAMD) || defined(ARDUINO_ARCH_ESP32)
#define AW_HAS_SPI_BULK_TRANSFER 1
#else
#define AW_HAS_SPI_BULK_TRANSFER 0
#endif

// AVR (Uno/Leonardo): max SPI clock is typically F_CPU/2 => 8 MHz @ 16 MHz
#if defined(ARDUINO_ARCH_AVR)
#define AW_SPI_SPEED 8000000UL
#else
#define AW_SPI_SPEED 10000000UL // 10MHz Max SPI Speed [cite: 455]
#endif

#define AW_BASE_Y(y) (((uint8_t)(y) * 18u))
#define AW_BASE_X(x) (((uint8_t)(x) * 3u))
#define AW_BASE_INDEX(x, y) (uint8_t)(AW_BASE_Y(y) + AW_BASE_X(x))

#define AW_CMD_WRITE_PAGE(page) (uint8_t)(AW_CHIPID_SPI | ((page & 0x07) << 1) | 0x00)
#define AW_CMD_READ_PAGE(page) (uint8_t)(AW_CHIPID_SPI | ((page & 0x07) << 1) | 0x01)

// --- PATxCFG bits ---
#define AW_PATCFG_PATEN   (1u << 0)
#define AW_PATCFG_PATMD   (1u << 1)
#define AW_PATCFG_RAMPE   (1u << 2)
#define AW_PATCFG_SWITH   (1u << 3)
#define AW_PATCFG_LOGEN   (1u << 4)
#define AW_PATCFG_PATFLG  (1u << 5)

// PATx register helpers
#define AW_PAT_INDEX(pat) ((uint8_t)(pat) - 1u)  // PAT0->0, PAT1->1, PAT2->2
#define AW_PAT_T_BASE(idx) (uint8_t)( (uint8_t)AW_REG_PAT0T0 + ((uint8_t)(idx) * 4u))
#define AW_PAT_CFG_ADDR(idx) (uint8_t)( (uint8_t)AW_REG_PAT0CFG + (uint8_t)(idx))

//* AW20216S Class Definition */

class AW20216S
{
public:
    /**
     * @brief Construct the driver for one AW20216S device.
     *
     * Stores the geometry and SPI configuration but does NOT touch the
     * hardware yet; call begin() before any other method.
     *
     * @param rows    Number of rows (SWy lines) wired to the matrix.
     *                For the LMX2 6x12 RGB panel this is 12. Valid range 1-12.
     * @param cols    Number of columns (RGB triplets / CSx groups).
     *                For the LMX2 6x12 RGB panel this is 6. Valid range 1-6.
     * @param csPin   MCU GPIO used as Chip Select (active LOW).
     *                Typical values: 10 on Arduino UNO, 5 or 15 on ESP32.
     * @param spiPort SPI bus instance to drive the chip. Defaults to the
     *                board's primary `SPI`. Pass e.g. `SPI1` / `HSPI` on MCUs
     *                that expose several SPI peripherals.
     */
    AW20216S(uint8_t rows, uint8_t cols, uint8_t csPin, SPIClass &spiPort = SPI);

    /**
     * @brief Initialize the chip and bring it to a usable default state.
     *
     * Sets CS as output, starts the SPI bus, performs a software reset,
     * enables the device (GCR) and applies a safe default global current.
     *
     * @return true  if communication succeeded (GCR read back as expected).
     * @return false if the chip did not acknowledge (check wiring / CS pin).
     */
    bool begin();

    /**
     * @brief Perform a software reset, restoring power-on register defaults.
     *
     * Writes 0xAE to RSTN and blocks ~2 ms while the OTP reloads.
     */
    void reset();

    /**
     * @brief Clear the internal framebuffer (all pixels set to 0 / off).
     * @note RAM-only operation. Call show() to push the cleared buffer to the chip.
     */
    void clearScreen();

    /**
     * @brief Fill the whole framebuffer with a single RGB color.
     *
     * @param r Red   PWM value, 0 (off) - 255 (full).
     * @param g Green PWM value, 0 (off) - 255 (full).
     * @param b Blue  PWM value, 0 (off) - 255 (full).
     * @note RAM-only operation. Call show() to make it visible.
     */
    void fillScreen(uint8_t r, uint8_t g, uint8_t b);

    /**
     * @brief Set the global current (master brightness) shared by all LEDs.
     *
     * Writes the GCCR register immediately over SPI.
     *
     * @param current Master current, 0 (LEDs off) - 255 (maximum drive).
     *                Higher values increase brightness and power draw; keep
     *                it moderate (e.g. 0x40) to limit total consumption.
     */
    void setGlobalCurrent(uint8_t current);

    /**
     * @brief Write a single RGB pixel into the framebuffer.
     *
     * Out-of-range coordinates are ignored (no-op).
     *
     * @param x Column index, 0 - (cols-1) (0-5 on the 6x12 panel).
     * @param y Row index,    0 - (rows-1) (0-11 on the 6x12 panel).
     * @param r Red   PWM value, 0 (off) - 255 (full).
     * @param g Green PWM value, 0 (off) - 255 (full).
     * @param b Blue  PWM value, 0 (off) - 255 (full).
     * @note RAM-only operation. Call show() to push the buffer to the chip.
     */
    void setPixel(uint8_t x, uint8_t y, uint8_t r, uint8_t g, uint8_t b);

    /**
     * @brief Push the whole framebuffer to the chip (Page 1) in one burst.
     *
     * Sends the 216 PWM bytes in a single SPI transaction. Call this after
     * setPixel/fillScreen/clearScreen to make the changes visible.
     */
    void show();

    /**
     * @brief Set the per-channel scaling (current trim) for white balance.
     *
     * Applies the same scaling to every pixel and writes Page 2 immediately
     * over SPI (does not use the framebuffer).
     *
     * @param r_scale Red   channel scale, 0 (off) - 255 (no attenuation).
     * @param g_scale Green channel scale, 0 (off) - 255 (no attenuation).
     * @param b_scale Blue  channel scale, 0 (off) - 255 (no attenuation).
     * @note Final brightness ≈ globalCurrent x scaling x PWM. Set to
     *       (0xFF, 0xFF, 0xFF) for full output, then lower a channel to
     *       correct the white point.
     */
    void setScaling(uint8_t r_scale, uint8_t g_scale, uint8_t b_scale);

    /** PWM support */

    /**
     * @brief Low-level write of the raw PCCR register (PWM clock control).
     *
     * Reserved bits [4:2] are masked off before writing.
     *
     * @param pccr Raw register value. Bits [7:5] = PWM frequency divider,
     *             bits [1:0] = PWM phase. Prefer setPwmFrequency() unless
     *             you need direct register access.
     */
    void setPwmClock(uint8_t pccr);

    /**
     * @brief Configure the PWM frequency and phase via the PCCR register.
     *
     * @param freq  PWM frequency (AwPwmFreq), from High = 62.5 kHz down to
     *              Low = 488 Hz. Higher frequencies reduce visible flicker,
     *              especially when filming the panel.
     * @param phase PWM phase / delay scheme (AwPwmPhase). Default PhaseDelay
     *              staggers channels to lower peak current; the alternatives
     *              invert or split the phase across drivers.
     */
    void setPwmFrequency(AwPwmFreq freq, AwPwmPhase phase = AwPwmPhase::PhaseDelay);

    /** Breathing support */

    /**
     * @brief Configure the timing of one breathing engine (PATx) and enable it.
     *
     * Enables the engine in autonomous mode. The four phases form one breath
     * cycle: rise (T0) -> hold high (T1) -> fall (T2) -> hold low (T3).
     *
     * @param pat         Pattern engine to configure: PAT0, PAT1 or PAT2.
     *                    Passing PWM is ignored (no engine).
     * @param t0          Phase 0 rise time (dim -> bright), 0-255.
     * @param t1          Phase 1 on time (hold at max), 0-255.
     * @param t2          Phase 2 fall time (bright -> dim), 0-255.
     * @param t3          Phase 3 off time (hold at min), 0-255.
     *                    Larger values mean longer phases (slower breathing).
     * @param logarithmic true  -> logarithmic ramp (perceptually smoother);
     *                    false -> linear ramp (default).
     */
    void configureBreathing(
        AwPattern pat,
        uint8_t t0,
        uint8_t t1,
        uint8_t t2,
        uint8_t t3,
        bool logarithmic = false);

    /**
     * @brief Assign a single color channel of one pixel to a breathing pattern.
     *
     * Out-of-range coordinates are ignored (no-op).
     *
     * @param x   Column index, 0 - (cols-1) (0-5 on the 6x12 panel).
     * @param y   Row index,    0 - (rows-1) (0-11 on the 6x12 panel).
     * @param ch  Channel to drive: AwChannel::R, ::G or ::B.
     * @param pat Pattern to bind: PWM (direct control, no breathing),
     *            PAT0, PAT1 or PAT2.
     */
    void setChannelPattern(uint8_t x, uint8_t y, AwChannel ch, AwPattern pat);

    /**
     * @brief Assign breathing patterns to the R, G and B channels of one pixel.
     *
     * Convenience wrapper that calls setChannelPattern() for each channel.
     *
     * @param x    Column index, 0 - (cols-1) (0-5 on the 6x12 panel).
     * @param y    Row index,    0 - (rows-1) (0-11 on the 6x12 panel).
     * @param rPat Pattern for the Red   channel: PWM, PAT0, PAT1 or PAT2.
     * @param gPat Pattern for the Green channel: PWM, PAT0, PAT1 or PAT2.
     * @param bPat Pattern for the Blue  channel: PWM, PAT0, PAT1 or PAT2.
     */
    void setPixelPatternRGB(uint8_t x, uint8_t y, AwPattern rPat, AwPattern gPat, AwPattern bPat);

    /**
     * @brief Set the brightness envelope (min/max) of a breathing engine.
     *
     * @param pat  Pattern engine: PAT0, PAT1 or PAT2 (PWM is ignored).
     * @param minV Minimum brightness at the bottom of the breath, 0-255.
     * @param maxV Maximum brightness at the top of the breath, 0-255.
     *             Keep minV < maxV for a visible breathing effect.
     */
    void setBreathingBrightness(AwPattern pat, uint8_t minV, uint8_t maxV);

    /**
     * @brief Enable or disable a breathing pattern engine (writes PATxCFG).
     *
     * @param pat    Pattern engine: PAT0, PAT1 or PAT2.
     * @param enable true to enable the engine, false to disable it.
     */
    void enableBreathing(AwPattern pat, bool enable);

    /**
     * @brief Trigger / restart the breathing animation of a pattern engine.
     *
     * Sets the matching bit in the PATGO register.
     *
     * @param pat Pattern engine to start: PAT0, PAT1 or PAT2 (PWM ignored).
     */
    void startBreathing(AwPattern pat);

    /** Extra advanced functions */

    /**
     * @brief Low-level: write one raw byte to any register on any page.
     *
     * @param page  Target page: AW20216S_PAGE0..PAGE4 (0-4).
     * @param reg   Register address within the page (0x00-0xFF).
     * @param value Byte to write.
     */
    void writeRegister(uint8_t page, uint8_t reg, uint8_t value);

    /**
     * @brief Low-level: read one raw byte from any register on any page.
     *
     * @param page Target page: AW20216S_PAGE0..PAGE4 (0-4).
     * @param reg  Register address within the page (0x00-0xFF).
     * @return The byte currently stored in that register.
     */
    uint8_t readRegister(uint8_t page, uint8_t reg);

private:
    uint8_t _csPin;       // MCU GPIO used as Chip Select (active LOW)
    SPIClass *_spiPort;   // SPI bus instance driving the chip
    uint8_t _currentPage; // Last page selected, to skip redundant page commands
    uint8_t _rows;        // Number of rows (SWy lines), 1-12
    uint8_t _cols;        // Number of columns (RGB triplets), 1-6

    // Local framebuffer: 12 rows * 18 channels (6 R + 6 G + 6 B) = 216 bytes.
    uint8_t _frameBuffer[AW_MAX_LEDS];

#if AW_HAS_SPI_BULK_TRANSFER
    uint8_t _spiScratch[AW_MAX_LEDS]; // Copy buffer so the full-duplex bulk
                                      // transfer never clobbers _frameBuffer
#endif

    /**
     * @brief Burst-write a contiguous block of bytes to one page in a single
     *        SPI transaction, starting at register address 0x00.
     * @param page Target page (0-4).
     * @param data Source bytes to send.
     * @param len  Number of bytes to send.
     */
    void _writePageBurst(uint8_t page, const uint8_t *data, uint16_t len);

    /**
     * @brief Zero the whole framebuffer (RAM only, does not touch the chip).
     */
    inline void _clearFrameBuffer()
    {
        memset(_frameBuffer, 0, AW_MAX_LEDS);
    }
};

#endif // AW20216S_H