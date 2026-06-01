#pragma once
/*
 * TJR_ota.h — OTA firmware update subsystem.
 *
 * Ported from TJR_mini/TJR_ota.h. No Arduino_GFX — uses TJR_draw.h primitives.
 * Layout uses scrW()/scrH() so portrait and landscape self-adapt.
 *
 * Trigger: 3-second stationary hold on the value area (outside bar zone).
 * YES → WiFi AP "TJR_241" (open) + web server at 192.168.4.1.
 * Upload .bin → auto-reboot on success. EXIT or 5-minute timeout returns to gauge.
 *
 * g_otaStarted, g_otaSuccess, g_otaFailed, g_otaBytesWr, g_otaEnterTime,
 * g_otaLastAct, g_otaZYes, g_otaZNo, g_otaZExit — declared in TJR_241.ino.
 */

#define OTA_SSID      "TJR_241"
#define OTA_TIMEOUT   300000UL          // 5-minute auto-exit
#define OTA_PART_SZ   (1900UL * 1024)  // approx OTA partition size for progress %

// Pointer — avoids running WebServer constructor before setup().
WebServer *g_httpServer = nullptr;

void enterOtaMode();
void exitOtaMode();


// ── Shared draw helpers ────────────────────────────────────────────────────

static void otaDrawButton(int16_t x, int16_t y, int16_t w, int16_t h,
                          uint16_t fill, uint16_t border,
                          const char *label, uint16_t textCol) {
    fbFillRoundRect(x, y, w, h, 8, fill);
    fbDrawRoundRect(x, y, w, h, 8, border);
    printCentredIn(label, x, y, w, h, textCol, FONT_LABEL);
}

static void drawOtaBar(int16_t x, int16_t y, int16_t w, int16_t h) {
    fbFillRoundRect(x, y, w, h, 4, C_DARK);
    if (g_otaSuccess) {
        fbFillRoundRect(x, y, w, h, 4, C_GREEN);
    } else if (g_otaFailed) {
        fbFillRoundRect(x, y, w, h, 4, C_DANGER);
    } else if (g_otaStarted && g_otaBytesWr > 0) {
        int16_t fill = (int16_t)((uint32_t)w * min(g_otaBytesWr, OTA_PART_SZ) / OTA_PART_SZ);
        if (fill > 0) fbFillRoundRect(x, y, fill, h, 4, C_WARN);
    }
    fbDrawRoundRect(x, y, w, h, 4, colDim());
}

static void drawOtaCountdown(int16_t x, int16_t y) {
    if (g_otaStarted && !g_otaSuccess && !g_otaFailed) {
        char pbuf[8];
        snprintf(pbuf, sizeof(pbuf), "%u%%",
                 (uint8_t)(min(g_otaBytesWr, OTA_PART_SZ) * 100 / OTA_PART_SZ));
        printLeft(pbuf, x, y, C_WARN, FONT_LABEL);
    } else if (!g_otaStarted) {
        int32_t rem = (int32_t)(OTA_TIMEOUT - (millis() - g_otaEnterTime)) / 1000;
        if (rem < 0) rem = 0;
        char tbuf[24];
        snprintf(tbuf, sizeof(tbuf), "Timeout: %ld:%02ld", (long)(rem/60), (long)(rem%60));
        printLeft(tbuf, x, y, colDim(), FONT_LABEL);
    }
}


// ── Confirmation dialog ────────────────────────────────────────────────────

void drawOtaConfirm() {
    int16_t sw = scrW(), sh = scrH();
    fbFill(C_BG);

    int16_t fhH = fontH(FONT_HDG);
    int16_t fhL = fontH(FONT_LABEL);
    int16_t hdrH = fhH + 20;
    printCentred("OTA UPDATE", (hdrH - fhH) / 2, C_WHITE, FONT_HDG);
    fbHLine(0, hdrH, sw, colDim());

    int16_t pitch = fhL + 10;
    int16_t y     = hdrH + 24;
    printCentred("Enter OTA mode?",   y,             C_WHITE,  FONT_LABEL);
    printCentred("WiFi will start,",  y + pitch,     colDim(), FONT_LABEL);
    printCentred("gauge will pause.", y + pitch * 2, colDim(), FONT_LABEL);

    int16_t PAD  = 16, GAP = 12, btnH = 90;
    int16_t btnW = (sw - PAD * 2 - GAP) / 2;
    int16_t btnY = sh - btnH - 24;
    int16_t yesX = PAD;
    int16_t noX  = PAD + btnW + GAP;

    otaDrawButton(yesX, btnY, btnW, btnH, 0x0640, 0x07E0, "YES", C_BG);
    otaDrawButton(noX,  btnY, btnW, btnH, C_DARK, colDim(), "NO", C_WHITE);

    g_otaZYes = { yesX, btnY, btnW, btnH };
    g_otaZNo  = { noX,  btnY, btnW, btnH };

    fbFlush();
}

void handleOtaConfirmTouch(int16_t tx, int16_t ty, bool touched) {
    if (touched) { trackTouchDown(tx, ty); return; }
    Gesture g = computeGesture();
    if (!g.fired || !g.isTap) return;
    if      (inZone(g.sx, g.sy, g_otaZYes)) enterOtaMode();
    else if (inZone(g.sx, g.sy, g_otaZNo))  g_otaConfirm = false;
}


// ── Active OTA screen ──────────────────────────────────────────────────────

void drawOta() {
    int16_t sw = scrW(), sh = scrH();
    fbFill(C_BG);

    int16_t fhH = fontH(FONT_HDG);
    int16_t fhL = fontH(FONT_LABEL);
    int16_t hdrH = fhH + 20;
    printCentred("OTA MODE", (hdrH - fhH) / 2, C_WHITE, FONT_HDG);
    fbHLine(0, hdrH, sw, colDim());

    int16_t pitch = fhL + 10;
    const char *statusStr = g_otaSuccess ? "Upload complete!" :
                            g_otaFailed  ? "Upload failed."   :
                            g_otaStarted ? "Uploading..."     : "Waiting...";
    uint16_t statusCol    = g_otaSuccess ? C_GREEN   :
                            g_otaFailed  ? C_DANGER  :
                            g_otaStarted ? C_WARN    : colDim();

    int16_t PAD  = 16;
    int16_t barH = 20;

    if (!g_isLandscape) {
        // Portrait — single column
        int16_t barW = sw - PAD * 2;
        int16_t btnH = 90;
        int16_t btnY = sh - btnH - 20;

        uint16_t infoCol = g_otaStarted ? colDim() : C_WHITE;
        int16_t y = hdrH + 20;
        printLeft("WiFi: " OTA_SSID,   PAD, y,          infoCol,   FONT_LABEL); y += pitch;
        printLeft("IP:   192.168.4.1", PAD, y,          infoCol,   FONT_LABEL); y += pitch + 6;
        printLeft(statusStr,           PAD, y,          statusCol, FONT_LABEL); y += pitch + 4;
        drawOtaBar(PAD, y, barW, barH); y += barH + 10;
        drawOtaCountdown(PAD, y);

        if (!g_otaStarted || g_otaFailed) {
            uint16_t border = g_otaFailed ? C_DANGER : colDim();
            otaDrawButton(PAD, btnY, barW, btnH, C_DARK, border, "EXIT", C_WHITE);
            g_otaZExit = { PAD, btnY, barW, btnH };
        } else {
            g_otaZExit = { 0, 0, 0, 0 };
        }

    } else {
        // Landscape — left info + right EXIT column
        int16_t GAP   = 12;
        int16_t exitW = 180;
        int16_t lW    = sw - PAD * 2 - GAP - exitW;
        int16_t exitX = PAD + lW + GAP;
        int16_t exitY = hdrH + PAD;
        int16_t exitH = sh - exitY - PAD;

        uint16_t infoCol = g_otaStarted ? colDim() : C_WHITE;
        int16_t y = hdrH + PAD;
        printLeft("WiFi: " OTA_SSID, PAD, y, infoCol,   FONT_LABEL); y += pitch;
        printLeft("IP: 192.168.4.1", PAD, y, infoCol,   FONT_LABEL); y += pitch + 8;
        printLeft(statusStr,         PAD, y, statusCol, FONT_LABEL); y += pitch;
        drawOtaBar(PAD, y, lW, barH); y += barH + 8;
        drawOtaCountdown(PAD, y);

        if (!g_otaStarted || g_otaFailed) {
            uint16_t border = g_otaFailed ? C_DANGER : colDim();
            otaDrawButton(exitX, exitY, exitW, exitH, C_DARK, border, "EXIT", C_WHITE);
            g_otaZExit = { exitX, exitY, exitW, exitH };
        } else {
            g_otaZExit = { 0, 0, 0, 0 };
        }
    }

    fbFlush();
}

void handleOtaModeTouch(int16_t tx, int16_t ty, bool touched) {
    if (g_otaStarted && !g_otaFailed) return;
    if (touched) { trackTouchDown(tx, ty); return; }
    Gesture g = computeGesture();
    if (!g.fired || !g.isTap) return;
    if (inZone(g.sx, g.sy, g_otaZExit)) exitOtaMode();
}


// ── Enter / exit / loop ────────────────────────────────────────────────────

void enterOtaMode() {
    g_otaConfirm   = false;
    g_otaMode      = true;
    g_otaStarted   = false;
    g_otaSuccess   = false;
    g_otaFailed    = false;
    g_otaBytesWr   = 0;
    g_otaEnterTime = millis();
    g_otaLastAct   = millis();

    WiFi.mode(WIFI_AP);
    WiFi.softAP(OTA_SSID);

    g_httpServer = new WebServer(80);

    g_httpServer->on("/", HTTP_GET, []() {
        g_otaLastAct = millis();
        g_httpServer->send(200, "text/html",
            "<!DOCTYPE html><html><head>"
            "<meta name='viewport' content='width=device-width,initial-scale=1'>"
            "<title>TJR-241 OTA</title>"
            "<style>"
              "body{font-family:monospace;background:#111;color:#eee;padding:24px}"
              "h2{color:#fff;margin-bottom:6px}"
              "p{color:#aaa;margin:0 0 16px}"
              "input[type=file]{display:block;margin:0 0 16px}"
              "input[type=submit]{background:#0c0;color:#000;border:none;"
                "padding:12px 28px;font-size:16px;font-family:monospace;cursor:pointer}"
            "</style></head><body>"
            "<h2>TJR-241 Firmware Update</h2>"
            "<p>Select a compiled .bin file and tap Upload &amp; Flash.</p>"
            "<form method='POST' action='/update' enctype='multipart/form-data'>"
              "<input type='file' name='firmware' accept='.bin'>"
              "<input type='submit' value='Upload &amp; Flash'>"
            "</form>"
            "</body></html>"
        );
    });

    g_httpServer->on("/update", HTTP_POST,
        []() {
            if (!Update.hasError()) {
                g_httpServer->send(200, "text/plain",
                    "Upload complete — gauge rebooting in 2 s.");
                g_otaSuccess = true;
            } else {
                g_httpServer->send(500, "text/plain", "Upload failed — check Serial log.");
                g_otaFailed = true;
            }
        },
        []() {
            HTTPUpload &up = g_httpServer->upload();
            if (up.status == UPLOAD_FILE_START) {
                g_otaStarted = true;
                g_otaBytesWr = 0;
                g_otaLastAct = millis();
                if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
                    Update.printError(Serial);
                    g_otaFailed = true;
                }
            } else if (up.status == UPLOAD_FILE_WRITE) {
                g_otaLastAct = millis();
                if (!g_otaFailed) {
                    if (Update.write(up.buf, up.currentSize) != up.currentSize) {
                        Update.printError(Serial);
                        g_otaFailed = true;
                    } else {
                        g_otaBytesWr += up.currentSize;
                    }
                }
            } else if (up.status == UPLOAD_FILE_END) {
                if (!g_otaFailed && !Update.end(true)) {
                    Update.printError(Serial);
                    g_otaFailed = true;
                }
            }
        }
    );

    g_httpServer->begin();
    Serial.println("OTA: AP started — SSID: " OTA_SSID);
    Serial.println("OTA: http://192.168.4.1/");
}

void exitOtaMode() {
    g_httpServer->stop();
    delete g_httpServer;
    g_httpServer = nullptr;
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
    g_otaMode    = false;
    g_otaConfirm = false;
    Serial.println("OTA: exited");
}

void handleOta() {
    if (g_httpServer) g_httpServer->handleClient();

    if (!g_otaStarted && (millis() - g_otaEnterTime > OTA_TIMEOUT)) {
        Serial.println("OTA: timeout — returning to gauge");
        exitOtaMode();
        return;
    }
    if (g_otaSuccess && (millis() - g_otaLastAct > 2000)) {
        Serial.println("OTA: rebooting");
        ESP.restart();
    }
}
