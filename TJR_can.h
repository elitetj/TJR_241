#pragma once
/*
 * TJR_can.h — CAN bus task (Haltech ECU Broadcast decode).
 *
 * Runs on core 0 via xTaskCreatePinnedToCore.  Decodes Haltech broadcast
 * frames and writes live values to g_val[] (declared in TJR_params.h).
 *
 * HOW TO ACTIVATE:
 *   1.  In setup(), uncomment: xTaskCreatePinnedToCore(canTask, "can", 3072, NULL, 5, NULL, 0);
 *   2.  In loop(), remove the updateSimData() call.
 *   3.  Compile and flash.  g_canActive goes true once frames arrive.
 *
 * PIN ASSIGNMENTS (SN65HVD230 transceiver on TJR_241):
 *   CAN TX = GPIO 1  (CAN_TX, defined in TJR_241.ino PINS & DEFINES)
 *   CAN RX = GPIO 8  (CAN_RX)
 *   NOTE: GPIO 16/17 are used for battery control on the 2.41" board — do NOT use.
 *
 * CAN SPEED:
 *   Default Haltech Broadcast: 1 Mbit/s.
 *   To use 500 kbit/s instead, change TWAI_TIMING_CONFIG_1MBITS() below.
 *
 * DEPENDENCY:
 *   TJR_params.h must be included before this file in TJR_241.ino so that
 *   NUM_SIGNALS, PARAMS[], g_val[], and ParamDef are in scope.
 */

#include <driver/twai.h>   // Espressif TWAI (CAN) peripheral driver


// Set true by canTask() when valid frames are arriving.
volatile bool g_canActive = false;

// Raw 16-bit DTC code from frame 0x6F3 bytes 6-7 (Engine Protection Reason).
// OBD-II encoding: upper 2 bits = P/B/C/U, lower 14 = code suffix.
volatile uint16_t g_engineProtReason = 0;


// ── decodeFrame ───────────────────────────────────────────────────────────────
// Called from canTask() for each received message.
// Iterates PARAMS[], finds any signal whose canID matches msg.identifier,
// and writes the decoded native-unit value to g_val[i].
static void decodeFrame(const twai_message_t &msg) {
  if (msg.identifier == 0x6F3 && msg.data_length_code >= 8) {
    g_engineProtReason = ((uint16_t)msg.data[6] << 8) | msg.data[7];
  }

  for (int i = 0; i < NUM_SIGNALS; i++) {
    const ParamDef &p = PARAMS[i];
    if (p.canID != msg.identifier)               continue;
    if (p.endByte >= msg.data_length_code)        continue;

    float raw;
    if (p.isBitField) {
      raw = (float)(msg.data[p.startByte] & p.bitMask) / p.divisor;
    } else {
      int      nbytes = p.endByte - p.startByte + 1;
      uint32_t u      = 0;
      for (int b = p.startByte; b <= p.endByte; b++) u = (u << 8) | msg.data[b];

      if (p.isSigned) {
        uint32_t signBit = 1UL << (nbytes * 8 - 1);
        int32_t  s       = (int32_t)((u ^ signBit) - signBit);
        raw = (float)s / p.divisor - p.offset;
      } else {
        raw = (float)u / p.divisor - p.offset;
      }
    }
    g_val[i] = raw;
  }
}


// ── canTask ───────────────────────────────────────────────────────────────────
// Pinned to core 0 by xTaskCreatePinnedToCore in setup().
void canTask(void *pvParameters) {
  twai_general_config_t g_cfg = TWAI_GENERAL_CONFIG_DEFAULT(
      (gpio_num_t)CAN_TX, (gpio_num_t)CAN_RX, TWAI_MODE_NORMAL);
  twai_timing_config_t  t_cfg = TWAI_TIMING_CONFIG_1MBITS();
  twai_filter_config_t  f_cfg = TWAI_FILTER_CONFIG_ACCEPT_ALL();

  if (twai_driver_install(&g_cfg, &t_cfg, &f_cfg) != ESP_OK) {
    Serial.println("CAN: driver install failed — check GPIO wiring");
    vTaskDelete(NULL);
    return;
  }
  if (twai_start() != ESP_OK) {
    Serial.println("CAN: start failed");
    twai_driver_uninstall();
    vTaskDelete(NULL);
    return;
  }
  Serial.println("CAN: started @ 1 Mbit/s");

  const uint32_t CAN_INACTIVE_MS = 1000;
  uint32_t lastFrameMs = millis();

  twai_message_t msg;
  for (;;) {
    if (twai_receive(&msg, pdMS_TO_TICKS(100)) == ESP_OK) {
      lastFrameMs  = millis();
      g_canActive  = true;
      decodeFrame(msg);
    } else if (millis() - lastFrameMs > CAN_INACTIVE_MS) {
      g_canActive = false;
    }
  }
}
