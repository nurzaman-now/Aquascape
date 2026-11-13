#include "Config.h"
#include "Actuators.h"
#include "Scheduler.h"
#include "WiFiPortal.h"
#include "TelegramBot.h"

// --- PENGUJIAN MANUAL VIA SERIAL MONITOR ---
void handleSerialCommands() {
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if (cmd.length() == 0) return;

    Serial.println("\n[Console] Perintah diterima: '" + cmd + "'");

    if (cmd.equalsIgnoreCase("pakan")) {
      String errFeed = "";
      if (processFeeding(errFeed)) {
        Serial.printf("[Console] Pakan berhasil diberikan (%d/%d hari ini).\n", feedCountToday, maxFeedPerDay);
      } else {
        Serial.printf("[Console] Pakan dibatalkan: Batas harian terpenuhi (%d kali sehari)!\n", maxFeedPerDay);
      }
    } else if (cmd.equalsIgnoreCase("reset_pakan")) {
      feedCountToday = 0;
      pref.putInt("f_cnt", 0);
      Serial.println("[Console] Hitungan pakan hari ini di-reset ke 0.");
    } else if (cmd.startsWith("buih ") || cmd.startsWith("BUIH ")) {
      int lvl = cmd.substring(5).toInt();
      setBubbleLevel(lvl);
    } else if (cmd.equalsIgnoreCase("lampu on")) {
      turnLampOnManual(currentLampMode);
    } else if (cmd.equalsIgnoreCase("lampu off")) {
      turnLampOffManual();
      Serial.println("[Console] Lampu dimatikan.");
    } else if (cmd.equalsIgnoreCase("help")) {
      Serial.println("Daftar perintah: 'pakan', 'reset_pakan', 'buih [0-100]', 'lampu on', 'lampu off'");
    } else {
      Serial.println("Perintah tidak dikenal. Ketik 'help' untuk daftar perintah.");
    }
  }
}

// --- SETUP UTAMA ---
void setup() {
  Serial.begin(115200);

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, RELAY_OFF);

  pref.begin("aquarium", false);
  savedLampScheduleStr = pref.getString("lamp_sched", "09:00-13:00, 17:00-21:00");
  String dummyErr = "";
  applyLampSchedule(savedLampScheduleStr, dummyErr);
  lampOnMinutesToday = pref.getInt("l_min_tdy", 0);
  savedFeedScheduleStr = pref.getString("feed_sched", "08:00, 16:00");
  String dummyFeedErr = "";
  applyFeedSchedule(savedFeedScheduleStr, dummyFeedErr);
  maxFeedPerDay      = pref.getInt("f_max", feedSessionCount);
  feedCountToday     = pref.getInt("f_cnt", 0);
  lastFeedDay        = pref.getInt("f_day", -1);
  lastFedTime        = pref.getString("last_fed", "--:--");
  feedPortionMode    = pref.getInt("f_por", 3);
  setFeedPortion(feedPortionMode);
  currentLampMode    = pref.getInt("lamp_mode", 1);
  bubbleLevel        = pref.getInt("buih_lvl", 100);
  lampState          = false;
  lastScheduledState = false;

  // Inisialisasi Hardware PWM Servo
  setupServoPin(SERVO1_PIN, 0);
  setupServoPin(SERVO2_PIN, 1);

  // Pastikan katup servo pakan tertutup rapat (0 derajat) saat awal booting
  moveServo(SERVO1_PIN, 0, 0, 100, true);

  // Pulihkan posisi katup buih filter sesuai persentase tersimpan
  Serial.println("--- Memulihkan Posisi Katup Buih Filter ---");
  setBubbleLevel(bubbleLevel);

  // Inisialisasi Mode Access Point & Captive Portal
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(AP_SSID, AP_PASS);
  initWiFiPortal();

  savedSSID = pref.getString("wifi_ssid", WIFI_SSID);
  savedPass = pref.getString("wifi_pass", WIFI_PASS);

  if (savedSSID.length() > 0) {
    Serial.println("Menghubungkan ke WiFi: " + savedSSID);
    WiFi.setAutoReconnect(true);
    WiFi.persistent(true);
    WiFi.begin(savedSSID.c_str(), savedPass.c_str());

    unsigned long startAttemptTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 10000) {
      delay(500);
      Serial.print(".");
    }
  }

  if (WiFi.status() == WL_CONNECTED) {
    initTelegramConnection();
  } else {
    Serial.println("\nWiFi belum tersambung (kemungkinan router masih proses booting).");
    Serial.println("Koneksi otomatis di latar belakang tetap aktif mencari WiFi '" + savedSSID + "'.");
    lastWifiCheck = millis();
  }
}

// --- LOOP UTAMA ---
void loop() {
  handleSerialCommands();
  server.handleClient();
  updateLampTransition();

  bool isConnected = (WiFi.status() == WL_CONNECTED);

  // Rekoneksi berkala jika WiFi belum tersambung
  if (!isConnected) {
    if (savedSSID.length() > 0 && millis() - lastWifiCheck > 15000) {
      lastWifiCheck = millis();
      Serial.println("[WiFi] Menghubungkan kembali secara otomatis ke " + savedSSID + "...");
      WiFi.disconnect(false);
      delay(50);
      WiFi.begin(savedSSID.c_str(), savedPass.c_str());
    }
  } else {
    if (!ntpConfigured) {
      initTelegramConnection();
    }

    if (millis() - lastBotCheck > botInterval) {
      int totalMessages = bot.getUpdates(bot.last_message_received + 1);
      while (totalMessages) {
        handleTelegramMessages(totalMessages);
        totalMessages = bot.getUpdates(bot.last_message_received + 1);
      }
      lastBotCheck = millis();
    }
  }

  // Waktu & Sinkronisasi RTC
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 100)) return;
  checkDayChange(timeinfo);

  int currentMinute = timeinfo.tm_hour * 60 + timeinfo.tm_min;

  // Akumulasi Durasi Nyala Lampu Harian (Proteksi Maks. 8 Jam)
  if (lampState) {
    if (lastLampCountMinute != timeinfo.tm_min) {
      lastLampCountMinute = timeinfo.tm_min;
      lampOnMinutesToday++;

      if (lampOnMinutesToday >= MAX_LAMP_MINUTES_PER_DAY && !manualLampConfirmed) {
        turnLampOffManual();
        if (isConnected) {
          bot.sendMessage(CHAT_ID, "💡 *Pengingat Lampu:*\nLampu otomatis dipadamkan karena sudah menyala 8 jam hari ini agar tanaman tetap sehat dan lumut tidak berlebih.\n\nJika ingin menyalakan kembali, ketik /lampu_on.", "Markdown");
        }
      }
    }
  } else {
    lastLampCountMinute = -1;
  }

  // Kontrol Lampu Otomatis Sesuai Jadwal
  bool scheduledOn = shouldLampBeOn(currentMinute);

  if (scheduledOn && !lastScheduledState) {
    lastScheduledState = true;
    if (!lampState && lampOnMinutesToday < MAX_LAMP_MINUTES_PER_DAY) {
      setLampColor(currentLampMode);
      if (isConnected) {
        bot.sendMessage(CHAT_ID, "💡 Lampu akuarium telah menyala sesuai jadwal (Warna " + String(currentLampMode) + ").", "Markdown");
      }
    }
  } else if (!scheduledOn && lastScheduledState) {
    lastScheduledState = false;
    if (lampState) {
      turnLampOffManual();
      if (isConnected) {
        bot.sendMessage(CHAT_ID, "🌑 Lampu akuarium telah padam sesuai jadwal.", "Markdown");
      }
    }
  }

  // Pemberian Pakan Ikan Otomatis Sesuai Jadwal
  bool isFeedTime = isCurrentTimeFeedSchedule(timeinfo.tm_hour, timeinfo.tm_min);

  if (isFeedTime && lastFedMinute != timeinfo.tm_min) {
    lastFedMinute = timeinfo.tm_min;
    String errFeed = "";
    if (processFeeding(errFeed)) {
      if (isConnected) {
        bot.sendMessage(CHAT_ID, "🐟 Ikan sudah diberi makan sesuai jadwal (" + String(feedCountToday) + "/" + String(maxFeedPerDay) + " kali hari ini).", "Markdown");
      }
    } else {
      if (isConnected) {
        bot.sendMessage(CHAT_ID, "🔔 Jadwal pakan dilewati karena batas makan hari ini sudah terpenuhi.", "Markdown");
      }
    }
  } else if (!isFeedTime) {
    lastFedMinute = -1;
  }
}
