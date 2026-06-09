#pragma once
/*
 * TJR_params.h — Signal parameter table, slot system, live value arrays,
 *                and unit conversion functions.
 *
 * Board-agnostic: nothing here is specific to any display, touch controller,
 * or graphics driver.  Copy verbatim to any TJR_* port — only pin numbers and
 * the CAN TX/RX GPIOs change between boards.
 *
 * Include exclusively from TJR_241.ino (single translation unit).
 * Full definitions live here rather than extern declarations because the
 * Arduino build system compiles everything into one .cpp.
 *
 * Depends on: TJR_types.h (ParamDef struct + UNIT_TYPE_* constants).
 */

#include "TJR_types.h"
#include <stdint.h>
#include <string.h>    // strlen
#include <stdio.h>     // snprintf
#include <math.h>      // isinf (used by callers)


// ── Signal / slot counts ─────────────────────────────────────────────────────
#define NUM_SIGNALS  75
#define NUM_PAIRS     5
#define NUM_SLOTS    15

// ── Slot index helpers ───────────────────────────────────────────────────────
//   portrait top  = pairs 0-4  →  slots  0-4
//   portrait bot  = pairs 0-4  →  slots  5-9
//   landscape     = pairs 0-4  →  slots 10-14
inline int slotTop(int pair) { return pair;      }
inline int slotBot(int pair) { return pair + 5;  }
inline int slotLS (int pair) { return pair + 10; }


// ── Signal parameter table ───────────────────────────────────────────────────
//   60 signals from Haltech ECU Broadcast CAN v2.0.
//   Native units: temperatures in Kelvin, pressures per offset field.
//   g_val[i] = raw / divisor - offset  (Haltech protocol formula).
//   displayMin/Max and warn thresholds are all in native engineering units.
//
//   Column order:
//   canID | sB | eB | sgn | bf | mask | div | off | name | unit |
//   dMin | dMax | wL | wH | dL | dH | dec | snap | unitType

static const ParamDef PARAMS[NUM_SIGNALS] = {

// [0] RPM
{0x360,0,1,false,false,0,  1.0f,  0,  "RPM",       "rpm",    0,  9000,  500, 7500,    0, 9000, 0, 1.0f, UNIT_TYPE_NONE},
// [1] MAP — absolute kPa
{0x360,2,3,false,false,0, 10.0f,  0,  "MAP",        "kPa",   50,   350,   50,  280,   50,  350, 1, 1.0f, UNIT_TYPE_PRESS_ABS},
// [2] Throttle Position
{0x360,4,5,false,false,0, 10.0f,  0,  "THROTTLE",   "%",      0,   100,    5,   98,    0,  100, 1, 1.0f, UNIT_TYPE_NONE},
// [3] Fuel Pressure — gauge kPa
{0x361,0,1,false,false,0, 10.0f,101.3f,"FUEL PRES", "kPa",    0,   600,  100,  500,    0,  600, 1, 1.0f, UNIT_TYPE_PRESS_G},
// [4] Oil Pressure — gauge kPa
{0x361,2,3,false,false,0, 10.0f,101.3f,"OIL PRES",  "kPa",    0,   700,  100,  600,    0,  700, 1, 1.0f, UNIT_TYPE_PRESS_G},
// [5] Engine Demand
{0x361,4,5,false,false,0, 10.0f,  0,  "ENG DMD",    "%",      0,   100,    5,   98,    0,  100, 1, 1.0f, UNIT_TYPE_NONE},
// [6] Wastegate Pressure — gauge kPa
{0x361,6,7,false,false,0, 10.0f,101.3f,"WGATE",     "kPa",  -50,   300,  -50,  250, -100,  300, 1, 1.0f, UNIT_TYPE_PRESS_G},
// [7] Injection Duty Cycle Stage 1
{0x362,0,1,false,false,0, 10.0f,  0,  "INJ DC1",    "%",      0,   100,    5,   85,    0,  100, 1, 1.0f, UNIT_TYPE_NONE},
// [8] Ignition Angle — signed degrees
{0x362,4,5,true, false,0, 10.0f,  0,  "IGN ANG",    "deg",  -20,    60,  -15,   50,  -25,   65, 1, 1.0f, UNIT_TYPE_NONE},
// [9] Wideband Lambda Sensor 1
{0x368,0,1,false,false,0,1000.0f, 0,  "WB1",        "Lam",  0.6f, 1.6f,0.75f,1.25f, 0.6f,1.6f, 3,0.005f,UNIT_TYPE_LAMBDA},
// [10] Wideband Lambda Sensor 2
{0x368,2,3,false,false,0,1000.0f, 0,  "WB2",        "Lam",  0.6f, 1.6f,0.75f,1.25f, 0.6f,1.6f, 3,0.005f,UNIT_TYPE_LAMBDA},
// [11] Wideband Overall
{0x470,0,1,false,false,0,1000.0f, 0,  "WB OVR",     "Lam",  0.6f, 1.6f,0.75f,1.25f, 0.6f,1.6f, 3,0.005f,UNIT_TYPE_LAMBDA},
// [12] Knock Level 1
{0x36A,0,1,false,false,0,100.0f,  0,  "KNOCK 1",    "dB",    0,    10,    0,    6,    0,   10, 1, 1.0f, UNIT_TYPE_NONE},
// [13] Knock Level 2
{0x36A,2,3,false,false,0,100.0f,  0,  "KNOCK 2",    "dB",    0,    10,    0,    6,    0,   10, 1, 1.0f, UNIT_TYPE_NONE},
// [14] Turbo Speed kRPM
{0x36B,4,5,false,false,0,100.0f,  0,  "TURBO SPD",  "kRPM",  0,   250,   10,  220,    0,  250, 1, 1.0f, UNIT_TYPE_NONE},
// [15] Lateral G — signed m/s2
{0x36B,6,7,true, false,0, 10.0f,  0,  "LAT G",      "m/s2", -30,   30,  -25,   25,  -35,   35, 1, 1.0f, UNIT_TYPE_NONE},
// [16] Longitudinal G — signed m/s2
{0x36E,6,7,true, false,0, 10.0f,  0,  "LONG G",     "m/s2", -30,   30,  -25,   25,  -35,   35, 1, 1.0f, UNIT_TYPE_NONE},
// [17] Vehicle Speed km/h
{0x370,0,1,false,false,0, 10.0f,  0,  "VEH SPD",    "km/h",  0,   350,   10,  320,    0,  350, 1, 1.0f, UNIT_TYPE_SPEED},
// [18] Gear — signed int8 (-1=R 0=N)
{0x470,7,7,true, false,0,  1.0f,  0,  "GEAR",       "",      -1,   10,   -1,    9,   -2,   11, 0, 1.0f, UNIT_TYPE_NONE},
// [19] Battery Voltage
{0x372,0,1,false,false,0, 10.0f,  0,  "BATTERY",    "V",     8,    16,   11,   15,    8,   16, 1, 1.0f, UNIT_TYPE_NONE},
// [20] Target Boost — gauge kPa
{0x372,4,5,false,false,0, 10.0f,  0,  "TGT BOOST",  "kPa",  -50,  300,  -50,  250, -100,  300, 1, 1.0f, UNIT_TYPE_PRESS_G},
// [21] Barometric Pressure — absolute kPa
{0x372,6,7,false,false,0, 10.0f,  0,  "BARO",       "kPa",   80,  110,   85,  108,   75,  115, 1, 1.0f, UNIT_TYPE_PRESS_ABS},
// [22] Boost Pre-Intercooler — gauge kPa
{0x377,0,1,false,false,0, 10.0f,  0,  "BOOST PRE",  "kPa",  -50,  300,  -50,  250, -100,  300, 1, 1.0f, UNIT_TYPE_PRESS_G},
// [23] EGT 1 — Kelvin (273K=0C  1673K=1400C)
{0x373,0,1,false,false,0, 10.0f,  0,  "EGT 1",      "K",    273, 1673,  873, 1173,  273, 1273, 0, 1.0f, UNIT_TYPE_TEMP},
// [24] EGT 2
{0x373,2,3,false,false,0, 10.0f,  0,  "EGT 2",      "K",    273, 1673,  873, 1173,  273, 1273, 0, 1.0f, UNIT_TYPE_TEMP},
// [25] EGT 3
{0x373,4,5,false,false,0, 10.0f,  0,  "EGT 3",      "K",    273, 1673,  873, 1173,  273, 1273, 0, 1.0f, UNIT_TYPE_TEMP},
// [26] EGT 4
{0x373,6,7,false,false,0, 10.0f,  0,  "EGT 4",      "K",    273, 1673,  873, 1173,  273, 1273, 0, 1.0f, UNIT_TYPE_TEMP},
// [27] Coolant Temp — Kelvin (313K=40C  368K=95C)
{0x3E0,0,1,false,false,0, 10.0f,  0,  "COOLANT",    "K",    278,  398,  313,  368,  273,  383, 0, 1.0f, UNIT_TYPE_TEMP},
// [28] Intake Air Temp — Kelvin
{0x3E0,2,3,false,false,0, 10.0f,  0,  "AIR TEMP",   "K",    233,  373,  258,  343,  223,  358, 0, 1.0f, UNIT_TYPE_TEMP},
// [29] Oil Temp — Kelvin
{0x3E0,6,7,false,false,0, 10.0f,  0,  "OIL TEMP",   "K",    278,  423,  323,  393,  273,  408, 0, 1.0f, UNIT_TYPE_TEMP},
// [30] Fuel Temp — Kelvin
{0x3E0,4,5,false,false,0, 10.0f,  0,  "FUEL TEMP",  "K",    233,  373,  258,  343,  223,  358, 0, 1.0f, UNIT_TYPE_TEMP},
// [31] Gearbox Oil Temp — Kelvin
{0x3E1,0,1,false,false,0, 10.0f,  0,  "GB OIL T",   "K",    278,  423,  323,  393,  273,  408, 0, 1.0f, UNIT_TYPE_TEMP},
// [32] Air Temp Pre-Intercooler — Kelvin
{0x3E1,6,7,false,false,0, 10.0f,  0,  "AIR PRE-IC", "K",    233,  423,  258,  373,  223,  423, 0, 1.0f, UNIT_TYPE_TEMP},
// [33] Fuel Composition / Ethanol %
{0x3E1,4,5,false,false,0, 10.0f,  0,  "FUEL COMP",  "%",     0,   100,    5,   90,    0,  100, 1, 1.0f, UNIT_TYPE_NONE},
// [34] Fuel Level litres
{0x3E2,0,1,false,false,0, 10.0f,  0,  "FUEL LVL",   "L",     0,   100,   10,   95,    0,  100, 1, 1.0f, UNIT_TYPE_NONE},
// [35] Total Fuel Used — 32-bit (0x473 b0-3), native cc, ÷1000 → litres
{0x473,0,3,false,false,0,1000.0f,0,  "FUEL USED",  "L",     0,   200,   10,  180,    0,  200, 1, 1.0f, UNIT_TYPE_NONE},
// [36] Average Fuel Consumption L/100km
{0x701,0,1,false,false,0, 10.0f,  0,  "AVG CONS",   "L/100", 0,    30,    2,   25,    0,   30, 1, 1.0f, UNIT_TYPE_NONE},
// [37] Instantaneous Fuel Consumption
{0x701,2,3,false,false,0, 10.0f,  0,  "INST CONS",  "L/100", 0,    50,    2,   40,    0,   50, 1, 1.0f, UNIT_TYPE_NONE},
// [38] Fuel Flow Rate L/hr
{0x371,0,1,false,false,0,  1.0f,  0,  "FUEL FLOW",  "cc/min",0,  4000,    0, 3500,    0, 4000, 0, 1.0f, UNIT_TYPE_NONE},
// [39] Short Term Fuel Trim Bank 1 — signed %
{0x3E3,0,1,true, false,0, 10.0f,  0,  "ST TRIM B1", "%",    -25,   25,  -15,   15,  -25,   25, 1, 1.0f, UNIT_TYPE_NONE},
// [40] Long Term Fuel Trim Bank 1 — signed %
{0x3E3,4,5,true, false,0, 10.0f,  0,  "LT TRIM B1", "%",    -25,   25,  -15,   15,  -25,   25, 1, 1.0f, UNIT_TYPE_NONE},
// [41] Wheel Slip — signed km/h
{0x363,0,1,true, false,0, 10.0f,  0,  "WHL SLIP",   "km/h", -50,   50,  -30,   30,  -60,   60, 1, 1.0f, UNIT_TYPE_NONE},
// [42] Accelerator Pedal Position
{0x471,2,3,false,false,0, 10.0f,  0,  "ACCEL PED",  "%",     0,   100,    5,   98,    0,  100, 1, 1.0f, UNIT_TYPE_NONE},
// [43] Exhaust Manifold Pressure — gauge kPa
{0x471,4,5,false,false,0, 10.0f,  0,  "EXH MAN P",  "kPa",   0,   300,    0,  250,    0,  300, 1, 1.0f, UNIT_TYPE_PRESS_G},
// [44] ECU Internal Temperature — Kelvin
{0x469,0,1,false,false,0, 10.0f,  0,  "ECU TEMP",   "K",    273,  373,  303,  358,  273,  373, 0, 1.0f, UNIT_TYPE_TEMP},
// [45] Engine State nibble
{0x6F4,1,1,false,true, 0x0F, 1.0f,0,  "ENG STATE",  "",      0,    4,    0,    4,    0,    4, 0, 1.0f, UNIT_TYPE_NONE},
// [46] Check Engine Light — 0x3E4 bit 7:7
{0x3E4,7,7,false,true, 0x80,128.0f,0, "CHK ENG",    "",      0,    1,    0,    0,    0,    1, 0, 1.0f, UNIT_TYPE_NONE},
// [47] Oil Pressure Warning Light
{0x3E4,1,1,false,true, 0x01, 1.0f,0,  "OIL LITE",   "",      0,    1,    0,    0,    0,    1, 0, 1.0f, UNIT_TYPE_NONE},
// [48] Brake Pressure Front — gauge kPa
{0x36B,0,1,false,false,0, 1.0f,101.3f,"BRK PRES",   "kPa",   0,   200,    0,  180,    0,  200, 1, 1.0f, UNIT_TYPE_PRESS_G},
// [49] Fuel Level Percentage
{0x6F9,0,1,false,false,0, 10.0f,  0,  "FUEL LVL%",  "%",     0,   100,   10,   95,    0,  100, 1, 1.0f, UNIT_TYPE_NONE},
// [50] Trigger / sync error count
{0x369,0,1,false,false,0,  1.0f,  0,  "TRIG ERR",   "",      0,  1000,    0,   10,    0, 1000, 0, 1.0f, UNIT_TYPE_NONE},
// [51] Battery Light Active — 0x3E4 bit 7:6
{0x3E4,7,7,false,true, 0x40,64.0f,0,  "BATT LT",    "",      0,    1,    0,    0,    0,    1, 0, 1.0f, UNIT_TYPE_NONE},
// [52] Engine Limiting Active
{0x36E,0,1,false,false,0,  1.0f,  0,  "ENG LIM",    "",      0,    1,    0,    0,    0,    1, 0, 1.0f, UNIT_TYPE_NONE},
// [53] Launch Control Active
{0x3E4,2,2,false,true, 0x80,128.0f,0, "LAUNCH",     "",      0,    1,    0,    0,    0,    1, 0, 1.0f, UNIT_TYPE_NONE},
// [54] Aux RPM Limiter Active
{0x3E4,2,2,false,true, 0x20, 32.0f,0, "AUX LIM",    "",      0,    1,    0,    0,    0,    1, 0, 1.0f, UNIT_TYPE_NONE},
// [55] Decel Cut Active
{0x3E4,1,1,false,true, 0x10, 16.0f,0, "DECEL",      "",      0,    1,    0,    0,    0,    1, 0, 1.0f, UNIT_TYPE_NONE},
// [56] Traction Control Active
{0x3E4,3,3,false,true, 0x40, 64.0f,0, "TC ACT",     "",      0,    1,    0,    0,    0,    1, 0, 1.0f, UNIT_TYPE_NONE},
// [57] ABS Error
{0x3E5,3,3,false,true, 0x04,  4.0f,0, "ABS ERR",    "",      0,    1,    0,    0,    0,    1, 0, 1.0f, UNIT_TYPE_NONE},
// [58] Antilag Output State — 0x473 bit 4:5
{0x473,4,4,false,true, 0x20, 32.0f,0, "ANTILAG",    "",      0,    1,    0,    0,    0,    1, 0, 1.0f, UNIT_TYPE_NONE},
// [59] Pit Lane Speed Limiter Active
{0x3E5,3,3,false,true, 0x20, 32.0f,0, "PIT LIM",    "",      0,    1,    0,    0,    0,    1, 0, 1.0f, UNIT_TYPE_NONE},
// [60] Coolant Pressure — gauge kPa (0x360 bytes 6-7)
{0x360,6,7,false,false,0, 10.0f,101.3f,"COOL PRES",  "kPa",  -50,  300,  -50,  250, -100,  300, 1, 1.0f, UNIT_TYPE_PRESS_G},
// [61] Injection Stage 2 Duty Cycle (0x362 bytes 2-3)
{0x362,2,3,false,false,0, 10.0f,  0,  "INJ DC2",     "%",      0,   100,    5,   85,    0,  100, 1, 1.0f, UNIT_TYPE_NONE},
// [62] Trigger Counter — raw count (0x369 bytes 2-3)
{0x369,2,3,false,false,0,  1.0f,  0,  "TRIG CNT",    "",       0, 65535,    0,    0,    0,65535, 0, 1.0f, UNIT_TYPE_NONE},
// [63] NOS Pressure Sensor 1 — gauge kPa (0x36B bytes 2-3, y=x*11/50-101.3)
{0x36B,2,3,false,false,0, 50.0f/11.0f,101.3f,"NOS PRES1","kPa", 0,14000,    0,12000,    0,14316, 1, 1.0f, UNIT_TYPE_PRESS_G},
// [64] Exhaust Cam Angle 1 — signed degrees (0x36D bytes 4-5)
{0x36D,4,5,true, false,0, 10.0f,  0,  "EXH CAM1",   "deg",   -50,   50,  -40,   40,  -60,   60, 1, 1.0f, UNIT_TYPE_NONE},
// [65] Exhaust Cam Angle 2 — signed degrees (0x36D bytes 6-7)
{0x36D,6,7,true, false,0, 10.0f,  0,  "EXH CAM2",   "deg",   -50,   50,  -40,   40,  -60,   60, 1, 1.0f, UNIT_TYPE_NONE},
// [66] Generic Output 1 Duty Cycle (0x36F bytes 0-1)
{0x36F,0,1,false,false,0, 10.0f,  0,  "GEN OUT1",    "%",      0,   100,    0,  100,    0,  100, 1, 1.0f, UNIT_TYPE_NONE},
// [67] Boost Control Output (0x36F bytes 2-3)
{0x36F,2,3,false,false,0, 10.0f,  0,  "BST CTRL",    "%",      0,   100,    0,  100,    0,  100, 1, 1.0f, UNIT_TYPE_NONE},
// [68] Intake Cam Angle 1 — signed degrees (0x370 bytes 4-5)
{0x370,4,5,true, false,0, 10.0f,  0,  "INT CAM1",   "deg",   -50,   50,  -40,   40,  -60,   60, 1, 1.0f, UNIT_TYPE_NONE},
// [69] Intake Cam Angle 2 — signed degrees (0x370 bytes 6-7)
{0x370,6,7,true, false,0, 10.0f,  0,  "INT CAM2",   "deg",   -50,   50,  -40,   40,  -60,   60, 1, 1.0f, UNIT_TYPE_NONE},
// [70] Diff Oil Temperature — Kelvin (0x3E1 bytes 2-3)
{0x3E1,2,3,false,false,0, 10.0f,  0,  "DIFF TEMP",  "K",     278,  423,  323,  393,  273,  408, 0, 1.0f, UNIT_TYPE_TEMP},
// [71] Fuel Trim Short Term Bank 2 — signed % (0x3E3 bytes 2-3)
{0x3E3,2,3,true, false,0, 10.0f,  0,  "ST TRIM B2", "%",     -25,   25,  -15,   15,  -25,   25, 1, 1.0f, UNIT_TYPE_NONE},
// [72] Fuel Trim Long Term Bank 2 — signed % (0x3E3 bytes 6-7)
{0x3E3,6,7,true, false,0, 10.0f,  0,  "LT TRIM B2", "%",     -25,   25,  -15,   15,  -25,   25, 1, 1.0f, UNIT_TYPE_NONE},
// [73] Target Lambda (0x3E9 bytes 4-5)
{0x3E9,4,5,false,false,0,1000.0f, 0,  "TGT LAM",    "Lam",  0.6f, 1.6f,0.75f,1.25f, 0.6f, 1.6f, 3,0.005f,UNIT_TYPE_LAMBDA},
// [74] Injector Pressure Differential — signed kPa (0x471 bytes 0-1)
{0x471,0,1,true, false,0, 10.0f,  0,  "INJ P DIFF", "kPa",  -100, 1000, -100,  800, -100, 1000, 1, 1.0f, UNIT_TYPE_PRESS_G},
};


// ── Default slot→signal assignments (first boot / EEPROM invalid) ─────────────
//   Pair 0: RPM top / MAP bot / RPM landscape
//   Pair 1: THROTTLE top / INJ DC1 bot / OIL PRES landscape
//   Pair 2: COOLANT top / WB1 bot / COOLANT landscape
//   Pair 3: BATTERY top / ST TRIM bot / OIL TEMP landscape
//   Pair 4: GEAR top / FUEL USED bot / VEH SPD landscape
static const uint8_t DEFAULT_SLOTS[NUM_SLOTS] = {
   0,  2, 27, 19, 18,   // portrait top:  RPM THROTTLE COOLANT BATTERY GEAR
   1,  7,  9, 39, 35,   // portrait bot:  MAP INJ_DC1 WB1 ST_TRIM FUEL_USED
   0,  4, 27, 29, 17,   // landscape:     RPM OIL_PRES COOLANT OIL_TEMP VEH_SPD
};


// ── Slot assignments and per-slot warn thresholds ────────────────────────────
// NUM_SLOTS (15) covers pair-screen slots 0..14.
// Two extra entries (15=SLOT_DUO_TOP, 16=SLOT_DUO_BOT) back the duo-HS screen;
// their EEPROM persistence lives at EE_ADDR_DUO_* (separate from the main block).
#define NUM_SLOTS_TOTAL 17
uint8_t g_slotParam[NUM_SLOTS_TOTAL];
float   g_warnLow [NUM_SLOTS_TOTAL];
float   g_warnHigh[NUM_SLOTS_TOTAL];

// Helper: returns the ParamDef for whatever signal is currently in a slot
inline const ParamDef &slotDef(int slot) { return PARAMS[g_slotParam[slot]]; }

// ── Alarm-state predicates (single source of truth) ──────────────────────────
inline bool slotHasAlarms(int slot) { return g_warnHigh[slot] > g_warnLow[slot]; }
inline bool inDanger(int slot, float v) { return slotHasAlarms(slot) && v >= g_warnHigh[slot]; }
inline bool inWarn  (int slot, float v) { return slotHasAlarms(slot) && v <= g_warnLow[slot];  }


// ── Live values, peaks, minimums (indexed by signal, not slot) ───────────────
// g_val[] written by canTask (core 0), read by UI (core 1) — must stay volatile.
volatile float g_val [NUM_SIGNALS];
float          g_peak[NUM_SIGNALS];
float          g_min [NUM_SIGNALS];

inline void updatePeaks() {
  for (int i = 0; i < NUM_SIGNALS; i++) {
    float v = g_val[i];
    if (v > g_peak[i]) g_peak[i] = v;
    if (v < g_min[i])  g_min[i]  = v;
  }
}


// ── Unit preference flags ────────────────────────────────────────────────────
bool g_unitsCelsius = true;   // true=°C  false=°F
bool g_unitsKpa     = true;   // true=kPa false=psi
bool g_unitsLambda  = true;   // true=λ   false=AFR
bool g_unitsKmh     = true;   // true=km/h false=mph


// ── Unit conversion ──────────────────────────────────────────────────────────

float convertVal(int si, float v) {
  switch (PARAMS[si].unitType) {
    case UNIT_TYPE_TEMP:
      v -= 273.15f;
      return g_unitsCelsius ? v : v * 9.0f / 5.0f + 32.0f;
    case UNIT_TYPE_PRESS_G:
    case UNIT_TYPE_PRESS_ABS:
      return g_unitsKpa ? v : v * 0.145038f;
    case UNIT_TYPE_SPEED:
      return g_unitsKmh ? v : v * 0.621371f;
    case UNIT_TYPE_LAMBDA:
      return g_unitsLambda ? v : v * 14.7f;
    default:
      return v;
  }
}

const char *displayUnit(int si) {
  switch (PARAMS[si].unitType) {
    case UNIT_TYPE_TEMP:      return g_unitsCelsius ? "C"    : "F";
    case UNIT_TYPE_PRESS_G:
    case UNIT_TYPE_PRESS_ABS: return g_unitsKpa     ? "kPa"  : "psi";
    case UNIT_TYPE_SPEED:     return g_unitsKmh     ? "km/h" : "mph";
    case UNIT_TYPE_LAMBDA:    return g_unitsLambda  ? "Lam"  : "AFR";
    default:                  return PARAMS[si].unit;
  }
}

void fmtVal(char *buf, size_t sz, int si, float v) {
  float dv  = convertVal(si, v);
  int   dec = PARAMS[si].decimals;
  if (PARAMS[si].unitType == UNIT_TYPE_LAMBDA && !g_unitsLambda) dec = 1;
  switch (dec) {
    case 0:  snprintf(buf, sz, "%.0f", dv); break;
    case 1:  snprintf(buf, sz, "%.1f", dv); break;
    default: snprintf(buf, sz, "%.3f", dv); break;
  }
}
