# 🌿 IoT Smart Aquascape Controller (ESP32)

Sistem otomasi dan pemantauan akuarium cerdas berbasis mikrokontroler **ESP32**, dilengkapi integrasi **Bot Telegram**, **Web Portal Konfigurasi (Captive Portal)**, dan **Autonomi Offline Penuh**.

Dirancang khusus untuk menjaga ekosistem aquascape tetap stabil: mencegah *overfeeding*, membatasi paparan cahaya untuk menekan pertumbuhan alga/lumut, serta mendukung pergantian mode warna lampu secara otomatis dan non-blocking.

---

## ✨ Fitur Utama

- 🤖 **Kendali Jarak Jauh via Telegram Bot**
  - Antarmuka tombol interaktif (*Custom Reply Keyboard*).
  - Laporan otomatis saat pakan terjadwal berhasil diberikan atau lampu berganti status.
  - Perintah manual untuk lampu, mode warna, pengaturan buih filter, dan pemberian pakan.
- 💡 **Jadwal Lampu Multi-Sesi Dinamis & Proteksi Alga (Maks 8 Jam)**
  - Mendukung konfigurasi 1 hingga 5 sesi waktu hidup-mati dalam sehari (contoh: pagi dan malam).
  - Validasi ketat anti-tumpang tindih (*anti-overlap*) antar sesi.
  - Pembatasan durasi kumulatif lampu maksimal 8 jam/hari guna mencegah ledakan alga (*algae bloom*).
  - Proteksi bypass manual dengan konfirmasi ganda jika durasi harian telah tercapai.
- 🎨 **Transisi Mode Lampu Non-Blocking (Kandila MC-5 Compatible)**
  - Mengatur siklus pulsa daya untuk berpindah ke Mode Warna 1, 2, atau 3.
  - Dijalankan menggunakan mesin status waktu (`millis()`), menjaga web server dan sistem tetap responsif tanpa jeda beku (*zero blocking delay*).
- 🐟 **Otomasi Pakan Terjadwal & Proteksi Overfeeding**
  - Penjadwalan pakan 2 kali sehari dengan penyimpanan kuota di memori non-volatile.
  - Penggerak servo dengan fitur *auto-relax* setelah berputar untuk menghemat daya dan memperpanjang umur aktuator.
  - Dialog konfirmasi aman via bot jika pengguna mencoba memberi makan berlebih.
- 🫧 **Kontrol Aliran Buih Filter**
  - Pengaturan bukaan katup buih filter terkalibrasi persentase (0% hingga 100%).
- 🌐 **Web Portal Konfigurasi WiFi (Captive Portal)**
  - Mode Access Point mandiri (`Akuarium-Nurz`) untuk menghubungkan ESP32 ke jaringan WiFi lokal tanpa perlu *hardcode* SSID/password di kode program.
- 🛡️ **Autonomi Penuh Saat Offline**
  - Jika koneksi internet atau WiFi terputus, RTC internal ESP32 tetap menjalankan seluruh jadwal pakan dan siklus lampu secara mandiri.
  - Rekoneksi jaringan otomatis berjalan di latar belakang secara periodik.
- 💾 **Perlindungan Flash Storage (NVS Lifespan Preservation)**
  - Mengeliminasi penulisan memori flash per menit. Durasi kumulatif lampu hanya disimpan saat lampu berganti status menjadi mati.

---

## 🛠️ Kebutuhan Hardware & Komponen

1. **Mikrokontroler:** ESP32 / ESP32-S3 Board.
2. **Aktuator & Relai:**
   - Modul Relai AC (Saklar Daya Lampu Akuarium).
   - Motor Servo 1 (Katup Pakan Otomatis).
   - Motor Servo 2 (Katup Pengatur Aliran Buih Filter).
3. **Lampu Akuarium:** Kompatibel dengan lampu multi-mode pulsa (seperti seri Kandila MC-5).
4. **Catu Daya:** Adaptor 5V yang stabil untuk mikrokontroler dan motor servo.

> **Catatan Pengkabelan:**
> Jalur kontrol relai dan servo dikonfigurasikan melalui konstanta `#define` pada bagian awal kode (`RELAY_PIN`, `SERVO1_PIN`, dan `SERVO2_PIN`). Sesuaikan jalur GPIO tersebut dengan tata letak board yang Anda gunakan.

---

## 📦 Pustaka (Libraries) yang Dibutuhkan

Pastikan pustaka berikut sudah terpasang melalui **Library Manager** di Arduino IDE atau `platformio.ini`:

- **UniversalTelegramBot** (oleh Brian Lough)
- **ArduinoJson** (versi 6.x)
- **WiFi** & **WebServer** (bawaan ESP32 Core)
- **Preferences** (bawaan ESP32 Core)

Kompatibel dengan ESP32 Arduino Core versi 2.x maupun 3.x (menggunakan penanganan LEDC PWM adaptif).

---

## 🚀 Panduan Instalasi & Penggunaan

### 1. Persiapan Kredensial & Secrets
Kredensial rahasia (token bot dan SSID WiFi) dipisahkan agar aman dan tidak bocor saat di-*push* ke GitHub.

Salin template `secrets.h.example` menjadi `secrets.h`:
```bash
cp secrets.h.example secrets.h
```
Buka `secrets.h` dan sesuaikan nilainya:
```cpp
#define AP_SSID    "Akuarium-Nurz"
#define AP_PASS    "12345678"
#define WIFI_SSID  "Nama_WiFi_Rumah"      // Opsional (bisa diisi atau via Captive Portal)
#define WIFI_PASS  "Password_WiFi_Rumah"  // Opsional
#define BOT_TOKEN  "YOUR_TELEGRAM_BOT_TOKEN"
#define CHAT_ID    "YOUR_TELEGRAM_CHAT_ID"
```

> File `secrets.h` sudah otomatis masuk ke dalam `.gitignore` sehingga tidak akan terunggah ke repositori publik.

### 2. Unggah Kode ke Board
- Buka `Aquascape.ino` di Arduino IDE.
- Pilih board ESP32 yang sesuai (contoh: *ESP32 Dev Module* atau *ESP32S3 Dev Module*).
- Lakukan proses compile dan flash (*Upload*).

### 3. Konfigurasi Jaringan WiFi (Pertama Kali)
1. Setelah menyala, jika belum terhubung ke WiFi, ESP32 akan memancarkan hotspot Access Point:
   - **SSID:** `Akuarium-Nurz`
   - **Password:** `12345678`
2. Hubungkan smartphone atau laptop ke hotspot tersebut, lalu buka browser ke alamat: `http://192.168.4.1`.
3. Masukkan SSID dan password WiFi rumah Anda, kemudian klik **Simpan & Hubungkan**.
4. ESP32 akan tersambung ke jaringan lokal dan menyinkronkan waktu melalui server NTP (`pool.ntp.org` waktu WIB / GMT+7).

---

## 📱 Daftar Perintah Telegram Bot

| Perintah | Deskripsi |
| :--- | :--- |
| `/status` | Menampilkan ringkasan status lampu, durasi harian, level buih, status pakan, jam perangkat, dan IP lokal. |
| `/pakan` | Memberikan pakan ikan (maksimal kuota 2x sehari). |
| `/reset_pakan` | Mereset kuota pakan harian ke 0. |
| `/buih [0-100]` | Mengatur persentase bukaan katup buih filter (contoh: `/buih 75`). |
| `/buih_on` | Membuka buih maksimal (100%). |
| `/buih_off` | Menutup buih filter (0%). |
| `/lampu_on` | Menyalakan lampu akuarium secara manual. |
| `/lampu_off` | Mematikan lampu akuarium secara manual. |
| `/warna1` - `/warna3` | Mengubah mode warna lampu (Mode 1, Mode 2, atau Mode 3). |
| `/otomatis` | Mengembalikan siklus lampu ke jadwal otomatis. |
| `/set_lampu [sesi]` | Mengatur jadwal lampu dinamis (maksimal total 8 jam sehari). |
| `/set_pakan1 [HH:MM]` | Mengatur jadwal pakan sesi pertama (contoh: `/set_pakan1 08:30`). |
| `/set_pakan2 [HH:MM]` | Mengatur jadwal pakan sesi kedua (contoh: `/set_pakan2 16:30`). |
| `/batal` | Membatalkan permintaan konfirmasi aktif. |

### Contoh Format Jadwal Lampu Dinamis
- **1 Sesi:** `/set_lampu 10:00-18:00`
- **2 Sesi:** `/set_lampu 09:00-13:00, 17:00-21:00`
- **3 Sesi:** `/set_lampu 07:00-09:00, 12:00-14:00, 18:00-22:00`

---

## 💻 Kontrol Konsol Serial

Selain via Telegram, sistem juga dapat dikendalikan langsung melalui Serial Monitor (Baud rate `115200`):
- `pakan` : Eksekusi pakan manual.
- `reset_pakan` : Reset kuota pakan harian.
- `buih [0-100]` : Atur level buih filter.
- `lampu on` / `lampu off` : Kontrol saklar lampu.
- `help` : Menampilkan daftar perintah serial.

---

## 📄 Lisensi

Proyek ini dirilis di bawah lisensi [MIT](LICENSE). Bebas digunakan dan dimodifikasi untuk kebutuhan hobi maupun pengembangan otomasi akuarium Anda.
