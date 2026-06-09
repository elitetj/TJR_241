# Flashing TJR_241 with Arduino IDE

## 1. Install the ESP32 board package

1. Open Arduino IDE → **Preferences**
2. Add this URL to *Additional boards manager URLs*:
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
3. Go to **Tools → Board → Boards Manager**, search `esp32`, install **esp32 by Espressif Systems**

---

## 2. Select the board

**Tools → Board → esp32 → Waveshare ESP32-S3-Touch-AMOLED-2.41**

---

## 3. Set board options

Under the **Tools** menu, set these exactly:

| Setting | Value |
|---------|-------|
| PSRAM | **Enabled** |
| Partition Scheme | **Minimal SPIFFS** |
| USB CDC On Boot | **Enabled** |

> PSRAM **must** be enabled — the framebuffers live in PSRAM and the firmware will crash without it.

---

## 4. Connect the board

Plug in via USB. The port will appear as something like:

- macOS: `/dev/cu.usbmodem101`
- Windows: `COM3` (or similar)

Select it under **Tools → Port**.

---

## 5. Open the sketch

**File → Open** → navigate to `TJR_241/TJR_241.ino`

---

## 6. Upload

Click the **Upload** button (→) or press `Ctrl+U` / `Cmd+U`.

The IDE will compile then flash. You'll see progress in the output panel and the board will reboot automatically when done.
