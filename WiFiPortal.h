#pragma once

#include "Config.h"

// --- HALAMAN HTML SETUP WIFI (PROGMEM) ---
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

void initWiFiPortal() {
  server.on("/", handleRoot);
  server.on("/info", handleInfo);
  server.on("/scan", handleScan);
  server.on("/save", HTTP_POST, handleSave);
  server.begin();
}
