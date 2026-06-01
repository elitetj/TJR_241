#pragma once
/*
 * TJR_touch.h — Touch input handler and warn auto-jump.
 *
 * Ported from TJR_mini/TJR_touch.h with adjustments for the 2.41" panel:
 *   • No Arduino_GFX (gfx->fillScreen removed — draw fns clear per-frame).
 *   • Gesture thresholds slightly scaled for 600×450 coordinate space.
 *   • Bar hit zone widened to ±50 px (vs ±40 px in TJR_mini).
 *   • Stub handlers for subsystems not yet ported (picker, settings, OTA, DTC).
 *     These stubs are never reached because their flags start false.
 *
 * Requires globals declared in TJR_241.ino:
 *   g_touchDown, g_sx0/sy0, g_lastTx/Ty, g_st0
 *   g_dragPending, g_dragPendSlot, g_dragPendIsHigh, g_dragPendTime
 *   g_warnDragging, g_dragSlot, g_dragIsHigh
 *   g_labelSlot, g_labelIsHigh, g_labelVal, g_labelExpiry
 *   g_otaHoldArmed, g_otaHoldStart
 *   g_warnFirstMs, g_warnSwipeMs
 *   g_flash, g_flashT (used by drawFlashBorder in TJR_draw.h)
 *
 * Contents:
 *   SUBSYSTEM STUBS     — forward stubs for unimplemented subsystems
 *   BAR HIT HELPERS     — hitBar, barTouchToVal, touchIsHighHandle
 *   GESTURE HELPERS     — trackTouchDown, computeGesture
 *   handleTouch()       — main per-frame touch dispatcher
 *   checkWarnJump()     — auto-switch to alerting pair after 1 s dwell
 */


// ── Auto-jump timing constants ─────────────────────────────────────────────
#define WARN_TRIGGER_MS     1000   // warning must persist this long before auto-jump
#define WARN_SWIPE_GUARD_MS 30000  // manual swipe suppresses auto-jump for this long

// ── Gesture thresholds (pixels, in logical screen coordinates) ────────────
// PPI on the 2.41" panel (~311) is close to TJR_mini (~326), so physical swipe
// distances are nearly identical.  Thresholds are slightly loosened for comfort.
#define TOUCH_TAP_MAX_MOVE    30   // max movement (each axis) to classify as tap
#define TOUCH_TAP_MIN_MS      30   // min contact time for tap (debounce)
#define TOUCH_TAP_MAX_MS     350   // max contact time for tap
#define TOUCH_SWIPE_LR        55   // min |dx| for horizontal swipe
#define TOUCH_SWIPE_UD        50   // min |dy| for vertical swipe
#define TOUCH_SWIPE_MAX_MS   600   // max contact time for swipe
#define TOUCH_DRIFT_CANCEL    25   // movement that cancels OTA hold / drag arm
#define TOUCH_DRAG_JITTER     30   // single-frame jump filter for warn drag
#define TOUCH_HOLD_MS        500   // hold duration to arm warn drag
#define BAR_HIT_MARGIN        50   // ±px around bar that counts as a bar touch


// ============================================================
//  SUBSYSTEM STUBS
//  Handlers for subsystems not yet ported.  The priority routing
//  in handleTouch() only calls these when the corresponding flag
//  is true — which can't happen until the subsystem is added.
// ============================================================

// Implemented in TJR_ota.h, included after TJR_touch.h.
void handleOtaConfirmTouch(int16_t tx, int16_t ty, bool touched);
void handleOtaModeTouch(int16_t tx, int16_t ty, bool touched);
// Implemented in TJR_dtc.h, included after TJR_touch.h.
void handleDtcTouch(int16_t tx, int16_t ty, bool touched);
// Implemented in TJR_graph.h, included after TJR_touch.h.
void graphReset();

// Real implementations provided by TJR_picker.h and TJR_settings.h,
// both included after TJR_touch.h in TJR_241.ino.
void handlePickerTouch(int16_t tx, int16_t ty, bool touched);
void openPicker(int slot);
void handleSettingsTouch(int16_t tx, int16_t ty, bool touched);


// ============================================================
//  BAR HIT HELPERS
// ============================================================

// True if touch point is within the vertical hit zone around a bar (±BAR_HIT_MARGIN).
static bool hitBar(int16_t tx, int16_t ty, const BarCache &bc) {
    return (tx >= bc.x                  && tx <= bc.x + bc.w &&
            ty >= bc.y - BAR_HIT_MARGIN && ty <= bc.y + bc.h + BAR_HIT_MARGIN);
}

// Map a touch X coordinate to an engineering value for the signal shown in this bar.
static float barTouchToVal(int si, int16_t tx, const BarCache &bc) {
    float pct = constrain((float)(tx - bc.x) / (float)bc.w, 0.0f, 1.0f);
    return PARAMS[si].displayMin + pct * (PARAMS[si].displayMax - PARAMS[si].displayMin);
}

// X-proximity pick between the HIGH and LOW warn handles.
// Y-based detection is unreliable — users naturally touch from below the triangle tip.
static bool touchIsHighHandle(int16_t tx, int16_t /*ty*/, int slot, int si, const BarCache &bc) {
    float lo = PARAMS[si].displayMin, hi = PARAMS[si].displayMax;
    if (hi <= lo) return true;
    float   wHPct = constrain((g_warnHigh[slot] - lo) / (hi - lo), 0.0f, 1.0f);
    float   wLPct = constrain((g_warnLow [slot] - lo) / (hi - lo), 0.0f, 1.0f);
    int16_t wHx   = bc.x + (int16_t)(wHPct * bc.w);
    int16_t wLx   = bc.x + (int16_t)(wLPct * bc.w);
    return (abs(tx - wHx) <= abs(tx - wLx));
}


// ============================================================
//  GESTURE HELPERS
// ============================================================

// Record initial touch-down position.  Safe to call every frame — only acts
// on first contact.  Updates g_sx0/sy0, g_st0, g_touchDown, g_lastTx/Ty.
static void trackTouchDown(int16_t tx, int16_t ty) {
    if (!g_touchDown) {
        g_sx0 = tx; g_sy0 = ty; g_st0 = millis();
        g_touchDown = true;
    }
    g_lastTx = tx; g_lastTy = ty;
}

// Classify the completed gesture and clear touch state.
// Returns fired=false if no touch was pending — safe to call every frame on release.
// Gesture.sx/sy hold the initial finger-down position for zone hit-tests after release.
static Gesture computeGesture() {
    Gesture g = {};
    if (!g_touchDown) return g;

    int16_t  dx = g_lastTx - g_sx0;
    int16_t  dy = g_lastTy - g_sy0;
    uint32_t dt = millis() - g_st0;

    g.fired  = true;
    g.sx     = g_sx0;
    g.sy     = g_sy0;
    g.isTap  = (abs(dx) < TOUCH_TAP_MAX_MOVE && abs(dy) < TOUCH_TAP_MAX_MOVE &&
                dt > TOUCH_TAP_MIN_MS && dt < TOUCH_TAP_MAX_MS);
    g.swipeL = (dx < -TOUCH_SWIPE_LR && abs(dx) > abs(dy) * 1.5f && dt < TOUCH_SWIPE_MAX_MS);
    g.swipeR = (dx >  TOUCH_SWIPE_LR && abs(dx) > abs(dy) * 1.5f && dt < TOUCH_SWIPE_MAX_MS);
    g.swipeU = (dy < -TOUCH_SWIPE_UD && abs(dy) > abs(dx) * 1.5f && dt < TOUCH_SWIPE_MAX_MS);
    g.swipeD = (dy >  TOUCH_SWIPE_UD && abs(dy) > abs(dx) * 1.5f && dt < TOUCH_SWIPE_MAX_MS);

    g_touchDown = false;
    g_sx0       = -1;
    return g;
}


// ============================================================
//  TOUCH HANDLER
//  Called every loop iteration.
//  Priority routing: picker → OTA confirm → OTA mode → DTC → settings → gauge.
//  The gauge section handles threshold drag, OTA long-hold, and gesture routing.
// ============================================================

void handleTouch() {
    int16_t tx = 0, ty = 0;
    bool touched = readTouch(tx, ty);

    // readTouch() always returns landscape fb[] coordinates (verified on hardware).
    // In portrait mode, convert to logical screen coords so zone checks and gesture
    // deltas match the coordinates used by the draw functions.
    //   rot=1 (USB-left):  logical_x = NATIVE_H-1-ty,  logical_y = tx
    //   rot=3 (USB-right): logical_x = ty,              logical_y = NATIVE_W-1-tx
    if (!g_isLandscape && touched) {
        int16_t fx = tx, fy = ty;
        if (g_rotation == 1) { tx = NATIVE_H - 1 - fy;  ty = fx; }
        else                  { tx = fy;                  ty = NATIVE_W - 1 - fx; }
    }

    // ── Priority routing ─────────────────────────────────────────────────────
    if (g_pickerOpen)   { handlePickerTouch(tx, ty, touched);      return; }
    if (g_otaConfirm)   { handleOtaConfirmTouch(tx, ty, touched);  return; }
    if (g_otaMode)      { handleOtaModeTouch(tx, ty, touched);     return; }
    if (g_dtcPageOpen)  { handleDtcTouch(tx, ty, touched);         return; }
    if (g_settingsOpen) { handleSettingsTouch(tx, ty, touched);    return; }

    // ── Finger down ──────────────────────────────────────────────────────────
    if (touched) {

        // Continue an active threshold drag (gauge only — graph has no bar).
        // Single-frame jumps > TOUCH_DRAG_JITTER px are suppressed — the touch
        // controller occasionally emits a bad coordinate on lift-off.
        if (!isGraph() && g_warnDragging && g_dragSlot >= 0) {
            if (abs(tx - g_lastTx) > TOUCH_DRAG_JITTER) {
                g_lastTx = tx; g_lastTy = ty; return;
            }
            int      dragSI = g_slotParam[g_dragSlot];
            BarCache *bc    = g_isLandscape ? &g_barLS
                            : (g_dragSlot == slotTop(g_screen)) ? &g_barTop : &g_barBot;
            float newVal = barTouchToVal(dragSI, tx, *bc);
            float margin = (PARAMS[dragSI].displayMax - PARAMS[dragSI].displayMin) * 0.01f;
            float step   = PARAMS[dragSI].snapStep;
            // Dragging 10 px past the bar edge disables that threshold (infinity sentinel).
            if (g_dragIsHigh) {
                if (tx > bc->x + bc->w + 10) {
                    g_warnHigh[g_dragSlot] = INFINITY;
                } else {
                    float v = constrain(newVal, g_warnLow[g_dragSlot] + margin,
                                        PARAMS[dragSI].dangerHigh - margin);
                    g_warnHigh[g_dragSlot] = roundf(v / step) * step;
                }
            } else {
                if (tx < bc->x - 10) {
                    g_warnLow[g_dragSlot] = -INFINITY;
                } else {
                    float v = constrain(newVal, PARAMS[dragSI].dangerLow + margin,
                                        g_warnHigh[g_dragSlot] - margin);
                    g_warnLow[g_dragSlot] = roundf(v / step) * step;
                }
            }
            markDirty();
            if (g_labelSlot == g_dragSlot)
                g_labelVal = g_labelIsHigh ? g_warnHigh[g_labelSlot] : g_warnLow[g_labelSlot];
            g_lastTx = tx; g_lastTy = ty;
            return;
        }

        // First contact
        if (!g_touchDown) {
            g_sx0 = tx; g_sy0 = ty; g_st0 = millis();
            g_touchDown = true;

            // On gauge: check if finger landed in a bar zone — arm drag timer.
            // On graph: no bar zones, fall through to OTA arm.
            bool inBarZone = false;
            if (!isGraph()) {
                BarCache *bc = nullptr; int slot = -1;
                if (!g_isLandscape) {
                    if      (hitBar(tx, ty, g_barTop)) { bc = &g_barTop; slot = slotTop(g_screen); }
                    else if (hitBar(tx, ty, g_barBot)) { bc = &g_barBot; slot = slotBot(g_screen); }
                } else {
                    if      (hitBar(tx, ty, g_barLS))  { bc = &g_barLS;  slot = slotLS (g_screen); }
                }
                if (bc && slot >= 0) {
                    g_dragPending    = true;
                    g_dragPendSlot   = slot;
                    g_dragPendIsHigh = touchIsHighHandle(tx, ty, slot, g_slotParam[slot], *bc);
                    g_dragPendTime   = millis();
                    inBarZone        = true;
                }
            }
            if (!inBarZone) {
                // Outside bar zone (or graph) — arm OTA 3-second hold timer
                g_otaHoldArmed = true;
                g_otaHoldStart = millis();
            }
        }

        g_lastTx = tx; g_lastTy = ty;

        // OTA long-hold — universal across all non-overlay screens
        if (g_otaHoldArmed) {
            if (abs(tx - g_sx0) > TOUCH_DRIFT_CANCEL || abs(ty - g_sy0) > TOUCH_DRIFT_CANCEL) {
                g_otaHoldArmed = false;
            } else if (millis() - g_otaHoldStart >= 3000) {
                g_otaHoldArmed = false;
                g_otaConfirm   = true;
                g_touchDown    = false;
                g_sx0          = -1;
            }
        }

        // Pending drag promote — gauge only
        if (!isGraph() && g_dragPending) {
            if (abs(tx - g_sx0) > TOUCH_DRIFT_CANCEL || abs(ty - g_sy0) > TOUCH_DRIFT_CANCEL) {
                // Finger moved — cancel pending drag
                g_dragPending  = false;
                g_dragPendSlot = -1;
            } else if (millis() - g_dragPendTime >= TOUCH_HOLD_MS) {
                // Held still long enough — promote to active drag
                g_warnDragging = true;
                g_dragSlot     = g_dragPendSlot;
                g_dragIsHigh   = g_dragPendIsHigh;
                g_dragPending  = false;
                g_dragPendSlot = -1;
                g_touchDown    = false;   // prevent swipe/tap classification on release
                g_sx0          = -1;
                g_labelSlot    = g_dragSlot;
                g_labelIsHigh  = g_dragIsHigh;
                g_labelVal     = g_labelIsHigh ? g_warnHigh[g_labelSlot] : g_warnLow[g_labelSlot];
                g_labelExpiry  = 0;
            }
        }

    } else {
        // ── Finger up ────────────────────────────────────────────────────────
        g_otaHoldArmed = false;

        // End threshold drag
        if (!isGraph() && g_warnDragging) {
            g_warnDragging = false;
            g_dragSlot     = -1;
            if (g_labelSlot >= 0) g_labelExpiry = millis() + 3000;
            return;
        }
        if (!isGraph() && g_dragPending) {
            g_dragPending  = false;
            g_dragPendSlot = -1;
        }

        // Classify gesture — fired=false if touch state was already cleared (drag/OTA path).
        Gesture g = computeGesture();
        if (!g.fired) return;

        int16_t sw = scrW(), sh = scrH();

        // ── Universal gestures — same behaviour on every gauge screen ─────────

        if (g.isTap && g.sx > sw - NIGHT_ZONE_W && g.sy < NIGHT_ZONE_H) {
            // Night mode toggle — invisible zone, top-right corner
            g_nightMode = !g_nightMode;
            applyBrightness();
            markDirty();

        } else if (g.swipeL || g.swipeR) {
            // Cycle screens; record swipe time for warn auto-jump suppression
            if (g.swipeL) g_screen = (g_screen + 1)               % NUM_SCREENS;
            else          g_screen = (g_screen + NUM_SCREENS - 1)  % NUM_SCREENS;
            g_warnSwipeMs = millis();
            if (isGraph()) graphReset();
            markDirty();

        } else if (g.swipeU) {
            g_settingsOpen = true;
            g_settingsTab  = 0;

        } else if (g.swipeD) {
            // Reset accumulated peak / min for this screen
            if (isGraph()) {
                graphReset();
            } else {
                for (int i = 0; i < NUM_SIGNALS; i++) {
                    g_peak[i] = g_val[i];
                    g_min[i]  = g_val[i];
                }
            }

        // ── Screen-specific: tap opens signal picker ─────────────────────────
        } else if (g.isTap) {
            if (isGraph()) {
                g_pickerForGraph = true;
                openPicker(0);
            } else {
                int tapSlot = g_isLandscape ? slotLS(g_screen)
                            : (g.sy < sh / 2) ? slotTop(g_screen) : slotBot(g_screen);
                if (tapSlot >= 0) openPicker(tapSlot);
            }
        }
    }
}


// ============================================================
//  WARN AUTO-JUMP
//  Called every loop iteration during normal gauge operation.
//  If any slot on a different pair has been in warning for at
//  least WARN_TRIGGER_MS continuously, the display jumps to
//  that pair.  Lowest signal index = highest priority.
//
//  Guards: 30 s boot delay, engine running (RPM > 500), no modal
//  overlays, 30 s post-swipe suppression window.
// ============================================================

void checkWarnJump() {
    // Gate 1: suppress for the first 30 s after boot
    if (millis() < 30000) { g_warnFirstMs = 0; return; }

    // Gate 2: engine must be running
    if (g_val[0] <= 500.0f) { g_warnFirstMs = 0; return; }

    // Gate 3: no modal overlay open (graph intentionally excluded)
    if (g_settingsOpen || g_otaConfirm || g_otaMode || g_pickerOpen || g_dtcPageOpen) {
        g_warnFirstMs = 0;
        return;
    }

    // Gate 4: post-swipe guard window
    if (g_warnSwipeMs && (millis() - g_warnSwipeMs < WARN_SWIPE_GUARD_MS)) {
        g_warnFirstMs = 0;
        return;
    }

    // Stay put if the current pair already has an active warning
    if (g_isLandscape) {
        int   s = slotLS(g_screen);
        float v = g_val[g_slotParam[s]];
        if (inDanger(s, v) || inWarn(s, v)) { g_warnFirstMs = 0; return; }
    } else {
        int   topS = slotTop(g_screen), botS = slotBot(g_screen);
        float vT   = g_val[g_slotParam[topS]], vB = g_val[g_slotParam[botS]];
        if (inDanger(topS, vT) || inWarn(topS, vT) ||
            inDanger(botS, vB) || inWarn(botS, vB)) {
            g_warnFirstMs = 0;
            return;
        }
    }

    // Scan all other pairs for the highest-priority (lowest signal index) warning
    int bestPair = -1;
    int bestSig  = INT_MAX;

    for (int p = 0; p < NUM_PAIRS; p++) {
        if (p == g_screen) continue;
        if (g_isLandscape) {
            int   s   = slotLS(p);
            int   sig = g_slotParam[s];
            float v   = g_val[sig];
            if ((inDanger(s, v) || inWarn(s, v)) && sig < bestSig) {
                bestSig = sig; bestPair = p;
            }
        } else {
            int slots[2] = { slotTop(p), slotBot(p) };
            for (int i = 0; i < 2; i++) {
                int   s   = slots[i];
                int   sig = g_slotParam[s];
                float v   = g_val[sig];
                if ((inDanger(s, v) || inWarn(s, v)) && sig < bestSig) {
                    bestSig = sig; bestPair = p;
                }
            }
        }
    }

    if (bestPair < 0) { g_warnFirstMs = 0; return; }   // no off-screen warning

    // Start the dwell timer on first detection
    if (g_warnFirstMs == 0) { g_warnFirstMs = millis(); return; }

    // Dwell elapsed — jump to the alerting pair
    if (millis() - g_warnFirstMs >= WARN_TRIGGER_MS) {
        g_screen      = bestPair;
        g_warnFirstMs = 0;
        // No explicit screen clear needed — drawPortrait/drawLandscape fill each frame
    }
}
