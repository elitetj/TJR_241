#pragma once
/*
 * TJR_settings.h — Settings overlay (swipe-up from gauge).
 *
 * Ported from TJR_mini/TJR_settings.h with layout rederived for 600×450.
 * No Arduino_GFX — uses TJR_draw.h primitives throughout.
 *
 * Two tabs:
 *   UNITS   — four toggle rows (TEMP / PRES / FUEL / SPEED unit choice)
 *   DISPLAY — two brightness sliders (BR.DAY / BR.NIGHT)
 *
 * Layout summary
 * ──────────────
 * Landscape 600×450:
 *   Tab bar: 60px.  UNITS: left 400px + right EXIT 196px.
 *   DISPLAY: two full-width slider sections + EXIT strip at bottom.
 *
 * Portrait 450×600:
 *   Tab bar: 60px.  EXIT strip: 80px fixed at bottom.
 *   Content fills between tab bar and EXIT.
 *
 * Globals used (declared in TJR_241.ino):
 *   g_settingsTab, g_setDragging, g_setDragIsDay
 *   g_setZTab[2], g_setZUnits[4], g_setZBrDay, g_setZBrNight, g_setZExit
 *
 * computeGesture() / inZone() / applyBrightness() — in scope from earlier includes.
 */


// ── Settings-specific padding ─────────────────────────────────────────────────
// Slightly tighter than the gauge pad so content fits in the tab content area.
#define SPAD  20


// ============================================================
//  SLIDER HELPER
//  Draws a rounded track with filled progress and a circular knob.
//  Caches the track zone for drag tracking.  Returns fill width (px).
// ============================================================

static int16_t drawSlider(int16_t sx, int16_t sy, int16_t sW, int16_t sH,
                          uint8_t pct, uint16_t fillCol, SettingsZone *zone) {
    int16_t kr   = sH / 2;   // knob radius = half track height
    // Track background
    fbFillRoundRect(sx, sy, sW, sH, kr, colDark());
    // Filled portion
    int16_t fill = (int16_t)((uint32_t)pct * sW / 100);
    if (fill > 0) fbFillRoundRect(sx, sy, fill, sH, kr, fillCol);
    // Track outline
    fbDrawRoundRect(sx, sy, sW, sH, kr, colDim());
    // Knob
    int16_t kx = constrain((int16_t)(sx + fill), sx, (int16_t)(sx + sW));
    fbFillCircle(kx, sy + kr, kr, fillCol);
    fbDrawCircle(kx, sy + kr, kr, colDim());
    if (zone) *zone = { sx, sy, sW, sH };
    return fill;
}


// ============================================================
//  DRAW SETTINGS
//  Full redraw of the settings overlay.  Stores hit zones into
//  g_setZTab[], g_setZUnits[], g_setZBrDay/Night, g_setZExit.
// ============================================================

void drawSettings() {
    int16_t sw = scrW(), sh = scrH();
    fbFill(C_BG);

    int16_t fhL = fontH(FONT_LABEL);   // ≈ 36px

    // Unit label strings — same in both orientations
    static const char *lblA[4] = { "degC", "kPa",  "Lam",  "km/h" };
    static const char *lblB[4] = { "degF", "psi",  "AFR",  "mph"  };
    static const char *lblR[4] = { "TEMP", "PRES", "FUEL", "SPEED" };
    bool *flags[4] = { &g_unitsCelsius, &g_unitsKpa, &g_unitsLambda, &g_unitsKmh };

    // ── Tab bar — identical in both orientations ──────────────────────────────
    int16_t tabH = 60;
    int16_t tabW = sw / 2;
    static const char *tabLabels[2] = { "UNITS", "DISPLAY" };
    for (int t = 0; t < 2; t++) {
        int16_t  tx2    = t * tabW;
        bool     active = (g_settingsTab == t);
        uint16_t bg     = active ? colDark() : C_BG;
        uint16_t fg     = active ? C_WHITE   : colDim();
        fbFillRect(tx2, 0, tabW, tabH, bg);
        fbDrawRect(tx2, 0, tabW, tabH, colDim());
        printCentredIn(tabLabels[t], tx2, 0, tabW, tabH, fg, FONT_LABEL);
        g_setZTab[t] = { tx2, 0, tabW, tabH };
    }
    fbHLine(0, tabH, sw, C_WHITE);

    int16_t cY = tabH + 4;   // content area top
    int16_t cH = sh - cY;    // content area height

    // ════════════════════════════════════════════════════════════════════════════
    //  LANDSCAPE  600 × 450
    // ════════════════════════════════════════════════════════════════════════════
    if (g_isLandscape) {

        if (g_settingsTab == 0) {
            // ── UNITS tab: left content + right EXIT column ───────────────────
            int16_t exitW = 196;
            int16_t lW    = sw - exitW - 4;   // ≈ 400 px
            int16_t rowH  = cH / 4;           // ≈ 96 px each
            int16_t btnW  = (lW - SPAD * 2 - 8) / 2;
            int16_t btnH  = rowH - 18;

            for (int r = 0; r < 4; r++) {
                int16_t ry = cY + r * rowH;
                g_setZUnits[r] = { 0, ry, lW, rowH };

                // Row label — vertically centred
                printLeft(lblR[r], SPAD, ry + (rowH - fhL) / 2,
                          colDim(), FONT_LABEL);

                // Two option buttons side by side
                const char *lbls[2] = { lblA[r], lblB[r] };
                for (int s = 0; s < 2; s++) {
                    bool     active = (*flags[r] == (s == 0));
                    int16_t  bx     = SPAD + s * (btnW + 8);
                    int16_t  by     = ry + (rowH - btnH) / 2;
                    uint16_t bcol   = active ? C_WHITE : colDim();
                    if (active) fbFillRoundRect(bx, by, btnW, btnH, 5, colDark());
                    fbDrawRoundRect(bx, by, btnW, btnH, 5, bcol);
                    printCentredIn(lbls[s], bx, by, btnW, btnH, bcol, FONT_LABEL);
                }
                if (r < 3) fbHLine(0, ry + rowH, lW, colDark());
            }

            // Right EXIT button — spans full content height
            int16_t exitX = sw - exitW;
            fbVLine(exitX - 1, cY, cH, colDim());
            fbDrawRoundRect(exitX + 8, cY + 8, exitW - 16, cH - 16, 8, C_WHITE);
            printCentredIn("EXIT", exitX + 8, cY + 8, exitW - 16, cH - 16,
                           C_WHITE, FONT_LABEL);
            g_setZExit = { exitX, cY, exitW, cH };

            // Clear stale slider zones
            g_setZBrDay = g_setZBrNight = { 0, 0, 0, 0 };

        } else {
            // ── DISPLAY tab: two slider sections + EXIT strip ─────────────────
            int16_t exitH   = 60;
            int16_t sliderH = cH - exitH;
            int16_t secH    = sliderH / 2;
            int16_t sW      = sw - SPAD * 2;
            int16_t sH      = 44;

            // BR. DAY
            {
                int16_t sy = cY + (secH - fhL - sH - 12) / 2;
                char pct[8]; snprintf(pct, sizeof(pct), "%d%%", (int)g_brightDay);
                printLeft ("BR. DAY", SPAD,      sy, C_WHITE, FONT_LABEL);
                printRight(pct,        sw - SPAD, sy, C_WHITE, FONT_LABEL);
                drawSlider(SPAD, sy + fhL + 12, sW, sH, g_brightDay, C_WHITE, &g_setZBrDay);
            }
            fbHLine(0, cY + secH, sw, colDim());

            // BR. NIGHT
            {
                int16_t sy = cY + secH + (secH - fhL - sH - 12) / 2;
                char pct[8]; snprintf(pct, sizeof(pct), "%d%%", (int)g_brightNight);
                printLeft ("BR. NIGHT", SPAD,      sy, C_WHITE, FONT_LABEL);
                printRight(pct,          sw - SPAD, sy, C_WHITE, FONT_LABEL);
                drawSlider(SPAD, sy + fhL + 12, sW, sH, g_brightNight, C_NIGHT, &g_setZBrNight);
            }
            fbHLine(0, cY + sliderH, sw, colDim());

            // EXIT
            {
                int16_t sy  = cY + sliderH;
                int16_t ebW = 180, ebH = exitH - 14;
                int16_t ebX = (sw - ebW) / 2, ebY = sy + 7;
                fbDrawRoundRect(ebX, ebY, ebW, ebH, 8, C_WHITE);
                printCentredIn("EXIT", ebX, ebY, ebW, ebH, C_WHITE, FONT_LABEL);
                g_setZExit = { 0, sy, sw, exitH };
            }

            // Clear stale unit zones
            for (int r = 0; r < 4; r++) g_setZUnits[r] = { 0, 0, 0, 0 };
        }

        fbFlush();
        return;
    }

    // ════════════════════════════════════════════════════════════════════════════
    //  PORTRAIT  450 × 600
    //
    //  EXIT strip fixed at the bottom — never moves between tabs.
    // ════════════════════════════════════════════════════════════════════════════
    int16_t exitH    = 80;
    int16_t exitY    = sh - exitH;
    int16_t contentH = exitY - cY - 4;

    // Fixed EXIT strip
    fbHLine(0, exitY, sw, colDim());
    {
        int16_t ebW = 180, ebH = 54;
        int16_t ebX = (sw - ebW) / 2, ebY = exitY + (exitH - ebH) / 2;
        fbDrawRoundRect(ebX, ebY, ebW, ebH, 8, C_WHITE);
        printCentredIn("EXIT", ebX, ebY, ebW, ebH, C_WHITE, FONT_LABEL);
        g_setZExit = { 0, exitY, sw, exitH };
    }

    if (g_settingsTab == 0) {
        // ── UNITS tab — 4 rows ────────────────────────────────────────────────
        int16_t rowH = contentH / 4;
        int16_t btnW = (sw - SPAD * 2 - 8) / 2;
        int16_t btnH = rowH - 20;

        for (int r = 0; r < 4; r++) {
            int16_t ry = cY + r * rowH;
            g_setZUnits[r] = { 0, ry, sw, rowH };

            // Row label — vertically centred
            printLeft(lblR[r], SPAD, ry + (rowH - fhL) / 2,
                      colDim(), FONT_LABEL);

            const char *lbls[2] = { lblA[r], lblB[r] };
            for (int s = 0; s < 2; s++) {
                bool     active = (*flags[r] == (s == 0));
                int16_t  bx     = SPAD + s * (btnW + 8);
                int16_t  by     = ry + (rowH - btnH) / 2;
                uint16_t bcol   = active ? C_WHITE : colDim();
                if (active) fbFillRoundRect(bx, by, btnW, btnH, 5, colDark());
                fbDrawRoundRect(bx, by, btnW, btnH, 5, bcol);
                printCentredIn(lbls[s], bx, by, btnW, btnH, bcol, FONT_LABEL);
            }
            if (r < 3) fbHLine(0, ry + rowH, sw, colDark());
        }
    }

    if (g_settingsTab == 1) {
        // ── DISPLAY tab — 2 slider sections ──────────────────────────────────
        int16_t secH = contentH / 2;
        int16_t sW   = sw - SPAD * 2;
        int16_t sH   = 44;

        // BR. DAY
        {
            int16_t sy = cY + (secH - fhL - sH - 12) / 2;
            char pct[8]; snprintf(pct, sizeof(pct), "%d%%", (int)g_brightDay);
            printLeft ("BR. DAY", SPAD,      sy, C_WHITE, FONT_LABEL);
            printRight(pct,        sw - SPAD, sy, C_WHITE, FONT_LABEL);
            drawSlider(SPAD, sy + fhL + 12, sW, sH, g_brightDay, C_WHITE, &g_setZBrDay);
        }
        fbHLine(0, cY + secH, sw, colDim());

        // BR. NIGHT
        {
            int16_t sy = cY + secH + (secH - fhL - sH - 12) / 2;
            char pct[8]; snprintf(pct, sizeof(pct), "%d%%", (int)g_brightNight);
            printLeft ("BR. NIGHT", SPAD,      sy, C_WHITE, FONT_LABEL);
            printRight(pct,          sw - SPAD, sy, C_WHITE, FONT_LABEL);
            drawSlider(SPAD, sy + fhL + 12, sW, sH, g_brightNight, C_NIGHT, &g_setZBrNight);
        }
    }

    fbFlush();
}


// ============================================================
//  HANDLE SETTINGS TOUCH
//  Called from handleTouch() in TJR_touch.h when g_settingsOpen.
//  Slider drag arms immediately on first contact; no hold timer.
//  Tap/swipe gestures classified via computeGesture() on release.
// ============================================================

void handleSettingsTouch(int16_t tx, int16_t ty, bool touched) {
    if (touched) {
        // Continue active brightness slider drag
        if (g_setDragging) {
            const SettingsZone &sz = g_setDragIsDay ? g_setZBrDay : g_setZBrNight;
            if (sz.w > 0) {
                float   pct = constrain((float)(tx - sz.x) / (float)sz.w, 0.0f, 1.0f);
                uint8_t val = (uint8_t)constrain((int)(pct * 100.0f + 0.5f), 10, 100);
                if (g_setDragIsDay) g_brightDay   = val;
                else                g_brightNight = val;
                applyBrightness();
                markDirty();
            }
            return;
        }
        // First contact
        if (!g_touchDown) {
            g_sx0 = tx; g_sy0 = ty; g_st0 = millis();
            g_touchDown = true;
            // Arm slider drag immediately if touch is in slider zone
            if (g_settingsTab == 1) {
                if      (inZone(tx, ty, g_setZBrDay))   { g_setDragging = true; g_setDragIsDay = true; }
                else if (inZone(tx, ty, g_setZBrNight)) { g_setDragging = true; g_setDragIsDay = false; }
            }
        }
        g_lastTx = tx; g_lastTy = ty;

    } else {
        // Released
        if (g_setDragging) { g_setDragging = false; return; }

        Gesture g = computeGesture();
        if (!g.fired) return;

        if (g.isTap) {
            // Tab switch
            for (int t = 0; t < 2; t++) {
                if (inZone(g.sx, g.sy, g_setZTab[t])) {
                    g_settingsTab = t;
                    g_setDragging = false;
                    return;
                }
            }
            // UNITS toggle — left half = option A, right half = option B
            if (g_settingsTab == 0) {
                bool *flags[4] = { &g_unitsCelsius, &g_unitsKpa, &g_unitsLambda, &g_unitsKmh };
                for (int r = 0; r < 4; r++) {
                    if (inZone(g.sx, g.sy, g_setZUnits[r])) {
                        int16_t mid = g_setZUnits[r].x + g_setZUnits[r].w / 2;
                        *flags[r]   = (g.sx < mid);
                        markDirty();
                        return;
                    }
                }
            }
            // EXIT
            if (inZone(g.sx, g.sy, g_setZExit)) {
                g_settingsOpen = false;
            }
        }
        // Swipe-down also exits (quick dismiss)
        if (g.swipeD) g_settingsOpen = false;
    }
}
