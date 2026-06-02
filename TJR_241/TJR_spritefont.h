#pragma once
/*
 * TJR_spritefont.h — Sprite font type definitions.
 *
 * SpriteFont / SpriteGlyph types for pre-rendered 1bpp digit sprites.
 * Used by the sprite blitter in TJR_draw.h.
 *
 * Unlike GFXglyph, offsets are int16_t — no ceiling from int8_t overflow.
 * Character set is limited to dashboard-useful chars: 0123456789.-  (space).
 * Generate headers with Tools/gen_digit_sprites.py.
 *
 * Coordinate convention:
 *   xOffset    — pixels from cell left (cursor x) to bitmap left edge.
 *   yOffset    — pixels from cell TOP  (cursor y) to bitmap top edge.
 *   xAdvance   — mono cell width; identical for all chars in a monospace font.
 *   lineHeight — total cell height; use this for vertical layout maths.
 */

#include <stdint.h>

typedef struct {
    uint32_t bitmapOffset;  // byte offset into the PROGMEM bitmap array
    uint16_t width;         // bitmap width in pixels
    uint16_t height;        // bitmap height in pixels
    int16_t  xOffset;       // pixels from cursor-x to left edge of bitmap
    int16_t  yOffset;       // pixels from cursor-y (cell top) to top of bitmap
} SpriteGlyph;

typedef struct {
    const uint8_t    *bitmaps;   // PROGMEM 1bpp packed bitmap data
    const SpriteGlyph *glyphs;  // PROGMEM per-glyph metrics
    const char        *chars;   // character set string (same order as glyphs[])
    uint8_t           numGlyphs;
    uint16_t          xAdvance;  // mono advance in pixels (same for all chars)
    uint16_t          lineHeight; // total cell height in pixels
} SpriteFont;
