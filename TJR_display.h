/*
 * ============================================================
 *  TJR_display.h  —  RM690B0 QSPI driver for Waveshare 2.41" AMOLED
 *
 *  Derived from LilyGO T4-S3 driver (MIT).  Pin numbers remapped to
 *  Waveshare hardware.  No Arduino_GFX, no LVGL — raw ESP-IDF
 *  spi_master API + PSRAM framebuffer.
 *
 *  Requires macros from TJR_241.ino (included before this file):
 *    QSPI_CS, QSPI_CLK, QSPI_D0-D3, LCD_RST
 *    NATIVE_W (600), NATIVE_H (450), DISPLAY_SPI_MHZ (36)
 *
 *  Drawing model
 *  -------------
 *  Always draw into fb[] using landscape coordinates (0..599 × 0..449).
 *  fbFlush() handles the rest:
 *    • Landscape (rot 0/2): DMA fb[] directly to the panel — fast.
 *    • Portrait  (rot 1/3): software-transpose fb[] into a second PSRAM
 *      buffer, then DMA that — correct pixel order, no stride artifacts.
 *  MADCTL handles the panel scan direction; the transpose handles content
 *  rotation.  Drawing code never needs to know the current rotation.
 *
 *  Public API
 *  ----------
 *    bool displayInit()                      — call once in setup()
 *    void displaySetBrightness(uint8_t lvl)  — 0=off  255=max
 *    void displaySetRotation(uint8_t r)      — 0=landscape  1=portrait …
 *    void displayClearScreen()               — fill black + flush
 *    void fbFlush()                          — push fb[] to panel
 *    void fbFill(uint16_t colour)            — fill fb with solid colour
 *    uint16_t fbColour(uint16_t c)           — byte-swap helper (use on every pixel write)
 *
 *  Globals
 *  -------
 *    uint16_t *fb   — landscape PSRAM framebuffer, 600×450, RGB565
 *                     Pixel (x,y): fb[y * NATIVE_W + x]
 * ============================================================
 */
#pragma once

#include <driver/spi_master.h>
#include <driver/gpio.h>
#include <esp_heap_caps.h>

// ── framebuffers ─────────────────────────────────────────────────────────────
uint16_t *fb          = nullptr;   // landscape draw buffer — always 600×450
static uint16_t *_fpb = nullptr;   // portrait transpose buffer — same size, allocated in displayInit

// ── driver state ─────────────────────────────────────────────────────────────
static spi_device_handle_t _disp_spi = nullptr;
static uint16_t _off_x  = 0;
static uint16_t _off_y  = 16;    // landscape default
static uint8_t  _rot    = 0;     // current rotation (0-3)

// ── RM690B0 init sequence (from LilyGO T4-S3 — chip-level, unchanged) ────────
typedef struct { uint32_t addr; uint8_t param[20]; uint32_t len; } _lcd_cmd_t;
static const _lcd_cmd_t _rm690b0_init[] = {
    {0xFE, {0x20}, 0x01},
    {0x26, {0x0A}, 0x01},
    {0x24, {0x80}, 0x01},
    {0x5A, {0x51}, 0x01},   // SWIRE for BV6804 — absent from Waveshare demo
    {0x5B, {0x2E}, 0x01},   // SWIRE for BV6804 — absent from Waveshare demo
    {0xFE, {0x00}, 0x01},
    {0x3A, {0x55}, 0x01},   // RGB565
    {0xC2, {0x00}, 0x21},   // + delay 10ms
    {0x35, {0x00}, 0x01},   // TE ON
    {0x51, {0x00}, 0x01},   // brightness 0
    {0x11, {0x00}, 0x80},   // Sleep Out + delay 120ms
    {0x29, {0x00}, 0x20},   // Display On + delay 10ms
    {0x51, {0xFF}, 0x01},   // brightness MAX
};
#define _RM690B0_INIT_LEN (sizeof(_rm690b0_init)/sizeof(_rm690b0_init[0]))

#define _DISP_CHUNK 16384   // pixels per DMA transaction (32 KB)

static inline void _cs_lo() { gpio_set_level((gpio_num_t)QSPI_CS, 0); }
static inline void _cs_hi() { gpio_set_level((gpio_num_t)QSPI_CS, 1); }

static void _dispCmd(uint32_t reg, const uint8_t *data, uint32_t len) {
    _cs_lo();
    spi_transaction_t t = {};
    t.flags = SPI_TRANS_MULTILINE_CMD | SPI_TRANS_MULTILINE_ADDR;
    t.cmd   = 0x02;
    t.addr  = reg << 8;
    if (len) { t.tx_buffer = data; t.length = 8 * len; }
    spi_device_polling_transmit(_disp_spi, &t);
    _cs_hi();
}

static void _dispSetAddr(uint16_t xs, uint16_t ys, uint16_t xe, uint16_t ye) {
    xs += _off_x;  xe += _off_x;
    ys += _off_y;  ye += _off_y;
    uint8_t ca[4] = {(uint8_t)(xs>>8),(uint8_t)xs,(uint8_t)(xe>>8),(uint8_t)xe};
    uint8_t ra[4] = {(uint8_t)(ys>>8),(uint8_t)ys,(uint8_t)(ye>>8),(uint8_t)ye};
    _dispCmd(0x2A, ca, 4);
    _dispCmd(0x2B, ra, 4);
    _dispCmd(0x2C, nullptr, 0);
}

// DMA a pixel buffer (any size) to the panel — called after setAddr.
static void _dmaPixels(const uint16_t *src, uint32_t count) {
    _cs_lo();
    bool first = true;
    while (count > 0) {
        uint32_t chunk = (count > _DISP_CHUNK) ? _DISP_CHUNK : count;
        spi_transaction_ext_t t = {};
        if (first) {
            t.base.flags = SPI_TRANS_MODE_QIO;
            t.base.cmd   = 0x32;
            t.base.addr  = 0x002C00;
            first = false;
        } else {
            t.base.flags       = SPI_TRANS_MODE_QIO | SPI_TRANS_VARIABLE_CMD
                               | SPI_TRANS_VARIABLE_ADDR | SPI_TRANS_VARIABLE_DUMMY;
            t.command_bits = 0; t.address_bits = 0; t.dummy_bits = 0;
        }
        t.base.tx_buffer = src;
        t.base.length    = chunk * 16;
        spi_device_polling_transmit(_disp_spi, (spi_transaction_t *)&t);
        src   += chunk;
        count -= chunk;
    }
    _cs_hi();
}

// ── Byte-swap helper ──────────────────────────────────────────────────────────
// RM690B0 expects big-endian RGB565; ESP32 is little-endian.
// ALWAYS wrap colour constants with fbColour() before writing to fb[].
static inline uint16_t fbColour(uint16_t c) { return __builtin_bswap16(c); }

// ── fbFill ────────────────────────────────────────────────────────────────────
// For portrait rot=1: fill _fpb[] (the direct portrait render target).
// For all other modes: fill fb[].
void fbFill(uint16_t colour) {
    uint16_t s = fbColour(colour);
    uint32_t n = (uint32_t)NATIVE_W * NATIVE_H;
    uint16_t *buf = (_rot == 1 && _fpb) ? _fpb : fb;
    for (uint32_t i = 0; i < n; i++) buf[i] = s;
}

// ── fbFlush ───────────────────────────────────────────────────────────────────
// Landscape (rot=0/2):  DMA fb[] directly.
// Portrait  rot=1:      _fpb[] is already the render target — DMA directly, no transpose.
// Portrait  rot=3:      software-transpose fb[] → _fpb, then DMA.
void fbFlush() {
    if (_rot == 0 || _rot == 2) {
        // Landscape — rot=2 (USB-up): 180° flip fb[] in-place, then DMA with same MADCTL.
        if (_rot == 2) {
            uint16_t *p = fb, *q = fb + (uint32_t)NATIVE_W * NATIVE_H - 1;
            while (p < q) { uint16_t t = *p; *p++ = *q; *q-- = t; }
        }
        _dispSetAddr(0, 0, NATIVE_W-1, NATIVE_H-1);
        _dmaPixels(fb, (uint32_t)NATIVE_W * NATIVE_H);
        return;
    }
    if (!_fpb) return;

    if (_rot == 1) {
        // Portrait USB-left: draw functions wrote directly into _fpb[] — just DMA it.
        _dispSetAddr(0, 0, NATIVE_H-1, NATIVE_W-1);
        _dmaPixels(_fpb, (uint32_t)NATIVE_H * NATIVE_W);
    } else {
        // Portrait rot=3 (USB-right): software-transpose fb[] → _fpb, then DMA.
        const int BLK = 16;
        for (int py0 = 0; py0 < NATIVE_W; py0 += BLK) {
            int py_end = py0 + BLK < NATIVE_W ? py0 + BLK : NATIVE_W;
            for (int px0 = 0; px0 < NATIVE_H; px0 += BLK) {
                int px_end = px0 + BLK < NATIVE_H ? px0 + BLK : NATIVE_H;
                for (int py = py0; py < py_end; py++) {
                    uint16_t *dst = _fpb + py * NATIVE_H + px0;
                    for (int px = px0; px < px_end; px++)
                        *dst++ = fb[px * NATIVE_W + (NATIVE_W - 1 - py)];
                }
            }
        }
        _dispSetAddr(0, 0, NATIVE_H-1, NATIVE_W-1);
        _dmaPixels(_fpb, (uint32_t)NATIVE_H * NATIVE_W);
    }
}

// ── Public API ────────────────────────────────────────────────────────────────

void displaySetBrightness(uint8_t level) {
    _dispCmd(0x51, &level, 1);
}

// Rotation + MADCTL table (Waveshare-specific — panel mounted 180° from LilyGO T4-S3).
// Landscape MADCTLs confirmed on hardware.
// Portrait MADCTLs: both use 0x00 (natural scan) because fbFlush() handles
// orientation via software transpose.  If portrait appears 180° wrong, change
// both to 0xC0 — verify with corner-dot diagnostic.
void displaySetRotation(uint8_t rot) {
    _rot = rot & 3;
    uint8_t madctl;
    switch (_rot) {
        case 0:  madctl = 0xA0; _off_x = 0;  _off_y = 16; break;  // landscape USB-down ✓
        case 1:  madctl = 0xC0; _off_x = 16; _off_y = 0;  break;  // portrait  USB-left  — draw into _fpb[], DMA direct
        case 2:  madctl = 0xA0; _off_x = 0;  _off_y = 16; break;  // landscape USB-up   — same content as rot=0
        default: madctl = 0x00; _off_x = 16; _off_y = 0;  break;  // portrait  USB-right
    }
    _dispCmd(0x36, &madctl, 1);
}

// displayClearScreen: writes black to the ENTIRE panel RAM, including the
// 16-pixel gutter that normal fbFlush() cannot reach (because off_x or off_y
// starts the window at 16, leaving panel columns/rows 0..15 unwritten).
// Those stale pixels become visible when MADCTL changes orientation.
// Fix: temporarily zero the offsets and DMA the full portrait panel (450×600)
// with the all-black fb.  Pixel count is identical (NATIVE_W*NATIVE_H = 270,000)
// so no extra buffer is needed.
void displayClearScreen() {
    fbFill(0x0000);   // fills fb[] in landscape, _fpb[] in portrait rot=1
    uint16_t *buf = (_rot == 1 && _fpb) ? _fpb : fb;
    uint16_t sx = _off_x, sy = _off_y;
    _off_x = 0;  _off_y = 0;
    _dispSetAddr(0, 0, NATIVE_H - 1, NATIVE_W - 1);
    _dmaPixels(buf, (uint32_t)NATIVE_H * NATIVE_W);
    _off_x = sx; _off_y = sy;
}

bool displayInit() {
    fb = (uint16_t *)heap_caps_malloc((size_t)NATIVE_W * NATIVE_H * 2, MALLOC_CAP_SPIRAM);
    if (!fb) {
        Serial.println("ERROR: PSRAM landscape fb alloc failed");
        return false;
    }
    memset(fb, 0, (size_t)NATIVE_W * NATIVE_H * 2);

    // Portrait transpose buffer — same pixel count, different stride
    _fpb = (uint16_t *)heap_caps_malloc((size_t)NATIVE_W * NATIVE_H * 2, MALLOC_CAP_SPIRAM);
    if (!_fpb) {
        Serial.println("WARN: portrait fb alloc failed — portrait will be garbled");
    }

    // Hardware reset
    pinMode(LCD_RST, OUTPUT);
    digitalWrite(LCD_RST, HIGH); delay(200);
    digitalWrite(LCD_RST, LOW);  delay(300);
    digitalWrite(LCD_RST, HIGH); delay(200);

    gpio_config_t io = {};
    io.pin_bit_mask = 1ULL << QSPI_CS;
    io.mode         = GPIO_MODE_OUTPUT;
    gpio_config(&io);
    _cs_hi();

    spi_bus_config_t bus = {};
    bus.data0_io_num    = QSPI_D0;
    bus.data1_io_num    = QSPI_D1;
    bus.sclk_io_num     = QSPI_CLK;
    bus.data2_io_num    = QSPI_D2;
    bus.data3_io_num    = QSPI_D3;
    bus.data4_io_num    = -1; bus.data5_io_num = -1;
    bus.data6_io_num    = -1; bus.data7_io_num = -1;
    bus.max_transfer_sz = _DISP_CHUNK * 2 + 8;
    bus.flags           = SPICOMMON_BUSFLAG_MASTER | SPICOMMON_BUSFLAG_GPIO_PINS;

    esp_err_t r = spi_bus_initialize(SPI3_HOST, &bus, SPI_DMA_CH_AUTO);
    if (r != ESP_OK) {
        Serial.printf("ERROR: spi_bus_initialize: %s\n", esp_err_to_name(r));
        return false;
    }

    spi_device_interface_config_t dev = {};
    dev.command_bits   = 8;
    dev.address_bits   = 24;
    dev.mode           = SPI_MODE0;
    dev.clock_speed_hz = DISPLAY_SPI_MHZ * 1000000;
    dev.spics_io_num   = -1;
    dev.flags          = SPI_DEVICE_HALFDUPLEX;
    dev.queue_size     = 17;

    r = spi_bus_add_device(SPI3_HOST, &dev, &_disp_spi);
    if (r != ESP_OK) {
        Serial.printf("ERROR: spi_bus_add_device: %s\n", esp_err_to_name(r));
        return false;
    }

    for (int pass = 0; pass < 2; pass++) {
        for (uint32_t i = 0; i < _RM690B0_INIT_LEN; i++) {
            const _lcd_cmd_t *c = &_rm690b0_init[i];
            _dispCmd(c->addr, c->param, c->len & 0x1F);
            if (c->len & 0x80) delay(120);
            if (c->len & 0x20) delay(10);
        }
    }

    // Clear entire panel RAM (including 16-px gutter) before first frame
    displayClearScreen();

    Serial.printf("displayInit OK — fb=%p fpb=%p  %dx%d @ %dMHz\n",
                  (void*)fb, (void*)_fpb, NATIVE_W, NATIVE_H, DISPLAY_SPI_MHZ);
    return true;
}
