/*
 * ============================================================
 *  TJR_241.ino  —  v0.1  (Scaffold: CAN + IMU + EEPROM)
 *
 *  Hardware: Waveshare ESP32-S3-Touch-AMOLED-2.41 (SKU 30563)
 *    Display : RM690B0  QSPI  600 × 450  (landscape native)
 *    Touch   : FT6336   I2C   addr 0x38  (SDA=47 SCL=48)
 *    IMU     : QMI8658C I2C   addr 0x6B  (same bus, raw reads)
 *    CAN     : SN65HVD230  TX=1 RX=8
 *    NOTE: GPIO 16/17 are battery control on this board — CAN moved to 1/8
 *
 *  PSRAM=enabled is MANDATORY (RM690B0 framebuffer + WiFi when added).
 *
 *  Compile (scaffold — no display yet):
 *    arduino-cli compile \
 *      --fqbn "esp32:esp32:waveshare_esp32_s3_touch_amoled_241:PSRAM=enabled,PartitionScheme=min_spiffs" \
 *      --export-binaries /Users/tj/CANBUS/TJR_241
 *
 *  Port status:
 *    DONE  TJR_types.h    — structs, unit type constants (verbatim)
 *    DONE  TJR_params.h   — 60-signal table, slot system, unit conversions (verbatim)
 *    DONE  TJR_can.h      — TWAI task on core 0, CAN pins updated to 1/8
 *    DONE  TJR_simdata.h  — sinusoidal sim data (verbatim)
 *    DONE  TJR_eeprom.h   — EEPROM persistence, same 144-byte layout (verbatim)
 *    DONE  IMU (QMI8658)  — raw I2C, verbatim from TJR_mini
 *    TODO  Display        — RM690B0 init (driver in Resources/…/RM690B0_driver/)
 *    TODO  TJR_layout.h   — geometry for 600×450, font sizes (needs screen)
 *    TODO  Touch          — FT6336 register map check + coordinate transforms
 *    TODO  TJR_draw.h     — full rewrite for 600×450 landscape-native
 *    TODO  TJR_settings.h — settings overlay, rederive for new resolution
 *    TODO  TJR_picker.h   — signal picker overlay
 *    TODO  TJR_ota.h      — OTA firmware update (WiFi AP)
 *    TODO  TJR_graph.h    — rolling graph screen
 *    TODO  TJR_touch.h    — touch handler + gesture system
 *    TODO  TJR_dtc.h      — DTC / CEL warning page
 *    TODO  Sprite digits  — gen_digit_sprites.py for large value fonts (>160px)
 * ============================================================
 */

// TJR_types.h first — custom types must be in scope before Arduino auto-prototypes
#include "TJR_types.h"

#include <Arduino.h>
#include <Wire.h>
#include <EEPROM.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>
// Note: Arduino_GFX_Library not used on this board.
//       Display uses raw ESP-IDF spi_master + RM690B0 driver in TJR_display.h.
//       WiFi / WebServer / Update will be added when display is confirmed working.

#include "TJR_params.h"    // signal table, slot system, conversion functions


// ============================================================
//  PINS & DEFINES
// ============================================================

// — QSPI display (RM690B0) — same physical pins as TJR_mini CO5300 —
#define QSPI_CS    9
#define QSPI_CLK  10
#define QSPI_D0   11
#define QSPI_D1   12
#define QSPI_D2   13
#define QSPI_D3   14
#define LCD_RST   21

// — I2C shared bus (touch FT6336 + IMU QMI8658C) — same as TJR_mini —
#define IIC_SDA      47
#define IIC_SCL      48
#define TOUCH_ADDR   0x38   // FT6336 — same register map as FT3168, verify on hardware
#define QMI_ADDR     0x6B

// — CAN (SN65HVD230) — moved from 16/17 which are battery pins on this board —
#define CAN_TX   1
#define CAN_RX   8

// — Native display dimensions (landscape native) —
#define NATIVE_W  600
#define NATIVE_H  450

// — Panel geometry —
// RM690B0 requires a 16px offset in the non-active axis during CASET/RASET.
// Portrait: add 16 to X (xs=16..465).  Landscape: add 16 to Y (ys=16..465).
// Confirmed from LilyGO T4-S3 setAddrWindow() source.
#define PANEL_OFFSET  16

// — SPI clock — 36 MHz (from LilyGO source; Waveshare demo says 40 MHz but that's wrong) —
#define DISPLAY_SPI_MHZ  36

#define FW_VERSION "v0.9"

// — EEPROM layout — identical to TJR_mini (same magic, same offsets) —
#define EE_MAGIC        0xA8
#define EE_ADDR_MAGIC     0
#define EE_ADDR_PAIR      1
#define EE_ADDR_NIGHT     2
#define EE_ADDR_BRIGHT    3
#define EE_ADDR_UNITS     5
#define EE_ADDR_SLOTS     6
#define EE_ADDR_WARNLO   21
#define EE_ADDR_WARNHI   81
#define EE_ADDR_GRAPH_SI 142
#define EE_ADDR_ROT     143
#define EE_SIZE         144
#define EE_DEBOUNCE_MS 2000

// — Colours (RGB565) — same palette as TJR_mini —
#define C_BG         0x0000
#define C_WHITE      0xFFFF
#define C_WARN       0xFD20   // amber
#define C_DANGER     0xF800   // red
#define C_DIM        0x7BEF
#define C_DARK       0x18C3
#define C_GREEN      0x07E0
#define C_NIGHT      0xF800
#define C_NIGHT_D    0xA000
#define C_GRAPH_LINE 0x07FF
#define C_GRAPH_FILL 0x0118

// CAN task — included after CAN_TX / CAN_RX macros are defined
#include "TJR_can.h"

// Display driver — included after all pin/dimension macros are defined
#include "TJR_display.h"


// ============================================================
//  UI STATE
// ============================================================

bool     g_nightMode    = false;
bool     g_gaugeFadeIn  = true;

int      g_screen      = 0;
uint8_t  g_rotation    = 0;     // 0=landscape upright (default for landscape-native panel)
bool     g_isLandscape = true;  // 2.41" is landscape-native; default to landscape

BarCache g_barTop, g_barBot, g_barLS;

bool     g_eeDirty    = false;
uint32_t g_eeLastChg  = 0;

uint8_t  g_brightDay     = 80;
uint8_t  g_brightNight   = 80;

// — Alert flash state (used by drawFlashBorder in TJR_draw.h) —
bool     g_flash  = false;
uint32_t g_flashT = 0;

// — Threshold drag state (used by TJR_touch.h and drawBar in TJR_draw.h) —
bool     g_warnDragging    = false;
int      g_dragSlot        = -1;
bool     g_dragIsHigh      = true;

// — Pending drag (hold-to-arm: 500 ms still in bar zone) —
bool     g_dragPending     = false;
int      g_dragPendSlot    = -1;
bool     g_dragPendIsHigh  = true;
uint32_t g_dragPendTime    = 0;

// — Threshold drag label (shown during / 3 s after drag) —
int      g_labelSlot    = -1;
bool     g_labelIsHigh  = true;
float    g_labelVal     = 0.0f;
uint32_t g_labelExpiry  = 0;

// — Touch gesture state —
int16_t  g_sx0 = -1, g_sy0 = -1;   // touch-down position
int16_t  g_lastTx = 0, g_lastTy = 0;
uint32_t g_st0 = 0;
bool     g_touchDown = false;

// — OTA long-hold detection —
bool     g_otaHoldArmed = false;
uint32_t g_otaHoldStart = 0;

// — Settings menu state —
uint8_t      g_settingsTab    = 0;   // 0=UNITS  1=DISPLAY
bool         g_setDragging    = false;
bool         g_setDragIsDay   = true;
SettingsZone g_setZTab[2];
SettingsZone g_setZUnits[4];
SettingsZone g_setZBrDay;
SettingsZone g_setZBrNight;
SettingsZone g_setZExit;

// — Signal picker state —
bool         g_pickerForGraph = false;
int          g_pickerSlot     = -1;
int          g_pickerScroll   = 0;
uint8_t      g_pickerOrder[NUM_SIGNALS];
SettingsZone g_pickerZCancel;
SettingsZone g_pickerZList;

// — Rolling graph state —
#define  GRAPH_BUF_LEN    100   // 5 s history at 20 fps
uint8_t  g_graphSI        = 2;  // default: THROTTLE
float    g_graphBuf[GRAPH_BUF_LEN];
int      g_graphHead      = 0;
int      g_graphCount     = 0;
float    g_graphScaleMin  =  1e38f;
float    g_graphScaleMax  = -1e38f;
#define  NUM_SCREENS      (NUM_PAIRS + 1)
inline bool isGraph()  { return g_screen == NUM_PAIRS; }

uint32_t g_lastDraw    = 0;
uint32_t g_lastIMU     = 0;
uint32_t g_warnFirstMs = 0;
uint32_t g_warnSwipeMs = 0;

// — Overlay flags —
bool g_settingsOpen  = false;
bool g_pickerOpen    = false;
bool g_otaConfirm    = false;
bool g_otaMode       = false;
bool g_dtcPageOpen   = false;

// — OTA state —
bool     g_otaStarted   = false;
bool     g_otaSuccess   = false;
bool     g_otaFailed    = false;
uint32_t g_otaBytesWr   = 0;
uint32_t g_otaEnterTime = 0;
uint32_t g_otaLastAct   = 0;
SettingsZone g_otaZYes, g_otaZNo, g_otaZExit;


// Real display functions — displaySetRotation / displayClearScreen /
// displaySetBrightness — are defined in TJR_display.h (included above).

// Fade display brightness from/to over 500 ms (50 × 10 ms steps).
static void fadeBrightness(uint8_t from, uint8_t to) {
  for (int i = 0; i <= 50; i++) {
    int16_t lev = (int16_t)from + ((int16_t)to - (int16_t)from) * i / 50;
    displaySetBrightness((uint8_t)constrain(lev, 0, 255));
    delay(10);
  }
}

void applyBrightness() {
  uint8_t pct = g_nightMode ? g_brightNight : g_brightDay;
  displaySetBrightness((uint8_t)((uint16_t)pct * 255 / 100));
}


// ============================================================
//  UTILITY HELPERS
// ============================================================

inline void markDirty() { g_eeDirty = true; g_eeLastChg = millis(); }


// ============================================================
//  I2C / TOUCH — FT6336
//  Standard FocalTech register map (same layout as FT3168):
//    0x01  GEST_ID
//    0x02  TD_STATUS  (touch count in low nibble)
//    0x03  TOUCH1_XH  (event[7:6] + x[11:8])
//    0x04  TOUCH1_XL  (x[7:0])
//    0x05  TOUCH1_YH  (touch ID[7:4] + y[11:8])
//    0x06  TOUCH1_YL  (y[7:0])
//  Raw coordinates are in native panel space (600×450 landscape-native).
//  Axis-to-rotation transforms must be re-verified on hardware — expect
//  the landscape-native panel to swap axes relative to TJR_mini's portrait layout.
// ============================================================

// Touch coordinate system — confirmed from serial + visual data 2026-06-01:
//   FT6336 X register (rx) = physical VERTICAL axis:   0=top,   ~449=bottom
//   FT6336 Y register (ry) = physical HORIZONTAL axis: 0=right, ~599=left
//   Resolution matches display 1:1 — no scaling needed.
//
//   Physical corners:
//     Top-right  = raw(  ~0,   ~0)   Top-left   = raw(  ~0, ~599)
//     Bot-right  = raw(~449,   ~0)   Bot-left   = raw(~449, ~599)

bool readTouch(int16_t &tx, int16_t &ty) {
  Wire.beginTransmission(TOUCH_ADDR);
  Wire.write(0x01);
  if (Wire.endTransmission(false) != 0) return false;
  Wire.requestFrom((uint8_t)TOUCH_ADDR, (uint8_t)6);
  if (Wire.available() < 6) return false;
  uint8_t d[6];
  for (int i = 0; i < 6; i++) d[i] = Wire.read();

  if ((d[1] & 0x0F) == 0) return false;

  int16_t rx = ((int16_t)(d[2] & 0x0F) << 8) | d[3];   // vertical:   0=top  ~449=bottom
  int16_t ry = ((int16_t)(d[4] & 0x0F) << 8) | d[5];   // horizontal: 0=right ~599=left

  // Same mapping for all rotations — touch sensor is fixed in landscape panel space.
  // MADCTL + portrait transpose handle display orientation independently.
  //   ry (horizontal: 0=right, ~599=left) → fb x = NATIVE_W-1-ry
  //   rx (vertical:   0=top,   ~449=bot)  → fb y = rx
  tx = NATIVE_W - 1 - ry;
  ty = rx;
  return true;
}


// ============================================================
//  I2C / IMU — QMI8658C (raw I2C, no external library)
//  Configured for ±4 g at 1000 Hz ODR, 13-bit sensitivity.
//  Same chip as TJR_mini — code transfers verbatim.
//  NOTE: axis-to-rotation mapping may need adjustment for the
//  new panel orientation; re-verify updateOrientation() on hardware.
// ============================================================

static bool qmiWrite(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(QMI_ADDR);
  Wire.write(reg); Wire.write(val);
  return Wire.endTransmission() == 0;
}
static bool qmiRead(uint8_t reg, uint8_t *buf, uint8_t len) {
  Wire.beginTransmission(QMI_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  Wire.requestFrom((uint8_t)QMI_ADDR, len);
  for (uint8_t i = 0; i < len; i++) {
    if (!Wire.available()) return false;
    buf[i] = Wire.read();
  }
  return true;
}
bool initQMI() {
  qmiWrite(0x60, 0xB0); delay(15);   // soft reset
  qmiWrite(0x02, 0x40);              // CTRL1: accel enable
  qmiWrite(0x03, 0x13);              // CTRL2: ±4 g, ODR 1000 Hz
  qmiWrite(0x04, 0xD3);              // CTRL3: gyro (not used, init anyway)
  qmiWrite(0x08, 0x03); delay(20);   // CTRL7: enable accel + gyro
  uint8_t id = 0;
  qmiRead(0x00, &id, 1);
  return id == 0x05;
}
bool readAccel(Vec3 &a) {
  uint8_t b[6];
  if (!qmiRead(0x35, b, 6)) return false;
  a.x = (int16_t)((b[1] << 8) | b[0]) / 8192.0f;
  a.y = (int16_t)((b[3] << 8) | b[2]) / 8192.0f;
  a.z = (int16_t)((b[5] << 8) | b[4]) / 8192.0f;
  return true;
}


// ============================================================
//  ORIENTATION — IMU-driven auto-rotation
//  Polls QMI8658 at 2 Hz.  0.6 g threshold prevents jitter.
//  Axis-to-rotation mapping inherited from TJR_mini; verify on
//  hardware that axes produce the expected orientation enum.
// ============================================================

void updateOrientation() {
  Vec3 a;
  if (!readAccel(a)) return;
  float ax = a.x, ay = a.y;
  float absX = fabsf(ax), absY = fabsf(ay);

  uint8_t newRot = g_rotation;
  if      (absX > absY && absX > 0.6f) newRot = (ax > 0) ? 0 : 2;
  else if (absY > absX && absY > 0.6f) newRot = (ay > 0) ? 3 : 1;

  if (newRot != g_rotation) {
    g_rotation    = newRot;
    g_isLandscape = !(newRot & 1);  // rot 0,2 are landscape on this board (portrait-native logic inverted)
    displaySetRotation(g_rotation);   // TODO: set RM690B0 MADCTL
    displayClearScreen();             // TODO: clear framebuffer
    markDirty();
  }
}


// ============================================================
//  SUBSYSTEM INCLUDES
//  Only hardware-independent subsystems included at this stage.
//  Display-dependent subsystems (layout, draw, touch, settings,
//  picker, OTA, graph, DTC) will be added once the RM690B0 driver
//  is confirmed working and the framebuffer is wired up.
// ============================================================

#include "TJR_simdata.h"   // sinusoidal bench/demo data — remove when CAN is live
#include "TJR_eeprom.h"    // EEPROM load / save

// Display-dependent subsystems — included after all globals and display init
#include "TJR_layout.h"    // screen geometry, font aliases, font data
#include "TJR_draw.h"      // framebuffer primitives, text helpers, gauge draw functions
#include "TJR_touch.h"     // touch handler, gesture system, warn auto-jump
#include "TJR_ota.h"       // OTA firmware update
#include "TJR_dtc.h"       // DTC / CEL warning page
#include "TJR_graph.h"     // rolling graph screen
#include "TJR_picker.h"    // signal picker overlay
#include "TJR_settings.h"  // settings overlay (units + brightness)


// ============================================================
//  SETUP
// ============================================================

void setup() {
  Serial.begin(115200);
  Serial.println("TJR_241 " FW_VERSION " boot");

  // Shared I2C bus — touch (FT6336) + IMU (QMI8658C)
  Wire.begin(IIC_SDA, IIC_SCL);
  Wire.setClock(400000);

  // Load persisted settings (pair, night mode, thresholds, rotation, unit flags)
  eepromLoad();

  // Init IMU — auto-rotation won't work without it but nothing crashes
  if (!initQMI()) {
    Serial.println("WARN: QMI8658C not found — orientation fixed at boot value");
  }

  // Seed signal values so bars and peaks start at sensible positions
  for (int i = 0; i < NUM_SIGNALS; i++) {
    g_val[i]  = PARAMS[i].displayMin;
    g_peak[i] = PARAMS[i].displayMin;
    g_min[i]  = PARAMS[i].displayMax;
  }

  // Display init — RM690B0 QSPI driver (TJR_display.h)
  if (!displayInit()) {
    Serial.println("ERROR: displayInit failed — check wiring and PSRAM=enabled");
  } else {
    // Rotation 0 = landscape CCW (600×450, USB connector on left side)
    displaySetRotation(0);
    g_rotation    = 0;
    g_isLandscape = true;

    // First-boot test: solid green fill to confirm display is alive.
    // This will be replaced by the real draw loop once layout is implemented.
    fbFill(C_GREEN);
    fbFlush();
    displaySetBrightness(128);
    Serial.println("Display: green test frame pushed");
  }

  // Start CAN task on core 0.
  // Uncomment when harness is connected and ECU is live.
  // xTaskCreatePinnedToCore(canTask, "can", 3072, NULL, 5, NULL, 0);

  Serial.printf("Ready — screen=%d night=%d rot=%d NATIVE=%dx%d\n",
                g_screen, g_nightMode, g_rotation, NATIVE_W, NATIVE_H);
  Serial.printf("Slots: [%d,%d,%d,%d,%d | %d,%d,%d,%d,%d | %d,%d,%d,%d,%d]\n",
                g_slotParam[0], g_slotParam[1], g_slotParam[2], g_slotParam[3], g_slotParam[4],
                g_slotParam[5], g_slotParam[6], g_slotParam[7], g_slotParam[8], g_slotParam[9],
                g_slotParam[10],g_slotParam[11],g_slotParam[12],g_slotParam[13],g_slotParam[14]);
}


// ============================================================
//  MADCTL PROBE  (enable by defining MADCTL_PROBE below)
//
//  With the board held in portrait (USB connector on one side):
//    – Fill fb[] with a test pattern (coloured quadrants + corner markers)
//    – Cycle through 8 MADCTL candidates, sending fb[] directly (no transpose)
//    – Each candidate shown for 3 s; MADCTL value printed to serial
//    – Note which value produces a correctly-oriented portrait image with no
//      scrambling — quadrant colours match the serial description
//
//  Test pattern in landscape fb[] coordinates:
//    Top-left  quadrant (x=0..299,  y=0..224):   RED
//    Top-right quadrant (x=300..599,y=0..224):   GREEN
//    Bot-left  quadrant (x=0..299,  y=225..449): BLUE
//    Bot-right quadrant (x=300..599,y=225..449): YELLOW
//    Corner dots (10×10):  TL=WHITE  TR=CYAN  BL=MAGENTA  BR=ORANGE
//
//  Correct portrait (USB-left, rot=1) would show the pattern rotated 90° CW.
// ============================================================

// #define MADCTL_PROBE    // ← uncomment to enable probe

#ifdef MADCTL_PROBE

static void _probeDrawPattern() {
  // Quadrants
  for (int y = 0;   y < 225; y++) for (int x = 0;   x < 300; x++) fb[y*NATIVE_W+x] = fbColour(0xF800); // TL red
  for (int y = 0;   y < 225; y++) for (int x = 300; x < 600; x++) fb[y*NATIVE_W+x] = fbColour(0x07E0); // TR green
  for (int y = 225; y < 450; y++) for (int x = 0;   x < 300; x++) fb[y*NATIVE_W+x] = fbColour(0x001F); // BL blue
  for (int y = 225; y < 450; y++) for (int x = 300; x < 600; x++) fb[y*NATIVE_W+x] = fbColour(0xFFE0); // BR yellow
  // Corner markers (10×10)
  auto dot = [](int ox, int oy, uint16_t col) {
    for (int y=oy;y<oy+10;y++) for (int x=ox;x<ox+10;x++) fb[y*NATIVE_W+x]=fbColour(col);
  };
  dot(0,   0,   0xFFFF); // TL white
  dot(590, 0,   0x07FF); // TR cyan
  dot(0,   440, 0xF81F); // BL magenta
  dot(590, 440, 0xFD20); // BR orange
}

static void _probeFlushDirect(uint8_t madctl, uint8_t off_x, uint8_t off_y) {
  uint8_t m = madctl;
  _dispCmd(0x36, &m, 1);

  uint16_t xs = off_x, xe = NATIVE_W - 1 + off_x;
  uint16_t ys = off_y, ye = NATIVE_H - 1 + off_y;
  uint8_t ca[4]={uint8_t(xs>>8),uint8_t(xs),uint8_t(xe>>8),uint8_t(xe)};
  uint8_t ra[4]={uint8_t(ys>>8),uint8_t(ys),uint8_t(ye>>8),uint8_t(ye)};
  _dispCmd(0x2A, ca, 4);
  _dispCmd(0x2B, ra, 4);
  _dispCmd(0x2C, nullptr, 0);
  _dmaPixels(fb, (uint32_t)NATIVE_W * NATIVE_H);
}

void loop() {
  static const uint8_t CANDIDATES[] = {
    0x00, 0x20, 0x40, 0x60, 0x80, 0xA0, 0xC0, 0xE0
  };
  // Two address-window configs per MADCTL:
  //   off_x=0 off_y=16 = landscape window  (CASET 0..599, RASET 16..465)
  //   off_x=16 off_y=0 = portrait window   (CASET 16..465, RASET 0..599)
  static const uint8_t OX[] = { 0, 16 };
  static const uint8_t OY[] = { 16, 0 };

  for (int w = 0; w < 2; w++) {
    for (int i = 0; i < 8; i++) {
      uint8_t mv = CANDIDATES[i];
      const char *wlabel = (w == 0) ? "LAND" : "PORT";

      // Draw fresh test pattern
      _probeDrawPattern();

      // Overlay label in white on black background, centred in landscape fb[]
      // g_isLandscape=true so fbStr uses direct landscape path (no transform)
      char label[16];
      snprintf(label, sizeof(label), "0x%02X %s", mv, wlabel);
      fbFillRect(100, 180, 400, 80, C_BG);        // black label background
      fbFillRect(100, 182, 400, 76, 0x18C3);      // dark grey box
      g_isLandscape = true;                        // ensure landscape path in fbStr
      printCentred(label, 195, C_WHITE, FONT_HDG); // white 56px text, centred

      Serial.printf("MADCTL=0x%02X  off_x=%d off_y=%d  (%s window)\n",
                    mv, OX[w], OY[w], wlabel);
      displaySetBrightness(200);
      _probeFlushDirect(mv, OX[w], OY[w]);
      delay(3000);
    }
  }

  // Restore landscape after probe
  displaySetRotation(0);
  Serial.println("Probe done — returning to gauge");
  for(;;) delay(1000);
}

#else  // normal build

// ============================================================
//  LOOP  (core 1)
//
//  Each iteration: update sim data → touch → IMU → EEPROM →
//    warn auto-jump → redraw at 20 Hz.
// ============================================================

void loop() {
  // ── Simulated data ─────────────────────────────────────────
  updateSimData();
  updatePeaks();

  // ── DTC / CEL monitoring ───────────────────────────────────
  dtcCheckIgnoreReset();
  dtcCheck();

  // ── Touch — every frame for low latency ───────────────────
  handleTouch();

  // ── IMU orientation at 2 Hz ───────────────────────────────
  if (millis() - g_lastIMU > 500) {
    g_lastIMU = millis();
    updateOrientation();
  }

  // ── EEPROM debounced write ─────────────────────────────────
  if (g_eeDirty && (millis() - g_eeLastChg > EE_DEBOUNCE_MS))
    eepromSave();

  // ── OTA web server pump ────────────────────────────────────
  if (g_otaMode) handleOta();

  // ── Warn auto-jump ─────────────────────────────────────────
  checkWarnJump();

  // ── Draw at 20 Hz ─────────────────────────────────────────
  if (millis() - g_lastDraw > 50) {
    g_lastDraw = millis();
    if      (g_pickerOpen)   drawPicker();
    else if (g_otaConfirm)   drawOtaConfirm();
    else if (g_otaMode)      drawOta();
    else if (g_dtcPageOpen)  drawDtcPage();
    else if (g_settingsOpen) drawSettings();
    else if (isGraph())      { graphAddSample(g_val[g_graphSI]); drawGraph(); }
    else if (g_isLandscape)  drawLandscape();
    else                     drawPortrait();
  }
}

#endif  // MADCTL_PROBE
