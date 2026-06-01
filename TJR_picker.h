#pragma once
/*
 * TJR_picker.h — Signal picker overlay.
 *
 * Ported from TJR_mini/TJR_picker.h. No Arduino_GFX.
 *
 * Layout
 * ──────
 * Landscape 600×450:  hdrH=70  rowH=64  btnH=80  listH=300  nVis=4
 * Portrait  450×600:  hdrH=70  rowH=70  btnH=80  listH=450  nVis=6
 *
 * Each row: signal name left, live value right (mono so it doesn't jiggle).
 * Currently assigned signal highlighted with a dim background.
 * Scroll triangles shown at the right edge when more content exists.
 *
 * Tap row → assign signal, close picker.
 * Tap CANCEL or swipe-down outside list → close without change.
 * Swipe-up inside list → scroll down. Swipe-down inside list → scroll up.
 *
 * Globals (declared in TJR_241.ino):
 *   g_pickerOpen, g_pickerSlot, g_pickerScroll, g_pickerOrder[]
 *   g_pickerForGraph, g_pickerZCancel, g_pickerZList
 *
 * graphReset() forward-declared as stub in TJR_touch.h (replaced by TJR_graph.h).
 */


// Build g_pickerOrder[] — signal indices sorted alphabetically by name.
static void pickerBuildOrder() {
    for (int i = 0; i < NUM_SIGNALS; i++) g_pickerOrder[i] = i;
    for (int i = 1; i < NUM_SIGNALS; i++) {
        uint8_t key = g_pickerOrder[i];
        int     j   = i - 1;
        while (j >= 0 && strcmp(PARAMS[g_pickerOrder[j]].name, PARAMS[key].name) > 0) {
            g_pickerOrder[j + 1] = g_pickerOrder[j];
            j--;
        }
        g_pickerOrder[j + 1] = key;
    }
}

void openPicker(int slot) {
    g_pickerSlot   = slot;
    g_pickerScroll = 0;
    g_pickerOpen   = true;
    pickerBuildOrder();
    // Scroll so the currently assigned signal is already in view
    uint8_t curSI = g_slotParam[slot];
    for (int i = 0; i < NUM_SIGNALS; i++) {
        if (g_pickerOrder[i] == curSI) {
            g_pickerScroll = max(0, i - 2);
            break;
        }
    }
}

void drawPicker() {
    int16_t sw = scrW(), sh = scrH();
    fbFill(C_BG);

    int16_t fhL  = fontH(FONT_LABEL);
    int16_t PAD  = 16;
    int16_t hdrH = 70;
    int16_t btnH = 80;
    int16_t rowH = g_isLandscape ? 64 : 70;
    int16_t listY = hdrH;
    int16_t listH = sh - hdrH - btnH;
    int     nVis  = listH / rowH;

    // ── Header ───────────────────────────────────────────────────────────────
    printCentred("SELECT SIGNAL", (hdrH - fhL) / 2, C_WHITE, FONT_LABEL);
    fbHLine(0, hdrH, sw, colDim());

    // ── List rows ────────────────────────────────────────────────────────────
    uint8_t curSI = (g_pickerForGraph) ? g_graphSI : (uint8_t)g_slotParam[g_pickerSlot];
    for (int row = 0; row < nVis; row++) {
        int idx = g_pickerScroll + row;
        if (idx >= NUM_SIGNALS) break;
        uint8_t si  = g_pickerOrder[idx];
        int16_t ry  = listY + row * rowH;
        bool    sel = (si == curSI);

        if (sel) fbFillRect(0, ry, sw, rowH, colDark());

        uint16_t col = sel ? C_WHITE : colDim();

        // Signal name — left
        printLeft(PARAMS[si].name, PAD, ry + (rowH - fhL) / 2, col, FONT_LABEL);

        // Live value — right, advance-based so digits don't jiggle
        char vbuf[14];
        fmtVal(vbuf, sizeof(vbuf), si, (float)g_val[si]);
        printRightMono(vbuf, sw - PAD, ry + (rowH - fhL) / 2, col, FONT_LABEL);

        // Row divider
        if (row < nVis - 1) fbHLine(0, ry + rowH - 1, sw, colDark());
    }

    // Scroll indicators — triangles at right edge
    {
        int16_t ax = sw - 14;
        if (g_pickerScroll > 0) {
            int16_t ay = listY + 8;
            fbFillTriangle(ax, ay + 10, ax + 10, ay + 10, ax + 5, ay, colDim());
        }
        if (g_pickerScroll + nVis < NUM_SIGNALS) {
            int16_t ay = listY + listH - 18;
            fbFillTriangle(ax, ay, ax + 10, ay, ax + 5, ay + 10, colDim());
        }
    }

    // ── CANCEL button ────────────────────────────────────────────────────────
    fbHLine(0, sh - btnH, sw, colDim());
    {
        int16_t ebW = 180, ebH = 54;
        int16_t ebX = (sw - ebW) / 2;
        int16_t ebY = sh - btnH + (btnH - ebH) / 2;
        fbDrawRoundRect(ebX, ebY, ebW, ebH, 8, colDim());
        printCentredIn("CANCEL", ebX, ebY, ebW, ebH, colDim(), FONT_LABEL);
    }
    g_pickerZCancel = { 0, (int16_t)(sh - btnH), sw, btnH };
    g_pickerZList   = { 0, listY, sw, listH };

    fbFlush();
}

void handlePickerTouch(int16_t tx, int16_t ty, bool touched) {
    if (touched) { trackTouchDown(tx, ty); return; }

    Gesture g = computeGesture();
    if (!g.fired) return;

    int16_t rowH = g_isLandscape ? 64 : 70;

    if (g.isTap) {
        if (inZone(g.sx, g.sy, g_pickerZCancel)) {
            g_pickerOpen = false;

        } else if (inZone(g.sx, g.sy, g_pickerZList)) {
            int row = (g.sy - g_pickerZList.y) / rowH;
            int idx = g_pickerScroll + row;
            if (idx >= 0 && idx < NUM_SIGNALS) {
                uint8_t si = g_pickerOrder[idx];
                if (g_pickerForGraph) {
                    g_graphSI        = si;
                    g_pickerForGraph = false;
                    graphReset();
                    markDirty();
                } else {
                    if (si != g_slotParam[g_pickerSlot]) {
                        g_slotParam[g_pickerSlot] = si;
                        g_warnLow [g_pickerSlot]  = PARAMS[si].warnLow;
                        g_warnHigh[g_pickerSlot]  = PARAMS[si].warnHigh;
                        markDirty();
                    }
                }
                g_pickerOpen = false;
            }
        }

    } else if (g.swipeU && inZone(g.sx, g.sy, g_pickerZList)) {
        int nVis = g_pickerZList.h / rowH;
        g_pickerScroll = constrain(g_pickerScroll + 3, 0, NUM_SIGNALS - nVis);

    } else if (g.swipeD) {
        if (inZone(g.sx, g.sy, g_pickerZList))
            g_pickerScroll = max(0, g_pickerScroll - 3);
        else
            g_pickerOpen = false;
    }
}
