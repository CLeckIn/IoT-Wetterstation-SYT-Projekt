#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <WebServer.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include <time.h>

// =========================
// WLAN
// =========================

const char* ssid = "S23 Ultra";
const char* password = "luka2010";


// =========================
// TELEGRAM
// =========================

#define BOT_TOKEN "8697671962:AAEsVA89v2OvPXvdE6O8hWAsOZAm1rm_hdY"
#define CHAT_ID "1005537122"

// =========================
// LED PINS
// =========================

#define L1_R 25
#define L1_G 26

#define L2_R 13
#define L2_B 14

// =========================
// DATENSTRUKTUR
// =========================

typedef struct struct_message {

  float temp;
  float press;
  bool magnet;

} struct_message;

struct_message incomingData;

// =========================
// VARIABLEN
// =========================

bool led1Active = true;
bool led2Active = true;

unsigned long lastRx = 0;
unsigned long lastBotCheck = 0;

String lastTime = "--:--:--";

// =========================
// SERVER + TELEGRAM
// =========================

WebServer server(80);

WiFiClientSecure client;

UniversalTelegramBot bot(
  BOT_TOKEN,
  client
);

// =========================
// ZEIT
// =========================

void updateTime() {

  struct tm timeinfo;

  if(getLocalTime(&timeinfo)) {

    char buf[20];

    strftime(
      buf,
      sizeof(buf),
      "%H:%M:%S",
      &timeinfo
    );

    lastTime = String(buf);
  }
}

// =========================
// LEDS
// =========================

void updateLEDs() {

  // =====================
  // LED 1
  // =====================

  if (!led1Active) {

    digitalWrite(L1_R, LOW);
    digitalWrite(L1_G, LOW);

  } else {

    if(incomingData.magnet) {

      digitalWrite(L1_R, LOW);
      digitalWrite(L1_G, HIGH);

    } else {

      digitalWrite(L1_R, HIGH);
      digitalWrite(L1_G, LOW);
    }
  }

  // =====================
  // LED 2
  // =====================

  if (!led2Active) {

    digitalWrite(L2_R, LOW);
    digitalWrite(L2_B, LOW);

  } else {

    if (millis() - lastRx > 30000) {

      digitalWrite(
        L2_R,
        (millis() / 500) % 2
      );

      digitalWrite(L2_B, LOW);

    } else {

      if (incomingData.temp > 28.0) {

        digitalWrite(L2_R, HIGH);
        digitalWrite(L2_B, LOW);

      } else {

        digitalWrite(L2_R, LOW);
        digitalWrite(L2_B, HIGH);
      }
    }
  }
}

// =========================
// TELEGRAM
// =========================

void handleNewMessages(
  int numNewMessages
) {

  for (int i = 0; i < numNewMessages; i++) {

    String text =
      bot.messages[i].text;

    if (text == "/info") {

      updateTime();

      String msg;

      msg += "📊 IoT Bericht\n\n";

      msg += "🌡 Temperatur: ";
      msg += String(incomingData.temp, 2);
      msg += " °C\n";

      msg += "☁ Druck: ";
      msg += String(incomingData.press, 2);
      msg += " hPa\n";

      msg += "🧲 Magnet: ";

      if(incomingData.magnet) {
        msg += "JA ✅";
      } else {
        msg += "NEIN ❌";
      }

      msg += "\n⏰ Zeit: ";
      msg += lastTime;

      bot.sendMessage(
        bot.messages[i].chat_id,
        msg,
        ""
      );
    }
  }
}

// =========================
// ESP NOW
// =========================

void OnDataRecv(
  const esp_now_recv_info_t *info,
  const uint8_t *data,
  int len
) {

  memcpy(
    &incomingData,
    data,
    sizeof(incomingData)
  );

  lastRx = millis();

  updateTime();

  Serial.println(
    "\n===== DATEN EMPFANGEN ====="
  );

  Serial.print("Temperatur: ");
  Serial.println(incomingData.temp);

  Serial.print("Druck: ");
  Serial.println(incomingData.press);

  Serial.print("Magnet: ");
  Serial.println(incomingData.magnet);
}

// =========================
// WEBSEITE
// =========================

void handleRoot() {

  String html;

  html += "<!DOCTYPE html><html><head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta http-equiv='refresh' content='5'>";

  html += "<style>";

  html += "body{";
  html += "font-family:sans-serif;";
  html += "background:#121212;";
  html += "color:white;";
  html += "text-align:center;";
  html += "padding-top:50px;";
  html += "}";

  html += ".card{";
  html += "background:#1e1e1e;";
  html += "padding:30px;";
  html += "border-radius:15px;";
  html += "display:inline-block;";
  html += "border:1px solid #333;";
  html += "}";

  html += ".btn{";
  html += "display:block;";
  html += "margin:10px auto;";
  html += "padding:15px;";
  html += "width:220px;";
  html += "border-radius:10px;";
  html += "font-weight:bold;";
  html += "text-decoration:none;";
  html += "color:black;";
  html += "}";

  html += "</style></head><body>";

  html += "<div class='card'>";

  html += "<h1>🚀 IoT Dashboard</h1>";

  html += "<div style='font-size:3.5em; color:#03dac6;'>";

  html += String(incomingData.temp, 2);

  html += " &deg;C</div>";

  html += "<h3>Druck: ";

  html += String(incomingData.press, 2);

  html += " hPa</h3>";

  html += "<h3>Magnet: ";

  html += (
    incomingData.magnet
    ? "JA ✅"
    : "NEIN ❌"
  );

  html += "</h3>";

  html += "<p style='color:gray;'>";

  html += "Update: ";
  html += lastTime;

  html += "</p>";

  html += "<hr>";

  html += "<a href='/t1' ";
  html += "style='background:#bb86fc;' ";
  html += "class='btn'>LED 1 AN/AUS</a>";

  html += "<a href='/t2' ";
  html += "style='background:#03dac6;' ";
  html += "class='btn'>LED 2 AN/AUS</a>";

  html += "</div></body></html>";

  server.send(
    200,
    "text/html",
    html
  );
}

// =========================
// SETUP
// =========================

void setup() {

  Serial.begin(115200);

  delay(1000);

  pinMode(L1_R, OUTPUT);
  pinMode(L1_G, OUTPUT);

  pinMode(L2_R, OUTPUT);
  pinMode(L2_B, OUTPUT);

  // =====================
  // WLAN
  // =====================

  WiFi.mode(WIFI_STA);

  WiFi.begin(
    ssid,
    password
  );

  Serial.print("Verbinde WLAN");

  while (
    WiFi.status()
    != WL_CONNECTED
  ) {

    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWLAN verbunden");

  Serial.print("IP Adresse: ");
  Serial.println(WiFi.localIP());

  Serial.print("WLAN Kanal: ");
  Serial.println(WiFi.channel());

  Serial.print("MAC Adresse: ");
  Serial.println(WiFi.macAddress());

  // =====================
  // ZEIT
  // =====================

  configTime(
    3600,
    3600,
    "pool.ntp.org"
  );

  // =====================
  // TELEGRAM
  // =====================

  client.setInsecure();

  bot.sendMessage(
    CHAT_ID,
    "✅ ESP32 Empfänger gestartet",
    ""
  );

  // =====================
  // KANAL 11
  // =====================

  esp_wifi_set_channel(
    11,
    WIFI_SECOND_CHAN_NONE
  );

  // =====================
  // ESP NOW
  // =====================

  if (
    esp_now_init()
    != ESP_OK
  ) {

    Serial.println("ESP NOW Fehler");
    return;
  }

  esp_now_register_recv_cb(
    OnDataRecv
  );

  Serial.println("ESP NOW bereit");

  // =====================
  // WEBSERVER
  // =====================

  server.on(
    "/",
    handleRoot
  );

  server.on("/t1", []() {

    led1Active = !led1Active;

    server.sendHeader(
      "Location",
      "/"
    );

    server.send(303);
  });

  server.on("/t2", []() {

    led2Active = !led2Active;

    server.sendHeader(
      "Location",
      "/"
    );

    server.send(303);
  });

  server.begin();

  Serial.println(
    "Webserver gestartet"
  );
}

// =========================
// LOOP
// =========================

void loop() {

  server.handleClient();

  updateLEDs();

  if (
    millis()
    - lastBotCheck
    > 2000
  ) {

    int n =
      bot.getUpdates(
        bot.last_message_received + 1
      );

    if (n > 0) {

      handleNewMessages(n);
    }

    lastBotCheck = millis();
  }
}
