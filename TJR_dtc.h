#pragma once
/*
 * TJR_dtc.h — Check Engine Light detection and DTC warning page.
 *
 * Ported from TJR_mini/TJR_dtc.h. Draw/touch functions rewritten for
 * raw fb[] (no Arduino_GFX). DTC lookup table verbatim.
 *
 * Layout
 * ──────
 * Landscape 600×450: left panel (icon+label, 230px) | divider | right panel (codes+OK)
 * Portrait  450×600: icon top, "CHECK ENGINE", divider, codes, OK button at bottom.
 *
 * FONT_DTC (28px) used for descriptions — fits longest Haltech strings in landscape.
 * g_dtcPageOpen declared in TJR_241.ino — NOT redeclared here.
 */

#include "cel_icon.h"   // CEL_ICON_RGB[7680], CEL_ICON_RGB_W=96, CEL_ICON_RGB_H=80


// ── DTC lookup table ──────────────────────────────────────────────────────
// Raw 16-bit key uses OBD-II DTC encoding:
//   bits 15-14 = letter (00=P 01=B 10=C 11=U)
//   bits 13-0  = 4 hex digits of the code suffix (BCD, e.g. P0300 → 0x0300)
// Source: Haltech DTC Index (support.haltech.com) — all descriptions are
// Haltech-specific; codes with hex digits A-F are genuine Haltech extensions.
// Table is sorted ascending by raw value — required for binary search.

struct DtcEntry { uint16_t raw; char desc[24]; };

static const DtcEntry DTC_TABLE[] PROGMEM = {

    // ── P00xx ────────────────────────────────────────────────────────────
    {0x0003, "Fuel Flow Raw Min"},
    {0x0004, "Fuel Flow Raw Max"},
    {0x0070, "Amb Air Temp Op Min"},
    {0x0071, "Amb Air Temp Op Max"},
    {0x0072, "Amb Air Temp Raw Min"},
    {0x0073, "Amb Air Temp Raw Max"},
    {0x0087, "Fuel Flow Rtn Raw Min"},
    {0x0088, "Fuel Flow Rtn Raw Max"},
    {0x00F2, "Humidity Raw Min"},
    {0x00F3, "Humidity Raw Max"},
    {0x00F5, "Humidity Op Max"},
    // ── P01xx ────────────────────────────────────────────────────────────
    {0x0100, "MAF Sensor 1 Raw Min"},
    {0x0101, "MAF Circuit Range"},
    {0x0102, "MAF Sensor 2 Raw Min"},
    {0x0103, "MAF Sensor 2 Raw Max"},
    {0x0104, "MAF Sensor 1 Raw Max"},
    {0x0105, "MAP Sensor Op Min"},
    {0x0106, "MAP Sensor Op Max"},
    {0x0107, "MAP Sensor Raw Min"},
    {0x0108, "MAP Sensor Raw Max"},
    {0x0109, "MAP Hose Failure"},
    {0x0110, "Intake Air Temp Op Min"},
    {0x0111, "Intake Air Temp Op Max"},
    {0x0112, "Intake Air Temp Raw Min"},
    {0x0113, "Intake Air Temp Raw Max"},
    {0x0115, "Coolant Pres Op Min"},
    {0x0116, "Coolant Temp Op Max"},
    {0x0117, "Coolant Temp Raw Min"},
    {0x0118, "Coolant Temp Raw Max"},
    {0x0122, "TPS Switch Raw Min"},
    {0x0123, "TPS Switch Raw Max"},
    {0x0128, "Coolant Pres Op Max"},
    {0x0131, "WB O2 Sensor 1 Raw Min"},
    {0x0132, "WB O2 Sensor 1 Raw Max"},
    {0x0137, "WB O2 Sensor 3 Raw Min"},
    {0x0138, "WB O2 Sensor 3 Raw Max"},
    {0x0143, "NB O2 Sensor 1 Raw Min"},
    {0x0144, "NB O2 Sensor 1 Raw Max"},
    {0x0151, "WB O2 Sensor 2 Raw Min"},
    {0x0152, "WB O2 Sensor 2 Raw Max"},
    {0x0157, "WB O2 Sensor 4 Raw Min"},
    {0x0158, "WB O2 Sensor 4 Raw Max"},
    {0x0163, "NB O2 Sensor 2 Raw Min"},
    {0x0164, "NB O2 Sensor 2 Raw Max"},
    {0x0178, "Fuel Comp Raw Min"},
    {0x0179, "Fuel Comp Raw Max"},
    {0x0182, "Fuel Temp 1 Raw Min"},
    {0x0183, "Fuel Temp 1 Raw Max"},
    {0x0191, "Fuel Pres Sensor Op"},
    {0x0192, "Fuel Pres Raw Min"},
    {0x0193, "Fuel Pres Raw Max"},
    {0x0195, "Oil Temp Op Min"},
    {0x0196, "Oil Temp Op Max"},
    {0x0197, "Oil Temp Raw Min"},
    {0x0198, "Oil Temp Raw Max"},
    // ── P02xx ────────────────────────────────────────────────────────────
    {0x0227, "DBW1 TPS1 Volt Low"},
    {0x0228, "DBW1 TPS1 Volt High"},
    {0x0237, "Boost Pres Raw Min"},
    {0x0238, "Boost Pres Raw Max"},
    {0x0242, "Turbo Speed 2 Raw Max"},
    {0x0243, "Wastegate 1 Out1 OC"},
    {0x0244, "Wastegate 1 Out2 OC"},
    {0x0245, "Wastegate 1 Out1 I/O"},
    {0x0246, "Wastegate 1 Out2 I/O"},
    {0x0247, "Wastegate 2 Out1 OC"},
    {0x0248, "Wastegate 2 Out2 OC"},
    {0x0249, "Wastegate 2 Out1 I/O"},
    {0x0250, "Wastegate 2 Out2 I/O"},
    {0x0261, "Inj 1 Open Circuit"},
    {0x0262, "Inj 1 Short Circuit"},
    {0x0264, "Inj 2 Open Circuit"},
    {0x0265, "Inj 2 Short Circuit"},
    {0x0267, "Inj 3 Open Circuit"},
    {0x0268, "Inj 3 Short Circuit"},
    {0x0270, "Inj 4 Open Circuit"},
    {0x0271, "Inj 4 Short Circuit"},
    {0x0273, "Inj 5 Open Circuit"},
    {0x0274, "Inj 5 Short Circuit"},
    {0x0276, "Inj 6 Open Circuit"},
    {0x0277, "Inj 6 Short Circuit"},
    {0x0279, "Inj 7/Aux1 Open Cct"},
    {0x0280, "Inj 7/Aux1 Short Cct"},
    {0x0282, "Inj 8/Aux2 Open Cct"},
    {0x0283, "Inj 8/Aux2 Short Cct"},
    {0x0285, "Inj 9/Aux3 Open Cct"},
    {0x0286, "Inj 9/Aux3 Short Cct"},
    {0x0288, "Inj 10/Aux4 Open Cct"},
    {0x0289, "Inj 10/Aux4 Short Cct"},
    // ── P03xx ────────────────────────────────────────────────────────────
    {0x0328, "Knock Sensor 1 Disc"},
    {0x0332, "Knock Sensor 2 Disc"},
    {0x0373, "Engine Position Error"},
    // ── P04xx ────────────────────────────────────────────────────────────
    {0x0462, "Fuel Level Raw Min"},
    {0x0463, "Fuel Level Raw Max"},
    {0x0470, "Exhaust Pres Fault"},
    {0x0471, "Exhaust Pres Range"},
    {0x0472, "Exhaust Pres Low"},
    {0x0473, "Exhaust Pres High"},
    // ── P05xx ────────────────────────────────────────────────────────────
    {0x0500, "Drivetrain Speed Diag"},
    {0x0521, "Oil Pres Sensor Op"},
    {0x0522, "Oil Pres Raw Min"},
    {0x0523, "Oil Pres Raw Max"},
    {0x0524, "Oil Pres Switch"},
    {0x0532, "AC Refrig Pres Raw Min"},
    {0x0533, "AC Refrig Pres Raw Max"},
    {0x0545, "EGT 1 Raw Min"},
    {0x0546, "EGT 1 Raw Max"},
    {0x0548, "EGT 2 Raw Min"},
    {0x0549, "EGT 2 Raw Max"},
    {0x0552, "Power Steer Pres Min"},
    {0x0553, "Power Steer Pres Max"},
    {0x0562, "Battery Volt Op Min"},
    {0x0563, "Battery Volt Op Max"},
    // ── P06xx ────────────────────────────────────────────────────────────
    {0x0601, "Int Memory Checksum"},
    {0x0602, "Setting Last Entry CRC"},
    {0x0603, "Setting Bad CRC"},
    {0x0604, "External RAM Bus Err"},
    {0x0605, "Setting Corruption"},
    {0x0606, "Coolant Level Raw Min"},
    {0x0607, "Coolant Level Raw Max"},
    {0x0608, "Oil Level Raw Min"},
    {0x0609, "Oil Level Raw Max"},
    {0x0610, "Coolant Level Low"},
    {0x0611, "Oil Level Low"},
    {0x0612, "Brake Fluid Level Low"},
    {0x0613, "Coolant Flow Not Detcd"},
    {0x061F, "DBW1 Controller Perf"},
    {0x0641, "Sensor Ref Volt Ratio"},
    {0x0642, "Sensor Ref Volt Abs"},
    {0x0643, "Sensor Ref Volt FPGA1"},
    {0x0651, "Sensor Ref Volt FPGA2"},
    {0x0652, "Sensor Ref Volt -5V"},
    {0x0653, "Sensor Ref Volt -12V"},
    {0x0668, "ECU Temp Low"},
    {0x0669, "ECU Temp High"},
    {0x0697, "Sensor Ref Volt VDD"},
    {0x0698, "Sensor Ref Volt VAN5V"},
    {0x06A3, "+5V Sensor Supply A Err"},
    {0x06A4, "+5V Sensor Supply B Err"},
    // ── P07xx ────────────────────────────────────────────────────────────
    {0x0711, "Trans Fluid Temp Op Max"},
    {0x0712, "Trans Fluid Temp Min"},
    {0x0713, "Trans Fluid Temp Max"},
    // ── P08xx ────────────────────────────────────────────────────────────
    {0x0831, "Clutch Pres Raw Min"},
    {0x0832, "Clutch Pres Raw Max"},
    {0x0841, "GB Line Pres Op Min"},
    {0x0842, "GB Line Pres Raw Min"},
    {0x0843, "GB Line Pres Raw Max"},
    {0x0844, "GB Line Pres Op Max"},
    {0x0846, "GB TC Pres Op Min"},
    {0x0847, "GB TC Pres Raw Min"},
    {0x0848, "GB TC Pres Raw Max"},
    {0x0849, "GB TC Pres Op Max"},
    // ── P09xx ────────────────────────────────────────────────────────────
    {0x0933, "Transfer Case Pres Min"},
    {0x0934, "Transfer Case Pres Max"},
    {0x0935, "Transfer Case Pres Max"},
    // ── P11xx ────────────────────────────────────────────────────────────
    {0x1164, "ECU Internal Failure"},
    {0x1165, "Single WB RX Timeout"},
    {0x1166, "Dual WB Box A Timeout"},
    {0x1167, "ECU Internal Failure"},
    {0x1168, "ECU Internal Failure"},
    {0x1169, "ECU Internal Failure"},
    {0x1170, "ECU Internal Failure"},
    {0x1171, "ECU Internal Failure"},
    {0x1172, "ECU Internal Failure"},
    // ── P12xx ────────────────────────────────────────────────────────────
    {0x1276, "WB 1 AFR Lean Trip"},
    {0x1277, "WB 2 AFR Lean Trip"},
    {0x1278, "WB 3 AFR Lean Trip"},
    {0x1279, "WB 4 AFR Lean Trip"},
    // ── P13xx — Trigger / sync ────────────────────────────────────────────
    {0x1300, "Trigger Sync Error"},
    {0x1301, "Trigger Reference Error"},
    {0x1302, "No Home Error"},
    {0x1303, "Kill Switch Config Err"},
    {0x1304, "Flat Shift Config Err"},
    {0x1305, "Unknown Trigger Pattern"},
    {0x1321, "EGT 1 Op Max"},
    {0x1322, "EGT 2 Op Max"},
    {0x1323, "EGT 3 Op Max"},
    {0x1324, "EGT 4 Op Max"},
    {0x1325, "EGT 5 Op Max"},
    {0x1326, "EGT 6 Op Max"},
    {0x1327, "EGT 7 Op Max"},
    {0x1328, "EGT 8 Op Max"},
    {0x1329, "EGT 9 Op Max"},
    {0x1330, "EGT 10 Op Max"},
    {0x1331, "EGT 11 Op Max"},
    {0x1332, "EGT 12 Op Max"},
    {0x1333, "EGT 6 Raw Min"},
    {0x1334, "EGT 6 Raw Max"},
    {0x1335, "EGT 7 Raw Min"},
    {0x1336, "EGT 7 Raw Max"},
    {0x1337, "EGT 8 Raw Min"},
    {0x1338, "EGT 8 Raw Max"},
    {0x1339, "EGT 9 Raw Min"},
    {0x1340, "EGT 9 Raw Max"},
    {0x1341, "EGT 10 Raw Min"},
    {0x1342, "EGT 10 Raw Max"},
    {0x1343, "EGT 11 Raw Min"},
    {0x1344, "EGT 11 Raw Max"},
    {0x1345, "EGT 12 Raw Min"},
    {0x1346, "EGT 12 Raw Max"},
    {0x1347, "N2O Pres 1 Raw Min"},
    {0x1348, "N2O Pres 1 Raw Max"},
    {0x1349, "RTC Battery Low"},
    {0x1350, "Wastegate Pres Raw Min"},
    {0x1351, "Wastegate Pres Raw Max"},
    {0x1352, "AC Temp Raw Min"},
    {0x1353, "AC Temp Raw Max"},
    {0x1354, "Tumble Valve 1 Raw Min"},
    {0x1355, "Tumble Valve 1 Raw Max"},
    {0x1356, "Tumble Valve 2 Raw Min"},
    {0x1357, "Tumble Valve 2 Raw Max"},
    {0x1358, "N2O Pres 2 Raw Min"},
    {0x1359, "N2O Pres 2 Raw Max"},
    {0x1360, "N2O Pres 3 Raw Min"},
    {0x1361, "N2O Pres 3 Raw Max"},
    {0x1362, "N2O Pres 4 Raw Min"},
    {0x1363, "N2O Pres 4 Raw Max"},
    {0x1370, "N2O Controller 1 Init"},
    {0x1371, "N2O Controller 2 Init"},
    {0x1372, "N2O Controller 3 Init"},
    {0x1373, "N2O Controller 4 Init"},
    {0x1374, "N2O Controller 5 Init"},
    {0x1375, "N2O Controller 6 Init"},
    {0x1376, "CO2 Bottle Volt Low"},
    {0x1377, "CO2 Bottle Pres Volt Hi"},
    {0x1378, "CO2 Bottle Pres Low"},
    {0x1379, "CO2 Bottle Pres High"},
    {0x1380, "WB O2 Sensor 5 Raw Min"},
    {0x1381, "WB O2 Sensor 5 Raw Max"},
    {0x1382, "WB 5 AFR Lean Trip"},
    {0x1383, "WB O2 Sensor 6 Raw Min"},
    {0x1384, "WB O2 Sensor 6 Raw Max"},
    {0x1385, "WB 6 AFR Lean Trip"},
    {0x1386, "WB O2 Sensor 7 Raw Min"},
    {0x1387, "WB O2 Sensor 7 Raw Max"},
    {0x1388, "WB 7 AFR Lean Trip"},
    {0x1389, "WB O2 Sensor 8 Raw Min"},
    {0x1390, "WB O2 Sensor 8 Raw Max"},
    {0x1391, "WB 8 AFR Lean Trip"},
    {0x1392, "WB O2 Sensor 9 Raw Min"},
    {0x1393, "WB O2 Sensor 9 Raw Max"},
    {0x1394, "WB 9 AFR Lean Trip"},
    {0x1395, "WB O2 Sensor 10 Raw Min"},
    {0x1396, "WB O2 Sensor 10 Raw Max"},
    {0x1397, "WB 10 AFR Lean Trip"},
    {0x1398, "WB O2 Sensor 11 Raw Min"},
    {0x1399, "WB O2 Sensor 11 Raw Max"},
    {0x1400, "WB 11 AFR Lean Trip"},
    {0x1401, "WB O2 Sensor 12 Raw Min"},
    {0x1402, "WB O2 Sensor 12 Raw Max"},
    {0x1403, "WB 12 AFR Lean Trip"},
    {0x1404, "Wastegate 1 Temp Min"},
    {0x1405, "Wastegate 1 Temp Max"},
    {0x1406, "Wastegate 1 Temp Op Max"},
    {0x1407, "Wastegate 2 Temp Min"},
    {0x1408, "Wastegate 2 Temp Max"},
    {0x1409, "Wastegate 2 Temp Op Max"},
    {0x1410, "Wastegate 1 Pos I/O Err"},
    {0x1411, "Wastegate 2 Pos I/O Err"},
    {0x1412, "WG1 Pos Volt Err"},
    {0x1413, "WG2 Pos Volt Err"},
    {0x1414, "Exh Cam B1 Not Detected"},
    {0x1415, "Exh Cam B2 Not Detected"},
    {0x1416, "Int Cam B1 Not Detected"},
    {0x1417, "Int Cam B2 Not Detected"},
    // ── P15xx ────────────────────────────────────────────────────────────
    {0x1534, "Collision Detect Err"},
    {0x1590, "DBW1 TPS1 Volt Mismatch"},
    {0x1591, "DBW1 TPS2 Volt Mismatch"},
    {0x1592, "DBW1 PPS1 Volt Mismatch"},
    {0x1593, "DBW1 PPS2 Volt Mismatch"},
    // ── P16xx ────────────────────────────────────────────────────────────
    {0x1680, "Oil Pump Low Flow"},
    {0x1682, "Oil Pump High Flow"},
    {0x1684, "Oil Pump Raw Min"},
    {0x1686, "Oil Pump Raw Max"},
    {0x1687, "Trans Case Init Fault"},
    {0x1690, "Inj Stage 1 90% DC"},
    {0x1691, "Inj Stage 2 90% DC"},
    {0x1692, "Inj Stage 3 90% DC"},
    {0x1693, "Inj Stage 4 90% DC"},
    {0x1694, "Crankcase Pres Raw Low"},
    {0x1695, "Crankcase Pres Raw High"},
    {0x1696, "Crankcase Pres Low"},
    {0x1697, "Crankcase Pres High"},
    {0x1698, "Main Setup Page Error"},
    {0x1699, "Trans Input RPM Op Max"},
    // ── P17xx ────────────────────────────────────────────────────────────
    {0x1700, "Dual WB Box C Timeout"},
    {0x1701, "Dual WB Box D Timeout"},
    {0x1702, "Dual WB Box B Timeout"},
    // ── P18xx ────────────────────────────────────────────────────────────
    {0x1800, "Fuel Corrections Fail"},
    // ── P19xx — Outputs / PDM / PD-16 ────────────────────────────────────
    {0x1900, "Diff Temp Raw Min"},
    {0x1901, "Diff Temp Raw Max"},
    {0x1910, "Coolant Pres Raw Min"},
    {0x1911, "Coolant Pres Raw Max"},
    {0x1912, "IGN 1 Overcurrent"},
    {0x1913, "IGN 2 Overcurrent"},
    {0x1914, "IGN 3 Overcurrent"},
    {0x1915, "IGN 4 Overcurrent"},
    {0x1916, "IGN 5 Overcurrent"},
    {0x1917, "IGN 6 Overcurrent"},
    {0x1918, "IGN 7 Overcurrent"},
    {0x1919, "IGN 8 Overcurrent"},
    {0x191A, "IGN 9 Overcurrent"},
    {0x191B, "IGN 10 Overcurrent"},
    {0x191C, "IGN 11 Overcurrent"},
    {0x191D, "IGN 12 Overcurrent"},
    {0x191E, "DPO 1 Overcurrent"},
    {0x191F, "DPO 2 Overcurrent"},
    {0x1920, "DPO 3 Overcurrent"},
    {0x1921, "DPO 4 Overcurrent"},
    {0x1922, "DPO 5 Overcurrent"},
    {0x1923, "DPO 6 Overcurrent"},
    {0x1924, "DPO 7 Overcurrent"},
    {0x1925, "DPO 8 Overcurrent"},
    {0x1926, "HBO 1 Overcurrent"},
    {0x1927, "HBO 2 Overcurrent"},
    {0x1928, "HBO 3 Overcurrent"},
    {0x1929, "HBO 4 Overcurrent"},
    {0x192A, "HBO 1 Max Retries"},
    {0x192B, "HBO 2 Max Retries"},
    {0x192C, "HBO 3 Max Retries"},
    {0x192D, "HBO 4 Max Retries"},
    {0x192E, "HCO8 1 Overcurrent"},
    {0x192F, "HCO8 2 Overcurrent"},
    {0x1930, "HCO8 3 Overcurrent"},
    {0x1931, "HCO8 4 Overcurrent"},
    {0x1932, "HCO8 5 Overcurrent"},
    {0x1933, "HCO8 6 Overcurrent"},
    {0x1934, "HCO8 7 Overcurrent"},
    {0x1935, "HCO8 8 Overcurrent"},
    {0x1936, "HCO8 9 Overcurrent"},
    {0x1937, "HCO8 10 Overcurrent"},
    {0x1938, "HCO8 11 Overcurrent"},
    {0x1939, "HCO8 12 Overcurrent"},
    {0x193A, "8A HCO 1 Max Retries"},
    {0x193B, "8A HCO 2 Max Retries"},
    {0x193C, "8A HCO 3 Max Retries"},
    {0x193D, "8A HCO 4 Max Retries"},
    {0x193E, "8A HCO 5 Max Retries"},
    {0x193F, "8A HCO 6 Max Retries"},
    {0x1940, "8A HCO 7 Max Retries"},
    {0x1941, "8A HCO 8 Max Retries"},
    {0x1942, "8A HCO 9 Max Retries"},
    {0x1943, "8A HCO 10 Max Retries"},
    {0x1944, "8A HCO 11 Max Retries"},
    {0x1945, "8A HCO 12 Max Retries"},
    {0x1946, "25A HCO 1 Overcurrent"},
    {0x1947, "25A HCO 2 Overcurrent"},
    {0x1948, "25A HCO 3 Overcurrent"},
    {0x1949, "25A HCO 4 Overcurrent"},
    {0x194A, "25A HCO 1 Max Retries"},
    {0x194B, "25A HCO 2 Max Retries"},
    {0x194C, "25A HCO 3 Max Retries"},
    {0x194D, "25A HCO 4 Max Retries"},
    {0x194E, "PDM Comms Error"},
    {0x194F, "PDM Comms Error"},
    {0x1950, "PDM Comms Error"},
    {0x1951, "PDM Batt Volt Low"},
    {0x1952, "PDM Batt Volt High"},
    {0x1953, "PDM Main Rail Loss"},
    {0x1954, "PDM SEPIC Volt Low"},
    {0x1955, "PDM SEPIC Volt High"},
    {0x1956, "PDM ECU Volt Low"},
    {0x1957, "PDM ECU Volt High"},
    {0x1958, "PDM ECU Current High"},
    {0x1959, "PDM Boost Volt Low"},
    {0x195A, "PDM Bandgap Volt Low"},
    {0x195B, "PDM Bandgap Volt High"},
    {0x195C, "PDM UVLO Flagged"},
    {0x195D, "PDM CPU Temp Low"},
    {0x195E, "PDM CPU Temp High"},
    {0x195F, "PDM TVS Temp Low"},
    {0x1960, "PDM TVS Temp High"},
    {0x1961, "PDM Rail Temp Low"},
    {0x1962, "PDM Rail Temp High"},
    {0x1963, "PDM HCO25 1/2 Temp Low"},
    {0x1964, "PDM HCO25 1/2 Temp Hi"},
    {0x1965, "PDM HCO25 3/4 Temp Low"},
    {0x1966, "PDM HCO25 3/4 Temp Hi"},
    {0x1967, "PDM Ambient Temp Low"},
    {0x1968, "PDM Ambient Temp High"},
    {0x1969, "PDM Rev Polarity Fail"},
    {0x196A, "Int WB Comms Error"},
    {0x196B, "Int WB Comms Error"},
    {0x196C, "Int WB Comms Error"},
    {0x196D, "PD-16 Box A Timeout"},
    {0x196E, "PD-16A Batt Volt Low"},
    {0x196F, "PD-16A Batt Volt High"},
    {0x1970, "PD-16A SEPIC Volt Low"},
    {0x1971, "PD-16A Max Current"},
    {0x1972, "PD-16A Boost Volt Low"},
    {0x1973, "PD-16A Int Temp Warn"},
    {0x1974, "PD-16A Int Temp Shut"},
    {0x1995, "PD-16 Box B Timeout"},
    {0x1996, "PD-16B Batt Volt Low"},
    {0x1997, "PD-16B Batt Volt High"},
    {0x1998, "PD-16B SEPIC Volt Low"},
    {0x1999, "PD-16B Max Current"},
    {0x199A, "PD-16B Boost Volt Low"},
    {0x199B, "PD-16B Int Temp Warn"},
    {0x199C, "PD-16B Int Temp Shut"},
    {0x19BD, "PD-16 Box C Timeout"},
    {0x19BE, "PD-16C Batt Volt Low"},
    {0x19BF, "PD-16C Batt Volt High"},
    {0x19C0, "PD-16C SEPIC Volt Low"},
    {0x19C1, "PD-16C Max Current"},
    {0x19C2, "PD-16C Boost Volt Low"},
    {0x19C3, "PD-16C Int Temp Warn"},
    {0x19C4, "PD-16C Int Temp Shut"},
    {0x19E5, "PD-16 Box D Timeout"},
    {0x19E6, "PD-16D Batt Volt Low"},
    {0x19E7, "PD-16D Batt Volt High"},
    {0x19E8, "PD-16D SEPIC Volt Low"},
    {0x19E9, "PD-16D Max Current"},
    {0x19EA, "PD-16D Boost Volt Low"},
    {0x19EB, "PD-16D Int Temp Warn"},
    {0x19EC, "PD-16D Int Temp Shut"},
    // ── P1Axx ─────────────────────────────────────────────────────────────
    {0x1A15, "Sensor Ground Loop"},
    {0x1A53, "External Dash Timeout"},
    // ── P20xx ────────────────────────────────────────────────────────────
    {0x2032, "EGT 3 Raw Min"},
    {0x2033, "EGT 3 Raw Max"},
    {0x2035, "EGT 4 Raw Min"},
    {0x2036, "EGT 4 Raw Max"},
    // ── P21xx — DBW ───────────────────────────────────────────────────────
    {0x2109, "DBW1 Disabled"},
    {0x2113, "DBW1 TPS Track Error"},
    {0x2114, "DBW1 ECU Demand Error"},
    {0x2116, "DBW1 TPS Track Redund"},
    {0x2122, "DBW1 TPS2 Volt Low"},
    {0x2123, "DBW1 TPS2 Volt High"},
    {0x2127, "DBW1 PPS1 Volt Low"},
    {0x2128, "DBW1 PPS1 Volt High"},
    {0x2132, "DBW1 PPS2 Volt Low"},
    {0x2133, "DBW1 PPS2 Volt High"},
    {0x2135, "DBW1 TPS Volt Correl"},
    {0x2136, "DBW1 TPS Correl Redund"},
    {0x2138, "DBW1 PPS Volt Correl"},
    {0x2139, "DBW1 PPS Correl Redund"},
    {0x2146, "Inj 11/Aux5 Open Cct"},
    {0x2147, "Inj 11 Short Circuit"},
    {0x2148, "Aux5 Short Circuit"},
    {0x2149, "Inj 12/Aux6 Open Cct"},
    {0x2150, "Inj 12 Short Circuit"},
    {0x2151, "Aux6 Short Circuit"},
    {0x2152, "Inj 13/Aux7 Open Cct"},
    {0x2153, "Inj 13 Short Circuit"},
    {0x2154, "Aux7 Short Circuit"},
    {0x2155, "Inj 14/Aux8 Open Cct"},
    {0x2156, "Inj 14 Short Circuit"},
    {0x2157, "Aux8 Short Circuit"},
    {0x2163, "DBW1 Relaxed TPS Track"},
    {0x2164, "DBW1 Relaxed TPS Mod"},
    {0x2165, "DBW1 Relaxed Mismatch"},
    {0x2166, "DBW1 Redundancy Flag"},
    {0x2167, "DBW1 Redundancy Data"},
    {0x2168, "DBW1 Redundancy Pedal"},
    {0x2169, "DBW1 Redundancy Idle"},
    {0x216A, "Inj 15 Open Circuit"},
    {0x216B, "Inj 15 Short Circuit"},
    {0x216D, "Inj 16 Open Circuit"},
    {0x216E, "Inj 16 Short Circuit"},
    {0x217A, "Inj 17 Open Circuit"},
    {0x217B, "Inj 17 Short Circuit"},
    {0x217D, "Inj 18 Open Circuit"},
    {0x217E, "Inj 18 Short Circuit"},
    // ── P22xx ────────────────────────────────────────────────────────────
    {0x2228, "Baro Pres Raw Min"},
    {0x2229, "Baro Pres Raw Max"},
    // ── P24xx ────────────────────────────────────────────────────────────
    {0x242C, "EGT 5 Raw Min"},
    {0x242D, "EGT 5 Raw Max"},
    // ── P25xx ────────────────────────────────────────────────────────────
    {0x2581, "Turbo Speed 1 Raw Max"},
    // ── P2Axx — Wideband sensor failures ─────────────────────────────────
    {0x2A00, "WB 1 Sensor Failure"},
    {0x2A01, "WB 2 Sensor Failure"},
    {0x2A02, "WB 3 Sensor Failure"},
    {0x2A03, "WB 4 Sensor Failure"},
    {0x2A04, "WB 5 Sensor Failure"},
    {0x2A05, "WB 6 Sensor Failure"},
    {0x2A06, "WB 7 Sensor Failure"},
    {0x2A07, "WB 8 Sensor Failure"},
    {0x2A08, "WB 9 Sensor Failure"},
    {0x2A09, "WB 10 Sensor Failure"},
    {0x2A10, "WB 11 Sensor Failure"},
    {0x2A11, "WB 12 Sensor Failure"},
    // ── P2Axx — DBW Throttle 2 ───────────────────────────────────────────
    {0x2A20, "DBW2 TPS1 Volt Low"},
    {0x2A21, "DBW2 TPS1 Volt High"},
    {0x2A22, "DBW2 TPS2 Volt Low"},
    {0x2A23, "DBW2 TPS2 Volt High"},
    {0x2A24, "DBW2 TPS Volt Correl"},
    {0x2A25, "DBW2 TPS Correl Redund"},
    {0x2A26, "DBW2 Motor 100% DC"},
    {0x2A27, "DBW2 TPS1 Volt Mismatch"},
    {0x2A28, "DBW2 TPS2 Volt Mismatch"},
    {0x2A29, "DBW2 TPS Track Error"},

    // ── C codes — Chassis (raw = 0x8000 | code_hex) ──────────────────────
    {0x8615, "Shock FL Raw Low"},
    {0x8620, "Shock FR Raw Low"},
    {0x8625, "Shock RL Raw Low"},
    {0x8630, "Shock RR Raw Low"},
    {0x9020, "Brake Pres Raw Min"},
    {0x9021, "Brake Pres Raw Max"},
    {0x9022, "Wheel Speed RF Err"},
    {0x9024, "Wheel Speed LF Err"},
    {0x9026, "Wheel Speed RR Err"},
    {0x9028, "Wheel Speed LR Err"},
    {0x9029, "Wheel Speed Mismatch"},
    {0x9030, "Steer Angle Raw Min"},
    {0x9031, "Steer Angle Raw Max"},
    {0x9032, "Yaw Sensor Raw Min"},
    {0x9033, "Yaw Sensor Raw Max"},
    {0x9034, "Lateral G Raw Min"},
    {0x9035, "Lateral G Raw Max"},
    {0x9036, "Fuel Tank Pres Raw Min"},
    {0x9037, "Fuel Tank Pres Raw Max"},
    {0x9038, "Long G Raw Min"},
    {0x9039, "Long G Raw Max"},
    {0x9040, "Knock Light Init Err"},
    {0x9050, "Driveshaft RPM Op Max"},
    {0x9292, "Brake Pres Rear Raw Min"},
    {0x9296, "Brake Pres Rear Raw Max"},
    {0x9760, "Shock FL Raw High"},
    {0x9761, "Shock FR Raw High"},
    {0x9762, "Shock RL Raw High"},
    {0x9763, "Shock RR Raw High"},
    {0x9800, "Ride Height F Raw Low"},
    {0x9801, "Ride Height F Raw High"},
    {0x9802, "Ride Height R Raw Low"},
    {0x9803, "Ride Height R Raw High"},

    // ── U codes — Network (raw = 0xC000 | code_hex) ───────────────────────
    {0xD020, "I/O-12 Box A RX Err"},
    {0xD021, "I/O-12 Box A TX Err"},
    {0xD022, "I/O-12 Box B RX Err"},
    {0xD023, "I/O-12 Box B TX Err"},
    {0xD026, "TCA-2 Box A RX Err"},
    {0xD027, "TCA-2 Box B RX Err"},
    {0xD028, "TCA-4 Box A RX Err"},
    {0xD029, "TCA-4 Box B RX Err"},
    {0xD030, "I/O-12 Box A Dup ID"},
    {0xD031, "I/O-12 Box A HW Fail"},
    {0xD032, "I/O-12 Box A FW Erase"},
    {0xD033, "I/O-12 Box A Int Err"},
    {0xD034, "I/O-12 Box A Int Err"},
    {0xD040, "I/O-12 Box B Dup ID"},
    {0xD041, "I/O-12 Box B HW Fail"},
    {0xD042, "I/O-12 Box B FW Erase"},
    {0xD043, "I/O-12 Box B Int Err"},
    {0xD044, "I/O-12 Box B Int Err"},
    {0xD050, "TCA-2 Box A Dup ID"},
    {0xD051, "TCA-2 Box A HW Fail"},
    {0xD052, "TCA-2 Box A FW Erase"},
    {0xD053, "TCA-2 Box A Int Err"},
    {0xD054, "TCA-2 Box A Int Err"},
    {0xD060, "TCA-2 Box B Dup ID"},
    {0xD061, "TCA-2 Box B HW Fail"},
    {0xD062, "TCA-2 Box B FW Erase"},
    {0xD063, "TCA-2 Box B Int Err"},
    {0xD064, "TCA-2 Box B Int Err"},
    {0xD070, "TCA-4 Box A Dup ID"},
    {0xD071, "TCA-4 Box A HW Fail"},
    {0xD072, "TCA-4 Box A FW Erase"},
    {0xD073, "TCA-4 Box A Int Err"},
    {0xD074, "TCA-4 Box A Int Err"},
    {0xD080, "TCA-4 Box B Dup ID"},
    {0xD081, "TCA-4 Box B HW Fail"},
    {0xD082, "TCA-4 Box B FW Erase"},
    {0xD083, "TCA-4 Box B Int Err"},
    {0xD084, "TCA-4 Box B Int Err"},
    {0xD130, "WB Box A Dup ID"},
    {0xD131, "WB Box A RX Err"},
    {0xD132, "RTC Failure"},
    {0xD133, "WB Single Ch RX Err"},
    {0xD134, "ECU Internal Failure"},
    {0xD135, "ECU Internal Failure"},
    {0xD136, "ECU Internal Failure"},
    {0xD137, "ECU Internal Failure"},
    {0xD138, "ECU Internal Failure"},
    {0xD140, "ECU Internal Failure"},
    {0xD141, "ECU Internal Failure"},
    {0xD145, "ECU Internal Failure"},
    {0xD146, "ECU Internal Failure"},
    {0xD147, "ECU Internal Failure"},
    {0xD150, "ECU Internal Failure"},
    {0xD151, "ECU Internal Failure"},
    {0xD152, "ECU Internal Failure"},
    {0xD154, "ECU Internal Failure"},
    {0xD157, "CAN Bus Init Error"},
    {0xD159, "ECU Internal Failure"},
    {0xD160, "ECU Internal Failure"},
    {0xD161, "ECU Internal Failure"},
    {0xD162, "ECU Internal Failure"},
    {0xD163, "ECU Internal Failure"},
    {0xD164, "TC Init Failure"},
    {0xD165, "Rolling Antilag Init"},
    {0xD174, "REM Inj Driver RX1 None"},
    {0xD175, "REM Inj Driver RX2 None"},
    {0xD176, "REM Inj Driver RX3 None"},
    {0xD177, "REM Inj Driver RX1 Err"},
    {0xD178, "REM Inj Driver RX2 Err"},
    {0xD179, "REM Inj Driver RX3 Err"},
    {0xD180, "REM Inj 1 Collision"},
    {0xD181, "REM Inj 2 Collision"},
    {0xD182, "REM Inj 3 Collision"},
    {0xD183, "REM Inj 4 Collision"},
    {0xD184, "REM Inj 5 Collision"},
    {0xD185, "REM Inj 6 Collision"},
    {0xD186, "REM Inj 7 Collision"},
    {0xD187, "REM Inj 8 Collision"},
    {0xD188, "Torque Mgmt Init Fail"},
    {0xD189, "REM Init Failure"},
    {0xD190, "REM Not Found"},
    {0xD191, "REM Comms ECU to REM"},
    {0xD192, "REM Comms REM to ECU"},
    {0xD193, "REM Watchdog Lost"},
    {0xD194, "REM Inj 1 Open Cct"},
    {0xD195, "REM Inj 1 Short Cct"},
    {0xD196, "REM Inj 2 Open Cct"},
    {0xD197, "REM Inj 2 Short Cct"},
    {0xD198, "REM Inj 3 Open Cct"},
    {0xD199, "REM Inj 3 Short Cct"},
    {0xD200, "REM Inj 4 Open Cct"},
    {0xD201, "REM Inj 4 Short Cct"},
    {0xD202, "REM Inj 5 Open Cct"},
    {0xD203, "REM Inj 5 Short Cct"},
    {0xD204, "REM Inj 6 Open Cct"},
    {0xD205, "REM Inj 6 Short Cct"},
    {0xD206, "REM Inj 7 Open Cct"},
    {0xD207, "REM Inj 7 Short Cct"},
    {0xD208, "REM Inj 8 Open Cct"},
    {0xD209, "REM Inj 8 Short Cct"},
    {0xD210, "REM FW Update Required"},
    {0xD211, "Datalogger Memory Err"},
    {0xD212, "Cruise Ctrl Init Fail"},
    {0xD213, "Cruise Ctrl Redundancy"},
    {0xD214, "ECU Internal Failure"},
    {0xD215, "TCA-8 HW Failure"},
    {0xD216, "TCA-8 FW Erased"},
    {0xD217, "TCA-8 Int Error"},
    {0xD218, "TCA-8 Int Error"},
    {0xD219, "TCA-8 Dup ID"},
    {0xD220, "TCA-8 RX Error"},
    {0xD221, "TMS-4 RX Timeout"},
};

#define DTC_TABLE_LEN  (sizeof(DTC_TABLE) / sizeof(DTC_TABLE[0]))

// Binary search for raw code in DTC_TABLE. Returns description via out_desc.
// Returns true if found; false for unknown code (out_desc untouched).
static bool dtcLookup(uint16_t raw, char out_desc[24]) {
    int lo = 0, hi = (int)DTC_TABLE_LEN - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        DtcEntry e;
        memcpy_P(&e, &DTC_TABLE[mid], sizeof(DtcEntry));
        if      (e.raw == raw) { memcpy(out_desc, e.desc, 24); return true; }
        else if (e.raw  < raw) lo = mid + 1;
        else                   hi = mid - 1;
    }
    return false;
}

// Decode raw 16-bit DTC value to a printable "P0300"-style code string.
static void dtcFormatCode(uint16_t raw, char out[8]) {
    static const char LETTERS[] = "PBCU";
    snprintf(out, 8, "%c%04X", LETTERS[(raw >> 14) & 0x03], raw & 0x3FFF);
}

// ── DTC state ──────────────────────────────────────────────────────────────
#define DTC_MAX_CODES  8

// g_dtcPageOpen declared in TJR_241.ino

static uint16_t    s_dtcList  [DTC_MAX_CODES];
static uint8_t     s_dtcCount  = 0;
static uint16_t    s_dtcIgnore [DTC_MAX_CODES];
static uint8_t     s_dtcIgnCnt  = 0;
static bool        s_unavailAck  = false;
static uint32_t    s_stoppedSince = 0;
static SettingsZone s_dtcZOK;


// ── Ignore list helpers ─────────────────────────────────────────────────────

static bool dtcIsIgnored(uint16_t raw) {
    for (uint8_t i = 0; i < s_dtcIgnCnt; i++)
        if (s_dtcIgnore[i] == raw) return true;
    return false;
}
static void dtcIgnoreAdd(uint16_t raw) {
    if (s_dtcIgnCnt >= DTC_MAX_CODES) return;
    if (!dtcIsIgnored(raw)) s_dtcIgnore[s_dtcIgnCnt++] = raw;
}
static void dtcIgnoreAll() {
    for (uint8_t i = 0; i < s_dtcCount; i++) dtcIgnoreAdd(s_dtcList[i]);
    if (s_dtcCount == 0) s_unavailAck = true;
    g_dtcPageOpen = false;
}
static bool dtcCollect(uint16_t raw) {
    for (uint8_t i = 0; i < s_dtcCount; i++)
        if (s_dtcList[i] == raw) return false;
    if (s_dtcCount >= DTC_MAX_CODES) return false;
    s_dtcList[s_dtcCount++] = raw;
    if (s_dtcCount == 1) s_unavailAck = false;
    return true;
}


// ── Drive-cycle reset ───────────────────────────────────────────────────────

void dtcCheckIgnoreReset() {
    uint8_t engState = (uint8_t)g_val[45];
    if (engState == 0) {
        if (s_stoppedSince == 0) s_stoppedSince = millis();
    } else {
        if (s_stoppedSince != 0) {
            if (millis() - s_stoppedSince > 10000) {
                s_dtcIgnCnt  = 0;
                s_unavailAck = false;
            }
            s_stoppedSince = 0;
        }
    }
}


// ── CEL check + page trigger ────────────────────────────────────────────────

void dtcCheck() {
    bool celActive = (g_val[46] > 0.5f);
    if (!celActive) {
        s_dtcCount        = 0;
        s_unavailAck      = false;
        g_dtcPageOpen     = false;
        g_engineProtReason = 0;
        return;
    }
    uint16_t raw = g_engineProtReason;
    if (raw != 0) dtcCollect(raw);
    if (g_dtcPageOpen) return;
    if (s_dtcCount == 0) {
        if (!s_unavailAck) g_dtcPageOpen = true;
    } else {
        for (uint8_t i = 0; i < s_dtcCount; i++)
            if (!dtcIsIgnored(s_dtcList[i])) { g_dtcPageOpen = true; break; }
    }
}


// ── Draw helpers ────────────────────────────────────────────────────────────

// Centre a string within a horizontal band [cx .. cx+panelW] at y_top.
static int16_t dtcPrintCentred(const char *str, int16_t cx, int16_t panelW,
                                int16_t y_top, uint16_t col, const GFXfont *font) {
    int16_t w = strW(str, font);
    int16_t x = cx + (panelW - w) / 2;
    fbStr(str, x, baselineY(y_top, font), col, font);
    return y_top + fontH(font);
}

static void dtcDrawOkButton(int16_t bx, int16_t by, int16_t bw, int16_t bh) {
    fbFillRoundRect(bx, by, bw, bh, 8, C_WARN);
    printCentredIn("OK", bx, by, bw, bh, C_BG, FONT_LABEL);
    s_dtcZOK = {bx, by, bw, bh};
}

// Draw description + code string, returns y after drawing.
static int16_t dtcDrawEntry(uint16_t raw, int16_t cx, int16_t panelW, int16_t y) {
    char code[8];
    char desc[24] = "Unknown: see NSP";
    dtcFormatCode(raw, code);
    dtcLookup(raw, desc);

    y = dtcPrintCentred(desc, cx, panelW, y,     C_WHITE, FONT_DTC) + 2;
    y = dtcPrintCentred(code, cx, panelW, y,     C_WARN,  FONT_DTC) + 8;
    return y;
}

static int16_t dtcDrawUnavail(int16_t cx, int16_t panelW, int16_t y) {
    return dtcPrintCentred("Code unavailable", cx, panelW, y, colDim(), FONT_DTC) + 8;
}


// ── Full DTC warning page ───────────────────────────────────────────────────

void drawDtcPage() {
    int16_t sw = scrW(), sh = scrH();
    fbFill(C_BG);

    const int16_t PAD   = 16;
    const int16_t BTN_H = 66;
    int16_t fhL = fontH(FONT_LABEL);

    if (!g_isLandscape) {
        // ── Portrait 450 × 600 ────────────────────────────────────────────
        int16_t iconW = CEL_ICON_RGB_W, iconH = CEL_ICON_RGB_H;
        int16_t icoX  = (sw - iconW) / 2;
        int16_t icoY  = PAD;

        fbBitmap16(icoX, icoY, iconW, iconH, CEL_ICON_RGB);

        int16_t textY = icoY + iconH + 8;
        textY = dtcPrintCentred("CHECK ENGINE", 0, sw, textY, C_WARN, FONT_LABEL) + 6;

        fbHLine(PAD, textY, sw - PAD * 2, C_WARN);
        textY += 8;

        if (s_dtcCount == 0) {
            dtcDrawUnavail(0, sw, textY);
        } else {
            for (uint8_t i = 0; i < s_dtcCount && textY < sh - BTN_H - PAD - 40; i++)
                textY = dtcDrawEntry(s_dtcList[i], 0, sw, textY);
        }

        int16_t btnY = sh - BTN_H - PAD;
        dtcDrawOkButton(PAD, btnY, sw - PAD * 2, BTN_H);

    } else {
        // ── Landscape 600 × 450 ───────────────────────────────────────────
        const int16_t LEFT_W  = 230;
        const int16_t DIV_X   = LEFT_W + 4;
        const int16_t RIGHT_X = DIV_X + 8;
        const int16_t RIGHT_W = sw - RIGHT_X;

        int16_t iconW  = CEL_ICON_RGB_W, iconH = CEL_ICON_RGB_H;
        int16_t blockH = iconH + 8 + fhL;
        int16_t icoY   = (sh - blockH) / 2;
        int16_t icoX   = (LEFT_W - iconW) / 2;

        fbBitmap16(icoX, icoY, iconW, iconH, CEL_ICON_RGB);

        // "CHECK ENGINE" centred in left panel below icon
        dtcPrintCentred("CHECK ENGINE", 0, LEFT_W, icoY + iconH + 8, C_WARN, FONT_LABEL);

        // Amber vertical divider
        fbVLine(DIV_X, PAD, sh - PAD * 2, C_WARN);

        // Codes in right panel
        int16_t codeY = PAD;
        if (s_dtcCount == 0) {
            dtcDrawUnavail(RIGHT_X, RIGHT_W, codeY);
        } else {
            for (uint8_t i = 0; i < s_dtcCount && codeY < sh - BTN_H - PAD - 40; i++)
                codeY = dtcDrawEntry(s_dtcList[i], RIGHT_X, RIGHT_W, codeY);
        }

        int16_t btnY = sh - BTN_H - PAD;
        dtcDrawOkButton(RIGHT_X, btnY, RIGHT_W - PAD, BTN_H);
    }

    fbFlush();
}


// ── Touch handler ───────────────────────────────────────────────────────────

void handleDtcTouch(int16_t tx, int16_t ty, bool touched) {
    if (touched) { trackTouchDown(tx, ty); return; }
    Gesture g = computeGesture();
    if (!g.fired || !g.isTap) return;
    if (inZone(g.sx, g.sy, s_dtcZOK)) dtcIgnoreAll();
}
