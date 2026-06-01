#pragma once

// ============================================================
//  TJR_simdata.h — Simulated CAN data for bench/demo use
//
//  updateSimData() fills g_val[] with sinusoidal waveforms so
//  the dashboard looks alive without a live Haltech ECU.
//
//  Called every loop() iteration when no CAN data is available.
//  Remove this call (and eventually this file) once canTask is
//  hooked up and g_canActive is reliable.
// ============================================================

void updateSimData() {
  float t = millis() / 1000.0f;

  // ── Primary driving signals ────────────────────────────────
  g_val[ 0] = 4000.0f + 3500.0f * sinf(t * 0.8f);             // [0]  RPM
  g_val[ 1] = 150.0f  + 100.0f  * sinf(t * 0.6f);             // [1]  MAP abs kPa (101-350)
  g_val[ 2] = 50.0f   + 45.0f   * sinf(t * 1.1f);             // [2]  THROTTLE %
  g_val[ 3] = 300.0f  + 250.0f  * sinf(t * 0.55f);            // [3]  FUEL PRES gauge kPa
  g_val[ 4] = 300.0f  + 250.0f  * sinf(t * 0.5f);             // [4]  OIL PRES gauge kPa
  g_val[ 5] = 60.0f   + 35.0f   * sinf(t * 0.7f);             // [5]  ENG DMD %
  g_val[ 6] = 80.0f   + 60.0f   * sinf(t * 0.65f);            // [6]  WGATE gauge kPa
  g_val[ 7] = 45.0f   + 40.0f   * sinf(t * 0.9f);             // [7]  INJ DC1 %
  g_val[ 8] = 20.0f   + 15.0f   * sinf(t * 0.85f);            // [8]  IGN ANG deg
  g_val[ 9] = 1.0f    + 0.25f   * sinf(t * 1.3f);             // [9]  WB1 lambda
  g_val[10] = 1.0f    + 0.2f    * sinf(t * 1.2f);             // [10] WB2 lambda
  g_val[11] = 1.0f    + 0.22f   * sinf(t * 1.25f);            // [11] WB OVR lambda

  // ── Knock / turbo ─────────────────────────────────────────
  g_val[12] = 2.0f    + 2.5f    * fabsf(sinf(t * 2.1f));      // [12] KNOCK 1 dB
  g_val[13] = 1.5f    + 2.0f    * fabsf(sinf(t * 1.9f));      // [13] KNOCK 2 dB
  g_val[14] = 80.0f   + 70.0f   * sinf(t * 0.75f);            // [14] TURBO SPD kRPM
  g_val[15] = 5.0f    * sinf(t * 0.4f);                        // [15] LAT G m/s2
  g_val[16] = 3.0f    * sinf(t * 0.5f);                        // [16] LONG G m/s2
  g_val[17] = 80.0f   + 70.0f   * fabsf(sinf(t * 0.3f));      // [17] VEH SPD km/h
  g_val[18] = floorf(fabsf(sinf(t * 0.35f)) * 6.0f) + 1.0f;  // [18] GEAR 1-6
  g_val[19] = 13.0f   + 1.5f    * sinf(t * 0.4f);             // [19] BATTERY V

  // ── Boost / baro ──────────────────────────────────────────
  g_val[20] = 150.0f  + 80.0f   * sinf(t * 0.58f);            // [20] TGT BOOST gauge kPa
  g_val[21] = 101.3f  + 1.0f    * sinf(t * 0.1f);             // [21] BARO abs kPa
  g_val[22] = 120.0f  + 90.0f   * sinf(t * 0.62f);            // [22] BOOST PRE gauge kPa

  // ── EGTs in Kelvin (1023K = 750°C) ────────────────────────
  g_val[23] = 1023.0f + 150.0f  * sinf(t * 0.45f);            // [23] EGT 1 K
  g_val[24] = 1048.0f + 120.0f  * sinf(t * 0.47f);            // [24] EGT 2 K
  g_val[25] = 998.0f  + 140.0f  * sinf(t * 0.43f);            // [25] EGT 3 K
  g_val[26] = 1013.0f + 130.0f  * sinf(t * 0.41f);            // [26] EGT 4 K

  // ── Temperatures in Kelvin ─────────────────────────────────
  g_val[27] = 353.0f  + 15.0f   * sinf(t * 0.2f);             // [27] COOLANT K
  g_val[28] = 303.0f  + 15.0f   * sinf(t * 0.18f);            // [28] AIR TEMP K
  g_val[29] = 363.0f  + 20.0f   * sinf(t * 0.22f);            // [29] OIL TEMP K
  g_val[30] = 298.0f  + 8.0f    * sinf(t * 0.15f);            // [30] FUEL TEMP K
  g_val[31] = 348.0f  + 18.0f   * sinf(t * 0.21f);            // [31] GB OIL T K
  g_val[32] = 333.0f  + 25.0f   * sinf(t * 0.25f);            // [32] AIR PRE-IC K

  // ── Fuel / consumption ─────────────────────────────────────
  g_val[33] = 30.0f   + 20.0f   * sinf(t * 0.08f);            // [33] FUEL COMP % ethanol
  g_val[34] = 55.0f   - t * 0.02f < 5.0f ? 5.0f              // [34] FUEL LVL L (draining slowly)
            : 55.0f - t * 0.02f;
  g_val[35] = fmodf(t * 0.3f, 150.0f);                        // [35] FUEL USED L
  g_val[36] = 8.0f    + 4.0f    * sinf(t * 0.5f);             // [36] AVG CONS L/100
  g_val[37] = 12.0f   + 8.0f    * fabsf(sinf(t * 0.9f));      // [37] INST CONS L/100
  g_val[38] = 1500.0f + 1200.0f * fabsf(sinf(t * 0.7f));      // [38] FUEL FLOW cc/min

  // ── Fuel trims — signed values ─────────────────────────────
  g_val[39] = 5.0f    * sinf(t * 1.7f);                        // [39] ST TRIM B1 %
  g_val[40] = 3.0f    * sinf(t * 0.3f);                        // [40] LT TRIM B1 %
  g_val[41] = 3.0f    * sinf(t * 2.0f);                        // [41] WHL SLIP km/h
  g_val[42] = 50.0f   + 45.0f   * sinf(t * 1.1f);             // [42] ACCEL PED %
  g_val[43] = 80.0f   + 60.0f   * sinf(t * 0.6f);             // [43] EXH MAN P gauge kPa
  g_val[44] = 323.0f  + 5.0f    * sinf(t * 0.05f);            // [44] ECU TEMP K

  // ── Status / bit-field signals ─────────────────────────────
  g_val[45] = floorf(fabsf(sinf(t * 0.1f)) * 4.0f);           // [45] ENG STATE 0-3
  // CEL fires for ~18 s every ~90 s cycle.  Cycles through 4 DTC codes every 4 s.
  g_val[46] = (sinf(t * 0.07f) > 0.8f) ? 1.0f : 0.0f;         // [46] CHK ENG
  if (g_val[46] > 0.5f) {
    static const uint16_t SIM_DTCS[] = {
      0x0373,   // P0373 Engine Position Error
      0x2A00,   // P2A00 WB 1 Sensor Failure
      0x1300,   // P1300 Trigger Sync Error
      0x0328,   // P0328 Knock Sensor 1 Disc
    };
    g_engineProtReason = SIM_DTCS[(millis() / 4000) % 4];
  } else {
    g_engineProtReason = 0;
  }
  g_val[47] = (sinf(t * 0.12f) > 0.85f) ? 1.0f : 0.0f;       // [47] OIL LITE
  g_val[48] = 50.0f   + 30.0f   * sinf(t * 0.55f);            // [48] BRK PRES gauge kPa
  g_val[49] = 55.0f   + 40.0f   * sinf(t * 0.1f);             // [49] FUEL LVL% %
  g_val[50] = floorf(fabsf(sinf(t * 0.05f)) * 5.0f);          // [50] TRIG ERR count
  g_val[51] = (sinf(t * 0.09f) > 0.85f) ? 1.0f : 0.0f;       // [51] BATT LT
  g_val[52] = (sinf(t * 0.11f) > 0.85f) ? 1.0f : 0.0f;       // [52] ENG LIM
  g_val[53] = (sinf(t * 0.13f) > 0.9f)  ? 1.0f : 0.0f;       // [53] LAUNCH
  g_val[54] = (sinf(t * 0.14f) > 0.9f)  ? 1.0f : 0.0f;       // [54] AUX LIM
  g_val[55] = (sinf(t * 1.5f)  > 0.6f)  ? 1.0f : 0.0f;       // [55] DECEL
  g_val[56] = (sinf(t * 0.16f) > 0.85f) ? 1.0f : 0.0f;       // [56] TC ACT
  g_val[57] = (sinf(t * 0.08f) > 0.9f)  ? 1.0f : 0.0f;       // [57] ABS ERR
  g_val[58] = (sinf(t * 0.17f) > 0.85f) ? 1.0f : 0.0f;       // [58] ANTILAG
  g_val[59] = (sinf(t * 0.06f) > 0.9f)  ? 1.0f : 0.0f;       // [59] PIT LIM

  // Peak / min tracking lives in updatePeaks() (TJR_params.h), called every loop().
}
