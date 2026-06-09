# 🚀 Getting Started with WuaLeds AW20216S

A step-by-step guide for your **first time** using the **LMX2** panel (a 6 × 12 RGB
matrix driven by the **AW20216S** chip). No prior experience with the chip is
needed — just basic Arduino knowledge.

> 📖 Looking for the full description of every function? See the
> **[Manual / API Reference](MANUAL.md)**.

---

## 1. 📦 Install the library

The library is published in both the **PlatformIO Registry** and the **Arduino
Library Manager**, so you usually don't need to download anything manually.

> ✅ **No external dependencies.** It only uses the Arduino core's built-in
> `SPI` library, so there is nothing else to install.

### 🛠️ PlatformIO

Add it to your `platformio.ini` under `lib_deps`. You can **pin a version or not**:

```ini
lib_deps =
    ; 1) Latest compatible release (recommended) — registry shorthand "owner/name"
    wualeds/WuaLeds_AW20216S@^0.2.2     ; ^ = any 0.2.x or newer minor, < 1.0.0

    ; 2) An exact version (fully reproducible builds)
    ; wualeds/WuaLeds_AW20216S@0.2.3

    ; 3) Always the newest published version (no constraint)
    ; wualeds/WuaLeds_AW20216S

    ; 4) Straight from GitHub (e.g. to test unreleased code)
    ; https://github.com/WuaLeds/Wualeds_AW20216S.git
```

**Version syntax cheat-sheet:**

| Spec | Meaning |
|---|---|
| `@^0.2.2` | `>=0.2.2 <1.0.0` — newer patches/minors, no breaking change |
| `@~0.2.2` | `>=0.2.2 <0.3.0` — only newer patches |
| `@0.2.3` | exactly that version |
| *(omitted)* | latest available |

### 🔵 Arduino IDE

**Option A — Library Manager (recommended):**

1. **Tools → Manage Libraries…** (or `Ctrl+Shift+I`).
2. Search for **`WuaLeds AW20216S`**.
3. Pick a version in the dropdown (or leave the latest) and click **Install**.

**Option B — Manual ZIP:**

1. Download this repository as a ZIP.
2. **Sketch → Include Library → Add .ZIP Library…** and select the file.

In both cases the examples appear under **File → Examples → WuaLeds AW20216S**.

---

## 2. 🔌 Wire the panel to your board

The AW20216S talks **SPI**. You only need to connect 4 signals plus power.
**SCK, MOSI and MISO are fixed** by your board's SPI bus; **CS** is any free GPIO
you choose in code.

| Panel signal | What it is | ESP32 | RPi Pico | Arduino UNO |
|---|---|---|---|---|
| **SCK**  | SPI clock        | GPIO 18 | GP18 | D13 |
| **MOSI** | MCU → chip data  | GPIO 23 | GP19 | D11 |
| **MISO** | chip → MCU data  | GPIO 19 | GP16 | D12 |
| **CS**   | chip select (LOW)| GPIO 5  | GP17 | D10 |
| **VCC**  | logic power      | 3V3     | 3V3  | 5V / 3V3 |
| **VLED** | LED power rail   | ext.    | ext. | ext. |
| **GND**  | common ground    | GND     | GND  | GND |

> ⚠️ **Always share a common GND** between the MCU and the panel.
> The LED rail (VLED) must supply enough current for 216 LEDs — use a proper
> external supply, not the MCU's 3V3 pin, for full-brightness use.

---

## 3. 🟢 Your first sketch ("Hello, pixel")

Light up the top-left pixel in red:

```cpp
#include <SPI.h>
#include "AW20216S.h"

#define CS_PIN 10                       // change to match your wiring
AW20216S ledMatrix(12, 6, CS_PIN);      // 12 rows, 6 columns, CS pin

void setup() {
  Serial.begin(115200);

  // 1) Talk to the chip
  if (!ledMatrix.begin()) {
    Serial.println("AW20216S not detected — check wiring/CS pin!");
    while (1) {}                         // stop here on failure
  }

  // 2) Brightness setup
  ledMatrix.setGlobalCurrent(0x40);      // master brightness (~25%) — keep it low at first
  ledMatrix.setScaling(0xFF, 0xFF, 0xFF);// full white balance

  // 3) Draw + show
  ledMatrix.setPixel(0, 0, 255, 0, 0);   // x=0, y=0, red
  ledMatrix.show();                      // <-- nothing appears until you call show()
}

void loop() {}
```

### 🧠 The golden rule: **draw, then `show()`**

The library keeps a **framebuffer in RAM**. Drawing functions only change RAM —
the panel updates **only when you call `show()`**.

```cpp
ledMatrix.setPixel(1, 0, 0, 255, 0);   // RAM only
ledMatrix.setPixel(2, 0, 0, 0, 255);   // RAM only
ledMatrix.show();                       // now both pixels appear at once
```

---

## 4. 🎨 Common recipes

### Fill the whole screen

```cpp
ledMatrix.fillScreen(0, 0, 255);  // everything blue
ledMatrix.show();
```

### Clear the screen

```cpp
ledMatrix.clearScreen();          // all pixels off (RAM)
ledMatrix.show();                 // apply
```

### Animate a moving pixel

```cpp
void loop() {
  for (uint8_t y = 0; y < 12; y++) {
    for (uint8_t x = 0; x < 6; x++) {
      ledMatrix.setPixel(x, y, 255, 0, 0);
      ledMatrix.show();
      delay(50);
      ledMatrix.setPixel(x, y, 0, 0, 0); // turn it off before moving on
    }
  }
}
```

### Adjust brightness

```cpp
ledMatrix.setGlobalCurrent(0x20);  // dimmer / lower power
ledMatrix.setGlobalCurrent(0xFF);  // maximum (watch your power budget!)
```

---

## 5. 🌬️ Bonus: hardware breathing (no MCU load)

The chip has 3 built-in **breathing engines** (PAT0, PAT1, PAT2). Once started,
they animate on their own — your `loop()` can stay empty.

```cpp
// Configure timing: rise, on, fall, off  (+ logarithmic ramp)
ledMatrix.configureBreathing(AwPattern::PAT0, 80, 10, 80, 10, true);

// Min / max brightness of the breath
ledMatrix.setBreathingBrightness(AwPattern::PAT0, 0x20, 0xE0);

// Assign every pixel's RGB channels to PAT0
for (uint8_t y = 0; y < 12; y++)
  for (uint8_t x = 0; x < 6; x++)
    ledMatrix.setPixelPatternRGB(x, y, AwPattern::PAT0, AwPattern::PAT0, AwPattern::PAT0);

// Go!
ledMatrix.startBreathing(AwPattern::PAT0);
```

See the **[breathing example](../examples/breathing/breathing.ino)** for a full
RGB version using all three engines.

---

## 6. 📂 Where to go next

**Start here**
- 🔴 **[Basic example](../examples/Basic/Basic.ino)** — the simplest starting point.
- 🌈 **[ColorWheel example](../examples/ColorWheel/color_wheel.ino)** — full-panel rainbow from an integer HSV→RGB conversion.
- 💡 **[BrightnessFade example](../examples/BrightnessFade/brightness_fade.ino)** — fade in/out using only `setGlobalCurrent()`.
- ⚖️ **[WhiteBalance example](../examples/WhiteBalance/white_balance.ino)** — trim R/G/B over Serial to calibrate the white point.

**Graphics on the framebuffer**
- 🔤 **[TextScroll example](../examples/TextScroll/text_scroll.ino)** — scrolling marquee text with a 3×5 pixel font.
- 🖼️ **[IconViewer example](../examples/IconViewer/icon_viewer.ino)** — cycling gallery of ASCII-art color icons.
- 🦠 **[GameOfLife example](../examples/GameOfLife/game_of_life.ino)** — Conway's Game of Life with a wrap-around world.
- 🌈 **[SpatialSine example](../examples/SpatialSine/SpatialSine.ino)** — animated rainbow wave.
- 🔥 **[FirePalette example](../examples/FirePalette/fire_palette.ino)** — rising flame from a heat field + color palette.
- 🏓 **[Pong example](../examples/Pong/pong.ino)** — self-playing Pong with sub-pixel ball physics.

**Hardware features & reference**
- 🌬️ **[breathing example](../examples/breathing/breathing.ino)** — hardware breathing effects.
- 📖 **[Manual / API Reference](MANUAL.md)** — every function explained in detail.

---

## 7. 🆘 Troubleshooting

| Symptom | Likely cause |
|---|---|
| `begin()` returns `false` | Wrong CS pin, MISO not connected, or bad wiring. |
| Nothing lights up | Forgot `show()`, or `setGlobalCurrent()` is 0. |
| Very dim LEDs | Low `setGlobalCurrent()` **or** low `setScaling()` values. |
| Flicker on camera | Increase PWM frequency: `setPwmFrequency(AwPwmFreq::High);` |
| Wrong colors / coordinates | Check `rows`/`cols` order in the constructor (rows first!). |
| Brownout / resets | LED rail can't supply enough current — use an external supply. |

---

<p align="center"><b>Wualeds</b> · AW20216S · Getting Started · 06/2026</p>
