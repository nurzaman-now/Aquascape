#pragma once

#include "Config.h"
#include "Actuators.h"

// --- PEMBANTU FORMAT & PARSING WAKTU ---
String formatTime(int h, int m) {
  String s = "";
  if (h < 10) s += "0";
  s += String(h) + ":";
  if (m < 10) s += "0";
  s += String(m);
  return s;
}

bool parseHoursMinutes(String s, int &h, int &m) {
  s.trim();
  int colon = s.indexOf(':');
  if (colon == -1) {
    if (s.length() == 0) return false;
    for (unsigned int i = 0; i < s.length(); i++) {
      if (!isDigit(s.charAt(i))) return false;
    }
    int val = s.toInt();
    if (val >= 0 && val <= 23) {
      h = val;
      m = 0;
      return true;
    }
    return false;
  }

  String hStr = s.substring(0, colon);
  String mStr = s.substring(colon + 1);
  hStr.trim();
  mStr.trim();
  if (hStr.length() == 0 || mStr.length() == 0) return false;
  for (unsigned int i = 0; i < hStr.length(); i++) {
    if (!isDigit(hStr.charAt(i))) return false;
  }
  for (unsigned int i = 0; i < mStr.length(); i++) {
    if (!isDigit(mStr.charAt(i))) return false;
  }
  h = hStr.toInt();
  m = mStr.toInt();
  return (h >= 0 && h <= 23 && m >= 0 && m <= 59);
}

int calculateDurationMinutes(int onH, int onM, int offH, int offM) {
  int onTotal = onH * 60 + onM;
  int offTotal = offH * 60 + offM;
  if (onTotal == offTotal) return 0;
  return (offTotal > onTotal) ? (offTotal - onTotal) : (1440 - onTotal + offTotal);
}

bool isTimeInSession(int curMin, int onH, int onM, int offH, int offM) {
  int onTotal = onH * 60 + onM;
  int offTotal = offH * 60 + offM;
  if (onTotal == offTotal) return false;
  return (onTotal < offTotal) ? (curMin >= onTotal && curMin < offTotal)
                              : (curMin >= onTotal || curMin < offTotal);
}

// --- LOGIKA PERGANTIAN HARI & RESET HARIAN ---
void checkDayChange(struct tm &timeinfo) {
  if (timeinfo.tm_year < 120) return; // Abaikan bila waktu belum sinkron NTP (epoch 1970)
  if (lastFeedDay != timeinfo.tm_mday) {
    lastFeedDay = timeinfo.tm_mday;
    feedCountToday = 0;
    lastFedTime = "--:--";
    lampOnMinutesToday = 0;
    manualLampConfirmed = false;
    lastScheduledState = false;
    waitingColorSelection = false;
    waitingPortionSelection = false;
    lampWizState = LAMP_WIZ_IDLE;
    feedWizState = FEED_WIZ_IDLE;
    pref.putInt("f_day", lastFeedDay);
    pref.putInt("f_cnt", feedCountToday);
    pref.putString("last_fed", lastFedTime);
    pref.putInt("l_min_tdy", 0);
    Serial.printf("[Info] Date changed to %d: Feeding quota & lamp duration reset.\n", lastFeedDay);
  }
}

// --- LOGIKA JADWAL LAMPU ---
String formatLampSessionList() {
  String resp = "";
  for (int i = 0; i < sessionCount; i++) {
    int d = calculateDurationMinutes(lampSessions[i].onH, lampSessions[i].onM, lampSessions[i].offH, lampSessions[i].offM);
    resp += "  • " + formatTime(lampSessions[i].onH, lampSessions[i].onM) + " - " + formatTime(lampSessions[i].offH, lampSessions[i].offM) + " (" + String(d / 60) + " jam" + (d % 60 > 0 ? (" " + String(d % 60) + " m") : "") + ")\n";
  }
  return resp;
}

int calculateTotalLampDuration() {
  int total = 0;
  for (int i = 0; i < sessionCount; i++) {
    total += calculateDurationMinutes(lampSessions[i].onH, lampSessions[i].onM, lampSessions[i].offH, lampSessions[i].offM);
  }
  return total;
}

bool applyLampSchedule(String input, String &err) {
  LampSession tempSessions[MAX_SESSIONS];
  int count = 0, totalMinutes = 0;
  int startIdx = 0, len = input.length();

  while (startIdx < len) {
    int commaIdx = input.indexOf(',', startIdx);
    if (commaIdx == -1) commaIdx = input.indexOf(';', startIdx);
    if (commaIdx == -1) commaIdx = len;

    String token = input.substring(startIdx, commaIdx);
    token.trim();
    startIdx = commaIdx + 1;
    if (token.length() == 0) continue;

    if (count >= MAX_SESSIONS) {
      err = "⚠️ Maksimal hanya " + String(MAX_SESSIONS) + " sesi jadwal yang didukung.";
      return false;
    }

    int dashIdx = token.indexOf('-');
    if (dashIdx == -1) {
      err = "⚠️ Format salah pada sesi '" + token + "'. Gunakan format HH:MM-HH:MM.";
      return false;
    }

    int oH, oM, fH, fM;
    if (!parseHoursMinutes(token.substring(0, dashIdx), oH, oM) || 
        !parseHoursMinutes(token.substring(dashIdx + 1), fH, fM)) {
      err = "⚠️ Format jam tidak valid pada sesi '" + token + "'. Cth: 09:00-13:00.";
      return false;
    }

    int dur = calculateDurationMinutes(oH, oM, fH, fM);
    if (dur == 0) {
      err = "⚠️ Jam hidup dan mati tidak boleh sama pada sesi '" + token + "'.";
      return false;
    }

    for (int i = 0; i < count; i++) {
      if (isTimeInSession(oH * 60 + oM, tempSessions[i].onH, tempSessions[i].onM, tempSessions[i].offH, tempSessions[i].offM) ||
          isTimeInSession(tempSessions[i].onH * 60 + tempSessions[i].onM, oH, oM, fH, fM)) {
        err = "⚠️ Sesi '" + token + "' bertumpuk/tumpang tindih dengan sesi lain.";
        return false;
      }
    }

    tempSessions[count++] = {oH, oM, fH, fM};
    totalMinutes += dur;
  }

  if (count == 0) {
    err = "⚠️ Tidak ada sesi jadwal yang valid ditemukan.";
    return false;
  }

  if (totalMinutes > MAX_LAMP_MINUTES_PER_DAY) {
    err = "⚠️ *Total Durasi Melebihi Batas!*\nTotal waktu nyala: *" + String(totalMinutes / 60) + " jam " + String(totalMinutes % 60) + " menit*.\nMaksimal total nyala lampu adalah *8 jam sehari* (480 menit).";
    return false;
  }

  sessionCount = count;
  for (int i = 0; i < count; i++) lampSessions[i] = tempSessions[i];
  savedLampScheduleStr = input;
  pref.putString("lamp_sched", savedLampScheduleStr);
  return true;
}

bool shouldLampBeOn(int currentMinute) {
  if (lampOnMinutesToday >= MAX_LAMP_MINUTES_PER_DAY) return false;

  for (int i = 0; i < sessionCount; i++) {
    if (isTimeInSession(currentMinute, lampSessions[i].onH, lampSessions[i].onM, lampSessions[i].offH, lampSessions[i].offM)) {
      return true;
    }
  }
  return false;
}

// --- LOGIKA JADWAL PAKAN IKAN ---
bool applyFeedSchedule(String input, String &err) {
  FeedTime tempFeed[MAX_FEED_SESSIONS];
  int count = 0;
  int startIdx = 0, len = input.length();

  while (startIdx < len) {
    int commaIdx = input.indexOf(',', startIdx);
    if (commaIdx == -1) commaIdx = input.indexOf(';', startIdx);
    if (commaIdx == -1) commaIdx = len;

    String token = input.substring(startIdx, commaIdx);
    token.trim();
    startIdx = commaIdx + 1;
    if (token.length() == 0) continue;

    if (count >= MAX_FEED_SESSIONS) {
      err = "⚠️ Maksimal hanya " + String(MAX_FEED_SESSIONS) + " jadwal pakan per hari yang didukung.";
      return false;
    }

    int h, m;
    if (!parseHoursMinutes(token, h, m)) {
      err = "⚠️ Format jam salah pada '" + token + "'. Gunakan format HH:MM (contoh: 07:00, 16:00).";
      return false;
    }

    for (int i = 0; i < count; i++) {
      if (tempFeed[i].h == h && tempFeed[i].m == m) {
        err = "⚠️ Jam pakan '" + token + "' terdaftar dua kali.";
        return false;
      }
    }

    tempFeed[count++] = {h, m};
  }

  if (count == 0) {
    err = "⚠️ Tidak ada jadwal pakan yang valid ditemukan.";
    return false;
  }

  // Urutkan jadwal pakan dari jam paling awal
  for (int i = 0; i < count - 1; i++) {
    for (int j = i + 1; j < count; j++) {
      if ((tempFeed[i].h * 60 + tempFeed[i].m) > (tempFeed[j].h * 60 + tempFeed[j].m)) {
        FeedTime tmp = tempFeed[i];
        tempFeed[i] = tempFeed[j];
        tempFeed[j] = tmp;
      }
    }
  }

  feedSessionCount = count;
  maxFeedPerDay = feedSessionCount;
  for (int i = 0; i < count; i++) feedSessions[i] = tempFeed[i];

  savedFeedScheduleStr = input;
  pref.putString("feed_sched", savedFeedScheduleStr);
  pref.putInt("f_max", maxFeedPerDay);
  return true;
}

String formatFeedScheduleList() {
  String resp = "";
  for (int i = 0; i < feedSessionCount; i++) {
    if (i > 0) resp += ", ";
    resp += "`" + formatTime(feedSessions[i].h, feedSessions[i].m) + "`";
  }
  return resp;
}

bool isCurrentTimeFeedSchedule(int curH, int curM) {
  for (int i = 0; i < feedSessionCount; i++) {
    if (feedSessions[i].h == curH && feedSessions[i].m == curM) {
      return true;
    }
  }
  return false;
}

bool processFeeding(String &errorMessage) {
  struct tm timeinfo;
  if (getLocalTime(&timeinfo, 100)) {
    checkDayChange(timeinfo);
  }

  if (feedCountToday >= maxFeedPerDay) {
    errorMessage = "⚠️ *Pemberian Pakan Dibatalkan!*\nJatah pakan hari ini sudah mencapai batas maksimal (" + String(maxFeedPerDay) + " kali sehari) agar ikan tetap sehat.";
    return false;
  }

  feedFish();
  feedCountToday++;
  pref.putInt("f_cnt", feedCountToday);

  if (getLocalTime(&timeinfo, 100)) {
    lastFedTime = formatTime(timeinfo.tm_hour, timeinfo.tm_min);
    pref.putString("last_fed", lastFedTime);
  }

  return true;
}

String getNextFeedingSchedule(int currentHour, int currentMinute) {
  if (feedSessionCount == 0) return "-";
  int curMin = currentHour * 60 + currentMinute;

  for (int i = 0; i < feedSessionCount; i++) {
    int fMin = feedSessions[i].h * 60 + feedSessions[i].m;
    if (fMin > curMin) {
      return formatTime(feedSessions[i].h, feedSessions[i].m) + " WIB";
    }
  }

  return formatTime(feedSessions[0].h, feedSessions[0].m) + " WIB (besok)";
}
