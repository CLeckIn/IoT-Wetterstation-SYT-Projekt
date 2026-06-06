#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <WebServer.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include <time.h>
#include <WiFiManager.h>

#define REMOTEXY_MODE__WIFI_CLOUD

#define REMOTEXY_WIFI_SSID "Compact"
#define REMOTEXY_WIFI_PASSWORD "kroatien1000"
#define REMOTEXY_CLOUD_SERVER "cloud.remotexy.com"
#define REMOTEXY_CLOUD_PORT 6376
#define REMOTEXY_CLOUD_TOKEN "e6470e01722d0a4fd50cb3eb8d1a9246"

#include <RemoteXY.h>

#pragma pack(push, 1)
uint8_t const PROGMEM RemoteXY_CONF_PROGMEM[] =
  { 255,2,0,0,0,47,0,19,0,0,0,0,31,1,106,200,1,1,2,0,
  2,14,19,44,22,0,2,26,31,31,79,78,0,79,70,70,0,2,16,81,
  44,22,0,2,26,31,31,79,78,0,79,70,70,0 };

struct {
  uint8_t led1;
  uint8_t led2;
  uint8_t connect_flag;
} RemoteXY;
#pragma pack(pop)



void updateTime();
void updateLEDs();
void handleNewMessages(int numNewMessages);
void handleRoot();
void handleApiData();
void handleApiTemperature();
void handleApiSensor();
String getUptime();

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
String lastShortTime = "--:--";

float minTemp = 99.0;
float maxTemp = -99.0;

unsigned long packetCount = 0;

#define HISTORY_SIZE 60

float tempHistory[HISTORY_SIZE];
String timeHistory[HISTORY_SIZE];
int historyIndex = 0;
int historyCount = 0;

float tempSum = 0;
int tempCount = 0;

WebServer server(80);
WiFiClientSecure client;
UniversalTelegramBot bot(BOT_TOKEN, client);

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
    char shortBuf[10];

    strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &timeinfo);
    strftime(shortBuf, sizeof(shortBuf), "%H:%M:%S", &timeinfo);

    lastTime = String(buf);
    lastShortTime = String(shortBuf);
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
    if (millis() - lastRx > 25000 && lastRx != 0) {
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
    else if (text == "/reset") {
      minTemp = 99.0; maxTemp = -99.0; packetCount = 0;
      historyIndex = 0; historyCount = 0; tempSum = 0; tempCount = 0;
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
    packetCount++;
    updateTime();

    if (incomingData.temp < minTemp) minTemp = incomingData.temp;
    if (incomingData.temp > maxTemp) maxTemp = incomingData.temp;

    tempSum += incomingData.temp;
    tempCount++;

    if(tempCount >= 2) {
      tempHistory[historyIndex] = tempSum / tempCount;
      timeHistory[historyIndex] = lastShortTime;

      if(historyCount < HISTORY_SIZE) {
        historyCount++;
      }

      historyIndex++;
      if(historyIndex >= HISTORY_SIZE) {
        historyIndex = 0;
      }

      tempSum = 0;
      tempCount = 0;
    }
  }
}

void handleRoot() {
  String html; html.reserve(12000);
  String title = incomingData.magnet ? "✅ System OK" : "⚠️ MAGNET ALARM";
  
  html = "<!DOCTYPE html><html><head><meta charset='UTF-8'><meta http-equiv='refresh' content='5'>";
  html += "<title>" + title + "</title>";
  html += "<style>";
  html += "body{font-family:sans-serif;background:#121212;color:white;text-align:center;padding:30px;margin:0;}";
  html += ".card{background:#1e1e1e;padding:30px;border-radius:15px;display:inline-block;border:1px solid #333;max-width:900px;width:95%;}";
  html += ".btn{display:block;margin:10px auto;padding:15px;width:220px;border-radius:10px;font-weight:bold;text-decoration:none;color:black;}";
  html += ".graphBox{background:#000000;border:1px solid #555;border-radius:14px;padding:15px;margin-top:15px;overflow-x:auto;}";
  html += "</style></head><body><div class='card'>";

  html += "<h1>🚀 IoT Dashboard</h1>";
  html += "<div style='font-size:3.5em; color:#03dac6;'>" + String(incomingData.temp, 2) + " &deg;C</div>";
  html += "<p style='color:#ffab00;'>Min: " + String(minTemp, 2) + " | Max: " + String(maxTemp, 2) + "</p>";
  html += "<h3>Druck: " + String(incomingData.press, 2) + " hPa</h3>";
  html += "<h3>Magnet: " + String(incomingData.magnet ? "JA ✅" : "NEIN ❌") + "</h3>";
  html += "<p style='color:#aaa;'>📦 Pakete: " + String(packetCount) + " | ⏱ Uptime: " + getUptime() + "</p>";
  html += "<p style='color:gray;'>Update: " + lastTime + "</p>";

  html += "<hr>";
  html += "<a href='/t1' style='background:#bb86fc;' class='btn'>LED 1 AN/AUS</a>";
  html += "<a href='/t2' style='background:#03dac6;' class='btn'>LED 2 AN/AUS</a>";

  html += "<hr><h2>Temperaturverlauf</h2>";
  html += "<p style='color:gray;'>Neuer Durchschnittspunkt ca. alle 20 Sekunden</p>";
  html += "<div class='graphBox'>";
 html += "<svg viewBox='0 0 900 420' width='100%' height='420' style='background:#000000;'>";
  float graphMin = 999;
  float graphMax = -999;

  for(int i = 0; i < historyCount; i++) {
    int idx = (historyIndex - historyCount + i + HISTORY_SIZE) % HISTORY_SIZE;
    if(tempHistory[idx] < graphMin) graphMin = tempHistory[idx];
    if(tempHistory[idx] > graphMax) graphMax = tempHistory[idx];
  }

  if(historyCount >= 2) {
    if(graphMax - graphMin < 1.0) {
      graphMax += 0.5;
      graphMin -= 0.5;
    }

    int left = 80;
    int right = 860;
    int top = 40;
    int bottom = 330;
    int width = right - left;
    int height = bottom - top;

    html += "<rect x='0' y='0' width='900' height='420' rx='12' fill='black'/>";
    html += "<text x='80' y='25' fill='white' font-size='18'>Temperatur in °C</text>";

    for(int i = 0; i <= 4; i++) {
      int y = top + (i * height / 4);
      float value = graphMax - (i * (graphMax - graphMin) / 4.0);

      html += "<line x1='" + String(left) + "' y1='" + String(y) + "' x2='" + String(right) + "' y2='" + String(y) + "' stroke='#333'/>";
      html += "<text x='15' y='" + String(y + 5) + "' fill='#ddd' font-size='14'>" + String(value, 1) + " C</text>";
    }

    html += "<line x1='" + String(left) + "' y1='" + String(top) + "' x2='" + String(left) + "' y2='" + String(bottom) + "' stroke='#aaa' stroke-width='2'/>";
    html += "<line x1='" + String(left) + "' y1='" + String(bottom) + "' x2='" + String(right) + "' y2='" + String(bottom) + "' stroke='#aaa' stroke-width='2'/>";

    String points = "";

    for(int i = 0; i < historyCount; i++) {
      int idx = (historyIndex - historyCount + i + HISTORY_SIZE) % HISTORY_SIZE;

      int x;
      if(historyCount == 1) x = left;
      else x = left + (i * width / (historyCount - 1));

      int y = bottom - ((tempHistory[idx] - graphMin) * height / (graphMax - graphMin));

      points += String(x) + "," + String(y) + " ";
    }

    html += "<polyline points='" + points + "' fill='none' stroke='#03dac6' stroke-width='4' stroke-linecap='round' stroke-linejoin='round'/>";

    for(int i = 0; i < historyCount; i++) {
      int idx = (historyIndex - historyCount + i + HISTORY_SIZE) % HISTORY_SIZE;

      int x;
      if(historyCount == 1) x = left;
      else x = left + (i * width / (historyCount - 1));

      int y = bottom - ((tempHistory[idx] - graphMin) * height / (graphMax - graphMin));

      html += "<circle cx='" + String(x) + "' cy='" + String(y) + "' r='5' fill='#03dac6'/>";
      
      if(i == historyCount - 1 || i % 5 == 0) {
        html += "<text x='" + String(x - 18) + "' y='" + String(y - 12) + "' fill='white' font-size='13'>" + String(tempHistory[idx], 1) + "</text>";
      }
    }

    int firstIdx = (historyIndex - historyCount + HISTORY_SIZE) % HISTORY_SIZE;
    int middlePos = historyCount / 2;
    int middleIdx = (historyIndex - historyCount + middlePos + HISTORY_SIZE) % HISTORY_SIZE;
    int lastIdx = (historyIndex - 1 + HISTORY_SIZE) % HISTORY_SIZE;

    html += "<text x='" + String(left - 20) + "' y='370' fill='#ccc' font-size='14'>" + timeHistory[firstIdx] + "</text>";
    html += "<text x='" + String(left + width / 2 - 30) + "' y='370' fill='#ccc' font-size='14'>" + timeHistory[middleIdx] + "</text>";
    html += "<text x='" + String(right - 55) + "' y='370' fill='#ccc' font-size='14'>" + timeHistory[lastIdx] + "</text>";

    html += "<text x='395' y='405' fill='gray' font-size='14'>Zeit</text>";
  } else {
    html += "<rect x='0' y='0' width='900' height='420' rx='12' fill='#202020'/>";
    html += "<text x='310' y='210' fill='gray' font-size='24'>Noch nicht genug Daten</text>";
    html += "<text x='285' y='245' fill='gray' font-size='16'>Warte ca. 40 Sekunden</text>";
  }

  html += "</svg>";
  html += "</div>";

  html += "</div></body></html>";
  server.send(200, "text/html", html);
}

void setup() {
  Serial.begin(115200);

  pinMode(L1_R, OUTPUT); pinMode(L1_G, OUTPUT);
  pinMode(L2_R, OUTPUT); pinMode(L2_B, OUTPUT);

  WiFi.mode(WIFI_STA);

  WiFiManager wm;
  wm.setConfigPortalTimeout(180);

  bool ok = wm.autoConnect("IoT-Wetterstation-Setup", "12345678");

  if (!ok) {
    Serial.println("WLAN Verbindung fehlgeschlagen. Neustart...");
    ESP.restart();
  }

  Serial.print("MAC: ");
  Serial.println(WiFi.macAddress());

  uint8_t channel;
  wifi_second_chan_t second;
  esp_wifi_get_channel(&channel, &second);

  Serial.print("Kanal: ");
  Serial.println(channel);

  Serial.print("Verbunden! IP-Adresse: ");
  Serial.println(WiFi.localIP());

  RemoteXY_Init();

  RemoteXY.led1 = led1Active;
  RemoteXY.led2 = led2Active;

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
  RemoteXYEngine.handler();

  led1Active = RemoteXY.led1;
  led2Active = RemoteXY.led2;


  server.handleClient();
  updateLEDs();

  if (WiFi.status() == WL_CONNECTED && millis() - lastBotCheck > 2000) {
    int n = bot.getUpdates(bot.last_message_received + 1);
    if (n > 0) handleNewMessages(n);
    lastBotCheck = millis();
  }
}
