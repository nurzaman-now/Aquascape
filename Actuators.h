#pragma once

#include "Config.h"

// --- NATIVE SERVO HARDWARE PWM (ESP32 CORE V2 & V3 COMPATIBLE) ---
void setupServoPin(int pin, int channel) {
#if defined(ESP_ARDUINO_VERSION) && ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
  ledcAttach(pin, SERVO_FREQ, SERVO_RES);
#else
  ledcSetup(channel, SERVO_FREQ, SERVO_RES);
  ledcAttachPin(pin, channel);
#endif
}

void writeServoPulse(int pin, int channel, uint32_t duty) {
#if defined(ESP_ARDUINO_VERSION) && ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
  ledcWrite(pin, duty);
#else
  ledcWrite(channel, duty);
#endif
}

void moveServo(int pin, int channel, int angle, int waitMs = 0, bool disablePulse = false) {
  angle = constrain(angle, 0, 180);
  // Konversi sudut (0-180) ke 14-bit duty cycle (500-2400 us dalam periode 20000 us / 50Hz)
  uint32_t duty = (map(angle, 0, 180, 500, 2400) * 16384) / 20000;
  Serial.printf("[Servo] Pin %d (Ch %d) -> Angle: %d | Duty: %u\n", pin, channel, angle, duty);
  writeServoPulse(pin, channel, duty);
  if (waitMs > 0) delay(waitMs);
  if (disablePulse) writeServoPulse(pin, channel, 0);
}

// --- PENGATUR ALIRAN BUIH FILTER ---
void setBubbleLevel(int percentage) {
  percentage = constrain(percentage, 0, 100);
  int angle = map(percentage, 0, 100, 0, 180);
  Serial.printf("Moving Servo 2 (Bubble) -> Level: %d%% (Angle: %d deg)\n", percentage, angle);
  moveServo(SERVO2_PIN, 1, angle, 600, false);
  bubbleLevel = percentage;
  pref.putInt("buih_lvl", bubbleLevel);
}

// --- PENGATUR PORSI & PEMBERIAN PAKAN ---
String getPortionName(int mode) {
  if (mode == 1) return "Sedikit (1-2 ekor)";
  if (mode == 2) return "Sedang (3-5 ekor)";
  return "Standar (> 5 ekor)";
}

void setFeedPortion(int mode) {
  feedPortionMode = mode;
  if (feedPortionMode == 1) {
    feedOpenDurationMs = 180;
  } else if (feedPortionMode == 2) {
    feedOpenDurationMs = 350;
  } else {
    feedPortionMode = 3;
    feedOpenDurationMs = 500;
  }
  pref.putInt("f_por", feedPortionMode);
}

void feedFish() {
  Serial.printf("Activating Servo 1 (Feed) - Duration: %d ms...\n", feedOpenDurationMs);
  moveServo(SERVO1_PIN, 0, 90, feedOpenDurationMs, false);
  moveServo(SERVO1_PIN, 0, 0, 400, true);
  Serial.println("Servo 1 (Feed) complete.");
}

// --- TRANSISI WARNA & DAYA LAMPU (NON-BLOCKING MC-5) ---
void updateLampTransition() {
  if (!lampTransition.active || millis() < lampTransition.nextStepTime) return;

  switch (lampTransition.step) {
    case 0: // Reset phase: matikan daya relai selama 6.5 detik
      digitalWrite(RELAY_PIN, RELAY_OFF);
      lampTransition.nextStepTime = millis() + 6500;
      lampTransition.step = 1;
      break;
    case 1: // Nyalakan kembali daya ke Mode 1
      digitalWrite(RELAY_PIN, RELAY_ON);
      lampTransition.pulsesRemaining = (lampTransition.targetMode == 2) ? 1 : (lampTransition.targetMode == 3) ? 2 : 0;
      if (lampTransition.pulsesRemaining > 0) {
        lampTransition.nextStepTime = millis() + 600;
        lampTransition.step = 2;
      } else {
        lampTransition.active = false;
      }
      break;
    case 2: // Pulsa OFF
      digitalWrite(RELAY_PIN, RELAY_OFF);
      lampTransition.nextStepTime = millis() + 350;
      lampTransition.step = 3;
      break;
    case 3: // Pulsa ON
      digitalWrite(RELAY_PIN, RELAY_ON);
      lampTransition.pulsesRemaining--;
      if (lampTransition.pulsesRemaining > 0) {
        lampTransition.nextStepTime = millis() + 500;
        lampTransition.step = 2;
      } else {
        lampTransition.active = false;
      }
      break;
  }
}

void setLampColor(int mode) {
  if (mode < 1 || mode > 3) return;
  currentLampMode = mode;
  lampState = true;
  pref.putInt("lamp_mode", currentLampMode);
  pref.putBool("lamp_st", lampState);

  lampTransition.active = true;
  lampTransition.targetMode = mode;
  lampTransition.step = 0;
  lampTransition.nextStepTime = 0;
}

void turnLampOnManual(int mode) {
  if (lampOnMinutesToday >= MAX_LAMP_MINUTES_PER_DAY) manualLampConfirmed = true;
  setLampColor(mode);
}

void turnLampOffManual() {
  lampTransition.active = false;
  manualLampConfirmed = false;
  lampState = false;
  pref.putInt("l_min_tdy", lampOnMinutesToday);
  pref.putBool("lamp_st", false);
  digitalWrite(RELAY_PIN, RELAY_OFF);
}
