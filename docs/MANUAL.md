# 📖 WuaLeds AW20216S — Manual / API Reference

Complete reference for the **AW20216S** library driving the **LMX2** 6 × 12 RGB
matrix (216 LEDs).

> 🚀 New here? Start with the **[Getting Started guide](GETTING_STARTED.md)**.

---

## 📑 Table of contents

- [Mental model](#-mental-model)
- [Coordinate system](#-coordinate-system)
- [Class construction](#-class-construction)
- [Lifecycle](#-lifecycle-begin--reset)
- [Drawing & framebuffer](#-drawing--framebuffer)
- [Brightness & color](#-brightness--color)
- [PWM frequency](#-pwm-frequency)
- [Breathing engines](#-breathing-engines)
- [Low-level register access](#-low-level-register-access)
- [Enumerations](#-enumerations)
- [Brightness pipeline](#-brightness-pipeline-how-a-pixel-gets-its-final-color)

---

## 🧠 Mental model

The driver keeps a **216-byte framebuffer in RAM** (12 rows × 6 columns × 3
channels). Two kinds of methods exist:

| Kind | Effect | Examples |
|---|---|---|
| **Buffered** | Change RAM only. Need `show()` to become visible. | `setPixel`, `fillScreen`, `clearScreen` |
| **Immediate** | Write the chip's registers over SPI right away. | `setGlobalCurrent`, `setScaling`, `setPwmFrequency`, all breathing methods, `writeRegister` |

👉 **Rule of thumb:** after any buffered drawing, call `show()`.

---

## 📐 Coordinate system

```
        x = 0 ........... x = 5   (columns, "cols" = 6)
      ┌───────────────────────┐
y = 0 │ ● ● ● ● ● ●           │
      │ ● ● ● ● ● ●           │
  ... │                       │
y =11 │ ● ● ● ● ● ●           │   (rows = 12)
      └───────────────────────┘
```

- **x** = column, range `0 … cols-1` (0–5 on LMX2).
- **y** = row, range `0 … rows-1` (0–11 on LMX2).
- Each pixel has **3 channels**: Red, Green, Blue (0–255 each).
- Out-of-range coordinates are **silently ignored** (no crash).

---

## 🏗️ Class construction

```cpp
AW20216S(uint8_t rows, uint8_t cols, uint8_t csPin, SPIClass &spiPort = SPI);
```

| Param | Meaning | Valid values |
|---|---|---|
| `rows` | Number of rows (SWy lines) | 1–12 (**12** for LMX2) |
| `cols` | Number of columns (RGB triplets) | 1–6 (**6** for LMX2) |
| `csPin` | GPIO used as Chip Select (active LOW) | any free pin |
| `spiPort` | SPI bus instance | `SPI` (default), `SPI1`, `HSPI`… |

```cpp
AW20216S ledMatrix(12, 6, 10);          // primary SPI
AW20216S ledMatrix(12, 6, 5, SPI1);     // alternate SPI bus
```

> ⚠️ **Order matters:** it's `(rows, cols, …)`. Constructing only caches config;
> the hardware is untouched until `begin()`.

---

## 🔄 Lifecycle: `begin` & `reset`

### `bool begin()`

Sets CS as output, starts the SPI bus, performs a software reset, enables the
chip and applies a safe default global current.

- **Returns** `true` if the GCR register reads back as expected (communication OK),
  `false` if the chip didn't answer — usually a wiring/CS/MISO problem.

```cpp
if (!ledMatrix.begin()) { /* handle error */ }
```

### `void reset()`

Issues a software reset (writes `0xAE` to RSTN), restoring all power-on defaults.
Blocks ~2 ms while the chip reloads its OTP. Called internally by `begin()`.

---

## 🖼️ Drawing & framebuffer

### `void setPixel(x, y, r, g, b)` — *buffered*

Write one RGB pixel into the framebuffer.

| Param | Range |
|---|---|
| `x` | 0 … cols-1 |
| `y` | 0 … rows-1 |
| `r`, `g`, `b` | 0 (off) … 255 (full) |

### `void fillScreen(r, g, b)` — *buffered*

Fill the whole framebuffer with a single color (`r`, `g`, `b` each 0–255).

### `void clearScreen()` — *buffered*

Set every pixel to 0 (off). Equivalent to `fillScreen(0, 0, 0)`.

### `void show()` — *immediate*

Flush the entire framebuffer (216 PWM bytes) to the chip (Page 1) in **one burst
SPI transaction**. Call this to make buffered changes visible.

```cpp
ledMatrix.fillScreen(0, 0, 255);
ledMatrix.show();   // <-- without this, nothing changes on the panel
```

---

## 🔆 Brightness & color

### `void setGlobalCurrent(current)` — *immediate*

Master brightness shared by **all** LEDs (writes GCCR).

| Param | Range | Notes |
|---|---|---|
| `current` | 0 (off) … 255 (max) | Higher = brighter **and** more power. Start around `0x40`. |

### `void setScaling(r_scale, g_scale, b_scale)` — *immediate*

Per-channel current trim applied uniformly to every pixel — used for **white
balance**. Writes Page 2 directly (does not use the framebuffer).

| Param | Range | Notes |
|---|---|---|
| `r_scale` / `g_scale` / `b_scale` | 0 (off) … 255 (no attenuation) | Lower one channel to correct the white point. |

```cpp
ledMatrix.setScaling(0xFF, 0xC8, 0xB0);  // warm white correction
```

---

## 🎚️ PWM frequency

### `void setPwmFrequency(freq, phase = PhaseDelay)` — *immediate*

Configure the PWM frequency/phase (PCCR register). Raise the frequency to remove
flicker when filming the panel.

| Param | Type | Values |
|---|---|---|
| `freq` | `AwPwmFreq` | `High` (62.5 kHz) … `Low` (488 Hz) — see [enums](#-enumerations) |
| `phase` | `AwPwmPhase` | `PhaseDelay` (default), `PhaseInvert`, `ThreePhase2/3` |

```cpp
ledMatrix.setPwmFrequency(AwPwmFreq::High);  // anti-flicker
```

### `void setPwmClock(pccr)` — *immediate, advanced*

Low-level raw write of the PCCR register. Bits `[7:5]` = frequency divider,
bits `[1:0]` = phase; reserved bits `[4:2]` are masked off. Prefer
`setPwmFrequency()` unless you need direct register control.

---

## 🌬️ Breathing engines

The chip has **3 autonomous breathing engines**: `PAT0`, `PAT1`, `PAT2`. Each runs
a 4-phase cycle on its own once started:

```
   brightness
   max  ┤      ___________
        │     /           \
        │    /             \
   min  ┤___/               \________
        └──T0──┼──T1──┼──T2──┼──T3──→ time
          rise   on     fall   off
```

### Typical setup order

```cpp
// 1) timing + enable
ledMatrix.configureBreathing(AwPattern::PAT0, 80, 10, 80, 10, true);
// 2) brightness envelope
ledMatrix.setBreathingBrightness(AwPattern::PAT0, 0x20, 0xE0);
// 3) bind pixels to the engine
ledMatrix.setPixelPatternRGB(x, y, AwPattern::PAT0, AwPattern::PAT0, AwPattern::PAT0);
// 4) start
ledMatrix.startBreathing(AwPattern::PAT0);
```

### `void configureBreathing(pat, t0, t1, t2, t3, logarithmic = false)`

Program the timings of an engine and enable it (autonomous mode).

| Param | Meaning | Range |
|---|---|---|
| `pat` | Engine: `PAT0`/`PAT1`/`PAT2` (`PWM` ignored) | — |
| `t0` | Rise time | 0–255 |
| `t1` | On (hold high) time | 0–255 |
| `t2` | Fall time | 0–255 |
| `t3` | Off (hold low) time | 0–255 |
| `logarithmic` | `true` = log ramp (smoother), `false` = linear | bool |

### `void setBreathingBrightness(pat, minV, maxV)`

Set the brightness envelope of an engine.

| Param | Range |
|---|---|
| `pat` | `PAT0`/`PAT1`/`PAT2` |
| `minV` | 0–255 (bottom of breath) |
| `maxV` | 0–255 (top of breath); keep `minV < maxV` |

### `void setChannelPattern(x, y, ch, pat)`

Bind **one channel** of one pixel to a breathing pattern.

| Param | Values |
|---|---|
| `x`, `y` | coordinates (out-of-range ignored) |
| `ch` | `AwChannel::R` / `::G` / `::B` |
| `pat` | `PWM` (direct control), `PAT0`, `PAT1`, `PAT2` |

### `void setPixelPatternRGB(x, y, rPat, gPat, bPat)`

Convenience wrapper: binds the R, G and B channels of a pixel in one call.

### `void startBreathing(pat)`

Trigger/restart the animation of an engine (sets its PATGO bit). `pat` =
`PAT0`/`PAT1`/`PAT2`.

### `void enableBreathing(pat, enable)`

Enable/disable an engine by writing its PATxCFG register.

> 🐞 **Known issue:** `enableBreathing()` currently indexes the config register as
> `PAT0CFG + (uint8_t)pat`, while the other methods use `pat − 1`. For `PAT0` this
> targets `PAT1CFG` instead of `PAT0CFG`. Prefer `configureBreathing()` /
> `startBreathing()` until this off-by-one is fixed (see the README roadmap).

---

## 🔧 Low-level register access

For advanced users who need direct access to the datasheet registers.

### `void writeRegister(page, reg, value)`

Write one byte to any register.

| Param | Range |
|---|---|
| `page` | 0–4 (`AW20216S_PAGE0` … `PAGE4`) |
| `reg` | 0x00–0xFF |
| `value` | 0x00–0xFF |

### `uint8_t readRegister(page, reg)`

Read one byte from any register. Returns the stored value.

```cpp
uint8_t gcr = ledMatrix.readRegister(AW20216S_PAGE0, AW_REG_GCR);
```

---

## 🔢 Enumerations

### `AwChannel` — color channel / byte offset

| Value | Channel |
|---|---|
| `AwChannel::R` (0) | Red |
| `AwChannel::G` (1) | Green |
| `AwChannel::B` (2) | Blue |

### `AwPwmFreq` — PWM frequency (PCCR)

| Value | Frequency |
|---|---|
| `High` / `Hz62500` | 62.5 kHz (max) |
| `Hz32250` | 32.25 kHz |
| `Hz15600` | 15.6 kHz |
| `Hz7800` | 7.8 kHz |
| `Hz3900` | 3.9 kHz |
| `Hz1950` | 1.95 kHz |
| `Hz977` | 977 Hz |
| `Low` / `Hz488` | 488 Hz (min) |

### `AwPwmPhase` — PWM phase scheme

| Value | Meaning |
|---|---|
| `PhaseDelay` | Default: phase-delayed switching (lowers peak current) |
| `PhaseInvert` | Inverted phase |
| `ThreePhase2` | 3-phase mode, variant 2 |
| `ThreePhase3` | 3-phase mode, variant 3 |

### `AwPattern` — breathing assignment

| Value | Meaning |
|---|---|
| `PWM` | No breathing, direct PWM control |
| `PAT0` | Breathing engine 0 |
| `PAT1` | Breathing engine 1 |
| `PAT2` | Breathing engine 2 |

---

## 🧪 Brightness pipeline (how a pixel gets its final color)

```
 final output ≈  Global Current  ×  Per-channel Scaling  ×  Per-LED PWM
                 (setGlobalCurrent)   (setScaling)            (setPixel)
```

- **PWM** (Page 1) — the per-LED value you set with `setPixel` (the "image").
- **Scaling** (Page 2) — per-channel current trim for white balance.
- **Global Current** (GCCR) — master cap on total brightness/power.

To get the brightest possible output: max global current, max scaling, PWM 255.
To dim everything while keeping colors: lower the **global current**.

---

<p align="center"><b>Wualeds</b> · AW20216S · Manual · 06/2026</p>
