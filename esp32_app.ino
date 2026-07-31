#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include "html_str.h"

#define CONNECTION_DELAY_MS 250U
#define CONNECTION_TIMEOUT_ITERATIONS 40U

// HOME WIFI
const char* wifi_ssid = "Michael";
const char* wifi_pass = "";

// HARDCODED IP CONFIGURATION - MUST match your router
// If your router is 192.168.0.1, change all to 192.168.0.x
IPAddress local_IP(10, 100, 102, 142);  // The IP you want for the ESP32 - pick a free one like 100-200
IPAddress gateway(10, 100, 102, 1);     // Your router's IP - usually 192.168.1.1 or 192.168.0.1
IPAddress subnet(255, 255, 255, 0);
IPAddress dns1(8, 8, 8, 8);
IPAddress dns2(8, 8, 4, 4);

const char* HOSTNAME = "esp32"; // you can also open http://esp32.local
String currentTitle = "My ESP32 Web App"; // initial title

WebServer server(80);

String getPage() {
  // This page auto-updates the title with JS, no need to refresh browser
  String html = R"rawliteral(
  <!DOCTYPE html><html><head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>ESP32 Live Title</title>
    <style>
      body{font-family:Arial;display:flex;justify-content:center;align-items:center;height:100vh;margin:0;background:#f2f2f2}
      .card{background:white;padding:50px 70px;border-radius:16px;box-shadow:0 4px 20px rgba(0,0,0,0.1);text-align:center}
      h1{color:#222} p{color:#666}
    </style>
  </head><body>
    <div class="card">
      <h1 id="mainTitle">%TITLE%</h1>
      <p>Live update from ESP32 loop()</p>
    </div>
    <script>
      // Fetch new title every 1000ms
      setInterval(() => {
        fetch('/api/title').then(r=>r.text()).then(t=>{
          document.getElementById('mainTitle').innerText = t;
          document.title = t;
        });
      }, 1000);
    </script>
  </body></html>
  )rawliteral";
  html.replace("%TITLE%", currentTitle);
  return html;
}

void setPageTitle(String newTitle) {
  currentTitle = newTitle;
  Serial.println("Title updated to: " + currentTitle);
}

void handleRoot() {
  server.send(200, "text/html", getPage());
}

void handleApiTitle() {
  // Returns just the current title - used by JS
  server.send(200, "text/plain", currentTitle);
}

void handleNotFound() {
  server.send(404, "text/plain", "404 Not Found");
}

void setup() {
  int timeout = 0;

  Serial.begin(115200);
  while (!Serial) { }
  delay(CONNECTION_DELAY_MS);

  // Set static IP - do this before WiFi.begin()
  if (!WiFi.config(local_IP, gateway, subnet, dns1, dns2)) {
    Serial.println("Failed to configure Static IP");
    goto out;
  }

  WiFi.begin(wifi_ssid, wifi_pass);
  WiFi.setAutoReconnect(true);
  
  Serial.print("Connecting to WiFi SSID: ");
  Serial.println(wifi_ssid);

  while (WiFi.status() != WL_CONNECTED && timeout < CONNECTION_TIMEOUT_ITERATIONS) {
    delay(CONNECTION_DELAY_MS);
    Serial.print(".");
    timeout++;
  }

  if (timeout == 40) {
    Serial.println("\nTimeout: failed to connect to WiFi");
    goto out;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConnected!");
    Serial.print("IP: " + WiFi.localIP().toString());
    
    if (MDNS.begin(HOSTNAME)) {
      Serial.print("mDNS: http://"); Serial.print(HOSTNAME); Serial.println(".local");
    }

    server.on("/", handleRoot);
    server.on("/api/title", handleApiTitle);
    server.onNotFound(handleNotFound);
    server.begin();
    Serial.println("Web server started");
  } else {
    Serial.println("\nFailed to connect - check SSID/PASS and IP config");
    goto out;
  }

  return; // Success

out:
  exit_loop();
}

void exit_loop() {
  while (true) {
    Serial.println("Error state");
    delay(2000);
  }
}

void loop() {
  server.handleClient();

  delay(20000);
  setPageTitle("Device IP: " + String(WiFi.localIP().toString()));
}
