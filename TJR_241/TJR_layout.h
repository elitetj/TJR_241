#pragma once
/*
 * TJR_layout.h — Screen geometry and font aliases for TJR_241.
 *
 * Everything here changes when porting to a different display.
 * Gauge draw functions (TJR_draw.h) use these aliases — do not call
 * ShareTechMono_* directly in draw code.
 *
 * Font files generated/adapted from ShareTechMono-Regular.ttf using
 * Tools/ttf2gfx.py (python3 Tools/ttf2gfx.py font.ttf size_px out.h Name).
 */

// ── GFXfont struct definitions ───────────────────────────────────────────────
// Must come before any font header that uses GFXglyph / GFXfont types.
#include "TJR_gfxfont.h"

// ── Sprite font struct definitions ───────────────────────────────────────────
// SpriteFont / SpriteGlyph types — must come before sprite font data headers.
#include "TJR_spritefont.h"

// ── Font data headers ────────────────────────────────────────────────────────
// Sprite font (1bpp pre-rendered) — no GFXglyph int8_t ceiling; 220px safe.
#include "ShareTechMono_220px_sprites.h"  // landscape cascade step 0 (largest, short strings)

// Numeric-only GFXglyph fonts (ASCII 45-57: - . / 0-9) — landscape cascade steps 1-3
#include "ShareTechMono_160px.h"  // landscape cascade step 1
#include "ShareTechMono_130px.h"  // landscape cascade step 2
#include "ShareTechMono_90px.h"   // landscape cascade step 3

// Numeric-only GFXglyph font — portrait main value
#include "ShareTechMono_100px.h"  // portrait value primary

// Full ASCII 32-126 fonts — labels, units, headings, settings text
#include "ShareTechMono_56px.h"   // headings, landscape cascade step 4, portrait value fallback
#include "ShareTechMono_36px.h"   // param names, units, footer MIN/MAX labels
#include "ShareTechMono_28px.h"   // DTC fault descriptions (fits long Haltech strings in landscape)


// ── Native display dimensions (landscape native) ──────────────────────────────
// RM690B0 panel is 600×450 native.  fb[] is always 600×450.
// Portrait rendering is handled by software transpose in fbFlush().
// These macros are already defined in TJR_241.ino — guard to avoid redefinition.
#ifndef NATIVE_W
#define NATIVE_W  600
#define NATIVE_H  450
#endif


// ── Font aliases ──────────────────────────────────────────────────────────────
// Replace these (and the #includes above) to port to a different screen size.

#define FONT_MISC    (&ShareTechMono_36px)   // debug overlay, small UI text
#define FONT_LABEL   (&ShareTechMono_36px)   // param name / unit / footer labels
#define FONT_HDG     (&ShareTechMono_56px)   // screen headings (OTA, graph, settings)
#define FONT_DTC     (&ShareTechMono_28px)   // DTC fault descriptions (long strings in landscape)

// Portrait value cascade: try FONT_VALUE_P first, fall back if string too wide/tall
#define FONT_VALUE_P    (&ShareTechMono_100px)  // portrait primary (numeric-only font)
#define FONT_VALUE_P_FB (&ShareTechMono_56px)   // portrait fallback (full ASCII covers all fmtVal output)

// Landscape value cascade: sprite step 0 tried first in drawLandscape(),
// then GFXfont steps L1-L4 in order until string fits in scrW - 2×LANDSCAPE_PAD.
#define FONT_VALUE_SPRITE (&ShareTechMono_220px)  // landscape step 0 — sprite, no size ceiling
#define FONT_VALUE_L1   (&ShareTechMono_160px)    // landscape step 1 — GFXfont fallback
#define FONT_VALUE_L2   (&ShareTechMono_130px)    // landscape step 2
#define FONT_VALUE_L3   (&ShareTechMono_90px)     // landscape step 3
#define FONT_VALUE_L4   (&ShareTechMono_56px)     // landscape step 4 — always fits


// ── Portrait geometry ─────────────────────────────────────────────────────────
// Portrait logical screen: 450×600 px (NATIVE_H wide × NATIVE_W tall).
// Two equal slots stacked: each 450×300 px.

// Horizontal margin — keeps content clear of panel rounded corners
#define PORTRAIT_PAD          20

// Bar height (the filled progress bar strip)
#define PORTRAIT_BAR_H        16

// Total height at the bottom of each slot for bar + handles + MIN/MAX footer:
//   warn-HIGH handle triangle above bar:  ~15 px
//   bar itself:                           16 px  (PORTRAIT_BAR_H)
//   warn-LOW handle triangle below bar:   ~15 px
//   gap:                                   4 px
//   MIN label row  (FONT_LABEL ~36 px):   36 px
//   MAX value row  (FONT_LABEL ~36 px):   36 px
//   bottom margin:                          8 px
//   ≈ 130 px total  →  use 110 (handles overlap bar zone, label rows share space)
#define PORTRAIT_BAR_REGION    90


// ── Landscape geometry ────────────────────────────────────────────────────────
// Landscape logical screen: 600×450 px (NATIVE_W × NATIVE_H).

// Horizontal margin — keeps text clear of panel rounded corners
#define LANDSCAPE_PAD         24
#define LANDSCAPE_SPRITE_PAD   2  // looser fit threshold for centred sprite values only

// Bar height
#define LANDSCAPE_BAR_H       16

// Total height at the bottom for bar + handles + MIN/MAX footer:
//   warn-HIGH handle above bar:            ~13 px
//   bar:                                   16 px  (LANDSCAPE_BAR_H)
//   warn-LOW handle below bar:             ~13 px
//   gap:                                    5 px
//   MIN / MAX footer row (FONT_LABEL 36):  36 px
//   bottom margin:                          7 px
//   ≈ 90 px total
#define LANDSCAPE_BAR_REGION  90


// ── Night-mode tap zone ───────────────────────────────────────────────────────
// Invisible hit zone in top-right corner.  Tap to toggle day/night.
#define NIGHT_ZONE_W  90
#define NIGHT_ZONE_H  50
