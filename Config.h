#pragma once

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

// --- INSTANSIASI OBJEK GLOBAL ---
WiFiClientSecure secured_client;
UniversalTelegramBot bot(BOT_TOKEN, secured_client);
Preferences pref;
WebServer server(80);

// --- STRUKTUR DATA JADWAL & STATUS ---
struct LampSession {
  int onH;
  int onM;
  int offH;
  int offM;
};

const int MAX_SESSIONS = 5;
const int MAX_LAMP_MINUTES_PER_DAY = 480; // Maksimal 8 jam kumulatif sehari
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

// Konfirmasi & Dialog
bool waitingFeedConfirmation = false;
unsigned long feedConfirmationTime = 0;
bool waitingLampConfirmation = false;
unsigned long lampConfirmationTime = 0;
bool waitingLampOffConfirmation = false;
unsigned long lampOffConfirmationTime = 0;
bool waitingColorSelection = false;
unsigned long colorSelectionTime = 0;
bool waitingPortionSelection = false;
unsigned long portionSelectionTime = 0;

// Porsi Pakan
int feedPortionMode = 3;
int feedOpenDurationMs = 500;

// Mesin Status Transisi Lampu (Kandila MC-5)
struct LampColorState {
  bool active = false;
  int targetMode = 1;
  int step = 0;
  int pulsesRemaining = 0;
  unsigned long nextStepTime = 0;
} lampTransition;

// Wizard Jadwal Lampu
enum LampWizardState {
  LAMP_WIZ_IDLE,
  LAMP_WIZ_WAIT_START_TIME,
  LAMP_WIZ_WAIT_END_TIME,
  LAMP_WIZ_WAIT_CONFIRM_NEXT
};
LampWizardState lampWizState = LAMP_WIZ_IDLE;
unsigned long lampWizTime = 0;
LampSession tempLampSessions[MAX_SESSIONS];
int tempLampCount = 0;
int tempLampStartH = 0;
int tempLampStartM = 0;

// Wizard Jadwal Pakan
enum FeedWizardState {
  FEED_WIZ_IDLE,
  FEED_WIZ_WAIT_TIME,
  FEED_WIZ_WAIT_CONFIRM_NEXT
};
FeedWizardState feedWizState = FEED_WIZ_IDLE;
unsigned long feedWizTime = 0;
FeedTime tempFeedSessions[MAX_FEED_SESSIONS];
int tempFeedCount = 0;

bool manualLampConfirmed = false;
unsigned long lastBotCheck = 0;
const unsigned long botInterval = 1500;
unsigned long lastWifiCheck = 0;
bool ntpConfigured = false;

String savedSSID = "";
String savedPass = "";
