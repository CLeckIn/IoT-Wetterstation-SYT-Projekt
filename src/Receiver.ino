#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <WebServer.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include <time.h>

// Funktionen ankündigen, damit der Compiler weiß, dass sie später kommen
void updateTime();
void updateLEDs();
void handleNewMessages(int numNewMessages);
void handleRoot();
void handleApiData();
void handleApiTemperature();
void handleApiSensor();

// Zugangsdaten für das lokale Netzwerk
const char* ssid = "S23 Ultra";
const char* password = "luka2010";

// Telegram Bot Konfiguration
#define BOT_TOKEN "8697671962:AAEsVA89v2OvPXvdE6O8hWAsOZAm1rm_hdY"
#define CHAT_ID "1005537122"

// Definition der LED-Anschlüsse
#define L1_R 25
#define L1_G 26
#define L2_R 13
#define L2_B 14

// Paketstruktur für den Datenempfang (muss zum Sender passen)
typedef struct struct_message {
  float temp;
  float press;
  bool magnet;
} struct_message;

struct_message incomingData;

// Status-Variablen für die Steuerung und Überwachung
bool led1Active = true;
bool led2Active = true;
unsigned long lastRx = 0;
unsigned long lastBotCheck = 0;
String lastTime = "--:--:--";
bool linkLossAlarmSent = false;

// Speicher für die Extremwerte
float minTemp = 99.0;
float maxTemp = -99.0;

WebServer server(80);
WiFiClientSecure client;
UniversalTelegramBot bot(BOT_TOKEN, client);

// Holt die aktuelle Uhrzeit vom NTP-Server
void updateTime() {
  struct tm timeinfo;
  if(getLocalTime(&timeinfo)) {
    char buf[25];
    strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &timeinfo);
    lastTime = String(buf);
  }
}

// Regelt das Leuchtverhalten der beiden RGB-LEDs
void updateLEDs() {
  // Steuerung der Magnet-LED
  if (!led1Active) {
    digitalWrite(L1_R, LOW); digitalWrite(L1_G, LOW);
  } else {
    if(incomingData.magnet) { 
      digitalWrite(L1_R, LOW); digitalWrite(L1_G, HIGH); 
    } else { 
      digitalWrite(L1_R, HIGH); digitalWrite(L1_G, LOW); 
    }
  }

  // Steuerung der System- und Temperatur-LED
  if (!led2Active) {
    digitalWrite(L2_R, LOW); digitalWrite(L2_B, LOW);
  } else {
    // Wenn über 70 Sekunden keine Daten kommen, blinkt es rot (Sender-Ausfall)
    if (millis() - lastRx > 70000) {
      digitalWrite(L2_R, (millis() / 500) % 2); digitalWrite(L2_B, LOW);
      if (!linkLossAlarmSent && lastRx != 0) {
        bot.sendMessage(CHAT_ID, "⚠️ ALARM: Verbindung zum Sender verloren!", "");
        linkLossAlarmSent = true;
      }
    } else {
      // Normaler Betrieb: Rot bei Hitze, sonst Blau
      if (incomingData.temp > 28.0) { 
        digitalWrite(L2_R, HIGH); digitalWrite(L2_B, LOW); 
      } else { 
        digitalWrite(L2_R, LOW); digitalWrite(L2_B, HIGH); 
      }
    }
  }
}

// JSON-Schnittstellen für externe Abfragen
void handleApiData() {
  String json = "{\"timestamp\":\"" + lastTime + "\",\"temperature\":" + String(incomingData.temp, 2) + ",\"min\":" + String(minTemp, 2) + ",\"max\":" + String(maxTemp, 2) + ",\"pressure\":" + String(incomingData.press, 2) + ",\"sensor\":" + String(incomingData.magnet) + "}";
  server.send(200, "application/json", json);
}

void handleApiTemperature() {
  String json = "{\"temperature\":" + String(incomingData.temp, 2) + ",\"min\":" + String(minTemp, 2) + ",\"max\":" + String(maxTemp, 2) + "}";
  server.send(200, "application/json", json);
}

void handleApiSensor() {
  String json = "{\"sensor\":" + String(incomingData.magnet) + "}";
  server.send(200, "application/json", json);
}

// Verarbeitet eingehende Telegram-Befehle
void handleNewMessages(int numNewMessages) {
  for (int i = 0; i < numNewMessages; i++) {
    String text = bot.messages[i].text;
    String from_id = String(bot.messages[i].chat_id);

    if (text == "/info") {
      updateTime();
      String msg = "📊 IoT Bericht\n\n";
      msg += "🌡 Temp: " + String(incomingData.temp, 2) + " °C\n";
      msg += "❄️ Min: " + String(minTemp, 2) + " | 🔥 Max: " + String(maxTemp, 2) + "\n";
      msg += "☁ Druck: " + String(incomingData.press, 2) + " hPa\n";
      msg += "🧲 Magnet: " + String(incomingData.magnet ? "JA ✅" : "NEIN ❌") + "\n";
      msg += "⏰ Zeit: " + lastTime;
      bot.sendMessage(from_id, msg, "");
    }
    // Fernsteuerung der LEDs via Chat
    else if (text == "/led1_on") { led1Active = true; bot.sendMessage(from_id, "LED 1 AN", ""); }
    else if (text == "/led1_off") { led1Active = false; bot.sendMessage(from_id, "LED 1 AUS", ""); }
    else if (text == "/led2_on") { led2Active = true; bot.sendMessage(from_id, "LED 2 AN", ""); }
    else if (text == "/led2_off") { led2Active = false; bot.sendMessage(from_id, "LED 2 AUS", ""); }
  }
}

// Wird ausgeführt, wenn ein neues Funkpaket ankommt
void OnDataRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  memcpy(&incomingData, data, sizeof(incomingData));
  lastRx = millis();
  updateTime();

  // Min/Max Werte aktualisieren
  if (incomingData.temp < minTemp) minTemp = incomingData.temp;
  if (incomingData.temp > maxTemp) maxTemp = incomingData.temp;

  // Alarm zurücksetzen, wenn der Sender wieder da ist
  if (linkLossAlarmSent) {
    bot.sendMessage(CHAT_ID, "✅ Verbindung wiederhergestellt.", "");
    linkLossAlarmSent = false;
  }
}

// Erstellt die HTML-Seite für das Dashboard
void handleRoot() {
  String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'><meta http-equiv='refresh' content='5'><style>body{font-family:sans-serif;background:#121212;color:white;text-align:center;padding-top:50px;}.card{background:#1e1e1e;padding:30px;border-radius:15px;display:inline-block;border:1px solid #333;}.btn{display:block;margin:10px auto;padding:15px;width:220px;border-radius:10px;font-weight:bold;text-decoration:none;color:black;}</style></head><body><div class='card'><h1>🚀 IoT Dashboard</h1><div style='font-size:3.5em; color:#03dac6;'>" + String(incomingData.temp, 2) + " &deg;C</div>";
  html += "<p style='color:#ffab00;'>Min: " + String(minTemp, 2) + " &deg;C | Max: " + String(maxTemp, 2) + " &deg;C</p>";
  html += "<h3>Druck: " + String(incomingData.press, 2) + " hPa</h3><h3>Magnet: " + String(incomingData.magnet ? "JA ✅" : "NEIN ❌") + "</h3><p style='color:gray;'>Update: " + lastTime + "</p><hr><a href='/t1' style='background:#bb86fc;' class='btn'>LED 1 AN/AUS</a><a href='/t2' style='background:#03dac6;' class='btn'>LED 2 AN/AUS</a></div></body></html>";
  server.send(200, "text/html", html);
}

void setup() {
  Serial.begin(115200);
  pinMode(L1_R, OUTPUT); pinMode(L1_G, OUTPUT);
  pinMode(L2_R, OUTPUT); pinMode(L2_B, OUTPUT);

  // WLAN-Verbindung aufbauen
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }

  // Zeit und Telegram-Sicherheit einstellen
  configTime(3600, 3600, "pool.ntp.org");
  client.setInsecure();
  esp_wifi_set_channel(11, WIFI_SECOND_CHAN_NONE);

  // Funkprotokoll starten
  if (esp_now_init() != ESP_OK) return;
  esp_now_register_recv_cb(OnDataRecv);

  // Webserver-Routen definieren
  server.on("/", handleRoot);
  server.on("/t1", []() { led1Active = !led1Active; server.sendHeader("Location", "/"); server.send(303); });
  server.on("/t2", []() { led2Active = !led2Active; server.sendHeader("Location", "/"); server.send(303); });
  server.on("/api/data", handleApiData);
  server.on("/api/data/temperature", handleApiTemperature);
  server.on("/api/data/sensor", handleApiSensor);
  server.begin();
}

void loop() {
  server.handleClient();
  updateLEDs();

  // Telegram-Bot in festen Intervallen abfragen
  if (millis() - lastBotCheck > 2000) {
    int n = bot.getUpdates(bot.last_message_received + 1);
    if (n > 0) { handleNewMessages(n); }
    lastBotCheck = millis();
  }
}
