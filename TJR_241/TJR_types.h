#pragma once
// TJR_types.h — forward types included before Arduino auto-generates
// function prototypes, so custom types are in scope at prototype time.

#include <stdint.h>

// 3-axis float vector (used by QMI8658 accel reader)
struct Vec3 { float x, y, z; };

// Cached bar geometry — refreshed each draw, read by touch handler
struct BarCache { int16_t x, y, w, h; };

// Cached hit zone for settings / picker menu items
struct SettingsZone { int16_t x, y, w, h; };

// Decoded gesture result — returned by computeGesture() on touch release.
// fired = true means a gesture was recognised this call (touch just lifted).
// sx/sy = finger-down position, used for zone hit-tests after release.
struct Gesture {
  bool    fired;               // true = gesture ready (touch just released)
  bool    isTap;               // short still contact
  bool    swipeL, swipeR;      // fast horizontal flick
  bool    swipeU, swipeD;      // fast vertical flick
  int16_t sx, sy;              // initial finger-down position (for inZone checks)
};

// Hit-test a point against a cached zone rectangle.
// Used by settings, picker, and OTA touch handlers.
static inline bool inZone(int16_t tx, int16_t ty, const SettingsZone &z) {
  return (tx >= z.x && tx <= z.x + z.w && ty >= z.y && ty <= z.y + z.h);
}

// ── Unit type constants ───────────────────────────────────────────────────────
#define UNIT_TYPE_NONE      0   // %, V, RPM, dB — no conversion
#define UNIT_TYPE_TEMP      1   // native Kelvin  → °C or °F
#define UNIT_TYPE_PRESS_G   2   // native gauge kPa → kPa or psi
#define UNIT_TYPE_PRESS_ABS 3   // native abs kPa   → kPa or psi
#define UNIT_TYPE_SPEED     4   // native km/h       → km/h or mph
#define UNIT_TYPE_LAMBDA    5   // native λ          → λ or AFR

// ── Combined signal / parameter definition ────────────────────────────────────
// CAN decode fields drive the TWAI task; display fields drive the gauge UI.
struct ParamDef {
  // CAN decoding
  uint32_t    canID;
  uint8_t     startByte;
  uint8_t     endByte;
  bool        isSigned;     // treat raw 16-bit as int16_t
  bool        isBitField;   // single byte, masked by bitMask
  uint8_t     bitMask;
  float       divisor;      // engineering = raw / divisor - offset
  float       offset;       // 101.3 for gauge pressures, 0 for temps/speed
  // Display
  const char *name;         // short label <= 10 chars
  const char *unit;         // native unit label
  float       displayMin;   // bar left edge (native units)
  float       displayMax;   // bar right edge (native units)
  float       warnLow;      // default warn-low  threshold
  float       warnHigh;     // default warn-high threshold
  float       dangerLow;    // threshold drag clamp min
  float       dangerHigh;   // threshold drag clamp max
  uint8_t     decimals;     // decimal places shown
  float       snapStep;     // threshold drag snap increment
  uint8_t     unitType;     // UNIT_TYPE_* constant
};
