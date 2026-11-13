#include <WiFi.h>
#include <WebServer.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include "time.h"
#include <esp_arduino_version.h>

// --- KREDENSIAL & RAHASIA (SECRETS) ---
#if __has_include("secrets.h")
  #include "secrets.h"
#else
  #define AP_SSID    "Akuarium-Nurz"
  #define AP_PASS    "12345678"
  #define WIFI_SSID  ""
  #define WIFI_PASS  ""
  #define BOT_TOKEN  "YOUR_TELEGRAM_BOT_TOKEN"
  #define CHAT_ID    "YOUR_TELEGRAM_CHAT_ID"
#endif

// --- KONFIGURASI WAKTU (WIB = UTC+7) ---
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 25200;
const int daylightOffset_sec = 0;

// --- PINOUT HARDWARE ---
#define RELAY_PIN  4  // GPIO 4 -> Saklar AC Relay Lampu
#define SERVO1_PIN 5  // GPIO 5 -> Servo 1 (Pakan)
#define SERVO2_PIN 6  // GPIO 6 -> Servo 2 (Buih Filter)

#define RELAY_ON  LOW
#define RELAY_OFF HIGH

// Konfigurasi Hardware PWM (LEDC) untuk Servo (50Hz, 14-bit)
#define SERVO_FREQ 50
#define SERVO_RES  14

WiFiClientSecure secured_client;
UniversalTelegramBot bot(BOT_TOKEN, secured_client);
Preferences pref;
WebServer server(80);

// Variabel Jadwal & Status
// Struktur & Variabel Jadwal Lampu Dinamis (Maksimal total 8 jam sehari = 480 menit)
struct LampSession {
  int onH;
  int onM;
  int offH;
  int offM;
};

const int MAX_SESSIONS = 5;
const int MAX_LAMP_MINUTES_PER_DAY = 480; // 8 jam
LampSession lampSessions[MAX_SESSIONS];
int sessionCount = 0;
String savedLampScheduleStr = "09:00-13:00, 17:00-21:00";
int lampOnMinutesToday = 0;
int lastLampCountMinute = -1;
struct FeedTime {
  int h;
  int m;
};

const int MAX_FEED_SESSIONS = 5;
FeedTime feedSessions[MAX_FEED_SESSIONS];
int feedSessionCount = 2;
int maxFeedPerDay = 2;
String savedFeedScheduleStr = "08:00, 16:00";

int currentLampMode = 1;
bool lampState = false;
bool lastScheduledState = false;
int pendingLampMode = 1;
int bubbleLevel = 100;

int lastFedMinute = -1;
int feedCountToday = 0;
int lastFeedDay = -1;
String lastFedTime = "--:--";
bool waitingFeedConfirmation = false;
unsigned long feedConfirmationTime = 0;
bool waitingLampConfirmation = false;
unsigned long lampConfirmationTime = 0;
bool waitingLampOffConfirmation = false;
unsigned long lampOffConfirmationTime = 0;
bool manualLampConfirmed = false;
unsigned long lastBotCheck = 0;
const unsigned long botInterval = 1500;
unsigned long lastWifiCheck = 0;
bool ntpConfigured = false;

String savedSSID = "";
String savedPass = "";

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
  // Convert angle (0-180) to 14-bit duty (500-2400 us over 20000 us / 50Hz)
  uint32_t duty = (map(angle, 0, 180, 500, 2400) * 16384) / 20000;
  Serial.printf("[Servo] Pin %d (Ch %d) -> Angle: %d | Duty: %u\n", pin, channel, angle, duty);
  writeServoPulse(pin, channel, duty);
  if (waitMs > 0) delay(waitMs);
  if (disablePulse) writeServoPulse(pin, channel, 0);
}

// --- HALAMAN HTML SETUP WIFI ---
const char SETUP_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Setup WiFi Akuarium</title>
  <style>
    body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; background: #f0f2f5; text-align: center; padding: 15px; margin: 0; }
    .card { background: white; max-width: 420px; margin: 20px auto; padding: 25px; border-radius: 12px; box-shadow: 0 4px 12px rgba(0,0,0,0.1); }
    h2 { color: #333; margin-bottom: 8px; margin-top: 0; }
    p { color: #666; font-size: 14px; margin-bottom: 15px; }
    .status-box { background: #e8f0fe; color: #1a73e8; padding: 10px; border-radius: 8px; font-size: 13px; margin-bottom: 15px; font-weight: 600; word-break: break-word; }
    .wifi-item { background: #f8f9fa; border: 1px solid #e1e4e8; padding: 12px 15px; margin: 8px 0; border-radius: 8px; display: flex; justify-content: space-between; align-items: center; cursor: pointer; transition: 0.2s; }
    .wifi-item:hover { background: #e8f0fe; border-color: #4285f4; }
    .wifi-name { font-weight: 600; color: #333; font-size: 15px; text-align: left; word-break: break-all; padding-right: 10px; }
    .wifi-signal { font-size: 12px; color: #555; white-space: nowrap; }
    .btn-scan { background: #28a745; color: white; border: none; padding: 12px 15px; width: 100%; border-radius: 8px; font-size: 15px; font-weight: 600; cursor: pointer; margin-bottom: 10px; }
    .btn-scan:hover { background: #218838; }
    .btn-manual { background: #6c757d; color: white; border: none; padding: 10px 15px; width: 100%; border-radius: 8px; font-size: 14px; font-weight: 600; cursor: pointer; margin-top: 10px; }
    
    .modal { display: none; position: fixed; z-index: 10; left: 0; top: 0; width: 100%; height: 100%; background: rgba(0,0,0,0.5); justify-content: center; align-items: center; }
    .modal-content { background: white; padding: 25px; border-radius: 12px; width: 85%; max-width: 320px; text-align: left; box-shadow: 0 4px 15px rgba(0,0,0,0.2); }
    .modal-content h3 { margin-top: 0; color: #333; font-size: 18px; word-break: break-word; }
    .modal-content input { width: 100%; padding: 10px; margin: 8px 0 14px 0; border: 1px solid #ccc; border-radius: 6px; box-sizing: border-box; font-size: 15px; }
    .modal-buttons { display: flex; gap: 10px; justify-content: flex-end; margin-top: 5px; }
    .btn-cancel { background: #f1f3f4; color: #333; border: none; padding: 8px 15px; border-radius: 6px; cursor: pointer; font-weight: 600; }
    .btn-connect { background: #007bff; color: white; border: none; padding: 8px 18px; border-radius: 6px; cursor: pointer; font-weight: 600; }
    .loading { font-size: 14px; color: #666; margin: 15px 0; }
  </style>
</head>
<body>
  <div class="card">
    <h2>Setup WiFi Akuarium</h2>
    <div id="current-status" class="status-box">Memuat status koneksi...</div>
    <p>Pilih jaringan WiFi 2.4 GHz dari daftar di bawah ini.</p>
    
    <button class="btn-scan" onclick="scanWifi()">Scan Ulang WiFi</button>
    
    <div id="wifi-list">
      <div class="loading">Memindai jaringan sekitar...</div>
    </div>

    <button class="btn-manual" onclick="openManualModal()">Input Nama WiFi Manual</button>
  </div>

  <div id="wifiModal" class="modal">
    <div class="modal-content">
      <h3 id="modalTitle">Koneksi WiFi</h3>
      <form action="/save" method="POST" accept-charset="UTF-8">
        <div id="ssidInputGroup" style="display:none;">
          <label style="font-size: 13px; color: #555;">Nama WiFi (SSID):</label>
          <input type="text" id="modalSsid" name="ssid" placeholder="Ketik nama WiFi...">
        </div>
        <label style="font-size: 13px; color: #555;">Password WiFi:</label>
        <input type="password" id="modalPass" name="pass" placeholder="Masukkan password...">
        <div class="modal-buttons">
          <button type="button" class="btn-cancel" onclick="closeModal()">Batal</button>
          <button type="submit" class="btn-connect">Simpan</button>
        </div>
      </form>
    </div>
  </div>

  <script>
    let wifiList = [];

    function loadInfo() {
      fetch('/info')
        .then(res => res.json())
        .then(info => {
          let st = document.getElementById('current-status');
          if(info.connected) {
            st.innerText = 'Terhubung ke: ' + info.ssid + ' (' + info.ip + ')';
            st.style.background = '#e6f4ea';
            st.style.color = '#137333';
          } else {
            st.innerText = 'Status: Belum terhubung ke WiFi rumah';
            st.style.background = '#fce8e6';
            st.style.color = '#c5221f';
          }
        });
    }

    function scanWifi() {
      let listDiv = document.getElementById('wifi-list');
      listDiv.innerHTML = '<div class="loading">Memindai jaringan sekitar (tunggu 3-5 detik)...</div>';
      fetch('/scan')
        .then(response => response.json())
        .then(data => {
          wifiList = data;
          listDiv.innerHTML = '';
          if(data.length === 0) {
            listDiv.innerHTML = '<p>Tidak ada WiFi 2.4GHz terdeteksi. Coba klik Scan Ulang.</p>';
            return;
          }
          data.forEach((net, idx) => {
            let item = document.createElement('div');
            item.className = 'wifi-item';
            
            let nameSpan = document.createElement('span');
            nameSpan.className = 'wifi-name';
            nameSpan.textContent = net.ssid;
            
            let sigSpan = document.createElement('span');
            sigSpan.className = 'wifi-signal';
            sigSpan.textContent = net.rssi + ' dBm';
            
            item.appendChild(nameSpan);
            item.appendChild(sigSpan);
            item.onclick = () => openModalByIndex(idx);
            listDiv.appendChild(item);
          });
        })
        .catch(err => {
          listDiv.innerHTML = '<p style="color:red;">Gagal memindai WiFi. Coba klik Scan Ulang.</p>';
        });
    }

    function openModalByIndex(idx) {
      let ssid = wifiList[idx].ssid;
      document.getElementById('ssidInputGroup').style.display = 'none';
      document.getElementById('modalSsid').value = ssid;
      document.getElementById('modalTitle').textContent = 'Hubungkan ke ' + ssid;
      document.getElementById('modalPass').value = '';
      document.getElementById('wifiModal').style.display = 'flex';
    }

    function openManualModal() {
      document.getElementById('ssidInputGroup').style.display = 'block';
      document.getElementById('modalSsid').value = '';
      document.getElementById('modalTitle').textContent = 'Input WiFi Manual';
      document.getElementById('modalPass').value = '';
      document.getElementById('wifiModal').style.display = 'flex';
    }

    function closeModal() {
      document.getElementById('wifiModal').style.display = 'none';
    }

    loadInfo();
    scanWifi();
  </script>
</body>
</html>
)rawliteral";

String escapeJson(String s) {
  s.replace("\\", "\\\\");
  s.replace("\"", "\\\"");
  return s;
}

String formatTime(int h, int m) {
  String s = "";
  if (h < 10) s += "0";
  s += String(h) + ":";
  if (m < 10) s += "0";
  s += String(m);
  return s;
}

void handleRoot() {
  server.send(200, "text/html; charset=utf-8", SETUP_PAGE);
}

void handleInfo() {
  bool isConn = (WiFi.status() == WL_CONNECTED);
  String json = "{";
  json += "\"connected\":" + String(isConn ? "true" : "false") + ",";
  json += "\"ssid\":\"" + (isConn ? escapeJson(WiFi.SSID()) : "-") + "\",";
  json += "\"ip\":\"" + (isConn ? WiFi.localIP().toString() : "-") + "\"";
  json += "}";
  server.send(200, "application/json; charset=utf-8", json);
}

void handleScan() {
  Serial.println("Starting WiFi network scan...");
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.disconnect(false);
    delay(100);
  }

  int n = WiFi.scanNetworks(false, false);
  Serial.printf("Found %d networks.\n", n);

  String json = "[";
  for (int i = 0; i < n; ++i) {
    if (i > 0) json += ",";
    json += "{";
    json += "\"ssid\":\"" + escapeJson(WiFi.SSID(i)) + "\",";
    json += "\"rssi\":" + String(WiFi.RSSI(i));
    json += "}";
  }
  json += "]";
  WiFi.scanDelete();

  server.send(200, "application/json; charset=utf-8", json);
  Serial.println("Scan complete, data sent to browser.");
}

void handleSave() {
  String ssid_in = server.arg("ssid");
  String pass_in = server.arg("pass");
  ssid_in.trim();

  pref.putString("wifi_ssid", ssid_in);
  pref.putString("wifi_pass", pass_in);

  String html = "<html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1'></head>";
  html += "<body style='text-align:center;font-family:Arial;padding:40px;'>";
  html += "<h2>WiFi Berhasil Disimpan!</h2>";
  html += "<p>Perangkat sedang merestart dan akan beralih ke <b>" + ssid_in + "</b>.</p>";
  html += "<p>Silakan tutup halaman ini.</p>";
  html += "</body></html>";

  server.send(200, "text/html; charset=utf-8", html);
  delay(2000);
  ESP.restart();
}

// --- ACTUATOR FUNCTIONS ---
void feedFish() {
  Serial.println("Activating Servo 1 (Feed)...");
  moveServo(SERVO1_PIN, 0, 90, 500, false); // Open 90 degrees for 0.5s (500 ms)
  moveServo(SERVO1_PIN, 0, 0, 400, true);    // Close back to 0 degrees and relax servo
  Serial.println("Servo 1 (Feed) complete.");
}

void checkDayChange(struct tm &timeinfo) {
  if (timeinfo.tm_year < 120) return; // Ignore if time is not yet synced via NTP (epoch 1970)
  if (lastFeedDay != timeinfo.tm_mday) {
    lastFeedDay = timeinfo.tm_mday;
    feedCountToday = 0;
    lastFedTime = "--:--";
    lampOnMinutesToday = 0;
    manualLampConfirmed = false;
    lastScheduledState = false;
    pref.putInt("f_day", lastFeedDay);
    pref.putInt("f_cnt", feedCountToday);
    pref.putString("last_fed", lastFedTime);
    pref.putInt("l_min_tdy", 0);
    Serial.printf("[Info] Date changed to %d: Feeding quota & lamp duration reset.\n", lastFeedDay);
  }
}

// --- TIME & SCHEDULE HELPERS ---
bool parseHoursMinutes(String s, int &h, int &m) {
  s.trim();
  int colon = s.indexOf(':');
  if (colon == -1) return false;
  h = s.substring(0, colon).toInt();
  m = s.substring(colon + 1).toInt();
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

String formatLampSessionList() {
  String resp = "";
  for (int i = 0; i < sessionCount; i++) {
    int d = calculateDurationMinutes(lampSessions[i].onH, lampSessions[i].onM, lampSessions[i].offH, lampSessions[i].offM);
    resp += "  • Sesi " + String(i + 1) + ": `" + formatTime(lampSessions[i].onH, lampSessions[i].onM) + " - " + formatTime(lampSessions[i].offH, lampSessions[i].offM) + "` (" + String(d / 60) + "j " + String(d % 60) + "m)\n";
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
    errorMessage = "⚠️ *Pemberian Pakan Dibatalkan!*\nJatah pakan hari ini sudah mencapai batas maksimal (" + String(maxFeedPerDay) + " kali sehari) untuk mencegah overfeeding.";
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

void setBubbleLevel(int percentage) {
  percentage = constrain(percentage, 0, 100);
  int angle = map(percentage, 0, 100, 0, 180);
  Serial.printf("Moving Servo 2 (Bubble) -> Level: %d%% (Angle: %d deg)\n", percentage, angle);
  moveServo(SERVO2_PIN, 1, angle, 600, false);
  bubbleLevel = percentage;
  pref.putInt("buih_lvl", bubbleLevel);
}

// --- NON-BLOCKING LIGHT SWITCH (KANDILA MC-5) ---
struct LampColorState {
  bool active = false;
  int targetMode = 1;
  int step = 0;
  int pulsesRemaining = 0;
  unsigned long nextStepTime = 0;
} lampTransition;

void updateLampTransition() {
  if (!lampTransition.active || millis() < lampTransition.nextStepTime) return;

  switch (lampTransition.step) {
    case 0: // Reset phase: turn OFF for 6.5s
      digitalWrite(RELAY_PIN, RELAY_OFF);
      lampTransition.nextStepTime = millis() + 6500;
      lampTransition.step = 1;
      break;
    case 1: // Power back ON to Mode 1
      digitalWrite(RELAY_PIN, RELAY_ON);
      lampTransition.pulsesRemaining = (lampTransition.targetMode == 2) ? 1 : (lampTransition.targetMode == 3) ? 2 : 0;
      if (lampTransition.pulsesRemaining > 0) {
        lampTransition.nextStepTime = millis() + 600;
        lampTransition.step = 2;
      } else {
        lampTransition.active = false;
      }
      break;
    case 2: // Pulse OFF
      digitalWrite(RELAY_PIN, RELAY_OFF);
      lampTransition.nextStepTime = millis() + 350;
      lampTransition.step = 3;
      break;
    case 3: // Pulse ON
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

void askLampConfirmation(String chatId, String reason, int targetMode) {
  pendingLampMode = targetMode;
  waitingLampConfirmation = true;
  lampConfirmationTime = millis();
  String confirmMsg = "⚠️ *Konfirmasi Lampu*\n";
  confirmMsg += reason + "\n\n";
  confirmMsg += "Apakah anda yakin ingin menyalakan lampu sekarang?";
  bot.sendMessageWithReplyKeyboard(chatId, confirmMsg, "Markdown", "[[\"/ya_lampu\", \"/batal\"]]", true, false, false);
}

void askLampOffConfirmation(String chatId) {
  waitingLampOffConfirmation = true;
  lampOffConfirmationTime = millis();
  String confirmMsg = "⚠️ *Konfirmasi Matikan Lampu*\n";
  confirmMsg += "Saat ini *sedang dalam jadwal lampu menyala*.\n\n";
  confirmMsg += "Apakah anda yakin ingin mematikan lampu sekarang? (Lampu akan tetap padam sampai sesi jadwal berikutnya).";
  bot.sendMessageWithReplyKeyboard(chatId, confirmMsg, "Markdown", "[[\"/ya_lampu_off\", \"/batal\"]]", true, false, false);
}

// --- MENU & TELEGRAM HANDLER ---
void sendHelpMenu(String chatId) {
  String msg = "🤖 *Panel Kontrol Akuarium ESP32-S3*\n\n";
  msg += "• `/status` - Cek status lampu, buih, pakan & IP\n";
  msg += "• `/pakan` - Beri makan ikan sekarang\n";
  msg += "• `/reset_pakan` - Reset kuota pakan hari ini\n";
  msg += "• `/buih [0-100]` - Atur level buih\n";
  msg += "• `/buih_on` / `/buih_off` - Buih Full (100%) / Mati (0%)\n";
  msg += "• `/lampu_on` / `/lampu_off` - Kontrol lampu\n";
  msg += "• `/warna1`, `/warna2`, `/warna3` - Pilih mode warna\n\n";
  msg += "📌 *Atur Jadwal:*\n";
  msg += "• `/set_lampu [sesi, sesi, ...]` (Maks total 8 jam)\n";
  msg += "  _Contoh:_ `/set_lampu 09:00-13:00, 17:00-21:00`\n";
  msg += "• `/set_pakan [jam, jam, ...]`\n";
  msg += "  _Contoh:_ `/set_pakan 07:00, 16:00`\n";

  String keyboardJson = "["
                        "[\"/pakan\", \"/status\"],"
                        "[\"/buih_on\", \"/buih_off\"],"
                        "[\"/lampu_on\", \"/lampu_off\"],"
                        "[\"/warna1\", \"/warna2\", \"/warna3\"]"
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
      sendHelpMenu(chatId);
      continue;
    }

    if (text.startsWith("/buih")) {
      int lvl = (text == "/buih_on") ? 100 : (text == "/buih_off") ? 0 : text.substring(6).toInt();
      bot.sendMessage(chatId, "⏳ Sedang memproses pengaturan level buih...", "");
      setBubbleLevel(lvl);
      bot.sendMessage(chatId, "🫧 Level buih filter diatur ke: *" + String(bubbleLevel) + "%*", "Markdown");
    } else if (text == "/lampu_on") {
      struct tm timeinfo;
      bool hasTime = getLocalTime(&timeinfo, 100);
      int curMin = hasTime ? (timeinfo.tm_hour * 60 + timeinfo.tm_min) : -1;
      bool inSchedule = (curMin >= 0) ? shouldLampBeOn(curMin) : false;
      bool overLimit = (lampOnMinutesToday >= MAX_LAMP_MINUTES_PER_DAY && !manualLampConfirmed);

      if (lampState) {
        bot.sendMessage(chatId, "💡 Lampu sudah dalam keadaan menyala.", "");
      } else if (!inSchedule || overLimit) {
        String reason = "";
        if (!inSchedule && overLimit) {
          reason = "Saat ini *di luar jadwal otomatis* dan durasi lampu sudah mencapai batas 8 jam (" + String(lampOnMinutesToday / 60) + "j " + String(lampOnMinutesToday % 60) + "m).";
        } else if (!inSchedule) {
          reason = "Saat ini *di luar jadwal otomatis* lampu.";
        } else {
          reason = "Lampu sudah menyala selama *" + String(lampOnMinutesToday / 60) + " jam " + String(lampOnMinutesToday % 60) + " menit* hari ini (batas harian: 8 jam).";
        }
        askLampConfirmation(chatId, reason, currentLampMode);
      } else {
        bot.sendMessage(chatId, "⏳ Sedang memproses menyalakan lampu...", "");
        turnLampOnManual(currentLampMode);
        bot.sendMessage(chatId, "💡 Lampu dinyalakan.", "");
      }
    } else if (text == "/lampu_off") {
      struct tm timeinfo;
      bool hasTime = getLocalTime(&timeinfo, 100);
      int curMin = hasTime ? (timeinfo.tm_hour * 60 + timeinfo.tm_min) : -1;
      bool inSchedule = (curMin >= 0) ? shouldLampBeOn(curMin) : false;

      if (!lampState) {
        bot.sendMessage(chatId, "🌑 Lampu sudah dalam keadaan padam.", "");
      } else if (inSchedule) {
        askLampOffConfirmation(chatId);
      } else {
        bot.sendMessage(chatId, "⏳ Sedang memproses mematikan lampu...", "");
        turnLampOffManual();
        bot.sendMessage(chatId, "🌑 Lampu dimatikan.", "");
      }
    } else if (text == "/warna1" || text == "/warna2" || text == "/warna3") {
      int mode = text.substring(6).toInt();
      struct tm timeinfo;
      bool hasTime = getLocalTime(&timeinfo, 100);
      int curMin = hasTime ? (timeinfo.tm_hour * 60 + timeinfo.tm_min) : -1;
      bool inSchedule = (curMin >= 0) ? shouldLampBeOn(curMin) : false;
      bool overLimit = (lampOnMinutesToday >= MAX_LAMP_MINUTES_PER_DAY && !manualLampConfirmed);

      if (lampState) {
        currentLampMode = mode;
        pref.putInt("lamp_mode", currentLampMode);
        bot.sendMessage(chatId, "⏳ Sedang memproses perubahan ke mode warna " + String(mode) + "...", "");
        setLampColor(mode);
        bot.sendMessage(chatId, "✅ Aktif: Mode Warna " + String(mode), "");
      } else if (!inSchedule || overLimit) {
        String reason = "";
        if (!inSchedule && overLimit) {
          reason = "Saat ini *di luar jadwal otomatis* dan durasi lampu telah mencapai batas 8 jam.\nIngin menyalakan lampu dengan *Mode Warna " + String(mode) + "*?";
        } else if (!inSchedule) {
          reason = "Saat ini *di luar jadwal otomatis* dan lampu sedang padam.\nIngin menyalakan lampu dengan *Mode Warna " + String(mode) + "*?";
        } else {
          reason = "Lampu sudah menyala selama *" + String(lampOnMinutesToday / 60) + " jam " + String(lampOnMinutesToday % 60) + " menit* hari ini.\nIngin menyalakan lampu dengan *Mode Warna " + String(mode) + "*?";
        }
        askLampConfirmation(chatId, reason, mode);
      } else {
        currentLampMode = mode;
        pref.putInt("lamp_mode", currentLampMode);
        bot.sendMessage(chatId, "⏳ Sedang memproses menyalakan lampu ke mode warna " + String(mode) + "...", "");
        turnLampOnManual(mode);
        bot.sendMessage(chatId, "✅ Lampu dinyalakan: Mode Warna " + String(mode), "");
      }
    } else if (text == "/pakan") {
      struct tm timeinfo;
      if (getLocalTime(&timeinfo, 100)) {
        checkDayChange(timeinfo);
      }

      if (feedCountToday >= maxFeedPerDay) {
        bot.sendMessage(chatId, "⚠️ *Pemberian Pakan Dibatalkan!*\nJatah pakan hari ini sudah mencapai batas maksimal (" + String(maxFeedPerDay) + " kali sehari) untuk mencegah overfeeding.", "Markdown");
      } else {
        String lastFedInfo = "Ikan belum diberi makan hari ini";
        String nextFeedInfo = (feedSessionCount > 0) ? (formatTime(feedSessions[0].h, feedSessions[0].m) + " WIB") : "-";

        if (getLocalTime(&timeinfo, 100)) {
          if (feedCountToday > 0 && lastFedTime != "--:--") {
            lastFedInfo = "ikan sudah di beri makan jam " + lastFedTime + " WIB";
          }
          nextFeedInfo = getNextFeedingSchedule(timeinfo.tm_hour, timeinfo.tm_min);
        }

        String confirmMsg = lastFedInfo + " dan akan di berikan makan lagi di jam " + nextFeedInfo + ".\n\n";
        confirmMsg += "Apakah anda yakin akan memberikan pakan sekarang? Karena maksimal pakan hanya " + String(maxFeedPerDay) + " kali sehari.";

        waitingFeedConfirmation = true;
        feedConfirmationTime = millis();

        String confirmKeyboard = "[[\"/ya_pakan\", \"/batal\"]]";
        bot.sendMessageWithReplyKeyboard(chatId, confirmMsg, "Markdown", confirmKeyboard, true, false, false);
      }
    } else if (text == "/ya_pakan") {
      if (!waitingFeedConfirmation || (millis() - feedConfirmationTime > 120000)) {
        waitingFeedConfirmation = false;
        bot.sendMessage(chatId, "⚠️ Konfirmasi sudah kadaluarsa. Silakan ketik /pakan kembali.", "");
        sendHelpMenu(chatId);
      } else {
        waitingFeedConfirmation = false;
        bot.sendMessage(chatId, "⏳ Sedang memproses pemberian pakan...", "");
        String errFeed = "";
        if (processFeeding(errFeed)) {
          bot.sendMessage(chatId, "🐟 Pakan berhasil diberikan! (" + String(feedCountToday) + "/" + String(maxFeedPerDay) + " kali hari ini)", "Markdown");
        } else {
          bot.sendMessage(chatId, errFeed, "Markdown");
        }
        sendHelpMenu(chatId);
      }
    } else if (text == "/ya_lampu") {
      if (!waitingLampConfirmation || (millis() - lampConfirmationTime > 120000)) {
        waitingLampConfirmation = false;
        bot.sendMessage(chatId, "⚠️ Konfirmasi sudah kadaluarsa. Silakan ulangi perintah.", "");
      } else {
        waitingLampConfirmation = false;
        bot.sendMessage(chatId, "⏳ Sedang memproses menyalakan lampu...", "");
        currentLampMode = pendingLampMode;
        pref.putInt("lamp_mode", currentLampMode);
        turnLampOnManual(currentLampMode);
        bot.sendMessage(chatId, "💡 Lampu berhasil dinyalakan (Mode Warna " + String(currentLampMode) + ").", "");
      }
      sendHelpMenu(chatId);
    } else if (text == "/ya_lampu_off") {
      if (!waitingLampOffConfirmation || (millis() - lampOffConfirmationTime > 120000)) {
        waitingLampOffConfirmation = false;
        bot.sendMessage(chatId, "⚠️ Konfirmasi sudah kadaluarsa. Silakan ulangi perintah.", "");
      } else {
        waitingLampOffConfirmation = false;
        bot.sendMessage(chatId, "⏳ Sedang memproses mematikan lampu...", "");
        turnLampOffManual();
        bot.sendMessage(chatId, "🌑 Lampu dimatikan (tetap padam sampai sesi jadwal berikutnya).", "");
      }
      sendHelpMenu(chatId);
    } else if (text == "/batal") {
      waitingFeedConfirmation = false;
      waitingLampConfirmation = false;
      waitingLampOffConfirmation = false;
      bot.sendMessage(chatId, "❌ Perintah dibatalkan.", "");
      sendHelpMenu(chatId);
    } else if (text == "/reset_pakan") {
      bot.sendMessage(chatId, "⏳ Sedang memproses reset kuota pakan...", "");
      feedCountToday = 0;
      lastFedTime = "--:--";
      pref.putInt("f_cnt", 0);
      pref.putString("last_fed", lastFedTime);
      bot.sendMessage(chatId, "🔄 Kuota pakan hari ini berhasil di-reset ke 0 kali.", "");
    } else if (text == "/status") {
      bot.sendMessage(chatId, "⏳ Sedang memproses pengambilan status...", "");
      struct tm timeinfo;
      String curTime = "--:--:--";
      bool inSched = false;
      if (getLocalTime(&timeinfo, 100)) {
        checkDayChange(timeinfo);
        curTime = formatTime(timeinfo.tm_hour, timeinfo.tm_min) + ":" + (timeinfo.tm_sec < 10 ? "0" : "") + String(timeinfo.tm_sec);
        inSched = shouldLampBeOn(timeinfo.tm_hour * 60 + timeinfo.tm_min);
      }

      String statusMsg = "📊 *STATUS AKUARIUM*\n\n";
      statusMsg += "🕒 Jam: `" + curTime + " WIB`\n";
      statusMsg += "💡 Lampu: *" + String(lampState ? "NYALA" : "PADAM") + "* (Mode " + String(currentLampMode) + " | " + String(inSched ? "Sesuai Jadwal" : "Di Luar Jadwal") + ")\n";
      statusMsg += "🫧 Buih: *" + String(bubbleLevel) + "%*\n";
      statusMsg += "🌐 WiFi: `" + WiFi.SSID() + "`\n\n";
      statusMsg += "🗓️ *Jadwal Tersimpan:*\n";
      statusMsg += "• Pakan Hari Ini: `" + String(feedCountToday) + "/" + String(maxFeedPerDay) + " kali`";
      if (lastFedTime != "--:--") {
        statusMsg += " (Terakhir: `" + lastFedTime + " WIB`)\n";
      } else {
        statusMsg += " (Belum makan)\n";
      }
      statusMsg += "• Jadwal Pakan (" + String(feedSessionCount) + "x/hari): " + formatFeedScheduleList() + "\n";
      statusMsg += "💡 *Jadwal Lampu Dinamis (" + String(sessionCount) + " Sesi):*\n";
      statusMsg += formatLampSessionList();
      statusMsg += "• Total Nyala Hari Ini: `" + String(lampOnMinutesToday / 60) + "j " + String(lampOnMinutesToday % 60) + "m` (Maks 8 jam)\n";

      bot.sendMessage(chatId, statusMsg, "Markdown");
    } else if (text.startsWith("/set_lampu ")) {
      bot.sendMessage(chatId, "⏳ Sedang memproses pengaturan jadwal lampu dinamis...", "");
      String scheduleErr = "";
      if (applyLampSchedule(text.substring(11), scheduleErr)) {
        lastScheduledState = false;
        int totalPlanMinutes = calculateTotalLampDuration();
        String resp = "✅ *Jadwal Lampu Dinamis Berhasil Disimpan!*\n\n";
        resp += formatLampSessionList();
        resp += "\n⏱️ *Total Nyala Rencana:* " + String(totalPlanMinutes / 60) + " jam " + String(totalPlanMinutes % 60) + " menit/hari (Maks 8 jam).";
        bot.sendMessage(chatId, resp, "Markdown");
      } else {
        bot.sendMessage(chatId, scheduleErr, "Markdown");
      }
    } else if (text.startsWith("/set_pakan ")) {
      bot.sendMessage(chatId, "⏳ Sedang memproses pengaturan jadwal pakan...", "");
      String feedErr = "";
      if (applyFeedSchedule(text.substring(11), feedErr)) {
        String resp = "✅ *Jadwal Pakan Berhasil Disimpan!*\n\n";
        resp += "• Jadwal: " + formatFeedScheduleList() + "\n";
        resp += "• Kuota Maksimal Harian: *" + String(maxFeedPerDay) + " kali sehari*.\n";
        bot.sendMessage(chatId, resp, "Markdown");
      } else {
        bot.sendMessage(chatId, feedErr, "Markdown");
      }
    } else {
      bot.sendMessage(chatId, "❓ Perintah tidak dikenali. Ketik /help untuk melihat menu.", "");
    }
  }
}

void initTelegramConnection() {
  Serial.println("\nWiFi Connected! Local IP: " + WiFi.localIP().toString());
  WiFi.config(WiFi.localIP(), WiFi.gatewayIP(), WiFi.subnetMask(), IPAddress(8, 8, 8, 8), IPAddress(1, 1, 1, 1));

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  secured_client.setInsecure();
  secured_client.setTimeout(10);

  String startMsg = "🚀 *Akuarium ESP32-S3 Aktif (Native PWM Stabil)*\n";
  startMsg += "🌐 WiFi: `" + WiFi.SSID() + "`\n";
  startMsg += "🫧 Level Buih: *" + String(bubbleLevel) + "%*\n";
  startMsg += "🔗 Web Setup: `http://" + WiFi.localIP().toString() + "` atau `192.168.4.1`\n\n";
  startMsg += "Ketik /help untuk membuka menu.";

  bot.sendMessage(CHAT_ID, startMsg, "Markdown");
  ntpConfigured = true;
}

// --- SETUP ---
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
  currentLampMode    = pref.getInt("lamp_mode", 1);
  bubbleLevel        = pref.getInt("buih_lvl", 100);
  lampState          = false; // Relai fisik selalu mulai dari kondisi OFF saat baru menyala
  lastScheduledState = false;

  // Setup Hardware PWM channels: Channel 0 for Feed, Channel 1 for Bubble
  setupServoPin(SERVO1_PIN, 0);
  setupServoPin(SERVO2_PIN, 1);

  // Ensure Feed Servo is closed at 0 degrees on startup
  moveServo(SERVO1_PIN, 0, 0, 100, true);

  // Restore Bubble Servo position based on saved level
  Serial.println("--- Restoring Bubble Servo Position ---");
  setBubbleLevel(bubbleLevel);

  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(AP_SSID, AP_PASS);

  server.on("/", handleRoot);
  server.on("/info", handleInfo);
  server.on("/scan", handleScan);
  server.on("/save", HTTP_POST, handleSave);
  server.begin();

  savedSSID = pref.getString("wifi_ssid", WIFI_SSID);
  savedPass = pref.getString("wifi_pass", WIFI_PASS);

  if (savedSSID.length() > 0) {
    Serial.println("Connecting to WiFi: " + savedSSID);
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
    // Jangan disconnect; router mungkin masih proses booting setelah listrik menyala.
    // Biarkan background auto-reconnect di loop() terus mencoba menyambung secara otomatis.
    Serial.println("\nWiFi belum tersambung (kemungkinan router masih proses booting).");
    Serial.println("Koneksi otomatis di latar belakang tetap aktif mencari WiFi '" + savedSSID + "'.");
    lastWifiCheck = millis();
  }
}

// --- MANUAL CONSOLE TESTING VIA SERIAL MONITOR ---
void handleSerialCommands() {
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if (cmd.length() == 0) return;

    Serial.println("\n[Console] Command received: '" + cmd + "'");

    if (cmd.equalsIgnoreCase("pakan")) {
      String errFeed = "";
      if (processFeeding(errFeed)) {
        Serial.printf("[Console] Feeding executed successfully (%d/%d today).\n", feedCountToday, maxFeedPerDay);
      } else {
        Serial.printf("[Console] Feeding BLOCKED: Daily quota reached (%d times a day)!\n", maxFeedPerDay);
      }
    } else if (cmd.equalsIgnoreCase("reset_pakan")) {
      feedCountToday = 0;
      pref.putInt("f_cnt", 0);
      Serial.println("[Console] Feeding quota reset to 0 today.");
    } else if (cmd.startsWith("buih ") || cmd.startsWith("BUIH ")) {
      int lvl = cmd.substring(5).toInt();
      setBubbleLevel(lvl);
    } else if (cmd.equalsIgnoreCase("lampu on")) {
      turnLampOnManual(currentLampMode);
    } else if (cmd.equalsIgnoreCase("lampu off")) {
      turnLampOffManual();
      Serial.println("Lampu dimatikan.");
    } else if (cmd.equalsIgnoreCase("help")) {
      Serial.println("Commands: 'pakan', 'reset_pakan', 'buih [0-100]', 'lampu on', 'lampu off'");
    } else {
      Serial.println("Unknown command. Type 'help' for command list.");
    }
  }
}

// --- MAIN LOOP ---
void loop() {
  handleSerialCommands();
  server.handleClient();
  updateLampTransition();

  bool isConnected = (WiFi.status() == WL_CONNECTED);

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

  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 100)) return;
  checkDayChange(timeinfo);

  int currentMinute = timeinfo.tm_hour * 60 + timeinfo.tm_min;

  // Track Daily Light On Time (8 Hours = 480 Minutes Protection)
  if (lampState) {
    if (lastLampCountMinute != timeinfo.tm_min) {
      lastLampCountMinute = timeinfo.tm_min;
      lampOnMinutesToday++;

      if (lampOnMinutesToday >= MAX_LAMP_MINUTES_PER_DAY && !manualLampConfirmed) {
        turnLampOffManual();
        if (isConnected) {
          bot.sendMessage(CHAT_ID, "⚠️ *Proteksi Lampu:*\nLampu otomatis dipadamkan karena telah mencapai batas maksimal 8 jam hari ini untuk mencegah alga/lumut.\n\nJika ingin menyalakan kembali secara manual, ketik /lampu_on.", "Markdown");
        }
      }
    }
  } else {
    lastLampCountMinute = -1;
  }

  // Automatic Light Control (Dynamic Sessions & Transition-Based)
  bool scheduledOn = shouldLampBeOn(currentMinute);

  if (scheduledOn && !lastScheduledState) {
    lastScheduledState = true;
    if (!lampState && lampOnMinutesToday < MAX_LAMP_MINUTES_PER_DAY) {
      setLampColor(currentLampMode);
      if (isConnected) {
        bot.sendMessage(CHAT_ID, "💡 *Laporan Otomatis:*\nLampu akuarium telah menyala sesuai jadwal (Pukul " + formatTime(timeinfo.tm_hour, timeinfo.tm_min) + " WIB, Mode Warna " + String(currentLampMode) + ").", "Markdown");
      }
    }
  } else if (!scheduledOn && lastScheduledState) {
    lastScheduledState = false;
    if (lampState) {
      lampTransition.active = false;
      digitalWrite(RELAY_PIN, RELAY_OFF);
      lampState = false;
      pref.putInt("l_min_tdy", lampOnMinutesToday);
      pref.putBool("lamp_st", false);
      if (isConnected) {
        bot.sendMessage(CHAT_ID, "🌑 *Laporan Otomatis:*\nLampu akuarium telah padam sesuai jadwal (Pukul " + formatTime(timeinfo.tm_hour, timeinfo.tm_min) + " WIB).", "Markdown");
      }
    }
  }

  // Automatic Feeding Control
  bool isFeedTime = isCurrentTimeFeedSchedule(timeinfo.tm_hour, timeinfo.tm_min);

  if (isFeedTime && lastFedMinute != timeinfo.tm_min) {
    lastFedMinute = timeinfo.tm_min;
    String errFeed = "";
    if (processFeeding(errFeed)) {
      if (isConnected) {
        bot.sendMessage(CHAT_ID, "🔔 *Laporan Otomatis:*\nPakan ikan terjadwal selesai diberikan (" + String(feedCountToday) + "/" + String(maxFeedPerDay) + " kali hari ini).", "Markdown");
      }
    } else {
      if (isConnected) {
        bot.sendMessage(CHAT_ID, "🔔 *Laporan Otomatis:*\nJadwal pakan dilewati karena kuota hari ini sudah penuh (" + String(maxFeedPerDay) + " kali).", "Markdown");
      }
    }
  } else if (!isFeedTime) {
    lastFedMinute = -1;
  }
}