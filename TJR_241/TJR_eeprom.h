#pragma once

// ============================================================
//  TJR_eeprom.h — EEPROM persistence (load and save)
//
//  Layout identical to TJR_mini (144 bytes, magic 0xA8).
//  Safe to copy EEPROM image between boards if slot layout is
//  unchanged (same NUM_SLOTS, NUM_SIGNALS, same EE_ADDR_* offsets).
//
//  EEPROM layout (148 bytes, magic 0xA8):
//    0         magic byte
//    1         g_screen (0..NUM_PAIRS-1 = gauge pair, NUM_PAIRS = quad, NUM_PAIRS+1 = graph)
//    2         g_nightMode flag
//    3–4       g_brightDay, g_brightNight (0–100 each)
//    5         unit flags (bit0=Celsius, bit1=kPa, bit2=Lambda, bit3=km/h)
//    6–20      slot→signal index (15 slots × 1 byte)
//    21–80     warnLow per slot  (15 × float = 60 bytes)
//    81–140    warnHigh per slot (15 × float = 60 bytes)
//    141       [reserved]
//    142       g_graphSI signal index
//    143       last rotation (0–3); 0xFF on first boot
//    144–147   g_quadParam[4] signal indices
// ============================================================

void eepromLoad() {
  EEPROM.begin(EE_SIZE);

  // Seed from compile defaults first so every slot is valid even on fresh EEPROM.
  for (int s = 0; s < NUM_SLOTS; s++) {
    g_slotParam[s] = DEFAULT_SLOTS[s];
    g_warnLow[s]   = PARAMS[DEFAULT_SLOTS[s]].warnLow;
    g_warnHigh[s]  = PARAMS[DEFAULT_SLOTS[s]].warnHigh;
  }
  for (int i = 0; i < 4; i++) g_quadParam[i] = (uint8_t)i;

  // Duo-HS defaults: RPM top, OIL PRES bottom (signal indices 0 and 4)
  g_slotParam[SLOT_DUO_TOP] = 0;  g_slotParam[SLOT_DUO_BOT] = 4;
  g_warnLow [SLOT_DUO_TOP]  = PARAMS[0].warnLow;   g_warnHigh[SLOT_DUO_TOP] = PARAMS[0].warnHigh;
  g_warnLow [SLOT_DUO_BOT]  = PARAMS[4].warnLow;   g_warnHigh[SLOT_DUO_BOT] = PARAMS[4].warnHigh;

  if (EEPROM.read(EE_ADDR_MAGIC) != EE_MAGIC) {
    g_screen    = 0;
    g_nightMode = false;
    return;
  }

  g_screen    = constrain((int)EEPROM.read(EE_ADDR_PAIR),  0, NUM_SCREENS-1);
  g_nightMode = (EEPROM.read(EE_ADDR_NIGHT) != 0);

  uint8_t bd = EEPROM.read(EE_ADDR_BRIGHT);
  uint8_t bn = EEPROM.read(EE_ADDR_BRIGHT + 1);
  if (bd >= 10 && bd <= 100) g_brightDay   = bd;
  if (bn >= 10 && bn <= 100) g_brightNight = bn;

  uint8_t uf  = EEPROM.read(EE_ADDR_UNITS);
  g_unitsCelsius = (uf & 0x01) != 0;
  g_unitsKpa     = (uf & 0x02) != 0;
  g_unitsLambda  = (uf & 0x04) != 0;
  g_unitsKmh     = (uf & 0x08) != 0;

  for (int s = 0; s < NUM_SLOTS; s++) {
    uint8_t v = EEPROM.read(EE_ADDR_SLOTS + s);
    if (v < NUM_SIGNALS) g_slotParam[s] = v;
  }

  for (int s = 0; s < NUM_SLOTS; s++) {
    int     si = g_slotParam[s];
    float lo, hi;
    EEPROM.get(EE_ADDR_WARNLO + s * 4, lo);
    EEPROM.get(EE_ADDR_WARNHI + s * 4, hi);
    bool loOk = (isinf(lo) && lo < 0) ||
                (lo > PARAMS[si].dangerLow && lo < PARAMS[si].dangerHigh);
    bool hiOk = (isinf(hi) && hi > 0) ||
                (hi > PARAMS[si].dangerLow && hi < PARAMS[si].dangerHigh);
    if (loOk) g_warnLow[s]  = lo;
    if (hiOk) g_warnHigh[s] = hi;
  }

  uint8_t gsi = EEPROM.read(EE_ADDR_GRAPH_SI);
  if (gsi < NUM_SIGNALS) g_graphSI = gsi;

  for (int i = 0; i < 4; i++) {
    uint8_t v = EEPROM.read(EE_ADDR_QUAD + i);
    if (v < NUM_SIGNALS) g_quadParam[i] = v;
  }

  {
    uint8_t vT = EEPROM.read(EE_ADDR_DUO_SLOTS);
    uint8_t vB = EEPROM.read(EE_ADDR_DUO_SLOTS + 1);
    if (vT < NUM_SIGNALS) g_slotParam[SLOT_DUO_TOP] = vT;
    if (vB < NUM_SIGNALS) g_slotParam[SLOT_DUO_BOT] = vB;
    float lo, hi;
    EEPROM.get(EE_ADDR_DUO_WARNLO,     lo); EEPROM.get(EE_ADDR_DUO_WARNHI,     hi);
    if (isfinite(lo) || (isinf(lo) && lo < 0)) g_warnLow [SLOT_DUO_TOP] = lo;
    if (isfinite(hi) || (isinf(hi) && hi > 0)) g_warnHigh[SLOT_DUO_TOP] = hi;
    EEPROM.get(EE_ADDR_DUO_WARNLO + 4, lo); EEPROM.get(EE_ADDR_DUO_WARNHI + 4, hi);
    if (isfinite(lo) || (isinf(lo) && lo < 0)) g_warnLow [SLOT_DUO_BOT] = lo;
    if (isfinite(hi) || (isinf(hi) && hi > 0)) g_warnHigh[SLOT_DUO_BOT] = hi;
  }

  uint8_t savedRot = EEPROM.read(EE_ADDR_ROT);
  if (savedRot <= 3) {
    g_rotation    = savedRot;
    g_isLandscape = !(savedRot & 1);  // rot 0,2 = landscape; rot 1,3 = portrait
  }
}

void eepromSave() {
  EEPROM.write(EE_ADDR_MAGIC, EE_MAGIC);
  EEPROM.write(EE_ADDR_PAIR,  (uint8_t)g_screen);
  EEPROM.write(EE_ADDR_NIGHT, g_nightMode ? 1 : 0);
  EEPROM.write(EE_ADDR_BRIGHT,     g_brightDay);
  EEPROM.write(EE_ADDR_BRIGHT + 1, g_brightNight);
  uint8_t uf = (g_unitsCelsius ? 0x01 : 0)
             | (g_unitsKpa     ? 0x02 : 0)
             | (g_unitsLambda  ? 0x04 : 0)
             | (g_unitsKmh     ? 0x08 : 0);
  EEPROM.write(EE_ADDR_UNITS, uf);
  for (int s = 0; s < NUM_SLOTS; s++) {
    EEPROM.write(EE_ADDR_SLOTS + s, g_slotParam[s]);
  }
  for (int s = 0; s < NUM_SLOTS; s++) {
    EEPROM.put(EE_ADDR_WARNLO + s * 4, g_warnLow[s]);
    EEPROM.put(EE_ADDR_WARNHI + s * 4, g_warnHigh[s]);
  }
  EEPROM.write(EE_ADDR_GRAPH_SI, g_graphSI);
  EEPROM.write(EE_ADDR_ROT,      g_rotation);
  for (int i = 0; i < 4; i++) EEPROM.write(EE_ADDR_QUAD + i, g_quadParam[i]);
  EEPROM.write(EE_ADDR_DUO_SLOTS,     g_slotParam[SLOT_DUO_TOP]);
  EEPROM.write(EE_ADDR_DUO_SLOTS + 1, g_slotParam[SLOT_DUO_BOT]);
  EEPROM.put(EE_ADDR_DUO_WARNLO,     g_warnLow [SLOT_DUO_TOP]);
  EEPROM.put(EE_ADDR_DUO_WARNHI,     g_warnHigh[SLOT_DUO_TOP]);
  EEPROM.put(EE_ADDR_DUO_WARNLO + 4, g_warnLow [SLOT_DUO_BOT]);
  EEPROM.put(EE_ADDR_DUO_WARNHI + 4, g_warnHigh[SLOT_DUO_BOT]);
  EEPROM.commit();
  g_eeDirty = false;
  Serial.println("EEPROM saved");
}
