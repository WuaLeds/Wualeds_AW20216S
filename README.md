# 🟥 WuaLeds AW20216S

### Arduino / PlatformIO SPI driver for the **AW20216S** RGB LED matrix controller

[![Framework](https://img.shields.io/badge/framework-Arduino-00979D)](https://www.arduino.cc/)
[![Bus](https://img.shields.io/badge/bus-SPI-blue)]()
[![License](https://img.shields.io/badge/license-MIT-green)]()
[![Version](https://img.shields.io/badge/version-0.2.3-orange)]()

> Driver for **LMX2** — a **6 × 12 RGB LED matrix** (216 LEDs) built around the **Awinic AW20216S** constant-current LED controller, driven over SPI from any Arduino-compatible MCU.

---

## ✨ What is LMX2?

**LMX2** is the WuaLeds panel name for a **6 columns × 12 rows RGB matrix** (72 RGB pixels = **216 individual LEDs**) controlled by a single **AW20216S** chip.

The AW20216S handles all the heavy lifting in hardware:

- 🔆 **Per-LED 8-bit PWM** brightness (256 levels) for each of the 216 LEDs.
- 🎨 **Per-LED current scaling** (8-bit) for white balance / color calibration.
- 🌬️ **Autonomous breathing engines** (PAT0–PAT2): smooth rise/hold/fall/off cycles with linear or logarithmic ramps — no MCU intervention once started.
- ⚡ **Global current control** (master brightness) to cap power consumption.
- 🎚️ **Configurable PWM frequency** (488 Hz → 62.5 kHz) to remove flicker on camera.
- 🛡️ Built-in open/short detection, over-temperature and UVLO protection.

The chip uses **Time-Division Multiplexing**: it scans rows SW1–SW12 thousands of times per second, so the human eye perceives a steady image.

---

## 🚀 What can this library do?

| Feature | Method(s) |
|---|---|
| Initialize / reset the chip | `begin()`, `reset()` |
| Draw single pixels | `setPixel(x, y, r, g, b)` |
| Fill / clear the whole matrix | `fillScreen(r, g, b)`, `clearScreen()` |
| Push the framebuffer to the panel | `show()` |
| Master brightness | `setGlobalCurrent(value)` |
| White balance | `setScaling(r, g, b)` |
| PWM frequency / phase | `setPwmFrequency(freq, phase)` |
| Hardware breathing effects | `configureBreathing()`, `setBreathingBrightness()`, `setPixelPatternRGB()`, `startBreathing()` |
| Raw register access | `writeRegister()`, `readRegister()` |

The library keeps a **216-byte framebuffer in RAM**: drawing calls (`setPixel`, `fillScreen`, `clearScreen`) only touch RAM, and `show()` flushes everything to the chip in a single burst SPI transaction (fast, flicker-free updates).

---

## 🔌 Hardware: connecting to an MCU

The AW20216S is an **SPI slave (Mode 0, MSB-first, up to 10 MHz)**. You need **4 logic signals + power**:

| AW20216S pin | Direction | Description |
|---|---|---|
| **SCK / CLK** | MCU → chip | SPI clock |
| **MOSI / SDI** | MCU → chip | Data to the chip |
| **MISO / SDO** | chip → MCU | Data from the chip (needed for `readRegister` / `begin()` check) |
| **CS / NSS** | MCU → chip | Chip select (active **LOW**) |
| **VCC (logic)** | — | Logic supply (3.3 V typical) |
| **VLED** | — | LED supply rail |
| **GND** | — | Common ground (MCU ⟷ panel) |

> ⚠️ Make sure the MCU and the panel **share a common GND**, and that the LED rail can deliver enough current for 216 LEDs at full brightness. Keep `setGlobalCurrent()` moderate (e.g. `0x40`) to stay within budget.

### 📍 Wiring examples

Only **CS** is configurable in software; SCK / MOSI / MISO are the board's default SPI bus pins.

#### 🟦 ESP32 (default VSPI)

| Signal | ESP32 GPIO |
|---|---|
| SCK  | GPIO 18 |
| MOSI | GPIO 23 |
| MISO | GPIO 19 |
| CS   | GPIO 5 (any free GPIO) |
| VCC  | 3V3 |
| GND  | GND |

```cpp
#define CS_PIN 5
AW20216S ledMatrix(12, 6, CS_PIN);   // rows, cols, CS
```

#### 🟢 Raspberry Pi Pico (SPI0, Arduino-Pico core)

| Signal | Pico pin |
|---|---|
| SCK  | GP18 |
| MOSI (TX) | GP19 |
| MISO (RX) | GP16 |
| CS   | GP17 (any free GPIO) |
| VCC  | 3V3 (OUT) |
| GND  | GND |

```cpp
#define CS_PIN 17
AW20216S ledMatrix(12, 6, CS_PIN);
```

#### 🔵 Arduino UNO (hardware SPI)

| Signal | UNO pin |
|---|---|
| SCK  | D13 |
| MOSI | D11 |
| MISO | D12 |
| CS   | D10 (any free digital pin) |
| VCC  | 5V / 3V3 |
| GND  | GND |

```cpp
#define CS_PIN 10
AW20216S ledMatrix(12, 6, CS_PIN);
```

> 💡 On MCUs with several SPI peripherals you can pass an alternate bus:
> `AW20216S ledMatrix(12, 6, CS_PIN, SPI1);`

---

## ⚡ Quick start

```cpp
#include <SPI.h>
#include "AW20216S.h"

#define CS_PIN 10
AW20216S ledMatrix(12, 6, CS_PIN);   // 12 rows, 6 columns

void setup() {
  Serial.begin(115200);

  if (!ledMatrix.begin()) {
    Serial.println("AW20216S not detected — check wiring!");
    while (1) {}
  }

  ledMatrix.setGlobalCurrent(0x40);          // master brightness (~25%)
  ledMatrix.setScaling(0xFF, 0xFF, 0xFF);    // full white balance

  ledMatrix.setPixel(0, 0, 255, 0, 0);       // top-left pixel red
  ledMatrix.show();                          // flush to the panel
}

void loop() {}
```

---

## 📂 Examples

The [`examples/`](examples/) folder contains ready-to-run sketches:

| Example | Description |
|---|---|
| 🔴 **[Basic](examples/Basic/Basic.ino)** | A red pixel running across the matrix + a full-screen blue flash. The best place to start. |
| 🌬️ **[breathing](examples/breathing/breathing.ino)** | Uses the chip's autonomous breathing engines (PAT0–PAT2) to make R/G/B fade in and out with no MCU load. |
| 🌈 **[SpatialSine](examples/SpatialSine/SpatialSine.ino)** | An animated spatial sine wave with per-channel phase offsets, producing a smooth scrolling rainbow. |

---

## 🧠 How the chip works (key concepts)

### Multiplexed scanning
The chip does **not** light all LEDs at once. It activates one row (SW1…SW12) at a time, reads the PWM registers for that row, and sinks current through the right columns (CS1…CS18). At >20 kHz this looks like a constant image.

### Memory "Pages"
An 8-bit address can only reach 256 locations, but the AW20216S has **500+ registers**. They are split into pages, and the page number is embedded in the **first byte of every SPI command** (no separate "switch page" command needed):

| Page | Purpose |
|---|---|
| **Page 0** | Global config (enable, reset, current, PWM clock, breathing setup) |
| **Page 1** | PWM (brightness) — one register per LED |
| **Page 2** | Scaling (current) — white balance per LED |
| **Page 3** | Breathing pattern assignment per channel |
| **Page 4** | Virtual page: writes PWM **and** scaling at once (fast animations) |

---

## 🛠️ Things to improve (roadmap / known limitations)

- [ ]🐞 **`enableBreathing()` register indexing.** It computes the target register as `AW_REG_PAT0CFG + (uint8_t)pat`, but every other breathing method uses `AW_PAT_INDEX(pat)` (`pat − 1`). Since `PAT0 = 0b01`, `enableBreathing(PAT0)` writes **PAT1CFG** instead of PAT0CFG — an off-by-one that should be unified.
- [ ]🗺️ **Coordinate ↔ register mapping is assumed.** `AW_BASE_INDEX` hard-codes 18 channels per row; it should be validated against the real LMX2 wiring and made tolerant of panels smaller than 6×12.
- [ ]🎨 **No high-level graphics primitives.** Lines, rectangles, text and `Adafruit_GFX` compatibility would make it far more usable.
- [ ]🌈 **Add a color helper / `setPixel` overload** taking a packed `uint32_t` RGB or HSV input.
- [ ]⚙️ **`_currentPage` is stored but never used.** The page-skip optimization it was meant for is not implemented; either implement it or remove the field.
- [ ]🚀 **Use the Page 4 virtual page** in `show()`/a combined update to push PWM + scaling together for animations.
- [ ]🔁 **`setScaling` only supports a uniform value.** A per-pixel scaling API would allow real gamma / white-point correction.
- [ ]🧪 **No unit tests / CI build matrix** for AVR, ESP32 and RP2040.
- [ ]📖 **`begin()` returns false on a read mismatch** but offers no diagnostics; an error/status enum would help debugging wiring issues.

---

## 📜 License

MIT © [WuaLeds](https://wualeds.com/)

---

<p align="center"><b>Wualeds</b> · AW20216S Driver · 06/2026</p>
