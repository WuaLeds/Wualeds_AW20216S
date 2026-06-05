---
name: aw20216s-port
description: >-
  Reference + recipe for porting or re-implementing the WuaLeds AW20216S RGB
  LED-matrix driver on ANY MCU (ESP32 / ESP-IDF, Arduino/AVR, Raspberry Pi Pico
  SDK, STM32 HAL, nRF, bare-metal, MicroPython, Rust, etc.). Use this whenever a
  user asks an AI to "make this library work on <board/framework>", port the
  driver, or generate AW20216S control code outside the Arduino framework.
  Everything below the "Platform-independent core" line is identical on every
  MCU; only the HAL (SPI + 1 GPIO + delay) changes.
license: MIT
---

# AW20216S driver — universal porting skill

This document gives an AI everything it needs to generate a **working AW20216S
driver for any MCU**, in any language, without the original Arduino source.

The AW20216S is a constant-current RGB-matrix controller. The reference panel
**LMX2** is a **6 columns × 12 rows RGB matrix = 72 pixels = 216 LEDs**, driven
over **SPI** by one chip.

The driver is ~99% platform-independent: it is just *bytes pushed over SPI*. To
port it you only re-implement a tiny **HAL** (4 primitives). Do **not** change
the register values, command-byte format, page numbers, framebuffer layout or
init sequence below — they are the hardware contract.

---

## 0. How to use this skill (AI instructions)

When asked to port / re-implement the driver:

1. **Pick the target's HAL.** Identify how the target does: SPI transfer (byte
   and buffer), one GPIO output (the CS pin), and a millisecond delay. See
   §7 "HAL contract".
2. **Copy the constants verbatim** from §2 (registers) and §3 (geometry). Never
   recompute or "optimize" them.
3. **Reproduce the init sequence** from §4 exactly, in order.
4. **Reproduce the SPI framing** from §1 exactly (command byte, CS timing,
   address auto-increment).
5. **Implement the public API** from §6 with the same semantics (which calls are
   *buffered* vs *immediate*).
6. Mirror the platform-independent algorithms in §5 (framebuffer math, page-3
   bit packing, breathing register math) bit-for-bit.
7. Provide a minimal "blink one pixel" example for the target so the user can
   verify wiring (§8).

Keep the buffered/immediate split: drawing (`setPixel`/`fill`/`clear`) only
touches RAM; `show()` and all config/breathing calls hit SPI immediately.

---

## 1. SPI wire protocol (hardware contract — identical on every MCU)

- **SPI mode 0** (CPOL=0, CPHA=0), **MSB-first**.
- **Clock**: ≤ 10 MHz (use ≤ 8 MHz on AVR, where max is F_CPU/2).
- **CS / NSS**: active **LOW**. Pull LOW before a frame, HIGH after.
- One frame = one CS-low..CS-high window.

### Command byte (first byte of every frame)

```
bit:   7 6 5 4 | 3 2 1 | 0
       1 0 1 0  |  PAGE | R/W      (fixed ID nibble 0b1010 = 0xA)
```

- Fixed ID = `0xA0` (bits 7..4 = 1010).
- `PAGE` = 3-bit page number (0..4) in bits 3..1.
- `R/W` = bit 0: **0 = write**, **1 = read**.

Formulas (use exactly these):

```
WRITE_CMD(page) = 0xA0 | ((page & 0x07) << 1) | 0x00
READ_CMD(page)  = 0xA0 | ((page & 0x07) << 1) | 0x01
```

### Single-register WRITE frame
```
CS↓  [WRITE_CMD(page)]  [reg_addr]  [value]  CS↑
```

### Single-register READ frame
```
CS↓  [READ_CMD(page)]  [reg_addr]  [0x00 dummy → returns value]  CS↑
```
The byte clocked back while sending the 3rd (dummy) byte is the register value.

### Burst WRITE frame (the fast path used by `show()` / `setScaling`)
The chip **auto-increments** the register address. Send the address once, then
stream N data bytes:
```
CS↓  [WRITE_CMD(page)]  [start_addr]  [d0] [d1] [d2] … [dN-1]  CS↑
```
`show()` uses `page=1, start_addr=0x00`, streaming all 216 PWM bytes in one
transaction. `setScaling()` does the same on `page=2`.

> ⚠️ SPI is full-duplex: if your platform's "transfer buffer" call writes back
> into the TX buffer (e.g. Arduino `SPI.transfer(buf, len)`), copy the
> framebuffer into a scratch buffer first so the source isn't clobbered.

---

## 2. Register map (copy verbatim)

### Pages
| Page | # | Purpose |
|---|---|---|
| PAGE0 | 0 | Global config (enable, reset, current, PWM clock, breathing) |
| PAGE1 | 1 | PWM brightness — 1 byte per LED (216 bytes) |
| PAGE2 | 2 | Scaling / current trim — 1 byte per LED (216 bytes) |
| PAGE3 | 3 | Breathing pattern assignment — 2 bits per channel |
| PAGE4 | 4 | Virtual page: writes PWM **and** scaling together |

### Page 0 registers
| Name | Addr | Purpose |
|---|---|---|
| GCR   | 0x00 | Global Control: bit0 CHIPEN (enable), bits7..4 SWSEL (active rows) |
| GCCR  | 0x01 | Global Current Control (master brightness, 0–255) |
| DGCR  | 0x02 | De-ghost control |
| OTCR  | 0x27 | Over-temperature control |
| SSCR  | 0x28 | Spread-spectrum control |
| PCCR  | 0x29 | PWM Clock Control (frequency + phase) |
| UVCR  | 0x2A | UVLO control |
| SRCR  | 0x2B | Slew-rate control |
| RSTN  | 0x2F | Soft reset (write 0xAE) |
| PWMH0 | 0x30 | Breathing max brightness PAT0..2 (0x30,0x31,0x32) |
| PWML0 | 0x33 | Breathing min brightness PAT0..2 (0x33,0x34,0x35) |
| PAT0T0| 0x36 | Breathing timer base; engine *idx* base = 0x36 + idx*4 |
| PAT0CFG|0x42 | Breathing config; engine *idx* = 0x42 + idx (0x42,0x43,0x44) |
| PATGO | 0x45 | Breathing start/trigger (bit *idx* = start engine idx) |
| MIXCR | 0x46 | Mix function (enable Page 4 virtual writes) |
| SDCR  | 0x4D | SW drive capability |

### Magic constants
| Name | Value | Meaning |
|---|---|---|
| Chip ID nibble | `0xA0` | Fixed top of every command byte |
| Reset command  | `0xAE` | Write to RSTN to soft-reset |
| GCR enable value | `0xB1` | `1011 0001` = 12 active rows (SWSEL=1011) + CHIPEN=1 |
| MAX_LEDS | `216` | 12 rows × 18 channels |
| Default global current | `0x80` | ~50%; the README recommends starting near `0x40` |

### PCCR (0x29) layout
```
bits [7:5] = frequency divider     bits [1:0] = phase    bits [4:2] = reserved (write 0)
```
Build it: `PCCR = ((freq & 0x07) << 5) | (phase & 0x03)`.

Frequency codes (bits 7:5): `0b000`=62.5 kHz (max), `001`=32.25 kHz,
`010`=15.6 kHz, `011`=7.8 kHz, `100`=3.9 kHz, `101`=1.95 kHz, `110`=977 Hz,
`111`=488 Hz (min).
Phase codes (bits 1:0): `00`=phase-delay (default), `01`=invert,
`10`=3-phase v2, `11`=3-phase v3.

### PATxCFG bit fields (Page 0, 0x42+idx)
| Bit | Mask | Name | Meaning |
|---|---|---|---|
| 0 | 0x01 | PATEN  | Pattern enable |
| 1 | 0x02 | PATMD  | Autonomous mode |
| 2 | 0x04 | RAMPE  | Ramp enable |
| 3 | 0x08 | SWITH  | Switch |
| 4 | 0x10 | LOGEN  | Logarithmic ramp |
| 5 | 0x20 | PATFLG | Pattern flag |

`configureBreathing` clears PATEN|LOGEN|PATMD, then sets PATEN|PATMD (+ LOGEN if
logarithmic), via read-modify-write.

---

## 3. Geometry & framebuffer layout (copy verbatim)

- Framebuffer = **216 bytes** in RAM, all zero at power-up.
- Layout: **18 bytes per row** (6 columns × 3 channels), rows stacked.
- Channel order within a pixel: **R, G, B** (offsets 0,1,2).

Index math (the single source of truth for coordinate → buffer mapping):

```
base(x, y) = y*18 + x*3          // byte offset of pixel (x,y)'s Red channel
R = buf[base + 0]
G = buf[base + 1]
B = buf[base + 2]
```

- `x` = column, `0 … cols-1` (0–5 on LMX2).
- `y` = row,    `0 … rows-1` (0–11 on LMX2).
- Out-of-range `(x,y)` → **silently ignore** (no write, no crash).
- The Page-1 register address equals the buffer index, because the burst write
  starts at addr 0x00 and auto-increments. Same mapping for Page-2 scaling.

> The "18 channels/row" assumes the LMX2 wiring. If a panel is smaller than
> 6×12, this constant must be validated against the real wiring — keep it as a
> single named constant so a port can adjust it in one place.

---

## 4. Init sequence (`begin()` — reproduce exactly, in order)

```
1. Configure CS GPIO as output, drive it HIGH.
2. delay 20 ms.
3. Initialize SPI (mode 0, MSB-first, ≤10 MHz / ≤8 MHz on AVR).
4. reset():  write PAGE0 RSTN(0x2F) = 0xAE ;  delay 2 ms.
5. write PAGE0 GCR(0x00) = 0xB1            // enable chip, 12 active rows.
6. write PAGE0 GCCR(0x01) = 0x80           // default global current (~50%).
7. read  PAGE0 GCR(0x00) → expect 0xB1.    // returns true if it matches.
```

`begin()` returns success only if step 7 reads back `0xB1`. A mismatch means
wiring / CS / MISO problem.

**Typical user setup after `begin()`** (from the examples):
```
setGlobalCurrent(0x40);          // keep modest for power
setScaling(0xFF, 0xFF, 0xFF);    // full white balance
setPixel(...); show();           // or fillScreen(...); show();
```

---

## 5. Platform-independent algorithms (mirror bit-for-bit)

These are pure logic — no platform calls. Reproduce them identically.

### 5.1 fillScreen(r,g,b)
For `i = 0; i < 216; i += 3`: `buf[i]=r; buf[i+1]=g; buf[i+2]=b;`

### 5.2 clearScreen()  → memset(buf, 0, 216).

### 5.3 setPixel(x,y,r,g,b)
```
if (x >= cols || y >= rows) return;
i = y*18 + x*3;
buf[i]=r; buf[i+1]=g; buf[i+2]=b;
```

### 5.4 show()  → burst-write PAGE1, start addr 0x00, 216 bytes from buf.

### 5.5 setScaling(rs,gs,bs)  → burst-write PAGE2, start addr 0x00,
repeating the triplet (rs,gs,bs) 72 times (216 bytes).

### 5.6 Breathing engine index math
`AwPattern` enum: `PWM=0b00, PAT0=0b01, PAT1=0b10, PAT2=0b11`.
The *engine index* used for register offsets is `idx = pat - 1`
(PAT0→0, PAT1→1, PAT2→2). For `pat == PWM`, breathing calls are no-ops.

- timer base:  `0x36 + idx*4`   → write T0,T1,T2,T3 at base+0..+3.
- config addr: `0x42 + idx`.
- max bright:  `0x30 + idx` = maxV ;  min bright: `0x33 + idx` = minV.
- start: write PATGO(0x45) = `(1 << idx)`.

> Known bug to fix in a port: the original `enableBreathing()` used
> `0x42 + pat` (not `+ idx`), an off-by-one. In a clean port use `0x42 + idx`.

### 5.7 Page-3 pattern assignment (2 bits per channel)
Each LED channel (0..215) gets a 2-bit pattern field. 3 channels... actually
4 fields fit per byte; the original packs by:
```
led   = base(x,y) + channelOffset        // 0..215, channelOffset R=0,G=1,B=2
reg   = led / 3                           // PAGE3 register
shift = (led % 3) * 2                     // bit position of this channel's field
// read-modify-write so neighbouring channels survive:
v = readRegister(PAGE3, reg);
v = (v & ~(0x03 << shift)) | ((pat & 0x03) << shift);
writeRegister(PAGE3, reg, v);
```
Reproduce this exact packing so pattern assignment matches the chip.

### 5.8 Brightness pipeline (conceptual)
```
final_output ≈ GlobalCurrent(GCCR) × per-channel Scaling(Page2) × per-LED PWM(Page1)
```
Brightest output = max global current, max scaling, PWM 255. To dim while
keeping color, lower the global current.

---

## 6. Public API to reproduce (same names & semantics)

| Method | Kind | What it does |
|---|---|---|
| `begin()` → bool | immediate | §4 init; true if GCR reads back 0xB1 |
| `reset()` | immediate | write RSTN=0xAE, delay 2 ms |
| `setPixel(x,y,r,g,b)` | **buffered** | write one pixel into RAM |
| `fillScreen(r,g,b)` | **buffered** | fill RAM with one color |
| `clearScreen()` | **buffered** | zero RAM |
| `show()` | immediate | burst PAGE1, 216 bytes |
| `setGlobalCurrent(v)` | immediate | write GCCR(0x01)=v |
| `setScaling(rs,gs,bs)` | immediate | burst PAGE2 white balance |
| `setPwmFrequency(freq,phase)` | immediate | build & write PCCR |
| `setPwmClock(pccr)` | immediate | raw PCCR (mask reserved bits [4:2]) |
| `configureBreathing(pat,t0,t1,t2,t3,log)` | immediate | timers + enable engine |
| `setBreathingBrightness(pat,minV,maxV)` | immediate | PWMH/PWML envelope |
| `setChannelPattern(x,y,ch,pat)` | immediate | §5.7 one channel |
| `setPixelPatternRGB(x,y,rPat,gPat,bPat)` | immediate | §5.7 for R,G,B |
| `startBreathing(pat)` | immediate | PATGO = 1<<idx |
| `enableBreathing(pat,enable)` | immediate | write PATxCFG |
| `writeRegister(page,reg,value)` | immediate | raw 3-byte write frame |
| `readRegister(page,reg)` → byte | immediate | raw read frame |

**Constructor** caches `(rows, cols, csPin, spiPort)` and zeroes the
framebuffer; it must **not** touch hardware (that's `begin()`'s job).

> **Golden rule for users:** after any buffered draw, call `show()`.

---

## 7. HAL contract — the ONLY thing that changes per platform

Implement these 4 primitives for the target, then everything above is reusable:

| HAL primitive | Signature (conceptual) | Notes |
|---|---|---|
| `hal_spi_init()` | `void()` | mode 0, MSB-first, ≤10 MHz |
| `hal_spi_transfer(byte) → byte` | full-duplex 1-byte exchange | returns MISO byte |
| `hal_spi_write(buf, len)` | stream `len` bytes | optional fast path for bursts; else loop `hal_spi_transfer` |
| `hal_cs(level)` | drive CS GPIO HIGH/LOW | active LOW |
| `hal_delay_ms(ms)` | blocking delay | used by init/reset only |

Wrap every frame as: `hal_cs(LOW); …transfers…; hal_cs(HIGH);` (with the
platform's begin/end-transaction around it if it has one).

### 7.1 Mapping to common platforms

| Concept | Arduino | ESP-IDF | Pico SDK | STM32 HAL |
|---|---|---|---|---|
| SPI init | `SPI.begin()` + `SPISettings(speed,MSBFIRST,SPI_MODE0)` | `spi_bus_initialize` + `spi_bus_add_device` (mode 0) | `spi_init(spi0, 10e6)` + `spi_set_format(...,SPI_CPOL_0,SPI_CPHA_0,SPI_MSB_FIRST)` | `MX_SPIx_Init` with `CPOL=LOW,CPHA=1EDGE,FIRSTBIT=MSB` |
| 1-byte xfer | `SPI.transfer(b)` | `spi_device_polling_transmit` (tx+rx) | `spi_write_read_blocking` | `HAL_SPI_TransmitReceive` |
| buffer write | `SPI.transfer(buf,len)` | `spi_device_polling_transmit` (length*8 bits) | `spi_write_blocking` | `HAL_SPI_Transmit` |
| CS | `digitalWrite(cs,LOW/HIGH)` | `gpio_set_level` | `gpio_put` | `HAL_GPIO_WritePin` |
| delay | `delay(ms)` | `vTaskDelay(pdMS_TO_TICKS(ms))` | `sleep_ms` | `HAL_Delay` |

### 7.2 Reference wiring (CS is the only software-chosen pin)
| Signal | ESP32 | RPi Pico | Arduino UNO |
|---|---|---|---|
| SCK  | GPIO18 | GP18 | D13 |
| MOSI | GPIO23 | GP19 | D11 |
| MISO | GPIO19 | GP16 | D12 |
| CS   | GPIO5  | GP17 | D10 |
| VCC  | 3V3 | 3V3 | 5V/3V3 |

Share a common GND; power 216 LEDs from an external VLED rail, not the MCU 3V3.

---

## 8. Minimal port checklist & smoke test

A correct port must:
- [ ] Use command bytes `0xA0 | (page<<1) | rw` — verify with a logic analyzer
      if possible.
- [ ] Drive CS LOW for the whole frame, HIGH between frames.
- [ ] Run the §4 init and have `begin()`/equivalent read GCR back as `0xB1`.
- [ ] Map `(x,y)` to `y*18 + x*3` and stream 216 PWM bytes in `show()`.
- [ ] Keep drawing buffered; only `show()`/config/breathing hit SPI.

**Smoke test** (any platform): `begin()` → `setGlobalCurrent(0x40)` →
`setScaling(0xFF,0xFF,0xFF)` → `setPixel(0,0,255,0,0)` → `show()`. The top-left
pixel must light red. If `begin()` fails, check CS pin and MISO.

---

## 9. Pseudocode reference (language-neutral)

```text
WRITE_CMD(p) = 0xA0 | ((p & 7) << 1)
READ_CMD(p)  = 0xA0 | ((p & 7) << 1) | 1

writeRegister(page, reg, val):
    cs(LOW); spi(WRITE_CMD(page)); spi(reg); spi(val); cs(HIGH)

readRegister(page, reg) -> byte:
    cs(LOW); spi(READ_CMD(page)); spi(reg); v = spi(0x00); cs(HIGH); return v

burstWrite(page, data[], len):
    cs(LOW); spi(WRITE_CMD(page)); spi(0x00)
    for b in data[0..len): spi(b)           # or one buffered write
    cs(HIGH)

begin():
    cs_pin = OUTPUT; cs(HIGH); delay_ms(20); spi_init()
    writeRegister(0, 0x2F, 0xAE); delay_ms(2)     # reset
    writeRegister(0, 0x00, 0xB1)                  # enable, 12 rows
    writeRegister(0, 0x01, 0x80)                  # global current
    return readRegister(0, 0x00) == 0xB1

setPixel(x,y,r,g,b):
    if x>=cols or y>=rows: return
    i = y*18 + x*3; buf[i]=r; buf[i+1]=g; buf[i+2]=b

show():            burstWrite(1, buf, 216)
setGlobalCurrent(v): writeRegister(0, 0x01, v)
setScaling(rs,gs,bs):
    tmp = repeat([rs,gs,bs], 72)   # 216 bytes
    burstWrite(2, tmp, 216)
setPwmFrequency(freq, phase):
    writeRegister(0, 0x29, ((freq&7)<<5) | (phase&3))

# breathing (idx = pat-1, pat in {1,2,3}; pat==0/PWM -> no-op)
configureBreathing(pat,t0,t1,t2,t3,log):
    idx = pat-1; tbase = 0x36+idx*4; cfg = 0x42+idx
    writeRegister(0,tbase+0,t0); ...; writeRegister(0,tbase+3,t3)
    c = readRegister(0,cfg)
    c &= ~(0x01|0x10|0x02); c |= 0x01|0x02; if log: c |= 0x10
    writeRegister(0,cfg,c)
setBreathingBrightness(pat,minV,maxV):
    idx=pat-1; writeRegister(0,0x30+idx,maxV); writeRegister(0,0x33+idx,minV)
startBreathing(pat): idx=pat-1; writeRegister(0,0x45, 1<<idx)
```

---

## 10. Pitfalls (warn the user / avoid in generated code)

- **Forgot `show()`** → nothing changes; drawing is RAM-only.
- **Wrong SPI mode** → garbage or no response; it must be mode 0, MSB-first.
- **CS not held LOW for the full frame** → corrupted writes.
- **Full-duplex buffer clobber** → copy framebuffer to scratch before a buffered
  `SPI.transfer(buf,len)`-style call.
- **rows/cols swapped** in the constructor → wrong coordinates (order is
  `rows, cols`).
- **Under-powered VLED rail** → brownouts at high brightness; keep global
  current modest (`0x40`) and use an external LED supply.
- **`enableBreathing` off-by-one** in the original — use `0x42 + (pat-1)`.

---

<p align="center"><b>WuaLeds</b> · AW20216S · Porting skill · for AI code generation · 06/2026</p>
