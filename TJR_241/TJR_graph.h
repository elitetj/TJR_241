#pragma once
/*
 * TJR_graph.h — Rolling graph screen (landscape only).
 *
 * Ported from TJR_mini/TJR_graph.h. No Arduino_GFX.
 *
 * Layout landscape 600×450:
 *   y   0–99:  header — name (left), unit (right), live value centred (FONT_HDG)
 *   y 100–399: graph area — cyan line + dark-blue fill
 *   y 400–449: footer — scale min left, scale max right (FONT_LABEL)
 *
 * Tap → open signal picker.  Swipe L/R → exit (handled in handleTouch).
 *
 * Globals needed (declared in TJR_241.ino):
 *   g_graphSI, g_graphBuf[GRAPH_BUF_LEN], g_graphHead, g_graphCount
 *   g_graphScaleMin, g_graphScaleMax
 */


void graphReset() {
    g_graphHead     = 0;
    g_graphCount    = 0;
    g_graphScaleMin =  1e38f;
    g_graphScaleMax = -1e38f;
}

void graphAddSample(float rawVal) {
    g_graphBuf[g_graphHead] = rawVal;
    g_graphHead = (g_graphHead + 1) % GRAPH_BUF_LEN;
    if (g_graphCount < GRAPH_BUF_LEN) g_graphCount++;
    if (rawVal < g_graphScaleMin) g_graphScaleMin = rawVal;
    if (rawVal > g_graphScaleMax) g_graphScaleMax = rawVal;
}

void drawGraph() {
    int16_t sw = scrW(), sh = scrH();   // landscape: 600 × 450
    fbFill(C_BG);

    const int16_t PAD         = 20;
    const int16_t hdrH        = 100;   // name row + value row + gap
    const int16_t footH       = 50;
    const int16_t graphY      = hdrH;
    const int16_t graphH      = sh - hdrH - footH;
    const int16_t graphBottom = graphY + graphH;
    int16_t fhL = fontH(FONT_LABEL);
    int16_t fhH = fontH(FONT_HDG);

    // ── Header: signal name + unit ────────────────────────────────────────────
    uint16_t hdrCol = g_nightMode ? C_NIGHT : C_WHITE;
    printLeft (PARAMS[g_graphSI].name,  PAD,      6, hdrCol, FONT_LABEL);
    printRight(displayUnit(g_graphSI),  sw - PAD, 6, hdrCol, FONT_LABEL);

    // Live value centred below name, anchored near the separator
    char vbuf[16];
    fmtVal(vbuf, sizeof(vbuf), g_graphSI, g_val[g_graphSI]);
    int16_t valY = hdrH - fhH - 4;
    printCentredMono(vbuf, valY, hdrCol, FONT_HDG);

    // ── Graph area ────────────────────────────────────────────────────────────
    fbHLine(0, graphY,      sw, colDim());
    fbHLine(0, graphBottom, sw, colDim());

    if (g_graphCount > 1) {
        uint16_t colFill = g_nightMode ? C_NIGHT_D  : C_GRAPH_FILL;
        uint16_t colLine = g_nightMode ? C_WARN      : C_GRAPH_LINE;

        // Convert raw scale to display units before normalising —
        // all Haltech conversions are linear so this is safe.
        float dispMin = convertVal(g_graphSI, g_graphScaleMin);
        float dispMax = convertVal(g_graphSI, g_graphScaleMax);
        float range   = dispMax - dispMin;
        if (fabsf(range) < 1e-6f) range = 1.0f;

        for (int i = 0; i < g_graphCount; i++) {
            int   bufIdx = (g_graphHead - g_graphCount + i + GRAPH_BUF_LEN) % GRAPH_BUF_LEN;
            float disp   = convertVal(g_graphSI, g_graphBuf[bufIdx]);
            float norm   = constrain((disp - dispMin) / range, 0.0f, 1.0f);

            int16_t x0   = (int16_t)((int32_t)i       * sw / g_graphCount);
            int16_t x1   = (int16_t)((int32_t)(i + 1) * sw / g_graphCount);
            int16_t colW = max((int16_t)1, (int16_t)(x1 - x0));
            int16_t y    = graphY + graphH - (int16_t)(norm * (float)graphH);
            int16_t fillH = graphBottom - y;

            if (fillH > 2) fbFillRect(x0, y + 2, colW, fillH - 2, colFill);
            if (fillH > 0) fbFillRect(x0, y,     colW, min((int16_t)2, fillH), colLine);
        }
    }

    // ── Footer: scale min left, scale max right ───────────────────────────────
    if (g_graphCount > 0) {
        char mnBuf[16], mxBuf[16];
        float dMin = convertVal(g_graphSI, g_graphScaleMin);
        float dMax = convertVal(g_graphSI, g_graphScaleMax);
        snprintf(mnBuf, sizeof(mnBuf), "%.*f", (int)PARAMS[g_graphSI].decimals, (double)dMin);
        snprintf(mxBuf, sizeof(mxBuf), "%.*f", (int)PARAMS[g_graphSI].decimals, (double)dMax);
        int16_t footY = graphBottom + (footH - fhL) / 2;
        printLeftMono (mnBuf, PAD,      footY, colDim(), FONT_LABEL);
        printRightMono(mxBuf, sw - PAD, footY, colDim(), FONT_LABEL);
    }

    fbFlush();
}
