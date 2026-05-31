#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <WebServer.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include <time.h>

// Prototypen
void updateTime();
void updateLEDs();
void handleNewMessages(int numNewMessages);
void handleRoot();
void handleApiData();
void handleApiTemperature();
void handleApiSensor();
String getUptime(); // Neu: Hilfsfunktion für Laufzeit

const char* ssid = "Compact";
const char* password = "kroatien1000";
const char* ap_ssid = "IoT-Wetterstation";
const char* ap_password = "12345678";

#define BOT_TOKEN "8494993733:AAE4DeuH7pJgCTzHth_SZnBQrVTAIJhaj7M"
#define CHAT_ID "8494993733"

#define L1_R 25
#define L1_G 26
#define L2_R 13
#define L2_B 14

typedef struct struct_message {
  float temp;
  float press;
  bool magnet;
} struct_message;

struct_message incomingData;
bool led1Active = true;
bool led2Active = true;
unsigned long lastRx = 0;
unsigned long lastBotCheck = 0;
String lastTime = "--:--:--";

float minTemp = 99.0;
float maxTemp = -99.0;

// Neue Variablen für Zusatzfeatures
unsigned long packetCount = 0; // Feature: Paketzähler

WebServer server(80);
WiFiClientSecure client;
UniversalTelegramBot bot(BOT_TOKEN, client);

// Berechnet die Laufzeit des Systems (Feature: Uptime)
String getUptime() {
  unsigned long sec = millis() / 1000;
  unsigned long min = sec / 60;
  unsigned long hr = min / 60;
  char buf[20];
  snprintf(buf, sizeof(buf), "%02lu:%02lu:%02lu", hr, min % 60, sec % 60);
  return String(buf);
}

void updateTime() {
  struct tm timeinfo;
  if(getLocalTime(&timeinfo)) {
    char buf[25];
    strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &timeinfo);
    lastTime = String(buf);
  }
}

void updateLEDs() {
  if (!led1Active) {
    digitalWrite(L1_R, LOW); digitalWrite(L1_G, LOW);
  } else {
    if(incomingData.magnet) { digitalWrite(L1_R, LOW); digitalWrite(L1_G, HIGH); } 
    else { digitalWrite(L1_R, HIGH); digitalWrite(L1_G, LOW); }
  }

  if (!led2Active) {
    digitalWrite(L2_R, LOW); digitalWrite(L2_B, LOW);
  } else {
    if (millis() - lastRx > 70000 && lastRx != 0) {
      digitalWrite(L2_R, (millis() / 500) % 2); digitalWrite(L2_B, LOW);
    } else {
      if (incomingData.temp > 28.0) { digitalWrite(L2_R, HIGH); digitalWrite(L2_B, LOW); } 
      else { digitalWrite(L2_R, LOW); digitalWrite(L2_B, HIGH); }
    }
  }
}

void handleApiData() {
  String json; json.reserve(300);
  json = "{\"timestamp\":\"" + lastTime + "\",\"temperature\":" + String(incomingData.temp, 2) + ",\"min\":" + String(minTemp, 2) + ",\"max\":" + String(maxTemp, 2) + ",\"pressure\":" + String(incomingData.press, 2) + ",\"sensor\":" + String(incomingData.magnet) + ",\"packets\":" + String(packetCount) + ",\"uptime\":\"" + getUptime() + "\"}";
  server.send(200, "application/json", json);
}

void handleApiTemperature() {
  String json; json.reserve(150);
  json = "{\"temperature\":" + String(incomingData.temp, 2) + ",\"min\":" + String(minTemp, 2) + ",\"max\":" + String(maxTemp, 2) + "}";
  server.send(200, "application/json", json);
}

void handleApiSensor() {
  String json; json.reserve(100);
  json = "{\"sensor\":" + String(incomingData.magnet ? "1" : "0") + ",\"packets\":" + String(packetCount) + "}";
  server.send(200, "application/json", json);
}

void handleNewMessages(int numNewMessages) {
  for (int i = 0; i < numNewMessages; i++) {
    String text = bot.messages[i].text;
    String from_id = String(bot.messages[i].chat_id);
    if (text == "/info") {
      updateTime();
      String msg; msg.reserve(400);
      msg = "📊 IoT Bericht\n\n🌡 Temp: " + String(incomingData.temp, 2) + " C\n❄️ Min: " + String(minTemp, 2) + " | 🔥 Max: " + String(maxTemp, 2) + "\n☁ Druck: " + String(incomingData.press, 2) + " hPa\n🧲 Magnet: " + (incomingData.magnet?"JA ✅":"NEIN ❌") + "\n📦 Pakete: " + String(packetCount) + "\n⏱ Uptime: " + getUptime() + "\n⏰ Zeit: " + lastTime;
      bot.sendMessage(from_id, msg, "");
    }
    // Feature: Fern-Reset via Telegram
    else if (text == "/reset") {
      minTemp = 99.0; maxTemp = -99.0; packetCount = 0;
      bot.sendMessage(from_id, "♻️ Statistiken wurden zurückgesetzt!", "");
    }
    else if (text == "/led1_on") { led1Active = true; bot.sendMessage(from_id, "LED 1 AN", ""); }
    else if (text == "/led1_off") { led1Active = false; bot.sendMessage(from_id, "LED 1 AUS", ""); }
    else if (text == "/led2_on") { led2Active = true; bot.sendMessage(from_id, "LED 2 AN", ""); }
    else if (text == "/led2_off") { led2Active = false; bot.sendMessage(from_id, "LED 2 AUS", ""); }
  }
}

void OnDataRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  if (len == sizeof(incomingData)) {
    memcpy(&incomingData, data, sizeof(incomingData));
    lastRx = millis();
    packetCount++; // Feature: Paketzähler erhöhen
    updateTime();
    if (incomingData.temp < minTemp) minTemp = incomingData.temp;
    if (incomingData.temp > maxTemp) maxTemp = incomingData.temp;
  }
}

void handleRoot() {
  String html; html.reserve(2500);
  // Feature: Dynamischer Browser-Tab Titel
  String title = incomingData.magnet ? "✅ System OK" : "⚠️ MAGNET ALARM";
  
  html = "<!DOCTYPE html><html><head><meta charset='UTF-8'><meta http-equiv='refresh' content='5'>";
  html += "<title>" + title + "</title>";
  html += "<style>body{font-family:sans-serif;background:#121212;color:white;text-align:center;padding-top:50px;}.card{background:#1e1e1e;padding:30px;border-radius:15px;display:inline-block;border:1px solid #333;}.btn{display:block;margin:10px auto;padding:15px;width:220px;border-radius:10px;font-weight:bold;text-decoration:none;color:black;}</style></head><body><div class='card'><h1>🚀 IoT Dashboard</h1><div style='font-size:3.5em; color:#03dac6;'>" + String(incomingData.temp, 2) + " &deg;C</div>";
  html += "<p style='color:#ffab00;'>Min: " + String(minTemp, 2) + " | Max: " + String(maxTemp, 2) + "</p>";
  html += "<h3>Druck: " + String(incomingData.press, 2) + " hPa</h3><h3>Magnet: " + String(incomingData.magnet ? "JA ✅" : "NEIN ❌") + "</h3>";
  html += "<p style='color:#555;'>📦 Pakete: " + String(packetCount) + " | ⏱ Uptime: " + getUptime() + "</p>";
  html += "<p style='color:gray;'>Update: " + lastTime + "</p><hr><a href='/t1' style='background:#bb86fc;' class='btn'>LED 1 AN/AUS</a><a href='/t2' style='background:#03dac6;' class='btn'>LED 2 AN/AUS</a></div></body></html>";
  server.send(200, "text/html", html);
}

void setup() {
  Serial.begin(115200);
  pinMode(L1_R, OUTPUT); pinMode(L1_G, OUTPUT);
  pinMode(L2_R, OUTPUT); pinMode(L2_B, OUTPUT);
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(ap_ssid, ap_password, 11);
  WiFi.begin(ssid, password);
  configTime(3600, 3600, "pool.ntp.org");
  client.setInsecure();
  if (esp_now_init() != ESP_OK) return;
  esp_now_register_recv_cb(OnDataRecv);
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
  if (WiFi.status() == WL_CONNECTED && millis() - lastBotCheck > 2000) {
    int n = bot.getUpdates(bot.last_message_received + 1);
    if (n > 0) handleNewMessages(n);
    lastBotCheck = millis();
  }
}
