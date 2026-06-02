# TJR_241 — Waveshare ESP32-S3-Touch-AMOLED-2.41 CAN Dashboard

Port of TJR_mini targeting the larger 2.41" AMOLED board.  
Research notes: `/Users/tj/CANBUS/Resources/ESP32-S3-Touch-AMOLED-2.41/NOTES.md`  
RM690B0 driver analysis: `/Users/tj/CANBUS/Resources/ESP32-S3-Touch-AMOLED-2.41/RM690B0_driver/DRIVER_NOTES.md`

---

## Hardware — Waveshare ESP32-S3-Touch-AMOLED-2.41 (SKU 30563)

| Component | Detail |
|-----------|--------|
| MCU | ESP32-S3R8 — dual-core LX7, 240 MHz |
| RAM | 512 KB SRAM + 8 MB PSRAM |
| Flash | 16 MB |
| Display | 2.41" AMOLED, **600 × 450** px, QSPI |
| Display controller | **RM690B0** (Raydium) — NOT SH8601 (Waveshare demo has copy-paste error from 1.8" board) |
| Touch | FT6336 (FocalTech) — I2C addr 0x38 |
| IMU | QMI8658C — same as TJR_mini, I2C addr 0x6B |
| RTC | PCF85063, I2C |
| SD card | TF slot, SPI |

---

## Pin Assignments

### Display — QSPI (identical to TJR_mini)

| Signal | GPIO |
|--------|------|
| CS     | 9    |
| CLK    | 10   |
| D0     | 11   |
| D1     | 12   |
| D2     | 13   |
| D3     | 14   |
| RESET  | 21   |

### Touch & IMU — I2C (identical to TJR_mini)

| Signal    | GPIO |
|-----------|------|
| SDA       | 47   |
| SCL       | 48   |
| Touch RST | 3    |

### CAN Bus — moved from TJR_mini (GPIO 16/17 are battery pins on this board)

| Signal | GPIO |
|--------|------|
| CAN TX | 1    |
| CAN RX | 8    |

### Other

| Signal  | GPIO | Note |
|---------|------|------|
| BAT_CTL | 16   | Do not use for CAN |
| BAT_ADC | 17   | Do not use for CAN |
| SD CS   | 2    | |
| SD SCLK | 4    | |
| SD MOSI | 5    | |
| SD MISO | 6    | |
| UART TX | 43   | |
| UART RX | 44   | |

---

## Display Driver — TJR_display.h

Self-contained RM690B0 QSPI driver. No Arduino_GFX, no LVGL — raw ESP-IDF `spi_master` API.  
All behaviour below confirmed on hardware 2026-06-01/02.

### Pixel byte order — CRITICAL

The RM690B0 expects **big-endian RGB565**; the ESP32 is little-endian.  
`uint16_t 0x07E0` (green) is stored `[0xE0, 0x07]` in memory → panel sees `0xE007` → red.

**Rule: never write a C_xxx colour constant directly into `fb[]`.**  
Always call `fbColour(c)` (= `__builtin_bswap16(c)`) before writing any pixel.

### Framebuffers

```cpp
uint16_t *fb;    // landscape PSRAM buffer, 600×450 = 270,000 pixels
                 // Pixel (x, y): fb[y * NATIVE_W + x]
uint16_t *_fpb;  // second PSRAM buffer — render target for portrait (rot=1/3)
                 // and reverse-copy target for landscape USB-up (rot=2)
```

- Both allocated via `heap_caps_malloc(600*450*2, MALLOC_CAP_SPIRAM)` — **PSRAM=enabled is mandatory**
- `fbFlush()` handles all four orientations automatically
- `fbFill(colour)` / `fbColour(c)` / `fbFlush()` / `displaySetBrightness()` / `displaySetRotation()` / `displayClearScreen()` are the public API

### MADCTL — confirmed for all 4 rotations (2026-06-02)

The Waveshare panel is mounted **180° rotated** relative to LilyGO T4-S3. All MADCTL values differ.  
**Important:** MADCTL=0x60 (MV+MX) does NOT produce a clean 90° rotation from landscape data — it creates a non-rectangular scrambled mapping. Do not use it for portrait.

| rot | Physical position | MADCTL | off_x | off_y | Render target | Notes |
|-----|------------------|--------|-------|-------|---------------|-------|
| 0 | Landscape USB-down  | 0xA0 | 0  | 16 | `fb[]`   | Direct DMA |
| 1 | Portrait  USB-left  | 0xC0 | 16 | 0  | `_fpb[]` | Direct DMA — draw functions write portrait-row-major into `_fpb[]` |
| 2 | Landscape USB-up    | 0xA0 | 0  | 16 | `_fpb[]` | Reverse-copy `fb[]`→`_fpb[]`, then DMA |
| 3 | Portrait  USB-right | 0x00 | 16 | 0  | `_fpb[]` | Direct DMA — same draw path as rot=1; MADCTL=0x00 produces 180° flip |

### Portrait rendering architecture (rot=1 and rot=3)

Both portrait rotations write **directly into `_fpb[]`** in portrait-row-major order — no coordinate transform:

```
_fpb[ly * NATIVE_H + lx]   where lx ∈ [0, NATIVE_H-1=449], ly ∈ [0, NATIVE_W-1=599]
```

- `fbFill()`, `fbFillRect()`, `fbChar()` all use this layout for both rot=1 and rot=3
- `fbFlush()` for both: DMA `_fpb[]` directly with portrait window — no software transpose
- MADCTL=0xC0 (rot=1) vs MADCTL=0x00 (rot=3) produces the 180° visual difference between the two orientations
- `fbFlush()` uses `_rot` (not `g_isLandscape`) to select the buffer, since `g_isLandscape` isn't visible in `TJR_display.h`

**Performance (confirmed on hardware 2026-06-02):** all 4 orientations run at ~20fps at the 20 Hz cap (50ms budget). Uncapped measured: ~20fps landscape, ~20fps portrait — essentially identical.

### rot=2 (USB-up) 180° flip

`fbFlush()` for rot=2 reverse-copies `fb[]` into `_fpb[]` (forward write, no temp variable) then DMAs `_fpb[]` with MADCTL=0xA0. Draw code is identical to rot=0 — the flip is transparent to draw functions.

### Panel gutter

The 16px panel offset (off_x or off_y) means rows/columns 0..15 of the panel RAM are outside the normal address window and retain old data. `displayClearScreen()` does a full-panel write (zero offset, 450×600) to clear the gutter. Also called at init. Eliminates stale-pixel artifacts when switching orientations.

### Init sequence — 13 commands

Run twice (per LilyGO practice). Critical: the 0x5A/0x5B SWIRE commands are absent from the Waveshare demo and must be included.

```cpp
{0xFE, {0x20}, 0x01},  // SET PAGE
{0x26, {0x0A}, 0x01},  // MIPI OFF
{0x24, {0x80}, 0x01},  // SPI write RAM
{0x5A, {0x51}, 0x01},  // SWIRE for BV6804 — absent from Waveshare demo
{0x5B, {0x2E}, 0x01},  // SWIRE for BV6804 — absent from Waveshare demo
{0xFE, {0x00}, 0x01},  // SET PAGE
{0x3A, {0x55}, 0x01},  // 16bpp RGB565
{0xC2, {0x00}, 0x21},  // + delay 10ms
{0x35, {0x00}, 0x01},  // TE ON
{0x51, {0x00}, 0x01},  // Brightness = 0
{0x11, {0x00}, 0x80},  // Sleep Out + delay 120ms
{0x29, {0x00}, 0x20},  // Display On + delay 10ms
{0x51, {0xFF}, 0x01},  // Brightness = MAX
```

**SPI clock: 36 MHz** (Waveshare demo says 40 MHz — use 36).

---

## IMU — QMI8658C (confirmed, verbatim from TJR_mini)

Same chip, same raw I2C code, same axis→rotation mapping:

| Condition | Rotation |
|-----------|----------|
| ax > +0.6 | rot=0 — landscape USB-down |
| ax < -0.6 | rot=2 — landscape USB-up |
| ay < -0.6 | rot=1 — portrait USB-left |
| ay > +0.6 | rot=3 — portrait USB-right |

`g_isLandscape = !(newRot & 1)` — TJR_mini used `(newRot & 1)` because it was portrait-native. **Inverted here.**

---

## Touch — FT6336 (confirmed, all 4 rotations, 2026-06-02)

The touch sensor's coordinate space is fixed in landscape panel coordinates, independent of display rotation. The single formula below is correct for all rotations:

```cpp
int16_t rx = ((d[2] & 0x0F) << 8) | d[3];  // physical vertical:   0=top,  ~449=bottom
int16_t ry = ((d[4] & 0x0F) << 8) | d[5];  // physical horizontal: 0=right, ~599=left

tx = NATIVE_W - 1 - ry;   // landscape fb x
ty = rx;                   // landscape fb y
```

No `switch` on `g_rotation`. No scaling.

Touch coordinates are then transformed in `handleTouch()` (TJR_touch.h) to match the draw coordinate system for each rotation:

- rot=0: no transform — landscape fb coords match draw coords directly
- rot=1: `tx = NATIVE_H-1 - fty;  ty = ftx;`
- rot=2: `tx = NATIVE_W-1 - ftx;  ty = NATIVE_H-1 - fty;`  ← both axes inverted to match display flip
- rot=3: `tx = fty;  ty = NATIVE_W-1 - ftx;`

Physical corners: top-right=raw(~0,~0), bot-right=raw(~449,~0), top-left=raw(~0,~599), bot-left=raw(~449,~599).

---

## Resolution & Screen Real Estate

| Mode      | TJR_mini (CO5300) | TJR_241 (RM690B0) |
|-----------|-------------------|-------------------|
| Landscape | 456 × 280         | 600 × 450         |
| Portrait  | 280 × 456         | 450 × 600         |

The 2.41" has ~2.5× the pixel area of TJR_mini. All layout pixel math must be rederived from scratch.

---

## Large Value Fonts — Sprite Approach (implemented v0.10)

GFXglyph stores offsets as int8_t (−128..127) — safe ceiling ~160px.  
For the 2.41" we use **pre-rendered 1bpp digit sprites** at 220px — no size ceiling.

**Files:**
- `Tools/gen_digit_sprites.py` — Python/Pillow generator. Run once to regenerate the header:  
  `python3 Tools/gen_digit_sprites.py ShareTechMono-Regular.ttf 220 ShareTechMono_220px_sprites.h ShareTechMono_220px`  
  (TTF source is in TJR_mini/ — copy or symlink as needed)
- `TJR_spritefont.h` — `SpriteGlyph` / `SpriteFont` struct defs
- `ShareTechMono_220px_sprites.h` — generated 1bpp data (24 KB, 13 glyphs)

**Landscape cascade** (drawLandscape, TJR_draw.h):
1. **220px sprite** — fits strings ≤ 5 chars (usable threshold `LANDSCAPE_SPRITE_PAD=2`, so 596px)  
   Covers RPM "9999"/"10000", gear, voltage, temps <100°
2. **160px GFXfont (L1)** — 6-char fallback
3. **130px / 90px / 56px GFXfont (L2-L4)** — progressively shorter strings

**Blitter functions** (TJR_draw.h): `drawSpriteChar`, `printSpriteLeft`, `printSpriteCentred`.  
All use advance-based centering: `xAdvance × strlen` — never bounding-box width.

GFXglyph fonts (via `ttf2gfx.py`) for all other text: labels, units, MIN/MAX, settings, DTC.

**Rule: always use advance-based (mono) centering for numeric values.** Never `getTextBounds` width on a changing number — causes jiggle.

---

## What Ports Verbatim from TJR_mini

| Section | Notes |
|---------|-------|
| CAN task, Haltech frames | Verbatim — CAN_TX=1, CAN_RX=8 |
| Param structs, EEPROM layout, thresholds | Verbatim |
| IMU QMI8658 raw I2C | Verbatim — same chip |
| Unit conversions, colour rules | Verbatim |
| EEPROM debounce write | Verbatim |
| Gesture detection (swipe, hold-to-arm) | Copy, adjust px thresholds for larger screen |
| Layout / draw functions | Full rewrite — different resolution |
| Font sizes | Regenerate with ttf2gfx.py at larger px |
| Large value display | New sprite approach |

---

## Port Status

| File | Status | Notes |
|------|--------|-------|
| `TJR_types.h` | Done | Verbatim copy |
| `TJR_params.h` | Done | Verbatim copy |
| `TJR_can.h` | Done | CAN_TX=1, CAN_RX=8 |
| `TJR_simdata.h` | Done | Verbatim copy |
| `TJR_eeprom.h` | Done | Verbatim copy |
| `TJR_display.h` | **Done** | All hardware verified — all 4 orientations confirmed |
| `TJR_gfxfont.h` | **Done** | GFXfont/GFXglyph struct defs (no Arduino_GFX dep) |
| `TJR_layout.h` | **Done** | 600×450 geometry, font aliases |
| `TJR_draw.h` | **Done** | Full draw layer: fb primitives + GFXfont blitter + sprite blitter + gauge draw |
| `TJR_241.ino` | **Done v0.10** | Full loop: all subsystems wired, all orientations confirmed ~20fps |
| `TJR_touch.h` | **Done** | Gesture system, warn auto-jump, threshold drag, all 4 rotations confirmed |
| `TJR_settings.h` | **Done** | Units + brightness overlay, slider drag |
| `TJR_picker.h` | **Done** | Signal picker, alpha-sorted, scrollable |
| `TJR_graph.h` | **Done** | Rolling graph screen, 600×450 layout |
| `TJR_dtc.h` | **Done** | DTC/CEL page, 200+ code lookup table |
| `TJR_ota.h` | **Done** | WiFi AP OTA, YES/NO confirm, progress bar |
| `TJR_spritefont.h` | **Done** | SpriteGlyph/SpriteFont struct defs |
| `ShareTechMono_220px_sprites.h` | **Done** | 220px 1bpp sprite font, 13 glyphs, 24 KB |
| `Tools/gen_digit_sprites.py` | **Done** | Pillow-based generator — run to regenerate at any size |

> **Before committing any functional change:** bump `FW_VERSION` in `TJR_241.ino`.

---

## Compile & Flash

```bash
# Compile — CDCOnBoot=cdc required for USB serial output
arduino-cli compile \
  --fqbn "esp32:esp32:waveshare_esp32_s3_touch_amoled_241:PSRAM=enabled,PartitionScheme=min_spiffs,CDCOnBoot=cdc" \
  --export-binaries \
  /Users/tj/CANBUS/TJR_241

# Flash — port enumerates as usbmodem101 or usbmodem1101 depending on connection
ESPTOOL=~/Library/Arduino15/packages/esp32/tools/esptool_py/5.2.0/esptool
BUILD=/Users/tj/CANBUS/TJR_241/build/esp32.esp32.waveshare_esp32_s3_touch_amoled_241
$ESPTOOL --chip esp32s3 --port /dev/cu.usbmodem101 \
  --baud 921600 --before default-reset --after hard-reset \
  write-flash -z \
  0x0000 "$BUILD/TJR_241.ino.bootloader.bin" \
  0x8000 "$BUILD/TJR_241.ino.partitions.bin" \
  0xe000 "$BUILD/boot_app0.bin" \
  0x10000 "$BUILD/TJR_241.ino.bin"
```
