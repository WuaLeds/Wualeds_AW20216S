# 🟥 WuaLeds AW20216S

### Arduino / PlatformIO SPI driver for the **AW20216S** RGB LED matrix controller

[![Framework](https://img.shields.io/badge/framework-Arduino-00979D)](https://www.arduino.cc/)
[![Bus](https://img.shields.io/badge/bus-SPI-blue)]()
[![License](https://img.shields.io/badge/license-MIT-green)]()
[![Version](https://img.shields.io/badge/version-0.2.5-orange)]()

> Driver for **LMX2** — a **6 × 12 RGB LED matrix** (216 LEDs) built around the **Awinic AW20216S** constant-current LED controller, driven over SPI from any Arduino-compatible MCU.

> 🆕 **New to the library?** Read the **[Getting Started guide](docs/GETTING_STARTED.md)** first, then keep the **[Manual / API Reference](docs/MANUAL.md)** handy.

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

## 📚 Documentation

Full guides live in the [`docs/`](docs/) folder:

| Document | What's inside |
|---|---|
| 🚀 **[Getting Started](docs/GETTING_STARTED.md)** | Install, wiring, your first sketch, common recipes and troubleshooting. **Start here.** |
| 📖 **[Manual / API Reference](docs/MANUAL.md)** | Every function, argument and enum explained, plus the brightness pipeline and coordinate system. |
| 🎓 **[Examples Guide](docs/EXAMPLES.md)** | Every example sketch explained in learning order across three levels, with code walkthroughs. |
| 🤖 **[Porting Skill](docs/skill/SKILL.md)** | AI-oriented reference to port / re-implement this driver on **any MCU** (ESP-IDF, Pico SDK, STM32 HAL, bare-metal, MicroPython, Rust…). The SPI protocol, register map and init sequence are platform-independent — only a tiny HAL changes. |

---

## 📂 Examples

The [`examples/`](examples/) folder contains ready-to-run sketches, ordered as a
learning path across three levels. Each one is fully explained — with code
walkthroughs — in the **[Examples guide](docs/EXAMPLES.md)**.

**Start here**

| Example | Description |
|---|---|
| 🔴 **[Basic](examples/Basic/Basic.ino)** | A red pixel running across the matrix + a full-screen blue flash. The best place to start. |
| 🌈 **[ColorWheel](examples/ColorWheel/color_wheel.ino)** | Fills the whole panel with a color that sweeps the rainbow, generated on the fly with an integer HSV→RGB conversion (no lookup table). |
| 💡 **[BrightnessFade](examples/BrightnessFade/brightness_fade.ino)** | Paints a fixed color once and fades it in/out using only `setGlobalCurrent()` — isolates the master-brightness stage. |
| ⚖️ **[WhiteBalance](examples/WhiteBalance/white_balance.ino)** | Shows full white and lets you trim R/G/B over Serial with `setScaling()` to calibrate the white point. |

**Graphics on the framebuffer**

| Example | Description |
|---|---|
| 🔤 **[TextScroll](examples/TextScroll/text_scroll.ino)** | Horizontal marquee text rendered with a built-in 3×5 pixel font, scrolled across a virtual canvas. |
| 🖼️ **[IconViewer](examples/IconViewer/icon_viewer.ino)** | A gallery of multi-color icons stored as editable ASCII-art bitmaps, cycling every couple of seconds. |
| 🦠 **[GameOfLife](examples/GameOfLife/game_of_life.ino)** | Conway's Game of Life on the 6×12 grid with a toroidal (wrap-around) world and auto-reseed when it stalls. |
| 🌈 **[SpatialSine](examples/SpatialSine/SpatialSine.ino)** | An animated spatial sine wave with per-channel phase offsets, producing a smooth scrolling rainbow. |
| 🔥 **[FirePalette](examples/FirePalette/fire_palette.ino)** | A rising flame built from a per-pixel "heat" field mapped to color through a fire palette. |
| 🏓 **[Pong](examples/Pong/pong.ino)** | A self-playing Pong: a bouncing ball with sub-pixel physics and two AI paddles. |

**Hardware features & integration**

| Example | Description |
|---|---|
| 🌬️ **[breathing](examples/breathing/breathing.ino)** | Uses the chip's autonomous breathing engines (PAT0–PAT2) to make R/G/B fade in and out with no MCU load. |
| 🫧 **[MixedBreathing](examples/MixedBreathing/mixed_breathing.ino)** | Mixes hardware breathing on one band with MCU-driven direct PWM on another, using per-channel `setChannelPattern()` routing. |
| 📷 **[PwmFrequencySweep](examples/PWMFrequencySweep/pwm_frequency_sweep.ino)** | Steps the PWM frequency from 62.5 kHz down to 488 Hz to show flicker on camera — the only example focused on `setPwmFrequency()`. |
| 🩺 **[RegisterDump](examples/RegisterDump/register_dump.ino)** | A Serial diagnostics tool: link check, config + Open/Short register dump and a write/read round-trip via `readRegister()`/`writeRegister()`. |
| 🧩 **[MultiPanel](examples/MultiPanel/multi_panel.ino)** | Drives two chips on one SPI bus (separate CS) as a single 12×12 canvas with a seamless rainbow. |
| 📊 **[VuMeter](examples/VuMeter/vu_meter.ino)** | A vertical VU meter fed by external input (Serial or analog mic/pot), with VU ballistics and a peak-hold marker. |

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
