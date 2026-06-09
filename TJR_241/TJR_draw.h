#pragma once
/*
 * TJR_draw.h — Framebuffer drawing layer for TJR_241.
 *
 * No Arduino_GFX.  All rendering writes directly into the PSRAM fb[]
 * defined in TJR_display.h.  fbFlush() pushes the completed frame to
 * the RM690B0 panel.
 *
 * Sections:
 *   SCREEN DIMENSIONS   — scrW() / scrH() logical helpers
 *   PIXEL WRITE         — fbPx(), fbPxS() (pre-swapped variant)
 *   PRIMITIVES          — fbFillRect, fbHLine, fbVLine, fbDrawRect,
 *                         fbFillRoundRect, fbDrawRoundRect, fbFillTriangle
 *   GFXFONT BLITTER     — fbChar(), fbStr()
 *   FONT METRICS        — fontH(), monoAdvance(), strW(), baselineY()
 *   TEXT PRINT HELPERS  — printLeft/Right/Centred/CentredMono/LeftMono/RightMono/CentredIn
 *   COLOUR HELPERS      — colValue(), colDim(), colDark()
 *   GAUGE DRAW          — drawFlashBorder, drawBar, drawSlot, drawPortrait, drawLandscape
 *
 * Coordinate system
 * -----------------
 * All draw functions use LOGICAL screen coordinates:
 *   Landscape  scrW=600  scrH=450   (identity — maps directly to fb[])
 *   Portrait   scrW=450  scrH=600   (transforms into landscape fb[] via fbPx)
 *
 * The landscape fb[] (600×450) is always the render target.  fbFlush() handles
 * the portrait transpose so draw code never switches on rotation.
 *
 * Colour convention
 * -----------------
 * All draw functions accept UNSWAPPED RGB565 colour constants (C_xxx macros).
 * The byte-swap (fbColour) is applied internally before writing to fb[].
 * Callers never need to call fbColour() themselves.
 *
 * Included after: TJR_display.h (for fb[], fbColour, NATIVE_W/H, g_isLandscape, g_rotation)
 *                 TJR_layout.h  (for font aliases and geometry constants)
 *                 TJR_params.h  (for PARAMS[], g_val[], g_peak[], g_min[], fmtVal(), etc.)
 * Requires globals: g_nightMode, g_warnHigh[], g_warnLow[], g_labelSlot,
 *                   g_labelExpiry, g_labelIsHigh, g_labelVal, g_warnDragging,
 *                   g_flash, g_flashT  (declared in TJR_241.ino)
 */

#include <pgmspace.h>   // pgm_read_byte


// ============================================================
//  SCREEN DIMENSIONS
//  Logical screen size — the coordinate space draw functions use.
//  In landscape: scrW=NATIVE_W, scrH=NATIVE_H.
//  In portrait:  scrW=NATIVE_H, scrH=NATIVE_W.
// ============================================================

inline int16_t scrW() { return g_isLandscape ? NATIVE_W : NATIVE_H; }
inline int16_t scrH() { return g_isLandscape ? NATIVE_H : NATIVE_W; }


// ============================================================
//  PIXEL WRITE
//  fbPxS — pre-swapped colour (internal only)
//  fbPx  — unswapped RGB565 colour (public interface)
//
//  Portrait coordinate transforms (matches fbFlush transpose):
//    rot=1  (90° CW):   fb_x = logical_y,           fb_y = NATIVE_H-1-logical_x
//    rot=3  (90° CCW):  fb_x = NATIVE_W-1-logical_y, fb_y = logical_x
// ============================================================

static inline void fbPxS(int16_t lx, int16_t ly, uint16_t swapped) {
    if (g_isLandscape) {
        if ((unsigned)lx < NATIVE_W && (unsigned)ly < NATIVE_H)
            fb[(uint32_t)ly * NATIVE_W + lx] = swapped;
    } else {
        // Portrait rot=1 and rot=3: both write portrait-row-major into _fpb[].
        // MADCTL (0xC0 / 0x00) handles the 180° difference at display time.
        if ((unsigned)lx < NATIVE_H && (unsigned)ly < NATIVE_W && _fpb)
            _fpb[(uint32_t)ly * NATIVE_H + lx] = swapped;
    }
}

static inline void fbPx(int16_t lx, int16_t ly, uint16_t col) {
    fbPxS(lx, ly, __builtin_bswap16(col));
}


// ============================================================
//  PRIMITIVES
//  All accept unswapped RGB565 colour constants.
//  Landscape path writes contiguous rows for speed; portrait falls
//  through to per-pixel transform.
// ============================================================

static void fbFillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t col) {
    if (w <= 0 || h <= 0) return;
    uint16_t sc = __builtin_bswap16(col);
    if (g_isLandscape) {
        // Fast path: contiguous row writes into fb[].
        int x1 = max((int)x, 0),             y1 = max((int)y, 0);
        int x2 = min((int)(x + w), NATIVE_W), y2 = min((int)(y + h), NATIVE_H);
        for (int row = y1; row < y2; row++) {
            uint16_t *p = fb + (uint32_t)row * NATIVE_W + x1;
            for (int c = x1; c < x2; c++) *p++ = sc;
        }
    } else if (_fpb) {
        // Portrait rot=1 and rot=3: both write portrait-row-major into _fpb[].
        // Each logical portrait row (fixed ly) is a contiguous run of NATIVE_H pixels.
        int lx1 = max((int)x,     0);
        int lx2 = min((int)(x+w), NATIVE_H);
        int ly1 = max((int)y,     0);
        int ly2 = min((int)(y+h), NATIVE_W);
        int run = lx2 - lx1;
        if (run <= 0) return;
        for (int ly = ly1; ly < ly2; ly++) {
            uint16_t *p = _fpb + (uint32_t)ly * NATIVE_H + lx1;
            for (int k = 0; k < run; k++) p[k] = sc;
        }
    }
}

static void fbHLine(int16_t x, int16_t y, int16_t len, uint16_t col) {
    fbFillRect(x, y, len, 1, col);
}

static void fbVLine(int16_t x, int16_t y, int16_t len, uint16_t col) {
    fbFillRect(x, y, 1, len, col);
}

static void fbDrawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t col) {
    fbHLine(x,         y,         w, col);
    fbHLine(x,         y + h - 1, w, col);
    fbVLine(x,         y,         h, col);
    fbVLine(x + w - 1, y,         h, col);
}


// ── Rounded rectangle helpers ─────────────────────────────────────────────────
// Midpoint circle algorithm — fills/draws the four corner quadrants.

static void _fbRRCornerFill(int16_t x0, int16_t y0, int16_t r, uint8_t quadrant, uint16_t sc) {
    // Fills corner arcs into horizontal fill bands (for filled rounded rect body).
    // quadrant bitmask: bit0=top-right, bit1=bottom-right, bit2=bottom-left, bit3=top-left
    int16_t f = 1 - r, ddF_x = 1, ddF_y = -2 * r, px = 0, py = r;
    while (px <= py) {
        if (quadrant & 0x01) { fbVLine(x0 + px, y0 - py, py + 1, 0); fbVLine(x0 + py, y0 - px, px + 1, 0); }
        if (quadrant & 0x04) { fbVLine(x0 - px, y0 - py, py + 1, 0); fbVLine(x0 - py, y0 - px, px + 1, 0); }
        // Not used for fill — use explicit horizontal spans below
        if (f >= 0) { py--; ddF_y += 2; f += ddF_y; }
        ddF_x += 2; f += ddF_x; px++;
    }
}

static void fbFillRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t col) {
    if (r <= 0) { fbFillRect(x, y, w, h, col); return; }
    if (r > h / 2) r = h / 2;
    if (r > w / 2) r = w / 2;
    // Fill the middle rectangle (width=w, height=h-2r, y offset=r)
    fbFillRect(x, y + r, w, h - 2 * r, col);
    // Top and bottom strips with rounded corners
    uint16_t sc = __builtin_bswap16(col);
    int16_t f = 1 - r, ddF_x = 1, ddF_y = -2 * r;
    int16_t px = 0, py = r;
    while (px <= py) {
        // Top edge strips
        fbHLine(x + r - px, y + r - py, w - 2 * (r - px), col);
        fbHLine(x + r - py, y + r - px, w - 2 * (r - py), col);
        // Bottom edge strips
        fbHLine(x + r - px, y + h - r + py - 1, w - 2 * (r - px), col);
        fbHLine(x + r - py, y + h - r + px - 1, w - 2 * (r - py), col);
        if (f >= 0) { py--; ddF_y += 2; f += ddF_y; }
        ddF_x += 2; f += ddF_x; px++;
    }
}

static void fbDrawRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t col) {
    if (r <= 0) { fbDrawRect(x, y, w, h, col); return; }
    // Four straight edges
    fbHLine(x + r,         y,         w - 2 * r, col);
    fbHLine(x + r,         y + h - 1, w - 2 * r, col);
    fbVLine(x,             y + r,     h - 2 * r, col);
    fbVLine(x + w - 1,     y + r,     h - 2 * r, col);
    // Four corner arcs (midpoint algorithm)
    int16_t f = 1 - r, ddF_x = 1, ddF_y = -2 * r;
    int16_t px = 0, py = r;
    int16_t x0 = x + r, y0 = y + r;
    int16_t x1 = x + w - 1 - r, y1 = y + h - 1 - r;
    while (px <= py) {
        fbPx(x1 + px, y0 - py, col); fbPx(x1 + py, y0 - px, col);
        fbPx(x1 + px, y1 + py, col); fbPx(x1 + py, y1 + px, col);
        fbPx(x0 - px, y0 - py, col); fbPx(x0 - py, y0 - px, col);
        fbPx(x0 - px, y1 + py, col); fbPx(x0 - py, y1 + px, col);
        if (f >= 0) { py--; ddF_y += 2; f += ddF_y; }
        ddF_x += 2; f += ddF_x; px++;
    }
}


// ── Filled triangle (scanline rasterisation) ──────────────────────────────────

// ── Circles ───────────────────────────────────────────────────────────────────

static void fbFillCircle(int16_t x0, int16_t y0, int16_t r, uint16_t col) {
    fbVLine(x0, y0 - r, 2 * r + 1, col);
    int16_t f = 1 - r, ddF_x = 1, ddF_y = -2 * r, x = 0, y = r;
    while (x < y) {
        if (f >= 0) { y--; ddF_y += 2; f += ddF_y; }
        x++; ddF_x += 2; f += ddF_x;
        fbVLine(x0 + x, y0 - y, 2 * y + 1, col);
        fbVLine(x0 - x, y0 - y, 2 * y + 1, col);
        fbVLine(x0 + y, y0 - x, 2 * x + 1, col);
        fbVLine(x0 - y, y0 - x, 2 * x + 1, col);
    }
}

static void fbDrawCircle(int16_t x0, int16_t y0, int16_t r, uint16_t col) {
    int16_t f = 1 - r, ddF_x = 1, ddF_y = -2 * r, x = 0, y = r;
    fbPx(x0, y0 + r, col); fbPx(x0, y0 - r, col);
    fbPx(x0 + r, y0, col); fbPx(x0 - r, y0, col);
    while (x < y) {
        if (f >= 0) { y--; ddF_y += 2; f += ddF_y; }
        x++; ddF_x += 2; f += ddF_x;
        fbPx(x0 + x, y0 + y, col); fbPx(x0 - x, y0 + y, col);
        fbPx(x0 + x, y0 - y, col); fbPx(x0 - x, y0 - y, col);
        fbPx(x0 + y, y0 + x, col); fbPx(x0 - y, y0 + x, col);
        fbPx(x0 + y, y0 - x, col); fbPx(x0 - y, y0 - x, col);
    }
}


// ── PROGMEM RGB565 bitmap blitter ─────────────────────────────────────────────
// Renders a uint16_t PROGMEM bitmap (native RGB565) into fb[].
// fbPx handles the byte-swap required by the RM690B0.

static void fbBitmap16(int16_t x, int16_t y, int16_t w, int16_t h,
                       const uint16_t *bmp_P) {
    for (int16_t row = 0; row < h; row++)
        for (int16_t col = 0; col < w; col++)
            fbPx(x + col, y + row, pgm_read_word(bmp_P + row * w + col));
}


static void fbFillTriangle(int16_t x0, int16_t y0,
                           int16_t x1, int16_t y1,
                           int16_t x2, int16_t y2, uint16_t col) {
    // Sort vertices by y (y0 <= y1 <= y2)
    if (y0 > y1) { int16_t t; t=y0;y0=y1;y1=t; t=x0;x0=x1;x1=t; }
    if (y1 > y2) { int16_t t; t=y1;y1=y2;y2=t; t=x1;x1=x2;x2=t; }
    if (y0 > y1) { int16_t t; t=y0;y0=y1;y1=t; t=x0;x0=x1;x1=t; }

    if (y0 == y2) {  // Degenerate: all three vertices on same scanline
        int16_t a = min(min(x0,x1),x2), b = max(max(x0,x1),x2);
        fbHLine(a, y0, b - a + 1, col);
        return;
    }

    int16_t dx01 = x1 - x0, dy01 = y1 - y0;
    int16_t dx02 = x2 - x0, dy02 = y2 - y0;  // dy02 always > 0 (y0 != y2)
    int16_t dx12 = x2 - x1, dy12 = y2 - y1;
    int32_t sa = 0, sb = 0;

    // Upper portion y0..y1.
    // When y1==y2 (flat bottom), include y1 here so the lower loop is empty.
    // When y0==y1 (flat top), last = y0-1 so this loop body never executes.
    int16_t last = (y1 == y2) ? y1 : y1 - 1;
    for (int16_t y = y0; y <= last; y++) {
        int16_t a2 = x0 + (dy01 ? sa / dy01 : 0);
        int16_t b2 = x0 + sb / dy02;
        sa += dx01; sb += dx02;
        if (a2 > b2) { int16_t t = a2; a2 = b2; b2 = t; }
        fbHLine(a2, y, b2 - a2 + 1, col);
    }

    // Lower portion y1..y2 — only entered when y1 < y2, so dy12 > 0.
    if (y1 < y2) {
        sa = (int32_t)dx12 * (y1 - y0);
        sb = (int32_t)dx02 * (y1 - y0);
        for (int16_t y = y1; y <= y2; y++) {
            int16_t a2 = x1 + sa / dy12;
            int16_t b2 = x0 + sb / dy02;
            sa += dx12; sb += dx02;
            if (a2 > b2) { int16_t t = a2; a2 = b2; b2 = t; }
            fbHLine(a2, y, b2 - a2 + 1, col);
        }
    }
}


// ============================================================
//  GFXFONT BLITTER
//  Renders one glyph (fbChar) or a string (fbStr) into fb[].
//  Bitmap format: 1bpp, MSB first, bits packed continuously across
//  rows with NO per-row byte alignment — matches ttf2gfx.py output.
//
//  cursor_x / baseline_y: standard GFX text cursor (baseline).
//  yOffset in GFXglyph is negative for glyphs above the baseline.
// ============================================================

static void fbChar(char c, int16_t cursor_x, int16_t baseline_y,
                   uint16_t col, const GFXfont *font) {
    uint8_t uc = (uint8_t)c;
    if (uc < font->first || uc > font->last) return;
    const GFXglyph *g = &font->glyph[uc - font->first];
    if (!g->width || !g->height) return;

    const uint8_t *bmp   = font->bitmap + g->bitmapOffset;
    int16_t        bx    = cursor_x  + g->xOffset;
    int16_t        by    = baseline_y + g->yOffset;
    uint16_t       sc    = __builtin_bswap16(col);

    uint8_t byte_val = pgm_read_byte(bmp++);
    uint8_t bit_pos  = 7;

    if (g_isLandscape) {
        // Landscape: row_ptr precomputed once per row → no per-pixel multiply.
        for (uint8_t row = 0; row < g->height; row++) {
            int16_t fb_y = by + row;
            uint16_t *rp = (fb_y >= 0 && fb_y < NATIVE_H)
                           ? fb + (uint32_t)fb_y * NATIVE_W : nullptr;
            for (uint8_t c2 = 0; c2 < g->width; c2++) {
                int16_t fx = bx + c2;
                if (rp && fx >= 0 && fx < NATIVE_W && (byte_val & (1u << bit_pos)))
                    rp[fx] = sc;
                if (bit_pos == 0) { byte_val = pgm_read_byte(bmp++); bit_pos = 7; }
                else bit_pos--;
            }
        }
    } else if (_fpb) {
        // Portrait rot=1 and rot=3: both write portrait-row-major into _fpb[ly*NATIVE_H + lx].
        for (uint8_t row = 0; row < g->height; row++) {
            int16_t fb_y = by + row;
            uint16_t *rp = (fb_y >= 0 && fb_y < NATIVE_W)
                           ? _fpb + (uint32_t)fb_y * NATIVE_H : nullptr;
            for (uint8_t c2 = 0; c2 < g->width; c2++) {
                int16_t fx = bx + c2;
                if (rp && fx >= 0 && fx < NATIVE_H && (byte_val & (1u << bit_pos)))
                    rp[fx] = sc;
                if (bit_pos == 0) { byte_val = pgm_read_byte(bmp++); bit_pos = 7; }
                else bit_pos--;
            }
        }
    }
}

// Render string, return cursor_x after last character.
static int16_t fbStr(const char *str, int16_t cursor_x, int16_t baseline_y,
                      uint16_t col, const GFXfont *font) {
    for (; *str; str++) {
        uint8_t uc = (uint8_t)*str;
        fbChar(*str, cursor_x, baseline_y, col, font);
        if (uc >= font->first && uc <= font->last)
            cursor_x += font->glyph[uc - font->first].xAdvance;
    }
    return cursor_x;
}


// ============================================================
//  FONT METRICS
// ============================================================

// Height of the '0' glyph — representative digit height for vertical positioning.
// Falls back to yAdvance if '0' is not in the font's range (shouldn't happen).
static int16_t fontH(const GFXfont *font) {
    if ('0' >= font->first && '0' <= font->last)
        return (int16_t)font->glyph['0' - font->first].height;
    return (int16_t)font->yAdvance;
}

// xAdvance of the '0' glyph — monospace cell width.
// Used by printCentredMono / printLeftMono / printRightMono.
static int16_t monoAdvance(const GFXfont *font) {
    if ('0' >= font->first && '0' <= font->last)
        return (int16_t)font->glyph['0' - font->first].xAdvance;
    return (int16_t)font->yAdvance;
}

// Sum of xAdvance for all characters in str — approximate bounding box width.
// Accurate enough for label centering on a monospace font.
static int16_t strW(const char *str, const GFXfont *font) {
    int16_t w = 0;
    for (; *str; str++) {
        uint8_t uc = (uint8_t)*str;
        if (uc >= font->first && uc <= font->last)
            w += (int16_t)font->glyph[uc - font->first].xAdvance;
    }
    return w;
}

// Baseline Y given desired top-of-text Y and a font.
// Uses fontH('0') as the representative digit height.
static int16_t baselineY(int16_t y_top, const GFXfont *font) {
    return y_top + fontH(font);
}

// xOffset of first character — used to left-align text at a pixel boundary.
static int16_t firstXOff(const char *str, const GFXfont *font) {
    uint8_t uc = (uint8_t)str[0];
    if (uc >= font->first && uc <= font->last)
        return (int16_t)font->glyph[uc - font->first].xOffset;
    return 0;
}


// ============================================================
//  TEXT PRINT HELPERS
//  Mirror the TJR_mini API (no gfx-> calls).
//
//  STATIC text  → printLeft / printRight / printCentred
//  LIVE values  → printCentredMono / printLeftMono / printRightMono
//                 (advance-based width prevents jiggle as digits change)
// ============================================================

void printLeft(const char *str, int16_t x, int16_t y_top,
               uint16_t col, const GFXfont *font) {
    int16_t x1 = firstXOff(str, font);
    fbStr(str, x - x1, baselineY(y_top, font), col, font);
}

void printRight(const char *str, int16_t x_right, int16_t y_top,
                uint16_t col, const GFXfont *font) {
    int16_t w  = strW(str, font);
    int16_t x1 = firstXOff(str, font);
    fbStr(str, x_right - w - x1, baselineY(y_top, font), col, font);
}

void printCentred(const char *str, int16_t y_top,
                  uint16_t col, const GFXfont *font) {
    int16_t w  = strW(str, font);
    int16_t x1 = firstXOff(str, font);
    int16_t cx = (scrW() - w) / 2 - x1;
    fbStr(str, cx, baselineY(y_top, font), col, font);
}

// ── Mono (advance-based) variants — ALWAYS use for live numeric values ────────

void printCentredMono(const char *str, int16_t y_top,
                      uint16_t col, const GFXfont *font) {
    int16_t adv = monoAdvance(font);
    int16_t cx;
    if (adv > 0)
        cx = (scrW() - adv * (int16_t)strlen(str)) / 2;
    else {
        int16_t x1 = firstXOff(str, font);
        cx = (scrW() - strW(str, font)) / 2 - x1;
    }
    fbStr(str, cx, baselineY(y_top, font), col, font);
}

void printLeftMono(const char *str, int16_t x, int16_t y_top,
                   uint16_t col, const GFXfont *font) {
    fbStr(str, x, baselineY(y_top, font), col, font);
}

void printRightMono(const char *str, int16_t x_right, int16_t y_top,
                    uint16_t col, const GFXfont *font) {
    int16_t adv = monoAdvance(font);
    int16_t cx;
    if (adv > 0)
        cx = x_right - adv * (int16_t)strlen(str);
    else
        cx = x_right - strW(str, font) - firstXOff(str, font);
    fbStr(str, cx, baselineY(y_top, font), col, font);
}

// Centred within a rectangle [rx, ry, rw, rh] — for button labels.
void printCentredIn(const char *str,
                    int16_t rx, int16_t ry, int16_t rw, int16_t rh,
                    uint16_t col, const GFXfont *font) {
    int16_t w  = strW(str, font);
    int16_t h  = fontH(font);
    int16_t x1 = firstXOff(str, font);
    int16_t cx = rx + (rw - w) / 2 - x1;
    int16_t cy = ry + (rh - h) / 2;
    fbStr(str, cx, cy + h, col, font);  // cy + h = baseline
}


// ============================================================
//  SPRITE FONT BLITTER
//  Renders 1bpp pre-rendered digit sprites into the PSRAM framebuffer.
//  Mirror of fbChar() — dual landscape / portrait path, pgm_read_byte
//  for bitmap bytes, direct struct field access (ESP32 flash is memory-mapped).
//
//  cx, cy = top-left of the character advance cell.
//  glyph.xOffset / yOffset are relative to cell top-left (not baseline).
// ============================================================

// Draw one character at cell position (cx, cy = cell top-left).
static void drawSpriteChar(const SpriteFont *sf, char c,
                           int16_t cx, int16_t cy, uint16_t col) {
    const char *p = strchr(sf->chars, c);
    if (!p) return;
    uint8_t idx = (uint8_t)(p - sf->chars);

    const SpriteGlyph *g = &sf->glyphs[idx];
    uint16_t gw = g->width, gh = g->height;
    if (!gw || !gh) return;    // space or empty glyph

    const uint8_t *bmp = sf->bitmaps + g->bitmapOffset;
    int16_t  bx0 = cx + g->xOffset;
    int16_t  by0 = cy + g->yOffset;
    uint16_t sc  = __builtin_bswap16(col);

    uint8_t byte_val = pgm_read_byte(bmp++);
    uint8_t bit_pos  = 7;

    if (g_isLandscape) {
        for (uint16_t row = 0; row < gh; row++) {
            int16_t  fb_y = by0 + row;
            uint16_t *rp  = (fb_y >= 0 && fb_y < NATIVE_H)
                            ? fb + (uint32_t)fb_y * NATIVE_W : nullptr;
            for (uint16_t col_px = 0; col_px < gw; col_px++) {
                int16_t fx = bx0 + (int16_t)col_px;
                if (rp && fx >= 0 && fx < NATIVE_W && (byte_val & (1u << bit_pos)))
                    rp[fx] = sc;
                if (bit_pos == 0) { byte_val = pgm_read_byte(bmp++); bit_pos = 7; }
                else --bit_pos;
            }
        }
    } else if (_fpb) {
        // Portrait rot=1/3: write portrait-row-major into _fpb[ly*NATIVE_H + lx].
        for (uint16_t row = 0; row < gh; row++) {
            int16_t  fb_y = by0 + row;
            uint16_t *rp  = (fb_y >= 0 && fb_y < NATIVE_W)
                            ? _fpb + (uint32_t)fb_y * NATIVE_H : nullptr;
            for (uint16_t col_px = 0; col_px < gw; col_px++) {
                int16_t fx = bx0 + (int16_t)col_px;
                if (rp && fx >= 0 && fx < NATIVE_H && (byte_val & (1u << bit_pos)))
                    rp[fx] = sc;
                if (bit_pos == 0) { byte_val = pgm_read_byte(bmp++); bit_pos = 7; }
                else --bit_pos;
            }
        }
    }
}

// Draw string left-aligned; x = left edge of first cell, y_top = cell top.
static void printSpriteLeft(const char *str, int16_t x, int16_t y_top,
                            uint16_t col, const SpriteFont *sf) {
    uint16_t adv = sf->xAdvance;
    for (; *str; str++, x += (int16_t)adv)
        drawSpriteChar(sf, *str, x, y_top, col);
}

// Draw string centred on scrW(); y_top = cell top.
static void printSpriteCentred(const char *str, int16_t y_top,
                               uint16_t col, const SpriteFont *sf) {
    int16_t total_w = (int16_t)sf->xAdvance * (int16_t)strlen(str);
    printSpriteLeft(str, (scrW() - total_w) / 2, y_top, col, sf);
}


// ============================================================
//  COLOUR HELPERS
//  Same logic as TJR_mini — check thresholds, return RGB565 colour.
//  Declared after g_nightMode, g_warnHigh[], g_warnLow[] are in scope.
// ============================================================

inline uint16_t colValue(int slot, float v) {
    if (inDanger(slot, v)) return C_DANGER;
    if (inWarn(slot, v))   return C_WARN;
    return g_nightMode ? C_NIGHT : C_WHITE;
}
inline uint16_t colDim()  { return g_nightMode ? C_NIGHT_D : C_DIM;  }
inline uint16_t colDark() { return g_nightMode ? 0x1800    : C_DARK; }


// ============================================================
//  DRAW — FLASH BORDER
//  Advances the 380 ms flash timer; draws a 3-pixel alert border
//  when col != 0 and the flash phase is "on".
//  Requires globals: g_flash, g_flashT  (declared in TJR_241.ino).
// ============================================================

static void drawFlashBorder(uint16_t col) {
    uint32_t now = millis();
    if (now - g_flashT > 380) { g_flash = !g_flash; g_flashT = now; }
    if (col && g_flash) {
        int16_t sw = scrW(), sh = scrH();
        for (int t = 0; t < 3; t++)
            fbDrawRect(t, t, sw - t * 2, sh - t * 2, col);
    }
}


// ============================================================
//  DRAW — BAR
//  Horizontal progress bar with warn handles, peak tick, drag label.
//  Geometry stored in *cache for use by the touch handler.
//
//  slot  — display slot index (0..14) — thresholds are per-slot
//  si    — signal index (0..59)       — range/name from PARAMS[si]
// ============================================================

void drawBar(int slot, int si, float v, float peak,
             int16_t x, int16_t y, int16_t w, int16_t h,
             BarCache *cache) {
    cache->x = x; cache->y = y; cache->w = w; cache->h = h;

    float lo = PARAMS[si].displayMin;
    float hi = PARAMS[si].displayMax;

    // Bar background
    fbFillRoundRect(x, y, w, h, 3, colDark());

    // Bar fill
    if (hi > lo) {
        float   pct  = constrain((v - lo) / (hi - lo), 0.0f, 1.0f);
        int16_t fill = (int16_t)(pct * w);
        if (fill > 0) fbFillRoundRect(x, y, fill, h, 3, colValue(slot, v));
    }
    fbDrawRoundRect(x, y, w, h, 3, colDim());

    if (hi > lo) {
        const int16_t DS = 6;   // triangle half-size (pixels)

        // Warn-HIGH handle: vertical line + downward triangle ABOVE bar
        {
            bool    hiOff = isinf(g_warnHigh[slot]) && g_warnHigh[slot] > 0;
            float   wHPct = hiOff ? 1.0f : constrain((g_warnHigh[slot] - lo) / (hi - lo), 0.0f, 1.0f);
            int16_t wHx   = x + (int16_t)(wHPct * w);
            uint16_t hCol = hiOff ? colDark() : C_DANGER;
            fbVLine(wHx, y - 4, h + 8, hCol);
            fbFillTriangle(wHx - DS, y - 4 - DS * 2,
                           wHx + DS, y - 4 - DS * 2,
                           wHx,      y - 4,            hCol);
        }

        // Warn-LOW handle: vertical line + upward triangle BELOW bar
        {
            bool    loOff = isinf(g_warnLow[slot]) && g_warnLow[slot] < 0;
            float   wLPct = loOff ? 0.0f : constrain((g_warnLow[slot] - lo) / (hi - lo), 0.0f, 1.0f);
            int16_t wLx   = x + (int16_t)(wLPct * w);
            uint16_t lCol = loOff ? colDark() : C_WARN;
            fbVLine(wLx, y - 4, h + 8, lCol);
            fbFillTriangle(wLx - DS, y + h + 4 + DS * 2,
                           wLx + DS, y + h + 4 + DS * 2,
                           wLx,      y + h + 4,            lCol);
        }

        // Peak-hold tick: vertical line + downward triangle above bar
        {
            float   ppct  = constrain((peak - lo) / (hi - lo), 0.0f, 1.0f);
            int16_t pmx   = x + (int16_t)(ppct * w);
            uint16_t pkCol = g_nightMode ? C_WARN : C_WHITE;
            fbVLine(pmx, y - 6, h + 12, pkCol);
            fbFillTriangle(pmx - 4, y - 8,
                           pmx + 4, y - 8,
                           pmx,     y - 2,  pkCol);
        }
    }

    // Threshold drag label — shown while drag is live or for 3 s after release
    bool labelActive = (g_labelSlot == slot &&
                        (g_labelExpiry == 0 ? g_warnDragging
                                            : millis() < g_labelExpiry));
    if (labelActive && hi > lo) {
        char valBuf[14];
        if (isinf(g_labelVal)) strcpy(valBuf, "OFF");
        else                   fmtVal(valBuf, sizeof(valBuf), si, g_labelVal);

        int16_t bw2  = strW(valBuf, FONT_LABEL) + 14;
        int16_t bh2  = fontH(FONT_LABEL) + 8;
        float   pct  = constrain((g_labelVal - lo) / (hi - lo), 0.0f, 1.0f);
        int16_t hx   = x + (int16_t)(pct * w);
        int16_t boxX = constrain(hx - bw2 / 2, x, x + w - bw2);
        int16_t boxY = y - 16 - bh2 - 12;   // 12 = DS*2, clear of warn triangle tip

        uint16_t lbCol = g_labelIsHigh ? C_DANGER : C_WARN;
        fbFillRect(boxX, boxY, bw2, bh2, C_BG);
        fbDrawRect(boxX, boxY, bw2, bh2, lbCol);
        printCentredIn(valBuf, boxX, boxY, bw2, bh2, lbCol, FONT_LABEL);
    }
}


// ============================================================
//  DRAW — PORTRAIT SLOT
//  Renders one value block for the vertical band [slotY .. slotY+slotH-1].
//  Logical screen is 450 wide × 300 px per slot (portrait mode).
//
//  slot  — display slot index (0..14)
//  si    — signal index (0..59)
// ============================================================

void drawSlot(int slot, int si, float v, float peak,
              int16_t slotY, int16_t slotH) {
    int16_t sw  = scrW();   // 450 in portrait
    int16_t PAD = PORTRAIT_PAD;

    // Label row: name left, unit right
    int16_t labelTopY = slotY + 12;
    uint16_t lblCol   = g_nightMode ? C_NIGHT : C_WHITE;
    printLeft (PARAMS[si].name,  PAD,      labelTopY, lblCol, FONT_LABEL);
    printRight(displayUnit(si),  sw - PAD, labelTopY, lblCol, FONT_LABEL);

    int16_t labelBotY = labelTopY + fontH(FONT_LABEL) + 2;

    // Bar position (bottom of slot)
    int16_t barY  = slotY + slotH - PORTRAIT_BAR_REGION;
    int16_t barW2 = sw - PAD * 2;

    int16_t valueArea = barY - labelBotY - 4;

    // Choose value font: primary, then fallback if string too wide or tall
    char vbuf[14];
    fmtVal(vbuf, sizeof(vbuf), si, v);

    const GFXfont *vfont = FONT_VALUE_P;
    {
        int16_t adv = monoAdvance(vfont);
        int16_t vw  = adv > 0 ? adv * (int16_t)strlen(vbuf) : strW(vbuf, vfont);
        if (vw > sw - 20 || fontH(vfont) > valueArea)
            vfont = FONT_VALUE_P_FB;
    }

    // Value centred in value area using advance-based width
    int16_t fh      = fontH(vfont);
    int16_t valTopY = labelBotY + (valueArea - fh) / 2;
    if (valTopY < labelBotY) valTopY = labelBotY;
    printCentredMono(vbuf, valTopY, colValue(slot, v), vfont);

    // Bar (stores geometry in cache for touch handler)
    BarCache *bc = (slotY == 0) ? &g_barTop : &g_barBot;
    drawBar(slot, si, v, peak, PAD, barY, barW2, PORTRAIT_BAR_H, bc);

    // MIN / MAX persistent footer (two rows: label, then value)
    {
        char mnBuf[14], mxBuf[14];
        fmtVal(mnBuf, sizeof(mnBuf), si, g_min[si]);
        fmtVal(mxBuf, sizeof(mxBuf), si, peak);
        int16_t labelRow = barY + PORTRAIT_BAR_H + 22;
        int16_t valueRow = labelRow + fontH(FONT_LABEL) + 4;
        uint16_t dc = colDim();
        printLeft    ("MIN", PAD,       labelRow, dc, FONT_LABEL);
        printRight   ("MAX", sw - PAD,  labelRow, dc, FONT_LABEL);
        printLeftMono (mnBuf, PAD,      valueRow, dc, FONT_LABEL);
        printRightMono(mxBuf, sw - PAD, valueRow, dc, FONT_LABEL);
    }
}


// ============================================================
//  DRAW — PORTRAIT FRAME
//  Two slots stacked at 0..slotH and slotH..2*slotH.
//  Divider line at mid-screen.  Flash border on alert.
// ============================================================

void drawPortrait() {
    int16_t sw    = scrW();    // 450
    int16_t sh    = scrH();    // 600
    int16_t slotH = sh / 2;   // 300

    // Clear to background — fbFill() is a single sequential PSRAM sweep,
    // faster than fbFillRect in portrait which writes non-contiguous column runs.
    fbFill(C_BG);

    int topSlot = slotTop(g_screen);
    int botSlot = slotBot(g_screen);
    int topSI   = g_slotParam[topSlot];
    int botSI   = g_slotParam[botSlot];

    drawSlot(topSlot, topSI, g_val[topSI], g_peak[topSI], 0,     slotH);
    drawSlot(botSlot, botSI, g_val[botSI], g_peak[botSI], slotH, slotH);

    fbHLine(0, slotH, sw, colDim());

    float    vT = g_val[topSI], vB = g_val[botSI];
    uint16_t alertCol = 0;
    if      (inDanger(topSlot, vT) || inDanger(botSlot, vB)) alertCol = C_DANGER;
    else if (inWarn  (topSlot, vT) || inWarn  (botSlot, vB)) alertCol = C_WARN;
    drawFlashBorder(alertCol);

    fbFlush();
}


// ============================================================
//  DRAW — LANDSCAPE FRAME
//  Single large value filling most of the 600×450 screen.
//  Uses the landscape slot for the current pair.
// ============================================================

void drawLandscape() {
    int16_t sw = scrW();    // 600
    int16_t sh = scrH();    // 450

    // Clear to background — fbFill() is a single sequential PSRAM sweep,
    // faster than fbFillRect in portrait which writes non-contiguous column runs.
    fbFill(C_BG);

    int     lsSlot = slotLS(g_screen);
    int     si     = g_slotParam[lsSlot];
    float   v      = g_val[si];
    float   pk     = g_peak[si];
    int16_t PAD    = LANDSCAPE_PAD;

    // Label row: name left, unit right
    int16_t labelTopY = 12;
    uint16_t lblCol   = g_nightMode ? C_NIGHT : C_WHITE;
    printLeft (PARAMS[si].name,  PAD,       labelTopY, lblCol, FONT_LABEL);
    printRight(displayUnit(si),  sw - PAD,  labelTopY, lblCol, FONT_LABEL);

    int16_t labelBotY = labelTopY + fontH(FONT_LABEL) + 2;

    // Bar at bottom
    int16_t barY2 = sh - LANDSCAPE_BAR_REGION;
    int16_t barW2 = sw - PAD * 2;

    // Value cascade: sprite font (220px) → GFXfont L1(160px) → L2 → L3 → L4
    // Sprite font has no int8_t offset ceiling; wins for short strings (≤4 chars).
    int16_t valueArea = barY2 - labelBotY - 8;
    char vbuf[14];
    fmtVal(vbuf, sizeof(vbuf), si, v);

    int16_t sprW = (int16_t)FONT_VALUE_SPRITE->xAdvance * (int16_t)strlen(vbuf);
    int16_t sprH = (int16_t)FONT_VALUE_SPRITE->lineHeight;

    if (sprW <= sw - LANDSCAPE_SPRITE_PAD * 2 && sprH <= valueArea) {
        int16_t valTopY = labelBotY + (valueArea - sprH) / 2;
        if (valTopY < labelBotY) valTopY = labelBotY;
        printSpriteCentred(vbuf, valTopY, colValue(lsSlot, v), FONT_VALUE_SPRITE);
    } else {
        const GFXfont *steps[] = { FONT_VALUE_L1, FONT_VALUE_L2, FONT_VALUE_L3, FONT_VALUE_L4 };
        const GFXfont *vfont   = FONT_VALUE_L4;
        for (int s = 0; s < 4; s++) {
            vfont = steps[s];
            int16_t adv = monoAdvance(vfont);
            int16_t vw  = adv > 0 ? adv * (int16_t)strlen(vbuf) : strW(vbuf, vfont);
            if (vw <= sw - PAD * 2 && fontH(vfont) <= valueArea) break;
        }
        int16_t fh      = fontH(vfont);
        int16_t valTopY = labelBotY + (valueArea - fh) / 2;
        if (valTopY < labelBotY) valTopY = labelBotY;
        printCentredMono(vbuf, valTopY, colValue(lsSlot, v), vfont);
    }

    drawBar(lsSlot, si, v, pk, PAD, barY2, barW2, LANDSCAPE_BAR_H, &g_barLS);

    // MIN / MAX footer: side-by-side
    {
        char mnBuf[14], mxBuf[14];
        fmtVal(mnBuf, sizeof(mnBuf), si, g_min[si]);
        fmtVal(mxBuf, sizeof(mxBuf), si, pk);
        char lMin[22], lMax[22];
        snprintf(lMin, sizeof(lMin), "MIN %s", mnBuf);
        snprintf(lMax, sizeof(lMax), "MAX %s", mxBuf);
        int16_t footerY = barY2 + LANDSCAPE_BAR_H + 22;
        uint16_t dc = colDim();
        printLeftMono (lMin, PAD,       footerY, dc, FONT_LABEL);
        printRightMono(lMax, sw - PAD,  footerY, dc, FONT_LABEL);
    }

    // Flash alert border
    uint16_t alertCol = 0;
    if      (inDanger(lsSlot, v)) alertCol = C_DANGER;
    else if (inWarn  (lsSlot, v)) alertCol = C_WARN;
    drawFlashBorder(alertCol);

    fbFlush();
}


// ============================================================
//  DRAW — QUAD CELL
//  One cell of the 2×2 four-parameter screen.
//  No bar, no threshold drag.  Value always white/night.
// ============================================================

#define QUAD_PAD 12

// Advance-based centering within an arbitrary horizontal band [cx, cx+cw).
// Used for quad cell values so centering doesn't depend on scrW().
static void printCellCentredMono(const char *str,
                                  int16_t cx, int16_t cw, int16_t y_top,
                                  uint16_t col, const GFXfont *font) {
    int16_t adv = monoAdvance(font);
    int16_t x;
    if (adv > 0)
        x = cx + (cw - adv * (int16_t)strlen(str)) / 2;
    else {
        int16_t x1 = firstXOff(str, font);
        x = cx + (cw - strW(str, font)) / 2 - x1;
    }
    fbStr(str, x, baselineY(y_top, font), col, font);
}

static void drawQuadCell(int /*qi*/, int si, float v, float pk,
                         int16_t cx, int16_t cy,
                         int16_t cw, int16_t ch) {
    uint16_t lblCol = g_nightMode ? C_NIGHT : C_WHITE;

    // Label row: name only (unit omitted — space too tight in quad cells)
    int16_t labelTopY = cy + 10;
    printLeft(PARAMS[si].name, cx + QUAD_PAD, labelTopY, lblCol, FONT_LABEL);

    // MIN/MAX footer: min value left, max value right
    char mnBuf[14], mxBuf[14];
    fmtVal(mnBuf, sizeof(mnBuf), si, g_min[si]);
    fmtVal(mxBuf, sizeof(mxBuf), si, pk);
    int16_t footerY = cy + ch - 8 - fontH(FONT_LABEL);
    uint16_t dc = colDim();
    printLeftMono (mnBuf, cx + QUAD_PAD,       footerY, dc, FONT_LABEL);
    printRightMono(mxBuf, cx + cw - QUAD_PAD,  footerY, dc, FONT_LABEL);

    // Value centred vertically in the band between label and footer
    int16_t valueTop = labelTopY + fontH(FONT_LABEL) + 4;
    int16_t valueBot = footerY - 4;
    int16_t valueH   = valueBot - valueTop;

    char vbuf[14];
    fmtVal(vbuf, sizeof(vbuf), si, v);

    // Try 90px (FONT_VALUE_L3), fall back to 56px (FONT_VALUE_L4) if too wide or tall
    const GFXfont *vfont = FONT_VALUE_L3;
    {
        int16_t adv = monoAdvance(vfont);
        int16_t vw  = adv > 0 ? adv * (int16_t)strlen(vbuf) : strW(vbuf, vfont);
        if (vw > cw - QUAD_PAD * 2 || fontH(vfont) > valueH)
            vfont = FONT_VALUE_L4;
    }
    int16_t fh      = fontH(vfont);
    int16_t valTopY = valueTop + (valueH - fh) / 2;
    if (valTopY < valueTop) valTopY = valueTop;
    printCellCentredMono(vbuf, cx, cw, valTopY, lblCol, vfont);
}


// ============================================================
//  DRAW — QUAD FRAME
//  2×2 grid, both landscape (300×225 cells) and portrait (225×300 cells).
//  No bars or threshold drag.  No alert colours or flash border.
// ============================================================

void drawQuad() {
    int16_t sw = scrW();
    int16_t sh = scrH();
    int16_t cw = sw / 2;
    int16_t ch = sh / 2;

    fbFill(C_BG);

    for (int row = 0; row < 2; row++) {
        for (int col = 0; col < 2; col++) {
            int qi = row * 2 + col;
            int si = g_quadParam[qi];
            drawQuadCell(qi, si, g_val[si], g_peak[si],
                         col * cw, row * ch, cw, ch);
        }
    }

    // Subtle divider lines
    uint16_t dc = colDim();
    fbVLine(cw, 0,  sh, dc);
    fbHLine(0,  ch, sw, dc);

    fbFlush();
}
