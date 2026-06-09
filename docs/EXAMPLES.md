# 🎓 WuaLeds AW20216S — Examples Guide

A guided tour of every example sketch in the [`examples/`](../examples/) folder,
ordered as a **learning path** and split into three levels. Each entry explains
what the sketch does, which API it teaches, and walks through the key parts of
its code.

> 🚀 New here? Read the **[Getting Started guide](GETTING_STARTED.md)** first,
> and keep the **[Manual / API Reference](MANUAL.md)** open while you read the
> code.

---

## 📑 Contents

**🟢 Level 1 — Basics (the brightness pipeline)**
1. [Basic](#1-basic) · 2. [ColorWheel](#2-colorwheel) · 3. [BrightnessFade](#3-brightnessfade) · 4. [WhiteBalance](#4-whitebalance)

**🟡 Level 2 — Graphics on the framebuffer**
5. [TextScroll](#5-textscroll) · 6. [IconViewer](#6-iconviewer) · 7. [GameOfLife](#7-gameoflife) · 8. [SpatialSine](#8-spatialsine) · 9. [FirePalette](#9-firepalette) · 10. [Pong](#10-pong)

**🔴 Level 3 — Hardware features & integration**
11. [breathing](#11-breathing) · 12. [MixedBreathing](#12-mixedbreathing) · 13. [PwmFrequencySweep](#13-pwmfrequencysweep) · 14. [RegisterDump](#14-registerdump) · 15. [MultiPanel](#15-multipanel) · 16. [VuMeter](#16-vumeter)

---

## 🧰 Before you start

Every sketch shares the same skeleton, so a few ideas are worth learning **once**:

- **Wiring & pins.** The examples target an ESP32 (SCK `18`, MOSI `23`, MISO
  `19`, CS `5`). Only `CS_PIN` is mandatory; change the `#define`s at the top to
  match your board. See the wiring tables in the [README](../README.md).
- **Open & upload.** Open `examples/<Name>/<name>.ino` in the Arduino IDE (or
  PlatformIO), select your board, and upload. Open the Serial Monitor at
  **115200 baud** — most sketches print status there.
- **The framebuffer + `show()`.** Drawing calls (`setPixel`, `fillScreen`,
  `clearScreen`) only change a 216-byte buffer in RAM. Nothing appears until you
  call `show()`, which flushes the whole buffer to the chip in one SPI burst.
- **The brightness pipeline.** Each LED's final brightness is roughly:
  `global current × per-channel scaling × per-pixel PWM`. Level 1 exists to make
  these three stages click.
- **Non-blocking timing.** Most loops avoid `delay()` and instead gate work with
  `millis()`, so input (e.g. Serial) stays responsive:

  ```cpp
  const uint32_t now = millis();
  if ((now - lastMs) < FRAME_MS) return; // not time yet
  lastMs = now;
  // ... draw one frame ...
  ```

---

# 🟢 Level 1 — Basics

Goal: get pixels on the panel and understand the three brightness stages.

## 1. Basic
📄 [`examples/Basic/Basic.ino`](../examples/Basic/Basic.ino)

**What it does.** A single red pixel travels across all 72 positions, then the
whole panel flashes blue and clears. The best first sketch.

**Teaches:** `begin()`, `setGlobalCurrent()`, `setScaling()`, `setPixel()`,
`fillScreen()`, `clearScreen()`, `show()`.

**How it works.** After `begin()` it sets a moderate master current and full
scaling, then walks the grid one pixel at a time:

```cpp
ledMatrix.setPixel(x, y, 255, 0, 0); // light it red in the buffer
ledMatrix.show();                     // flush -> visible
delay(50);
ledMatrix.setPixel(x, y, 0, 0, 0);   // turn it off again
ledMatrix.show();
```

The key takeaway is the **buffer → `show()`** cycle: every visible change needs
a `show()`. `fillScreen()` / `clearScreen()` are just whole-buffer versions of
the same idea.

**Try this:** change `255,0,0` to another color, or remove the "turn off" step
to leave a trail behind the moving pixel.

---

## 2. ColorWheel
📄 [`examples/ColorWheel/color_wheel.ino`](../examples/ColorWheel/color_wheel.ino)

**What it does.** Fills the entire panel with one color that smoothly sweeps the
rainbow (red → green → blue → red).

**Teaches:** generating color on the fly with an integer **HSV→RGB** conversion
(no lookup table), plus `fillScreen()` + `show()`.

**How it works.** A hue value `0..255` wraps around the color wheel. `hueToRgb()`
splits that range into three 85-wide sectors and ramps one channel up while
another ramps down:

```cpp
const uint8_t sector = hue / 85;        // 0,1,2 -> R→G, G→B, B→R
const uint8_t pos    = (hue % 85) * 3;  // 0..255 within the sector
// e.g. sector 0:  r = 255 - pos;  g = pos;  b = 0;
```

Each frame it converts the current hue, fills the panel and advances the hue
(`hue += HUE_STEP`), which wraps automatically because it is a `uint8_t`.

**Try this:** raise `HUE_STEP` for a faster cycle, or offset the hue per row to
turn the solid fill into a gradient.

---

## 3. BrightnessFade
📄 [`examples/BrightnessFade/brightness_fade.ino`](../examples/BrightnessFade/brightness_fade.ino)

**What it does.** Paints one fixed color **once** and fades the whole panel in
and out — by changing **only** the global current.

**Teaches:** what `setGlobalCurrent()` does in isolation, and how it differs from
PWM and scaling.

**How it works.** The color is written in `setup()` and never touched again:

```cpp
ledMatrix.fillScreen(COLOR_R, COLOR_G, COLOR_B);
ledMatrix.show();          // the only show() in the whole sketch
```

The loop just ramps a `level` up and down and pushes it to the master-current
register — no `fillScreen`, no `show`:

```cpp
ledMatrix.setGlobalCurrent(level); // animate brightness, framebuffer unchanged
```

This proves the **master current** stage sits *after* the per-pixel PWM: the
colors stay the same while the whole panel dims together.

**Try this:** lower `FADE_STEP` for a slower breath, or change the fixed color.

---

## 4. WhiteBalance
📄 [`examples/WhiteBalance/white_balance.ino`](../examples/WhiteBalance/white_balance.ino)

**What it does.** Shows full white and lets you trim the R/G/B channels live over
Serial until the white looks neutral.

**Teaches:** `setScaling()` (the per-channel white-balance stage) and reading
single-key commands from Serial.

**How it works.** The panel is filled white once; only the scaling changes.
Single-letter commands nudge each channel and re-apply:

```cpp
case 'r': scaleR = trim(scaleR, -TRIM_STEP); applyScaling(); break;
case 'R': scaleR = trim(scaleR, +TRIM_STEP); applyScaling(); break;
// ... g/G, b/B ...
// applyScaling() -> ledMatrix.setScaling(scaleR, scaleG, scaleB);
```

`setScaling()` writes the chip immediately and ignores the framebuffer, so the
change is visible without calling `show()`.

**Commands:** `r/R g/G b/B` (down/up), `p` print values, `f` reset to full.
Set the Serial Monitor to send a newline.

**Try this:** find the scaling that makes white look neutral on *your* panel and
reuse those numbers as your default `setScaling()` in other sketches.

---

# 🟡 Level 2 — Graphics on the framebuffer

Goal: build images and animations pixel by pixel, frame by frame.

## 5. TextScroll
📄 [`examples/TextScroll/text_scroll.ino`](../examples/TextScroll/text_scroll.ino)

**What it does.** Scrolls a text message horizontally like a marquee using a
built-in **3×5 pixel font**.

**Teaches:** rendering glyphs from a font table and animating by sliding a window
across a virtual canvas wider than the panel.

**How it works.** Each glyph is 5 rows of 3 bits (`FONT3X5`). The message is
treated as a strip `msgLen × 4` columns wide; a `scrollPos` slides a 6-column
window across it. For every on-screen column it finds the matching font column
and lights the set bits:

```cpp
const uint16_t vcol      = (scrollPos + x) % totalCols; // virtual column
const uint8_t  bits      = glyphColumnBits(glyph, colInChar); // 5 vertical bits
for (uint8_t row = 0; row < FONT_HEIGHT; row++)
  if (bits & (1u << row))
    ledMatrix.setPixel(x, Y_OFFSET + row, TEXT_R, TEXT_G, TEXT_B);
```

`Y_OFFSET` vertically centers the 5-tall text in the 12-row panel.

**Try this:** change `MESSAGE`, `TEXT_*` color or `SCROLL_MS` speed. Only A–Z,
0–9 and space are drawn; other characters render as blanks.

---

## 6. IconViewer
📄 [`examples/IconViewer/icon_viewer.ino`](../examples/IconViewer/icon_viewer.ino)

**What it does.** Cycles through a gallery of multi-color icons (heart, arrow,
check, cross, battery, exclamation), switching every couple of seconds.

**Teaches:** storing **static bitmaps** as data and mapping data → color through
a palette.

**How it works.** Icons are written as readable ASCII-art: one character per
pixel, where the character picks a color (`.`=off, `R`,`G`,`B`,`Y`…):

```cpp
const char ICONS[][HEIGHT][WIDTH+1] = {
  { "......", ".R..R.", "RRRRRR", /* ...heart... */ },
  /* ...more icons... */
};
```

`drawIcon()` walks every cell, turns its character into RGB with `charColor()`
and plots it. A `millis()` timer advances to the next icon. `ICON_COUNT` is
computed with `sizeof`, so adding an icon to the table is enough.

**Try this:** add your own 6×12 icon to `ICONS[]` (and a name to `ICON_NAMES`).

---

## 7. GameOfLife
📄 [`examples/GameOfLife/game_of_life.ino`](../examples/GameOfLife/game_of_life.ino)

**What it does.** Runs Conway's Game of Life on the 6×12 grid; reseeds with a
random soup when the colony dies or freezes.

**Teaches:** driving the panel from a **simulation in RAM** using **double
buffering**, and wrap-around (toroidal) neighbor lookups.

**How it works.** Two grids hold the current and next generation. Each cell counts
its 8 neighbors (edges wrap with modulo), applies the rules, and writes the
*next* buffer; the buffers are then swapped:

```cpp
const uint8_t n = countNeighbors(x, y);            // wraps at the edges
gridNext[y][x]  = alive ? (n == 2 || n == 3)       // survive
                        : (n == 3);                // birth
// after the loop: memcpy(gridCur, gridNext, ...);  // swap
```

`stepGeneration()` returns whether anything changed; the loop reseeds when it
freezes, empties, or exceeds `MAX_GEN` (small grids stall fast).

**Try this:** tune `SEED_PERCENT` and `GEN_MS`, or color cells by their state.

---

## 8. SpatialSine
📄 [`examples/SpatialSine/SpatialSine.ino`](../examples/SpatialSine/SpatialSine.ino)

**What it does.** An animated diagonal sine wave with a different phase per
color, producing a smooth scrolling rainbow plasma.

**Teaches:** building color from a **math function** (a sine lookup table) and
spatial phase offsets.

**How it works.** A 64-entry quarter-wave table reconstructs a full sine by
symmetry (`sin8_brightness()`). The phase of each pixel depends on its position
and time, and each channel is offset by ~120°:

```cpp
const uint8_t phase = t + x*kDx + y*kDy;   // space + time
r = sin8_brightness(phase + kOffR);        // 0
g = sin8_brightness(phase + kOffG);        // ~120°
b = sin8_brightness(phase + kOffB);        // ~240°
```

**Try this:** change `kDx`/`kDy` (wave direction) or `kSpeed` (animation rate).

---

## 9. FirePalette
📄 [`examples/FirePalette/fire_palette.ino`](../examples/FirePalette/fire_palette.ino)

**What it does.** A flame that rises from the bottom of the panel, white-hot at
the base and fading to dark red at the top.

**Teaches:** the **scalar field + palette** technique — keep one value (here
"heat") per pixel and map it to color, the opposite approach to SpatialSine's RGB
math.

**How it works.** The bottom row is reheated with random embers; each frame the
heat spreads upward (average of the cells below) and cools a little, then the
heat field is drawn through a fire palette:

```cpp
heat[base][x] = random(EMBER_MIN, EMBER_MAX + 1);   // fuel
// rows above: avg of below/left/right minus random cooling
heatToColor(heat[y][x], r, g, b);                   // black→red→yellow→white
```

**Try this:** raise `COOLING` for shorter flames, or swap `heatToColor()` for a
blue "ice" palette — the simulation is identical, only the colors change.

---

## 10. Pong
📄 [`examples/Pong/pong.ino`](../examples/Pong/pong.ino)

**What it does.** A self-playing game of Pong: a ball bounces between the side
walls while two AI paddles slide to catch it.

**Teaches:** simple **game physics** and collision handling with sub-pixel
(floating-point) positions.

**How it works.** The ball's position and velocity are floats, rounded to LEDs
only when drawing. Velocity reflects off walls and paddles, with a bit of angle
added by where it hits; the paddles chase the ball, capped by a max speed so they
can miss:

```cpp
ballX += velX; ballY += velY;             // integrate
if (ballX <= 0 || ballX >= W-1) velX = -velX; // walls
if (paddleCovers(topPaddleX, bx)) bounceOffPaddle(topPaddleX, +1); // hit
else { scoreBot++; serveBall(+1); }       // miss -> point + serve
```

**Try this:** lower `PADDLE_SPEED` to make the AI worse, or `SPEEDUP` to make
rallies accelerate faster.

---

# 🔴 Level 3 — Hardware features & integration

Goal: use the chip's dedicated hardware and connect the panel to the outside.

## 11. breathing
📄 [`examples/breathing/breathing.ino`](../examples/breathing/breathing.ino)

**What it does.** The whole panel breathes (fades in/out) using the chip's
**autonomous breathing engines** — once started, the MCU does nothing.

**Teaches:** `configureBreathing()`, `setBreathingBrightness()`,
`setPixelPatternRGB()`, `startBreathing()`.

**How it works.** The AW20216S has three breathing engines (PAT0/1/2). You set
their timing (rise/hold/fall/off) and brightness envelope, route each pixel's
channels to an engine, and start them:

```cpp
ledMatrix.configureBreathing(AwPattern::PAT0, 80, 10, 80, 10, true); // timing
ledMatrix.setBreathingBrightness(AwPattern::PAT0, 0x20, 0xE0);       // min/max
ledMatrix.setPixelPatternRGB(x, y, PAT0, PAT1, PAT2);                // route R/G/B
ledMatrix.startBreathing(AwPattern::PAT0);                           // go
```

The `loop()` is empty — the breathing runs entirely in hardware.

**Try this:** change the T0–T3 timings or the min/max range to speed up or
deepen the breath.

---

## 12. MixedBreathing
📄 [`examples/MixedBreathing/mixed_breathing.ino`](../examples/MixedBreathing/mixed_breathing.ino)

**What it does.** Splits the panel: the top band breathes by itself on the
hardware engines while the bottom band shows an MCU-driven scrolling rainbow —
both at once.

**Teaches:** `setChannelPattern()` (per-channel routing) and that PAT-driven and
PWM-driven pixels can coexist.

**How it works.** Each channel of each pixel is routed either to a breathing
engine or to direct PWM:

```cpp
if (y < SPLIT_ROW) {                       // top band: hardware breathing
  ledMatrix.setChannelPattern(x, y, AwChannel::R, AwPattern::PAT0);
  ledMatrix.setChannelPattern(x, y, AwChannel::G, AwPattern::PAT1);
  ledMatrix.setChannelPattern(x, y, AwChannel::B, AwPattern::PAT2);
} else {                                   // bottom band: direct PWM
  ledMatrix.setPixelPatternRGB(x, y, PWM, PWM, PWM);
}
```

The `loop()` only animates the bottom rows with `setPixel`/`show`; the top band
keeps breathing on its own with zero MCU load.

**Try this:** move `SPLIT_ROW`, or give the engines different timings for a
shifting color.

---

## 13. PwmFrequencySweep
📄 [`examples/PWMFrequencySweep/pwm_frequency_sweep.ino`](../examples/PWMFrequencySweep/pwm_frequency_sweep.ino)

**What it does.** Holds a steady 50% gray and steps the PWM frequency from
62.5 kHz down to 488 Hz, looping.

**Teaches:** `setPwmFrequency()` and the `AwPwmFreq` enum.

**How it works.** The PWM value sets a pixel's **duty cycle**; the PWM frequency
sets how fast that duty is switched. A table of frequencies is applied in turn:

```cpp
ledMatrix.setPwmFrequency(FREQS[index].freq, AwPwmPhase::PhaseDelay);
```

To your eyes the panel looks identical at every step. The difference shows up on
a **camera**: low frequencies cause visible flicker / rolling bands, high
frequencies look clean — which is why you raise the frequency when filming LEDs.

**Try this:** film the panel in slow-motion and watch the bands disappear as the
frequency rises.

---

## 14. RegisterDump
📄 [`examples/RegisterDump/register_dump.ino`](../examples/RegisterDump/register_dump.ino)

**What it does.** A diagnostic tool. It prints a report over Serial: a
communication check, a dump of the Page 0 config registers, the Open/Short fault
registers, and a write+read round-trip test. The report repeats every few
seconds.

**Teaches:** the **read** side of the chip — `readRegister()` / `writeRegister()`
— for debugging hardware.

**How it works.** There is no readable "chip ID" register, so the reliable
liveness test is reading **GCR** (which `begin()` leaves at `0xB1`):

```cpp
const uint8_t gcr = ledMatrix.readRegister(AW20216S_PAGE0, AW_REG_GCR);
// gcr == 0xB1  -> SPI link OK
```

It then dumps named registers and the OSR block (`0x03..0x26`), flagging any
non-zero byte as a possible open/short (the exact bit→LED mapping is
datasheet-specific). Finally it writes `0x55` to GCCR, reads it back to confirm
both directions work, and restores the original value.

**Try this:** disconnect MISO and watch the link check fail — exactly what you'd
see with a wiring fault.

---

## 15. MultiPanel
📄 [`examples/MultiPanel/multi_panel.ino`](../examples/MultiPanel/multi_panel.ino)

**What it does.** Drives **two** panels from one SPI bus and stitches them into a
single 12×12 canvas, with a rainbow flowing seamlessly across the seam.

**Teaches:** putting several chips on one bus (shared SCK/MOSI/MISO, separate CS)
and hiding the boundary behind helpers.

**How it works.** Two driver objects share `SPI` but use different CS pins. A
world-coordinate helper routes each pixel to the correct panel:

```cpp
AW20216S panelA(PANEL_H, PANEL_W, CS_PIN_A, SPI);
AW20216S panelB(PANEL_H, PANEL_W, CS_PIN_B, SPI);

void setPixelWorld(uint8_t wx, uint8_t wy, ...) {
  if (wx < PANEL_W) panelA.setPixel(wx, wy, ...);
  else              panelB.setPixel(wx - PANEL_W, wy, ...); // local x
}
// showAll() calls show() on both
```

The animation uses canvas coordinates `0..11` and never worries about the seam.

**Try this:** with a single panel connected, panel B reports "NOT detected" over
Serial and only the left half lights — harmless. Re-arrange the helpers for a
vertical (6×24) stack instead.

---

## 16. VuMeter
📄 [`examples/VuMeter/vu_meter.ino`](../examples/VuMeter/vu_meter.ino)

**What it does.** Turns the panel into a vertical VU meter: a level fills from the
bottom (green → yellow → red) with a white peak-hold marker that slowly falls.

**Teaches:** reacting to **external input in real time** — from Serial or an
analog mic/pot — plus VU "ballistics" (fast attack, slow release) and peak hold.

**How it works.** The level (0..100%) comes from a selectable source
(`USE_ANALOG_INPUT`). The display value jumps up instantly but eases down slowly,
and the peak marker holds before falling:

```cpp
if (targetPct >= dispPct) dispPct = targetPct;                 // fast attack
else dispPct = max((float)targetPct, dispPct - RELEASE_STEP);  // slow release
// peak holds for PEAK_HOLD_MS, then falls by PEAK_FALL
```

Over Serial, type a number `0..100` + ENTER to set the level (the parser also
accepts input with no line ending). The bar color comes from the height fraction
(`barColor()`): green low, amber mid, red top.

**Try this:** set `USE_ANALOG_INPUT 1` and wire a microphone or potentiometer to
`AUDIO_PIN`, then tune `AUDIO_FULLSCALE`.

---

## ➡️ Where to go next

- 📖 **[Manual / API Reference](MANUAL.md)** — every function and enum in detail.
- 🤖 **[Porting Skill](skill/SKILL.md)** — re-implement the driver on any MCU.
