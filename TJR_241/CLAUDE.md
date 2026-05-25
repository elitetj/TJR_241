# TJR_241 — Waveshare ESP32-S3-Touch-AMOLED-2.41 CAN Dashboard

Port of TJR_mini targeting the larger 2.41" AMOLED board.  
Full research notes: `/Users/tj/CANBUS/Resources/ESP32-S3-Touch-AMOLED-2.41/NOTES.md`  
RM690B0 driver analysis: `/Users/tj/CANBUS/Resources/ESP32-S3-Touch-AMOLED-2.41/RM690B0_driver/DRIVER_NOTES.md`

---

## Hardware — Waveshare ESP32-S3-Touch-AMOLED-2.41 (SKU 30563)

| Component        | Detail                                                    |
|------------------|-----------------------------------------------------------|
| MCU              | ESP32-S3R8 — dual-core LX7, 240 MHz                      |
| RAM              | 512 KB SRAM + 8 MB PSRAM                                  |
| Flash            | 16 MB                                                     |
| Display          | 2.41" AMOLED, **600 × 450** px, QSPI                     |
| Display controller | **RM690B0** (Raydium) — NOT SH8601 (see gotcha below)  |
| Touch            | FT5x06 family (FT6336 / FT3168 — FocalTech)              |
| IMU              | QMI8658C — same as TJR_mini, I2C addr 0x6B               |
| RTC              | PCF85063, I2C                                             |
| SD card          | TF slot, SPI                                              |

---

## Pin Assignments

### Display — QSPI (identical to TJR_mini — easy win)

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

| Signal   | GPIO |
|----------|------|
| SDA      | 47   |
| SCL      | 48   |
| Touch RST | 3   |

### CAN Bus — CONFLICT with TJR_mini

GPIO 16 = BAT_Control, GPIO 17 = BAT_ADC on this board.  
TJR_mini used 16=TX, 17=RX — **these must move.**  
Candidate GPIOs for CAN TX/RX: **1, 8, 18, 19, 20** (or UART pins 43/44 if unused).

### Other

| Signal  | GPIO | Note                     |
|---------|------|--------------------------|
| BAT_CTL | 16   | Do not use for CAN       |
| BAT_ADC | 17   | Do not use for CAN       |
| SD CS   | 2    |                          |
| SD SCLK | 4    |                          |
| SD MOSI | 5    |                          |
| SD MISO | 6    |                          |
| UART TX | 43   |                          |
| UART RX | 44   |                          |

---

## Display Driver — RM690B0

### CRITICAL GOTCHA: Waveshare demo uses wrong driver

The demo zip uses `esp_lcd_sh8601` — this is a Waveshare copy-paste error from their 1.8" board.  
The SH8601 (Sitronix) and RM690B0 (Raydium) are different chips. **Do not use the SH8601 driver.**  
The RM690B0 chip is confirmed in the Waveshare product page (listed 3 times) and in the LilyGO T4-S3 library.

### Confirmed working driver: LilyGO T4-S3

`LilyGo-AMOLED-Series` library targets RM690B0 at 600×450 — exactly this chip.  
Source files are saved at `Resources/ESP32-S3-Touch-AMOLED-2.41/RM690B0_driver/`.  
**To port: change pin numbers only.** Init sequence, offsets, MADCTL values are chip-level and transfer directly.

### Init sequence (13 commands — rm690b0_cmd)

```cpp
{0xFE, {0x20}, 1},            // SET PAGE 0x20
{0x26, {0x0A}, 1},            // MIPI OFF
{0x24, {0x80}, 1},            // SPI write RAM
{0x5A, {0x51}, 1},            // SWIRE for BV6804 — MISSING from Waveshare demo!
{0x5B, {0x2E}, 1},            // SWIRE for BV6804 — MISSING from Waveshare demo!
{0xFE, {0x00}, 1},            // SET PAGE 0x00
{0x3A, {0x55}, 1},            // Pixel format: 16bpp RGB565
{0xC2, {0x00}, delay_ms(10)},
{0x35, {0x00}, 1},            // TE ON
{0x51, {0x00}, 1},            // Brightness = 0
{0x11, {0x00}, delay_ms(120)}, // Sleep Out
{0x29, {0x00}, delay_ms(10)}, // Display On
{0x51, {0xFF}, 1},            // Brightness = MAX
```

Brightness at runtime: send command `0x51` with value 0x00–0xFF.

### SPI clock

**36 MHz** (from LilyGO source). The Waveshare SH8601 demo said 40 MHz — use 36.

### Panel offset (confirmed from LilyGO setAddrWindow source)

16px offset applied to non-active axis in all CASET/RASET commands:

| Rotation      | Width | Height | offset_x | offset_y | MADCTL |
|---------------|-------|--------|----------|----------|--------|
| Portrait up   | 450   | 600    | **16**   | 0        | 0x00   |
| Landscape CW  | 600   | 450    | 0        | **16**   | 0xA0   |
| Portrait 180° | 450   | 600    | **16**   | 0        | 0xC0   |
| Landscape CCW | 600   | 450    | 0        | **16**   | 0x60   |

Full-frame flush examples:

```cpp
// Portrait (450×600): CASET xs=16, xe=465; RASET ys=0, ye=599
esp_lcd_panel_draw_bitmap(panel, 0, 0, 450, 600, fb);
// internally adds offset — see driver source

// Landscape (600×450): CASET xs=0, xe=599; RASET ys=16, ye=465
esp_lcd_panel_draw_bitmap(panel, 0, 0, 600, 450, fb);
```

### MADCTL rotation values

```cpp
// Portrait upright (450w × 600h)  — MADCTL 0x00
// Landscape CW    (600w × 450h)   — MADCTL 0xA0  (MV | MY)
// Portrait 180°   (450w × 600h)   — MADCTL 0xC0  (MX | MY)
// Landscape CCW   (600w × 450h)   — MADCTL 0x60  (MX | MV)
```

---

## Graphics Stack

No Arduino_GFX on this board — the demo only ships LVGL v8. Display uses Espressif `esp_lcd` C API directly.

### Framebuffer approach (skip LVGL, match TJR_mini pattern)

```cpp
// 1. Allocate full PSRAM framebuffer (600×450 = 540,000 pixels, 1.08 MB @ 16bpp)
uint16_t *fb = (uint16_t*)heap_caps_malloc(600 * 450 * 2, MALLOC_CAP_SPIRAM);

// 2. Draw into fb directly (same logic as TJR_mini, adapted for new resolution)

// 3. Flush each frame
esp_lcd_panel_draw_bitmap(panel_handle, 0, 16, 600, 450+16, fb);
// Note: offset_y=16 baked into y coordinates for landscape
```

This replaces `Arduino_Canvas` + `gfx->flush()` from TJR_mini.

---

## Resolution & Screen Real Estate

| Mode       | TJR_mini (CO5300) | TJR_241 (RM690B0) |
|------------|-------------------|-------------------|
| Portrait   | 280 × 456         | 450 × 600         |
| Landscape  | 456 × 280         | 600 × 450         |

The 2.41" is **landscape-native** (physically wider than tall).  
In portrait mode: 450 wide × 600 tall — ~2.5× the pixel area of TJR_mini portrait.  
All layout pixel math must be rederived from scratch.

---

## Large Value Fonts — Sprite Approach

GFXglyph stores offsets as int8_t (−128..127). Safe ceiling for digit fonts is ~160px.  
For the 2.41" we want 200px+ main values — **use pre-rendered 1bpp digit sprites instead.**

### Plan

1. **`Tools/gen_digit_sprites.py`** — Python/Pillow script:
   - Input: `ShareTechMono-Regular.ttf` + desired px size
   - Characters: `0123456789.- ` (13 chars only)
   - Output: 1bpp packed bitmap C header file
   - Flash cost: ~50 KB total at 140×220px — tiny

2. **`drawDigit(char c, int x, int y, uint16_t colour)`** — blitter:
   - Reads 1bpp bitmap for the character
   - Sets pixels in PSRAM framebuffer where bits are set
   - Colourises at render time

3. **`printCentredMono()`** equivalent — trivial since monospace = fixed-width

GFXglyph fonts (via `ttf2gfx.py`) still used for labels, units, MIN/MAX footer, settings menu.  
Only the main large value uses sprites.

**Rule: always use advance-based (mono) centering for numeric values — no exceptions.**  
Never use tight bounding-box width (`getTextBounds`) to centre a changing number. Even in a monospace font the bounding box varies between strings (e.g. "1111" vs "9999"), causing the value to jiggle horizontally as digits change. Use `xAdvance × charCount` for the width instead. This applies to both the sprite blitter path and any GFXglyph fallback. Reserve bounding-box centering for static labels only (headings, unit strings, settings text).

---

## What Ports from TJR_mini

| Section                                  | Effort                                          |
|------------------------------------------|-------------------------------------------------|
| CAN task (TWAI driver, Haltech frames)   | Copy verbatim                                   |
| Param structs, thresholds, EEPROM layout | Copy verbatim                                   |
| IMU auto-rotation (QMI8658 raw I2C)      | Copy verbatim — same chip                       |
| Unit conversions (°C/°F, kPa/psi, λ/AFR) | Copy verbatim                                  |
| Colour rules (warn/normal/night)         | Copy verbatim                                   |
| EEPROM debounce write logic              | Copy verbatim                                   |
| Gesture detection (swipe, hold-to-arm)   | Copy, adjust px thresholds for larger screen    |
| Touch driver (FT6336 vs FT3168)          | Minor — same FocalTech family, check registers  |
| Touch coordinate transforms              | Rederive — expect iteration on device           |
| Display init                             | New — RM690B0 driver (see above)                |
| Layout / draw functions                  | Full rewrite — different resolution              |
| Font sizes                               | Regenerate with `ttf2gfx.py`, larger px         |
| Large value display                      | New sprite approach (see above)                 |

---

## Compile Command

```bash
# Board: Espressif ESP32-S3 Dev Module (or equivalent)
# PSRAM: OPI PSRAM enabled
# Flash size: 16MB
arduino-cli compile --fqbn esp32:esp32:esp32s3 TJR_241
arduino-cli upload  --fqbn esp32:esp32:esp32s3 --port /dev/cu.usbmodem* TJR_241
```

---

## TODO — Phase 1 (Foundation)

- [ ] Display init with RM690B0 driver (esp_lcd QSPI, Waveshare pins)
- [ ] PSRAM framebuffer alloc + flush test (white screen / colour fill)
- [ ] Touch init with `esp_lcd_touch_ft5x06`, derive coordinate transforms
- [ ] IMU raw I2C (copy from TJR_mini, confirm same QMI8658 register map)
- [ ] Portrait + landscape orientation switching

## TODO — Phase 2 (Port TJR_mini features)

- [ ] `Tools/gen_digit_sprites.py` — generate 1bpp digit sprite headers
- [ ] `drawDigit()` blitter + `printCentredMono()` equivalent
- [ ] Param structs + EEPROM (copy from TJR_mini, update CAN GPIO)
- [ ] CAN task — TWAI driver on new GPIO pair (avoid 16/17)
- [ ] Gauge layout — portrait (2 stacked values) + landscape (single large value)
- [ ] Warn handles + threshold drag
- [ ] MIN/MAX footer
- [ ] Night mode, settings menu
- [ ] Unit conversions + EEPROM persistence
