/**
 * Receiver.ino
 *
 * ESP32 Empfänger – IoT Wetterstation SYT-Projekt
 *
 * Aufgaben:
 *   - Empfängt Sensordaten (Temp, Druck, Magnet) per ESP-NOW vom Sender-ESP32
 *   - Verbindet sich per WiFiManager mit dem WLAN (Auto-Konfigurations-Portal)
 *   - Synchronisiert die Uhrzeit per NTP
 *   - Betreibt ein Webserver-Dashboard (Port 80, Dark Mode)
 *   - Unterstützt Telegram-Bot (/info Befehl)
 *   - Unterstützt RemoteXY-Smartphone-Steuerung
 *   - Steuert zwei RGB-LEDs:
 *       LED 1 (Magnetstatus): Grün = Magnet, Rot = kein Magnet
 *       LED 2 (Systemstatus): Blau = normal, Rot = Temp > 28°C,
 *                              Cyan = Temp < 18°C, Orange blinkend = Sender offline
 *
 * Bibliotheken:
 *   - WiFiManager       by tzapu     (Library Manager)
 *   - UniversalTelegramBot             (Library Manager)
 *   - ArduinoJson       ≥ 6.x         (Library Manager)
 *   - RemoteXY                         (Library Manager / remotexy.com)
 *   - esp_now.h, WiFi.h, WebServer.h  (im ESP32-Core enthalten)
 *
 * Geheimnisse: Kopiere src/secrets.h.example nach src/secrets.h und
 *              trage deine Zugangsdaten ein. Die Datei wird nicht eingecheckt.
 *
 * Pinbelegung:
 *   LED 1 (Magnetstatus)   R  --> GPIO 25
 *                          G  --> GPIO 26
 *                          B  --> GPIO 27
 *   LED 2 (Systemstatus)   R  --> GPIO 14
 *                          G  --> GPIO 12
 *                          B  --> GPIO 13
 */

// ---------------------------------------------------------------------------
// Includes
// ---------------------------------------------------------------------------

#include "secrets.h"           // BOT_TOKEN, CHAT_ID, NTP_SERVER, TZ_STRING

#include <esp_now.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <WebServer.h>
#include <time.h>

// Telegram
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>

// RemoteXY
#define REMOTEXY_MODE__WIFI_POINT
#include <RemoteXY.h>

// ---------------------------------------------------------------------------
// Konfiguration
// ---------------------------------------------------------------------------

// -- RGB LED 1: Magnetstatus --
static const int LED1_R = 25;
static const int LED1_G = 26;
static const int LED1_B = 27;

// -- RGB LED 2: Systemstatus --
static const int LED2_R = 14;
static const int LED2_G = 12;
static const int LED2_B = 13;

// Temperaturschwellen
static const float TEMP_HIGH  = 28.0F;  // °C → LED2 Rot
static const float TEMP_LOW   = 18.0F;  // °C → LED2 Cyan

// Sender als "offline" werten nach X Millisekunden ohne Paket
static const uint32_t SENDER_TIMEOUT_MS = 10000;

// Telegram: Bot-Abfrage-Intervall
static const uint32_t TELEGRAM_INTERVAL_MS = 3000;

// ---------------------------------------------------------------------------
// Datenstruktur (muss mit Sender.ino übereinstimmen)
// ---------------------------------------------------------------------------

typedef struct struct_message {
    float temp;
    float press;
    int   hallRaw;
    bool  magnet;
} struct_message;

// ---------------------------------------------------------------------------
// RemoteXY Konfiguration (erzeugt mit remotexy.com)
// ---------------------------------------------------------------------------
// Angepasst auf ein einfaches Display mit Temperatur- und Druckanzeige.
// REMOTEXY_CONF: 1 Seite, 2 Text-Elemente (temp, press)
#pragma pack(push, 1)
static const uint8_t REMOTEXY_CONF[] = {
    255, 1, 0, 0, 0, 24, 0, 17, 8, 201,
    // Textelement 1 (Temperatur)
    129, 0, 10, 10, 80, 12, 0, 84, 101, 109,
    112, 58, 32, 0,
    // Textelement 2 (Druck)
    129, 0, 10, 30, 80, 12, 0, 68, 114, 117,
    99, 107, 58, 32, 0
};
#pragma pack(pop)

struct {
    // Kein Steuer-Input – nur Anzeige
    uint8_t connect_flag;  ///< Verbindungsstatus (wird von RemoteXY gesetzt)
} remotexyData;

// ---------------------------------------------------------------------------
// Globale Variablen
// ---------------------------------------------------------------------------

static struct_message rxData;           ///< Zuletzt empfangene Sensordaten
static volatile bool  newDataAvailable = false;  ///< Flag: neues Paket empfangen
static uint32_t       lastPacketTime   = 0;      ///< millis() beim letzten Empfang

static WebServer      webServer(80);
static WiFiClientSecure botClient;
static UniversalTelegramBot bot(BOT_TOKEN, botClient);
static uint32_t       lastTelegramCheck = 0;

// Für Orange-Blinken (LED2 Offline-Status)
static bool           blinkState       = false;
static uint32_t       lastBlinkTime    = 0;
static const uint32_t BLINK_INTERVAL_MS = 500;

// ---------------------------------------------------------------------------
// Hilfsfunktion: RGB-LED setzen
// ---------------------------------------------------------------------------

/**
 * Setzt eine RGB-LED auf eine bestimmte Farbe.
 * @param r  GPIO-Pin für Rot
 * @param g  GPIO-Pin für Grün
 * @param b  GPIO-Pin für Blau
 * @param rv Rotwert    (0 = aus, 1 = an)
 * @param gv Grünwert   (0 = aus, 1 = an)
 * @param bv Blauwert   (0 = aus, 1 = an)
 */
static void setLED(int r, int g, int b, bool rv, bool gv, bool bv) {
    digitalWrite(r, rv ? HIGH : LOW);
    digitalWrite(g, gv ? HIGH : LOW);
    digitalWrite(b, bv ? HIGH : LOW);
}

// Vordefinierte Farben als Inline-Helfer
static inline void led1Green()  { setLED(LED1_R, LED1_G, LED1_B, false, true,  false); }
static inline void led1Red()    { setLED(LED1_R, LED1_G, LED1_B, true,  false, false); }
static inline void led2Blue()   { setLED(LED2_R, LED2_G, LED2_B, false, false, true ); }
static inline void led2Red()    { setLED(LED2_R, LED2_G, LED2_B, true,  false, false); }
static inline void led2Cyan()   { setLED(LED2_R, LED2_G, LED2_B, false, true,  true ); }
static inline void led2Orange() { setLED(LED2_R, LED2_G, LED2_B, true,  true,  false); }  // Gelb/Orange
static inline void led2Off()    { setLED(LED2_R, LED2_G, LED2_B, false, false, false); }

// ---------------------------------------------------------------------------
// ESP-NOW Empfangs-Callback
// ---------------------------------------------------------------------------

/**
 * Wird im ISR-Kontext aufgerufen – so kurz wie möglich halten!
 * Pakete werden nur übernommen wenn die Größe stimmt.
 */
void onDataReceived(const uint8_t *mac, const uint8_t *data, int len) {
    if (len != sizeof(struct_message)) {
        // Falsche Paketgröße → verwerfen
        return;
    }
    memcpy(&rxData, data, sizeof(struct_message));
    newDataAvailable = true;
    lastPacketTime   = millis();  // millis() ist im ISR-Kontext sicher
}

// ---------------------------------------------------------------------------
// Webserver: HTML-Dashboard
// ---------------------------------------------------------------------------

static String getFormattedTime() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        return "Zeit nicht verfügbar";
    }
    char buf[32];
    strftime(buf, sizeof(buf), "%d.%m.%Y  %H:%M:%S", &timeinfo);
    return String(buf);
}

static void handleRoot() {
    bool offline = (millis() - lastPacketTime > SENDER_TIMEOUT_MS);

    String html = R"rawhtml(<!DOCTYPE html>
<html lang="de">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<meta http-equiv="refresh" content="10">
<title>IoT Wetterstation</title>
<style>
  body{background:#121212;color:#e0e0e0;font-family:Arial,sans-serif;margin:0;padding:20px;}
  h1{color:#90caf9;text-align:center;}
  .card{background:#1e1e1e;border-radius:10px;padding:20px;margin:10px auto;max-width:400px;}
  .label{color:#9e9e9e;font-size:0.85em;}
  .value{font-size:2em;font-weight:bold;color:#80cbc4;}
  .offline{color:#ef9a9a;}
  .online{color:#a5d6a7;}
  footer{text-align:center;margin-top:20px;color:#616161;font-size:0.8em;}
</style>
</head>
<body>
<h1>&#127756; IoT Wetterstation</h1>
<div class="card">
  <div class="label">Uhrzeit</div>
  <div class="value" style="font-size:1.2em;">)rawhtml";
    html += getFormattedTime();
    html += R"rawhtml(</div>
</div>
<div class="card">
  <div class="label">Temperatur</div>
  <div class="value">)rawhtml";
    html += offline ? "<span class='offline'>Sender offline</span>"
                    : String(rxData.temp, 1) + " &deg;C";
    html += R"rawhtml(</div>
</div>
<div class="card">
  <div class="label">Luftdruck</div>
  <div class="value">)rawhtml";
    html += offline ? "<span class='offline'>–</span>"
                    : String(rxData.press, 1) + " hPa";
    html += R"rawhtml(</div>
</div>
<div class="card">
  <div class="label">Magnetstatus</div>
  <div class="value">)rawhtml";
    if (offline) {
        html += "<span class='offline'>–</span>";
    } else {
        html += rxData.magnet
                ? "<span class='online'>&#9679; Magnet erkannt</span>"
                : "<span class='offline'>&#9675; Kein Magnet</span>";
    }
    html += R"rawhtml(</div>
</div>
<div class="card">
  <div class="label">Sender</div>
  <div class="value">)rawhtml";
    html += offline ? "<span class='offline'>OFFLINE</span>"
                    : "<span class='online'>ONLINE</span>";
    html += R"rawhtml(</div>
</div>
<footer>Automatische Aktualisierung alle 10 Sekunden &bull; IoT Wetterstation SYT-Projekt</footer>
</body>
</html>)rawhtml";

    webServer.send(200, "text/html; charset=UTF-8", html);
}

static void handleNotFound() {
    webServer.send(404, "text/plain", "404 – Seite nicht gefunden");
}

// ---------------------------------------------------------------------------
// LED-Update-Logik
// ---------------------------------------------------------------------------

static void updateLEDs() {
    bool offline = (millis() - lastPacketTime > SENDER_TIMEOUT_MS);

    // --- LED 1: Magnetstatus ---
    if (!offline && rxData.magnet) {
        led1Green();
    } else {
        led1Red();
    }

    // --- LED 2: Systemstatus ---
    if (offline) {
        // Orange blinkend
        uint32_t now = millis();
        if (now - lastBlinkTime >= BLINK_INTERVAL_MS) {
            lastBlinkTime = now;
            blinkState    = !blinkState;
        }
        if (blinkState) {
            led2Orange();
        } else {
            led2Off();
        }
    } else if (rxData.temp > TEMP_HIGH) {
        led2Red();
    } else if (rxData.temp < TEMP_LOW) {
        led2Cyan();
    } else {
        led2Blue();
    }
}

// ---------------------------------------------------------------------------
// Telegram: Nachrichten verarbeiten
// ---------------------------------------------------------------------------

static void handleTelegramMessages(int numMessages) {
    for (int i = 0; i < numMessages; i++) {
        String chat_id = bot.messages[i].chat_id;
        String text    = bot.messages[i].text;

        if (text == "/info") {
            bool offline = (millis() - lastPacketTime > SENDER_TIMEOUT_MS);
            String reply;
            if (offline) {
                reply = "⚠️ *Sender offline!*\nKeine aktuellen Daten verfügbar.";
            } else {
                reply  = "🌡 *Wetterstation – Aktuelle Daten*\n";
                reply += "Temperatur: " + String(rxData.temp, 1) + " °C\n";
                reply += "Luftdruck:  " + String(rxData.press, 1) + " hPa\n";
                reply += "Magnet:     ";
                reply += rxData.magnet ? "Ja ✅" : "Nein ❌";
                reply += "\nZeit: " + getFormattedTime();
            }
            bot.sendMessage(chat_id, reply, "Markdown");
        } else if (text == "/start") {
            bot.sendMessage(chat_id,
                "Hallo! 👋\nVerfügbare Befehle:\n/info – Aktuelle Messdaten anzeigen",
                "");
        }
    }
}

// ---------------------------------------------------------------------------
// Setup
// ---------------------------------------------------------------------------

void setup() {
    Serial.begin(115200);
    Serial.println("\n=== Empfänger ESP32 – IoT Wetterstation ===");

    // --- LED-Pins konfigurieren ---
    int ledPins[] = {LED1_R, LED1_G, LED1_B, LED2_R, LED2_G, LED2_B};
    for (int pin : ledPins) {
        pinMode(pin, OUTPUT);
        digitalWrite(pin, LOW);
    }

    // --- WiFi (mit Auto-Konfigurations-Portal) ---
    WiFiManager wifiManager;
    wifiManager.setConfigPortalTimeout(180);  // Portal nach 3 Min schließen
    if (!wifiManager.autoConnect("Wetterstation-Setup")) {
        Serial.println("[WARN] WiFi-Verbindung fehlgeschlagen – Neustart...");
        ESP.restart();
    }
    Serial.print("[OK] WiFi verbunden – IP: ");
    Serial.println(WiFi.localIP());

    // --- NTP Zeitsynchronisation ---
    configTzTime(TZ_STRING, NTP_SERVER);
    Serial.println("[INFO] Warte auf NTP-Synchronisation...");
    struct tm timeinfo;
    int ntpRetries = 0;
    while (!getLocalTime(&timeinfo) && ntpRetries < 10) {
        delay(500);
        ntpRetries++;
    }
    if (ntpRetries < 10) {
        Serial.printf("[OK] Zeit synchronisiert: %s", asctime(&timeinfo));
    } else {
        Serial.println("[WARN] NTP-Synchronisation fehlgeschlagen");
    }

    // --- Webserver ---
    webServer.on("/", handleRoot);
    webServer.onNotFound(handleNotFound);
    webServer.begin();
    Serial.println("[OK] Webserver gestartet");

    // --- Telegram ---
    // Hinweis: setInsecure() deaktiviert die TLS-Zertifikatsprüfung.
    // Für Schulprojekte akzeptabel; in produktiven Systemen sollte stattdessen
    // das Telegram-Root-Zertifikat (ISRG Root X1) per setCACert() eingebunden werden.
    botClient.setInsecure();
    Serial.println("[OK] Telegram-Bot bereit");

    // --- RemoteXY ---
    RemoteXY_Init();
    Serial.println("[OK] RemoteXY initialisiert");

    // --- ESP-NOW ---
    WiFi.mode(WIFI_AP_STA);   // Gleichzeitig WiFi-Station + ESP-NOW
    if (esp_now_init() != ESP_OK) {
        Serial.println("[FEHLER] ESP-NOW Init fehlgeschlagen!");
        while (true) { delay(1000); }
    }
    esp_now_register_recv_cb(onDataReceived);
    Serial.println("[OK] ESP-NOW bereit");
    Serial.print("[INFO] Empfänger-MAC: ");
    Serial.println(WiFi.macAddress());

    // Initiale LED-Zustände (Sender zunächst als offline behandeln)
    lastPacketTime = 0;
    updateLEDs();
}

// ---------------------------------------------------------------------------
// Hauptschleife
// ---------------------------------------------------------------------------

void loop() {
    // --- Webserver-Anfragen abarbeiten ---
    webServer.handleClient();

    // --- RemoteXY-Handler aufrufen ---
    RemoteXY_Handler();

    // --- Neue ESP-NOW Daten verarbeiten ---
    if (newDataAvailable) {
        newDataAvailable = false;
        Serial.printf("[DATEN] Temp: %.1f°C | Druck: %.1f hPa | Magnet: %s\n",
                      rxData.temp, rxData.press, rxData.magnet ? "JA" : "NEIN");
    }

    // --- LED-Status aktualisieren ---
    updateLEDs();

    // --- Telegram-Bot abfragen ---
    uint32_t now = millis();
    if (now - lastTelegramCheck >= TELEGRAM_INTERVAL_MS) {
        lastTelegramCheck = now;
        int numMsg = bot.getUpdates(bot.last_message_received + 1);
        if (numMsg > 0) {
            handleTelegramMessages(numMsg);
        }
    }
}
