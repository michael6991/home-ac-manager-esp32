#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include "config.h"

#define CONNECTION_DELAY_MS 250U
#define CONNECTION_TIMEOUT_ITERATIONS 40U

// HARDCODED IP CONFIGURATION - MUST match your router
// If your router is 192.168.0.1, change all to 192.168.0.x
IPAddress local_IP(10, 100, 102, 142);  // The IP you want for the ESP32 - pick a free one like 100-200
IPAddress gateway(10, 100, 102, 1);     // Your router's IP - usually 192.168.1.1 or 192.168.0.1
IPAddress subnet(255, 255, 255, 0);
IPAddress dns1(8, 8, 8, 8);
IPAddress dns2(8, 8, 4, 4);

const char* HOSTNAME = "esp32"; // you can also open http://esp32.local
String currentTitle = "My ESP32 Web App"; // initial title
String lastReceivedNumber = "none";

WebServer server(80);
// This page auto-updates the title with JS, no need to refresh browser

String getPage() {
  String html = R"rawliteral(
  <!DOCTYPE html><html><head>
    <meta charset="UTF-8"><meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>ESP32</title>
    <style>
      body{font-family:Arial;display:flex;justify-content:center;align-items:center;min-height:100vh;margin:0;background:#f2f2f2}
     .card{background:white;padding:30px 40px;border-radius:16px;box-shadow:0 4px 20px rgba(0,0,0,0.1);text-align:center;min-width:320px}
      input{padding:10px;font-size:18px;width:160px;border-radius:8px;border:1px solid #ccc}
      button{padding:10px 20px;font-size:18px;margin-left:10px;border-radius:8px;border:0;background:#007bff;color:white;cursor:pointer}
      #status{color:green;min-height:20px}
    </style>
  </head><body>
    <div class="card">
      <h1 id="mainTitle">%TITLE%</h1>
      <p>Last number: <b id="lastNum">%LAST%</b></p>
      <hr>
      <input type="number" id="numInput" placeholder="Enter number">
      <button onclick="sendNumber()">Send</button>
      <p id="status"></p>
    </div>
    <script>
      setInterval(() => {
        fetch('/api/title').then(r=>r.text()).then(t=>{
          document.getElementById('mainTitle').innerText = t;
          document.title = t; // FIX: also update browser tab title
        });
      }, 500);

      function sendNumber(){
        let n = document.getElementById('numInput').value;
        if(n==='') return;
        fetch('/set?num=' + n).then(r=>r.text()).then(t=>{
          document.getElementById('status').innerText = t;
          document.getElementById('lastNum').innerText = n;
        });
      }
    </script>
  </body></html>
  )rawliteral";
  html.replace("%TITLE%", currentTitle);
  html.replace("%LAST%", lastReceivedNumber);
  return html;
}

void handleRoot() {
  Serial.print("Request from: ");
  Serial.println(server.client().remoteIP());
  server.send(200, "text/html", getPage());
}

void handleApiTitle() {
  /* Returns just the current title - used by JS */
  server.send(200, "text/plain", currentTitle);
}

void handleSetNumber() {
  if(server.hasArg("num")){
    String numStr = server.arg("num");
    lastReceivedNumber = numStr;
    uint32_t numVal = numStr.toInt();
    Serial.print("User entered: "); Serial.println(numVal);

    /* Example: use the number as new title */
    setPageTitle("Got number: " + numStr);

    server.send(200, "text/plain", "ESP32 received: " + numStr);
  } else {
    server.send(400, "text/plain", "Missing num");
  }
}

void handleNotFound() {
  server.send(404, "text/plain", "404 Not Found");
}

void setPageTitle(String newTitle) {
  currentTitle = newTitle;
}

void infinite_loop() {
  Serial.println("Infinite loop state");
  while (true) {
    delay(2000);
    Serial.print(".");
  }
}

/* Time format hr:min:sec */
static String time_clock = "00:00:00";

int clock_loop_24_hours(uint8_t initial_hour, uint8_t initial_minute) {
  unsigned long previous_millis = 0;
  unsigned long current_millis = 0;
  const long interval = 500;
  uint8_t h, m ,s;
  uint8_t hour, minute;

  if (initial_hour > 24) {
    return -1;
  }

  if (initial_minute > 59) {
    return -1;
  }

  if (initial_hour == 24) {
    hour = 0; /* Convert to midnight */
  } else {
    hour = initial_hour;
  }
  minute = initial_minute;

  /* Start the clock */
  for (h = 0; h < 24; h++) {
    /* Convert to ascii and update clock string */
    time_clock[0] = (hour / 10) + '0';
    time_clock[1] = (hour % 10) + '0';

    for (m = 0; m < 60; m++) {
      time_clock[3] = (minute / 10) + '0';
      time_clock[4] = (minute % 10) + '0';

      for (s = 0; s < 60; s++) {
        /* Measure precise 500ms without causing drift like delay() */
        /* I want the client to be handled twice a second */
        while (true) {
          current_millis = millis(); /* Get the current time */
          if (current_millis - previous_millis >= interval) {
            previous_millis = current_millis;
            break;
          }
        }
        server.handleClient();

        time_clock[6] = (s / 10) + '0';
        time_clock[7] = (s % 10) + '0';

        while (true) {
          current_millis = millis(); /* Get the current time */
          if (current_millis - previous_millis >= interval) {
            previous_millis = current_millis;
            break;
          }
        }

        Serial.println("Time  " + time_clock);
        setPageTitle(time_clock);
        server.handleClient();
      }

      minute++;
      if (minute == 60) {
        minute = 0;
      }
    }

    hour++;
    if (hour == 24) {
      hour = 0;
    }
  }

  /* Clock finished counting 24 hours */
  return 0;
}


void setup() {
  uint32_t timeout = 0;

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

  if (timeout == CONNECTION_TIMEOUT_ITERATIONS) {
    Serial.println("\nTimeout: failed to connect to WiFi");
    goto out;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConnected!");
    Serial.println("IP: " + WiFi.localIP().toString());
    
    if (MDNS.begin(HOSTNAME)) {
      Serial.print("mDNS: http://"); Serial.print(HOSTNAME); Serial.println(".local");
    }

    server.on("/", handleRoot);
    server.on("/api/title", handleApiTitle);
    server.on("/set", handleSetNumber);
    server.onNotFound(handleNotFound);
    server.begin();

    Serial.println("Web server started");
    setPageTitle("Device IP: " + String(WiFi.localIP().toString()));
  } else {
    Serial.println("\nFailed to connect - check SSID/PASS and IP config");
    goto out;
  }

  return; // Success

out:
  infinite_loop();
}

void loop() {
  int rc = 0;
  server.handleClient();

  rc = clock_loop_24_hours(19, 7);
  if (rc) {
    Serial.println("Error intializing 24-hour clock");
    goto out;
  }

  server.handleClient();
  /* */

out:
  infinite_loop();
}
