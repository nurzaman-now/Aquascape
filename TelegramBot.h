#pragma once

#include "Config.h"
#include "Actuators.h"
#include "Scheduler.h"

// Forward declaration
void sendHelpMenu(String chatId);

// --- KONFIRMASI INTERAKTIF LAMPU ---
void askLampConfirmation(String chatId, String reason, int targetMode) {
  pendingLampMode = targetMode;
  waitingLampConfirmation = true;
  lampConfirmationTime = millis();
  String confirmMsg = "💡 *Konfirmasi Lampu*\n";
  confirmMsg += reason + "\n\n";
  confirmMsg += "Apakah ingin tetap menyalakan lampu?";
  bot.sendMessageWithReplyKeyboard(chatId, confirmMsg, "Markdown", "[[\"/ya_lampu\", \"/batal\"]]", true, false, false);
}

void askLampOffConfirmation(String chatId) {
  waitingLampOffConfirmation = true;
  lampOffConfirmationTime = millis();
  String confirmMsg = "💡 *Konfirmasi Matikan Lampu*\n";
  confirmMsg += "Saat ini sedang dalam jam jadwal lampu menyala.\n\n";
  confirmMsg += "Apakah lampu ingin dimatikan sekarang? (Lampu akan tetap padam sampai jadwal berikutnya tiba).";
  bot.sendMessageWithReplyKeyboard(chatId, confirmMsg, "Markdown", "[[\"/ya_lampu_off\", \"/batal\"]]", true, false, false);
}

void executeSelectLampColor(String chatId, int mode) {
  if (mode < 1 || mode > 3) {
    bot.sendMessage(chatId, "⚠️ Pilihan warna tidak valid. Silakan pilih 1, 2, atau 3.", "");
    sendHelpMenu(chatId);
    return;
  }

  struct tm timeinfo;
  bool hasTime = getLocalTime(&timeinfo, 100);
  int curMin = hasTime ? (timeinfo.tm_hour * 60 + timeinfo.tm_min) : -1;
  bool inSchedule = (curMin >= 0) ? shouldLampBeOn(curMin) : false;
  bool overLimit = (lampOnMinutesToday >= MAX_LAMP_MINUTES_PER_DAY && !manualLampConfirmed);

  if (lampState) {
    currentLampMode = mode;
    pref.putInt("lamp_mode", currentLampMode);
    setLampColor(mode);
    bot.sendMessage(chatId, "🎨 Warna lampu diubah ke: *Mode " + String(mode) + "*", "Markdown");
    sendHelpMenu(chatId);
  } else if (!inSchedule || overLimit) {
    String reason = "";
    if (!inSchedule && overLimit) {
      reason = "Saat ini *di luar jadwal* dan durasi lampu telah mencapai batas 8 jam.\nIngin menyalakan lampu dengan *Warna " + String(mode) + "*?";
    } else if (!inSchedule) {
      reason = "Saat ini *di luar jadwal* dan lampu sedang padam.\nIngin menyalakan lampu dengan *Warna " + String(mode) + "*?";
    } else {
      reason = "Lampu sudah menyala selama *" + String(lampOnMinutesToday / 60) + " jam " + String(lampOnMinutesToday % 60) + " menit* hari ini.\nIngin menyalakan lampu dengan *Warna " + String(mode) + "*?";
    }
    askLampConfirmation(chatId, reason, mode);
  } else {
    currentLampMode = mode;
    pref.putInt("lamp_mode", currentLampMode);
    turnLampOnManual(mode);
    bot.sendMessage(chatId, "💡 Lampu dinyalakan dengan *Mode Warna " + String(mode) + "*", "Markdown");
    sendHelpMenu(chatId);
  }
}

// --- KEYBOARD TOMBOL PILIHAN JAM 24 JAM ---
const char HOUR_KEYBOARD_JSON[] = "["
  "[\"00:00\", \"01:00\", \"02:00\", \"03:00\"],"
  "[\"04:00\", \"05:00\", \"06:00\", \"07:00\"],"
  "[\"08:00\", \"09:00\", \"10:00\", \"11:00\"],"
  "[\"12:00\", \"13:00\", \"14:00\", \"15:00\"],"
  "[\"16:00\", \"17:00\", \"18:00\", \"19:00\"],"
  "[\"20:00\", \"21:00\", \"22:00\", \"23:00\"],"
  "[\"/batal\"]"
"]";

// --- WIZARD JADWAL LAMPU INTERAKTIF ---
void finalizeLampSchedule(String chatId) {
  if (tempLampCount == 0) {
    bot.sendMessage(chatId, "⚠️ Tidak ada sesi jadwal lampu yang disimpan.", "");
    lampWizState = LAMP_WIZ_IDLE;
    sendHelpMenu(chatId);
    return;
  }

  // Urutkan sesi secara kronologis
  for (int i = 0; i < tempLampCount - 1; i++) {
    for (int j = i + 1; j < tempLampCount; j++) {
      if ((tempLampSessions[i].onH * 60 + tempLampSessions[i].onM) > (tempLampSessions[j].onH * 60 + tempLampSessions[j].onM)) {
        LampSession tmp = tempLampSessions[i];
        tempLampSessions[i] = tempLampSessions[j];
        tempLampSessions[j] = tmp;
      }
    }
  }

  sessionCount = tempLampCount;
  for (int i = 0; i < sessionCount; i++) {
    lampSessions[i] = tempLampSessions[i];
  }

  savedLampScheduleStr = "";
  for (int i = 0; i < sessionCount; i++) {
    if (i > 0) savedLampScheduleStr += ", ";
    savedLampScheduleStr += formatTime(lampSessions[i].onH, lampSessions[i].onM) + "-" + formatTime(lampSessions[i].offH, lampSessions[i].offM);
  }
  pref.putString("lamp_sched", savedLampScheduleStr);
  lastScheduledState = false;
  lampWizState = LAMP_WIZ_IDLE;

  int totalPlanMinutes = calculateTotalLampDuration();
  String resp = "✅ *Jadwal Lampu Berhasil Disimpan!*\n\n";
  resp += formatLampSessionList();
  resp += "\n⏱️ *Total Nyala:* " + String(totalPlanMinutes / 60) + " jam " + String(totalPlanMinutes % 60) + " menit per hari (Maks. 8 jam).";
  bot.sendMessage(chatId, resp, "Markdown");
  sendHelpMenu(chatId);
}

void startLampScheduleWizard(String chatId) {
  feedWizState = FEED_WIZ_IDLE;
  waitingColorSelection = false;
  waitingPortionSelection = false;
  waitingFeedConfirmation = false;
  waitingLampConfirmation = false;
  waitingLampOffConfirmation = false;

  tempLampCount = 0;
  lampWizState = LAMP_WIZ_WAIT_START_TIME;
  lampWizTime = millis();

  String msg = "💡 *Pengaturan Jadwal Lampu*\n\n";
  msg += "Pilih jam nyala (jam awal) untuk *Sesi 1*:\n";
  msg += "_(Pilih jam di bawah ini atau ketik manual)_";
  bot.sendMessageWithReplyKeyboard(chatId, msg, "Markdown", HOUR_KEYBOARD_JSON, true, false, false);
}

void handleLampWizardInput(String chatId, String text) {
  if (millis() - lampWizTime > 120000) {
    lampWizState = LAMP_WIZ_IDLE;
    bot.sendMessage(chatId, "⚠️ Waktu pengaturan habis (melebihi 2 menit).", "");
    sendHelpMenu(chatId);
    return;
  }

  lampWizTime = millis();
  text.trim();

  if (lampWizState == LAMP_WIZ_WAIT_START_TIME) {
    int h, m;
    if (!parseHoursMinutes(text, h, m)) {
      bot.sendMessageWithReplyKeyboard(chatId, "⚠️ Jam tidak valid. Silakan pilih jam di bawah ini:", "Markdown", HOUR_KEYBOARD_JSON, true, false, false);
      return;
    }

    tempLampStartH = h;
    tempLampStartM = m;
    lampWizState = LAMP_WIZ_WAIT_END_TIME;

    String msg = "Jam awal Sesi " + String(tempLampCount + 1) + ": `" + formatTime(tempLampStartH, tempLampStartM) + " WIB`\n\n";
    msg += "Sekarang pilih jam padam (jam akhir) untuk *Sesi " + String(tempLampCount + 1) + "*:\n";
    msg += "_(Pilih jam di bawah ini atau ketik manual)_";
    bot.sendMessageWithReplyKeyboard(chatId, msg, "Markdown", HOUR_KEYBOARD_JSON, true, false, false);

  } else if (lampWizState == LAMP_WIZ_WAIT_END_TIME) {
    int fH, fM;
    if (!parseHoursMinutes(text, fH, fM)) {
      bot.sendMessageWithReplyKeyboard(chatId, "⚠️ Jam tidak valid. Silakan pilih jam di bawah ini:", "Markdown", HOUR_KEYBOARD_JSON, true, false, false);
      return;
    }

    int oH = tempLampStartH;
    int oM = tempLampStartM;
    int dur = calculateDurationMinutes(oH, oM, fH, fM);
    if (dur <= 0) {
      bot.sendMessageWithReplyKeyboard(chatId, "⚠️ Jam padam tidak boleh sama dengan atau lebih awal dari jam nyala.\nSilakan pilih jam padam kembali:", "Markdown", HOUR_KEYBOARD_JSON, true, false, false);
      return;
    }

    // Cek tumpang tindih dengan sesi yang sudah disimpan
    for (int i = 0; i < tempLampCount; i++) {
      if (isTimeInSession(oH * 60 + oM, tempLampSessions[i].onH, tempLampSessions[i].onM, tempLampSessions[i].offH, tempLampSessions[i].offM) ||
          isTimeInSession(tempLampSessions[i].onH * 60 + tempLampSessions[i].onM, oH, oM, fH, fM)) {
        bot.sendMessageWithReplyKeyboard(chatId, "⚠️ Jam ini tumpang tindih dengan sesi yang sudah ada.\nSilakan pilih jam padam lain:", "Markdown", HOUR_KEYBOARD_JSON, true, false, false);
        return;
      }
    }

    // Cek total durasi
    int currentTotal = 0;
    for (int i = 0; i < tempLampCount; i++) {
      currentTotal += calculateDurationMinutes(tempLampSessions[i].onH, tempLampSessions[i].onM, tempLampSessions[i].offH, tempLampSessions[i].offM);
    }
    if (currentTotal + dur > MAX_LAMP_MINUTES_PER_DAY) {
      int sisa = MAX_LAMP_MINUTES_PER_DAY - currentTotal;
      String err = "⚠️ Total durasi lampu melebihi batas 8 jam sehari.\n";
      err += "Sisa durasi tersedia: " + String(sisa / 60) + " jam " + String(sisa % 60) + " menit.\n";
      err += "Silakan pilih jam padam yang durasinya lebih pendek:";
      bot.sendMessageWithReplyKeyboard(chatId, err, "Markdown", HOUR_KEYBOARD_JSON, true, false, false);
      return;
    }

    tempLampSessions[tempLampCount++] = {oH, oM, fH, fM};
    currentTotal += dur;

    if (tempLampCount >= MAX_SESSIONS || currentTotal >= MAX_LAMP_MINUTES_PER_DAY) {
      finalizeLampSchedule(chatId);
    } else {
      lampWizState = LAMP_WIZ_WAIT_CONFIRM_NEXT;
      String conf = "✅ Sesi " + String(tempLampCount) + " tersimpan: `" + formatTime(oH, oM) + " - " + formatTime(fH, fM) + "`\n";
      conf += "⏱️ Total waktu nyala saat ini: " + String(currentTotal / 60) + " jam " + String(currentTotal % 60) + " menit (Maks. 8 jam)\n\n";
      conf += "Apakah ada sesi berikutnya?";
      String kb = "[[\"Ya\", \"Tidak\"], [\"/batal\"]]";
      bot.sendMessageWithReplyKeyboard(chatId, conf, "Markdown", kb, true, false, false);
    }

  } else if (lampWizState == LAMP_WIZ_WAIT_CONFIRM_NEXT) {
    if (text.equalsIgnoreCase("Ya") || text.equalsIgnoreCase("/ya")) {
      lampWizState = LAMP_WIZ_WAIT_START_TIME;
      String msg = "Pilih jam nyala (jam awal) untuk *Sesi " + String(tempLampCount + 1) + "*:\n";
      msg += "_(Pilih jam di bawah ini atau ketik manual)_";
      bot.sendMessageWithReplyKeyboard(chatId, msg, "Markdown", HOUR_KEYBOARD_JSON, true, false, false);
    } else if (text.equalsIgnoreCase("Tidak") || text.equalsIgnoreCase("/tidak")) {
      finalizeLampSchedule(chatId);
    } else {
      bot.sendMessage(chatId, "Silakan pilih *Ya* atau *Tidak* di bawah ini:", "Markdown");
    }
  }
}

// --- WIZARD JADWAL PAKAN INTERAKTIF ---
void finalizeFeedSchedule(String chatId) {
  if (tempFeedCount == 0) {
    bot.sendMessage(chatId, "⚠️ Tidak ada jadwal pakan yang disimpan.", "");
    feedWizState = FEED_WIZ_IDLE;
    sendHelpMenu(chatId);
    return;
  }

  // Urutkan jadwal pakan secara kronologis
  for (int i = 0; i < tempFeedCount - 1; i++) {
    for (int j = i + 1; j < tempFeedCount; j++) {
      if ((tempFeedSessions[i].h * 60 + tempFeedSessions[i].m) > (tempFeedSessions[j].h * 60 + tempFeedSessions[j].m)) {
        FeedTime tmp = tempFeedSessions[i];
        tempFeedSessions[i] = tempFeedSessions[j];
        tempFeedSessions[j] = tmp;
      }
    }
  }

  feedSessionCount = tempFeedCount;
  maxFeedPerDay = feedSessionCount;
  for (int i = 0; i < feedSessionCount; i++) {
    feedSessions[i] = tempFeedSessions[i];
  }

  savedFeedScheduleStr = "";
  for (int i = 0; i < feedSessionCount; i++) {
    if (i > 0) savedFeedScheduleStr += ", ";
    savedFeedScheduleStr += formatTime(feedSessions[i].h, feedSessions[i].m);
  }
  pref.putString("feed_sched", savedFeedScheduleStr);
  pref.putInt("f_max", maxFeedPerDay);
  feedWizState = FEED_WIZ_IDLE;

  String resp = "✅ *Jadwal Pakan Berhasil Disimpan!*\n\n";
  resp += "• Jam Makan: " + formatFeedScheduleList() + "\n";
  resp += "• Batas Harian: *" + String(maxFeedPerDay) + " kali sehari*";
  bot.sendMessage(chatId, resp, "Markdown");
  sendHelpMenu(chatId);
}

void startFeedScheduleWizard(String chatId) {
  lampWizState = LAMP_WIZ_IDLE;
  waitingColorSelection = false;
  waitingPortionSelection = false;
  waitingFeedConfirmation = false;
  waitingLampConfirmation = false;
  waitingLampOffConfirmation = false;

  tempFeedCount = 0;
  feedWizState = FEED_WIZ_WAIT_TIME;
  feedWizTime = millis();

  String msg = "🍽️ *Pengaturan Jadwal Pakan*\n\n";
  msg += "Pilih jam makan ikan yang *pertama*:\n";
  msg += "_(Pilih jam di bawah ini atau ketik manual)_";
  bot.sendMessageWithReplyKeyboard(chatId, msg, "Markdown", HOUR_KEYBOARD_JSON, true, false, false);
}

void handleFeedWizardInput(String chatId, String text) {
  if (millis() - feedWizTime > 120000) {
    feedWizState = FEED_WIZ_IDLE;
    bot.sendMessage(chatId, "⚠️ Waktu pengaturan habis (melebihi 2 menit).", "");
    sendHelpMenu(chatId);
    return;
  }

  feedWizTime = millis();
  text.trim();

  if (feedWizState == FEED_WIZ_WAIT_TIME) {
    int h, m;
    if (!parseHoursMinutes(text, h, m)) {
      bot.sendMessageWithReplyKeyboard(chatId, "⚠️ Jam tidak valid. Silakan pilih jam di bawah ini:", "Markdown", HOUR_KEYBOARD_JSON, true, false, false);
      return;
    }

    for (int i = 0; i < tempFeedCount; i++) {
      if (tempFeedSessions[i].h == h && tempFeedSessions[i].m == m) {
        bot.sendMessageWithReplyKeyboard(chatId, "⚠️ Jam " + formatTime(h, m) + " sudah ada di daftar. Silakan pilih jam lain:", "Markdown", HOUR_KEYBOARD_JSON, true, false, false);
        return;
      }
    }

    tempFeedSessions[tempFeedCount++] = {h, m};

    if (tempFeedCount >= MAX_FEED_SESSIONS) {
      finalizeFeedSchedule(chatId);
    } else {
      feedWizState = FEED_WIZ_WAIT_CONFIRM_NEXT;
      String conf = "✅ Jam makan ke-" + String(tempFeedCount) + " tersimpan: `" + formatTime(h, m) + " WIB`\n\n";
      conf += "Apakah ada waktu makan berikutnya?";
      String kb = "[[\"Ya\", \"Tidak\"], [\"/batal\"]]";
      bot.sendMessageWithReplyKeyboard(chatId, conf, "Markdown", kb, true, false, false);
    }
  } else if (feedWizState == FEED_WIZ_WAIT_CONFIRM_NEXT) {
    if (text.equalsIgnoreCase("Ya") || text.equalsIgnoreCase("/ya")) {
      feedWizState = FEED_WIZ_WAIT_TIME;
      String msg = "Pilih jam makan ikan ke-" + String(tempFeedCount + 1) + ":\n";
      msg += "_(Pilih jam di bawah ini atau ketik manual)_";
      bot.sendMessageWithReplyKeyboard(chatId, msg, "Markdown", HOUR_KEYBOARD_JSON, true, false, false);
    } else if (text.equalsIgnoreCase("Tidak") || text.equalsIgnoreCase("/tidak")) {
      finalizeFeedSchedule(chatId);
    } else {
      bot.sendMessage(chatId, "Silakan pilih *Ya* atau *Tidak* di bawah ini:", "Markdown");
    }
  }
}

// --- MENU & TELEGRAM MESSAGE HANDLER ---
void sendHelpMenu(String chatId) {
  String msg = "🌿 *Menu Kontrol Akuarium*\n\n";
  msg += "• `/status` - Lihat kondisi akuarium saat ini\n";
  msg += "• `/pakan` - Beri makan ikan sekarang\n";
  msg += "• `/porsi` - Atur porsi pakan (sesuaikan jumlah ikan)\n";
  msg += "• `/buih [0-100]` - Atur kekuatan gelembung buih\n";
  msg += "• `/buih_on` / `/buih_off` - Buka penuh (100%) / Matikan buih\n";
  msg += "• `/lampu_on` / `/lampu_off` - Nyalakan / matikan lampu\n";
  msg += "• `/warna` - Pilih warna lampu (1, 2, atau 3)\n\n";
  msg += "🗓️ *Pengaturan Jadwal:*\n";
  msg += "• `/set_lampu` - Atur jadwal nyala lampu\n";
  msg += "• `/set_pakan` - Atur jadwal makan ikan\n";
  msg += "• `/reset_pakan` - Reset hitungan pakan hari ini\n";

  String keyboardJson = "["
                        "[\"/pakan\", \"/status\"],"
                        "[\"/buih_on\", \"/buih_off\"],"
                        "[\"/lampu_on\", \"/lampu_off\"],"
                        "[\"/warna\", \"/porsi\"],"
                        "[\"/set_lampu\", \"/set_pakan\"]"
                        "]";
  bot.sendMessageWithReplyKeyboard(chatId, msg, "Markdown", keyboardJson, true, false, false);
}

void handleTelegramMessages(int totalMessages) {
  for (int i = 0; i < totalMessages; i++) {
    String chatId = String(bot.messages[i].chat_id);
    String text = bot.messages[i].text;

    Serial.println("Incoming message from ID " + chatId + ": " + text);

    if (chatId != CHAT_ID) continue;

    if (text == "/help" || text == "/start") {
      waitingFeedConfirmation = false;
      waitingLampConfirmation = false;
      waitingLampOffConfirmation = false;
      waitingColorSelection = false;
      waitingPortionSelection = false;
      lampWizState = LAMP_WIZ_IDLE;
      feedWizState = FEED_WIZ_IDLE;
      sendHelpMenu(chatId);
      continue;
    }

    if (text == "/batal") {
      waitingFeedConfirmation = false;
      waitingLampConfirmation = false;
      waitingLampOffConfirmation = false;
      waitingColorSelection = false;
      waitingPortionSelection = false;
      lampWizState = LAMP_WIZ_IDLE;
      feedWizState = FEED_WIZ_IDLE;
      bot.sendMessage(chatId, "❌ Tindakan dibatalkan.", "");
      sendHelpMenu(chatId);
      continue;
    }

    // Jika sedang dalam Wizard Jadwal Lampu
    if (lampWizState != LAMP_WIZ_IDLE) {
      handleLampWizardInput(chatId, text);
      continue;
    }

    // Jika sedang dalam Wizard Jadwal Pakan
    if (feedWizState != FEED_WIZ_IDLE) {
      handleFeedWizardInput(chatId, text);
      continue;
    }

    if (text.startsWith("/buih")) {
      int lvl = (text == "/buih_on") ? 100 : (text == "/buih_off") ? 0 : text.substring(6).toInt();
      setBubbleLevel(lvl);
      bot.sendMessage(chatId, "🫧 Gelembung buih diatur ke: *" + String(bubbleLevel) + "%*", "Markdown");
    } else if (text == "/lampu_on") {
      waitingColorSelection = false;
      waitingPortionSelection = false;
      waitingFeedConfirmation = false;
      waitingLampOffConfirmation = false;
      struct tm timeinfo;
      bool hasTime = getLocalTime(&timeinfo, 100);
      int curMin = hasTime ? (timeinfo.tm_hour * 60 + timeinfo.tm_min) : -1;
      bool inSchedule = (curMin >= 0) ? shouldLampBeOn(curMin) : false;
      bool overLimit = (lampOnMinutesToday >= MAX_LAMP_MINUTES_PER_DAY && !manualLampConfirmed);

      if (lampState) {
        bot.sendMessage(chatId, "💡 Lampu sudah menyala.", "");
      } else if (!inSchedule || overLimit) {
        String reason = "";
        if (!inSchedule && overLimit) {
          reason = "Saat ini *di luar jadwal* dan lampu sudah menyala 8 jam hari ini.";
        } else if (!inSchedule) {
          reason = "Saat ini *di luar jadwal* lampu menyala.";
        } else {
          reason = "Lampu sudah menyala selama *" + String(lampOnMinutesToday / 60) + " jam " + String(lampOnMinutesToday % 60) + " menit* hari ini (maks. 8 jam).";
        }
        askLampConfirmation(chatId, reason, currentLampMode);
      } else {
        turnLampOnManual(currentLampMode);
        bot.sendMessage(chatId, "💡 Lampu dinyalakan (Warna " + String(currentLampMode) + ").", "");
      }
    } else if (text == "/lampu_off") {
      waitingColorSelection = false;
      waitingPortionSelection = false;
      waitingFeedConfirmation = false;
      waitingLampConfirmation = false;
      struct tm timeinfo;
      bool hasTime = getLocalTime(&timeinfo, 100);
      int curMin = hasTime ? (timeinfo.tm_hour * 60 + timeinfo.tm_min) : -1;
      bool inSchedule = (curMin >= 0) ? shouldLampBeOn(curMin) : false;

      if (!lampState) {
        bot.sendMessage(chatId, "🌑 Lampu sudah padam.", "");
      } else if (inSchedule) {
        askLampOffConfirmation(chatId);
      } else {
        turnLampOffManual();
        bot.sendMessage(chatId, "🌑 Lampu dimatikan.", "");
      }
    } else if (text == "/warna") {
      waitingPortionSelection = false;
      waitingFeedConfirmation = false;
      waitingLampConfirmation = false;
      waitingLampOffConfirmation = false;
      waitingColorSelection = true;
      colorSelectionTime = millis();
      String colorMsg = "🎨 *Pilih Mode Warna Lampu*\n\n";
      colorMsg += "Silakan pilih warna lampu:\n";
      colorMsg += "• *1* - Mode Warna 1\n";
      colorMsg += "• *2* - Mode Warna 2\n";
      colorMsg += "• *3* - Mode Warna 3\n\n";
      colorMsg += "Pilih angka di bawah ini:";
      String colorKeyboard = "[[\"1\", \"2\", \"3\"], [\"/batal\"]]";
      bot.sendMessageWithReplyKeyboard(chatId, colorMsg, "Markdown", colorKeyboard, true, false, false);
    } else if (waitingColorSelection && (text == "1" || text == "2" || text == "3")) {
      if (millis() - colorSelectionTime > 120000) {
        waitingColorSelection = false;
        bot.sendMessage(chatId, "⚠️ Waktu konfirmasi habis. Silakan pilih menu kembali.", "");
        sendHelpMenu(chatId);
      } else {
        waitingColorSelection = false;
        executeSelectLampColor(chatId, text.toInt());
      }
    } else if (text.startsWith("/warna ") || text == "/warna1" || text == "/warna2" || text == "/warna3") {
      waitingColorSelection = false;
      int mode = 0;
      if (text.startsWith("/warna ")) {
        mode = text.substring(7).toInt();
      } else {
        mode = text.substring(6).toInt();
      }
      executeSelectLampColor(chatId, mode);
    } else if (text == "/porsi") {
      waitingColorSelection = false;
      waitingFeedConfirmation = false;
      waitingLampConfirmation = false;
      waitingLampOffConfirmation = false;
      waitingPortionSelection = true;
      portionSelectionTime = millis();
      String portionMsg = "🍽️ *Pilih Porsi Pakan Ikan*\n\n";
      portionMsg += "Sesuaikan dengan jumlah ikan di akuarium:\n";
      portionMsg += "• *1* - Sedikit (1-2 ekor)\n";
      portionMsg += "• *2* - Sedang (3-5 ekor)\n";
      portionMsg += "• *3* - Standar (> 5 ekor)\n\n";
      portionMsg += "Porsi aktif saat ini: *" + getPortionName(feedPortionMode) + "*\n";
      portionMsg += "Silakan pilih angka porsi:";
      String portionKeyboard = "[[\"1\", \"2\", \"3\"], [\"/batal\"]]";
      bot.sendMessageWithReplyKeyboard(chatId, portionMsg, "Markdown", portionKeyboard, true, false, false);
    } else if (waitingPortionSelection && (text == "1" || text == "2" || text == "3")) {
      if (millis() - portionSelectionTime > 120000) {
        waitingPortionSelection = false;
        bot.sendMessage(chatId, "⚠️ Waktu konfirmasi habis. Silakan pilih menu kembali.", "");
        sendHelpMenu(chatId);
      } else {
        waitingPortionSelection = false;
        int mode = text.toInt();
        setFeedPortion(mode);
        bot.sendMessage(chatId, "✅ Porsi pakan diatur ke: *" + getPortionName(feedPortionMode) + "*", "Markdown");
        sendHelpMenu(chatId);
      }
    } else if (text.startsWith("/porsi ")) {
      waitingPortionSelection = false;
      int mode = text.substring(7).toInt();
      if (mode >= 1 && mode <= 3) {
        setFeedPortion(mode);
        bot.sendMessage(chatId, "✅ Porsi pakan diatur ke: *" + getPortionName(feedPortionMode) + "*", "Markdown");
      } else {
        bot.sendMessage(chatId, "⚠️ Pilihan porsi tidak valid. Pilih 1 (Sedikit), 2 (Sedang), atau 3 (Standar).", "");
      }
      sendHelpMenu(chatId);
    } else if (text == "/pakan") {
      waitingColorSelection = false;
      waitingPortionSelection = false;
      waitingLampConfirmation = false;
      waitingLampOffConfirmation = false;
      struct tm timeinfo;
      if (getLocalTime(&timeinfo, 100)) {
        checkDayChange(timeinfo);
      }

      if (feedCountToday >= maxFeedPerDay) {
        bot.sendMessage(chatId, "⚠️ *Jatah Makan Hari Ini Sudah Habis*\nIkan sudah diberi makan " + String(maxFeedPerDay) + " kali hari ini agar tetap sehat dan air tidak cepat keruh.", "Markdown");
      } else {
        String lastFedInfo = "Ikan belum diberi makan hari ini";
        String nextFeedInfo = (feedSessionCount > 0) ? (formatTime(feedSessions[0].h, feedSessions[0].m) + " WIB") : "-";

        if (getLocalTime(&timeinfo, 100)) {
          if (feedCountToday > 0 && lastFedTime != "--:--") {
            lastFedInfo = "Ikan sudah diberi makan jam " + lastFedTime + " WIB";
          }
          nextFeedInfo = getNextFeedingSchedule(timeinfo.tm_hour, timeinfo.tm_min);
        }

        String confirmMsg = "🍽️ *Beri Makan Ikan*\n";
        confirmMsg += lastFedInfo + " (jadwal berikutnya jam " + nextFeedInfo + ").\n\n";
        confirmMsg += "Apakah ingin memberi makan ikan sekarang? (Batas harian: " + String(maxFeedPerDay) + " kali).";

        waitingFeedConfirmation = true;
        feedConfirmationTime = millis();

        String confirmKeyboard = "[[\"/ya_pakan\", \"/batal\"]]";
        bot.sendMessageWithReplyKeyboard(chatId, confirmMsg, "Markdown", confirmKeyboard, true, false, false);
      }
    } else if (text == "/ya_pakan") {
      if (!waitingFeedConfirmation || (millis() - feedConfirmationTime > 120000)) {
        waitingFeedConfirmation = false;
        bot.sendMessage(chatId, "⚠️ Waktu konfirmasi habis. Silakan ketik /pakan kembali.", "");
        sendHelpMenu(chatId);
      } else {
        waitingFeedConfirmation = false;
        String errFeed = "";
        if (processFeeding(errFeed)) {
          bot.sendMessage(chatId, "🐟 Nyam! Pakan berhasil diberikan (ke-" + String(feedCountToday) + " dari batas " + String(maxFeedPerDay) + " kali hari ini).", "Markdown");
        } else {
          bot.sendMessage(chatId, errFeed, "Markdown");
        }
        sendHelpMenu(chatId);
      }
    } else if (text == "/ya_lampu") {
      if (!waitingLampConfirmation || (millis() - lampConfirmationTime > 120000)) {
        waitingLampConfirmation = false;
        bot.sendMessage(chatId, "⚠️ Waktu konfirmasi habis. Silakan ulangi perintah.", "");
      } else {
        waitingLampConfirmation = false;
        manualLampConfirmed = true;
        currentLampMode = pendingLampMode;
        pref.putInt("lamp_mode", currentLampMode);
        turnLampOnManual(currentLampMode);
        bot.sendMessage(chatId, "💡 Lampu dinyalakan (Warna " + String(currentLampMode) + ").", "");
      }
      sendHelpMenu(chatId);
    } else if (text == "/ya_lampu_off") {
      if (!waitingLampOffConfirmation || (millis() - lampOffConfirmationTime > 120000)) {
        waitingLampOffConfirmation = false;
        bot.sendMessage(chatId, "⚠️ Waktu konfirmasi habis. Silakan ulangi perintah.", "");
      } else {
        waitingLampOffConfirmation = false;
        turnLampOffManual();
        bot.sendMessage(chatId, "🌑 Lampu dimatikan (akan menyala kembali saat jadwal berikutnya tiba).", "");
      }
      sendHelpMenu(chatId);
    } else if (text == "/reset_pakan") {
      feedCountToday = 0;
      lastFedTime = "--:--";
      pref.putInt("f_cnt", 0);
      pref.putString("last_fed", lastFedTime);
      bot.sendMessage(chatId, "🔄 Hitungan pakan hari ini telah di-reset ke 0 kali.", "");
    } else if (text == "/status") {
      struct tm timeinfo;
      String curTime = "--:--";
      bool inSched = false;
      if (getLocalTime(&timeinfo, 100)) {
        checkDayChange(timeinfo);
        curTime = formatTime(timeinfo.tm_hour, timeinfo.tm_min);
        inSched = shouldLampBeOn(timeinfo.tm_hour * 60 + timeinfo.tm_min);
      }

      String statusMsg = "🌿 *Kondisi Akuarium*\n\n";
      statusMsg += "🕒 Waktu: `" + curTime + " WIB`\n";
      if (lampState) {
        statusMsg += "💡 Lampu: *Menyala* (Mode Warna " + String(currentLampMode) + (inSched ? ", Sesuai Jadwal" : ", Manual") + ")\n";
      } else {
        statusMsg += "💡 Lampu: *Padam*\n";
      }
      statusMsg += "🫧 Buih Filter: *" + String(bubbleLevel) + "%*\n\n";

      statusMsg += "🍽️ *Pemberian Pakan:*\n";
      statusMsg += "• Hari ini: " + String(feedCountToday) + " dari " + String(maxFeedPerDay) + " kali";
      if (lastFedTime != "--:--") {
        statusMsg += " (Terakhir: `" + lastFedTime + " WIB`)\n";
      } else {
        statusMsg += " (Belum makan)\n";
      }
      statusMsg += "• Porsi: *" + getPortionName(feedPortionMode) + "*\n";
      statusMsg += "• Jadwal: " + formatFeedScheduleList() + "\n\n";

      statusMsg += "💡 *Jadwal Lampu:*\n";
      statusMsg += formatLampSessionList();
      statusMsg += "• Durasi Lampu Hari Ini: " + String(lampOnMinutesToday / 60) + " jam " + String(lampOnMinutesToday % 60) + " menit (Maks. 8 jam)\n";

      bot.sendMessage(chatId, statusMsg, "Markdown");
    } else if (text.startsWith("/set_lampu")) {
      startLampScheduleWizard(chatId);
    } else if (text.startsWith("/set_pakan")) {
      startFeedScheduleWizard(chatId);
    } else {
      bot.sendMessage(chatId, "❓ Perintah belum dipahami. Ketik /help untuk melihat menu.", "");
    }
  }
}

void initTelegramConnection() {
  Serial.println("\nWiFi Connected! Local IP: " + WiFi.localIP().toString());
  WiFi.config(WiFi.localIP(), WiFi.gatewayIP(), WiFi.subnetMask(), IPAddress(8, 8, 8, 8), IPAddress(1, 1, 1, 1));

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  secured_client.setInsecure();
  secured_client.setTimeout(3);

  String startMsg = "🌿 *Sistem Akuarium Aktif*\n\n";
  startMsg += "Semua sistem berjalan normal.\n";
  startMsg += "Ketik /help untuk membuka menu.";

  bot.sendMessage(CHAT_ID, startMsg, "Markdown");
  ntpConfigured = true;
}
