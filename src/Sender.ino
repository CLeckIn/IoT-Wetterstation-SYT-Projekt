/**
 * Sender.ino
 *
 * ESP32 Sender – IoT Wetterstation SYT-Projekt
 *
 * Aufgaben:
 *   - Liest Temperatur und Luftdruck vom BMP280-Sensor (I2C)
 *   - Liest den Digitalstatus des LM393-Hallsensors (GPIO)
 *   - Überträgt die Messdaten per ESP-NOW an den Empfänger-ESP32
 *
 * Bibliotheken:
 *   - Adafruit BMP280 (via Arduino Library Manager)
 *   - esp_now.h, WiFi.h (im ESP32-Arduino-Core enthalten)
 *
 * Pinbelegung:
 *   BMP280  SDA  --> GPIO 21
 *   BMP280  SCL  --> GPIO 22
 *   LM393   DO   --> GPIO 34  (nur Eingang, kein Pullup nötig)
 */

#include <esp_now.h>
#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_BMP280.h>

// ---------------------------------------------------------------------------
// Konfiguration
// ---------------------------------------------------------------------------

/**
 * MAC-Adresse des Empfänger-ESP32 (in HEX, ohne Doppelpunkte).
 *
 * WICHTIG: Ersetze diese Broadcast-Platzhalteradresse (0xFF:0xFF:...) durch die
 *          echte MAC-Adresse deines Empfänger-ESP32!
 *          Die MAC wird im Serial Monitor des Empfängers angezeigt (setup()).
 *
 * Beispiel: {0xAB, 0xCD, 0xEF, 0x12, 0x34, 0x56}
 *
 * Hinweis: Die aktuelle Broadcast-Adresse (alle 0xFF) sendet an alle
 *          ESP-NOW-fähigen Geräte in Reichweite und sollte nur zum Testen
 *          verwendet werden.
 */
static const uint8_t RECEIVER_MAC[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

/** GPIO-Pin des LM393 Digital-Ausgangs */
static const int PIN_HALL = 34;

/** Sendeintervall in Millisekunden */
static const uint32_t SEND_INTERVAL_MS = 2000;

// ---------------------------------------------------------------------------
// Datenstruktur (muss mit Receiver.ino übereinstimmen)
// ---------------------------------------------------------------------------

typedef struct struct_message {
    float temp;      ///< Temperatur in °C
    float press;     ///< Luftdruck in hPa
    int   hallRaw;   ///< Rohwert des Hall-Sensors (0 = Magnet erkannt, 1 = kein Magnet)
    bool  magnet;    ///< true wenn Magnet in der Nähe
} struct_message;

// ---------------------------------------------------------------------------
// Globale Variablen
// ---------------------------------------------------------------------------

static Adafruit_BMP280 bmp;
static struct_message  payload;
static bool            peerRegistered = false;
static uint32_t        lastSendTime   = 0;

// ---------------------------------------------------------------------------
// ESP-NOW Callback: wird nach jedem Sendeversuch aufgerufen
// ---------------------------------------------------------------------------

void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    // Callback soll kurz bleiben – nur Status im Serial ausgeben
    Serial.print("[ESP-NOW] Sendestatus: ");
    Serial.println(status == ESP_NOW_SEND_SUCCESS ? "OK" : "FEHLER");
}

// ---------------------------------------------------------------------------
// Setup
// ---------------------------------------------------------------------------

void setup() {
    Serial.begin(115200);
    Serial.println("\n=== Sender ESP32 – IoT Wetterstation ===");

    // --- Hall-Sensor Pin ---
    pinMode(PIN_HALL, INPUT);

    // --- BMP280 initialisieren ---
    Wire.begin();
    if (!bmp.begin(0x76)) {          // Adresse 0x76 oder 0x77 je nach Modul
        Serial.println("[FEHLER] BMP280 nicht gefunden! Kabel & Adresse prüfen.");
        // In einer Endlosschleife blockieren, damit der Fehler sichtbar bleibt
        while (true) { delay(1000); }
    }
    // Empfohlene Einstellungen für Wetterstationsbetrieb (Adafruit-Beispiel)
    bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,
                    Adafruit_BMP280::SAMPLING_X2,    // Temperatur
                    Adafruit_BMP280::SAMPLING_X16,   // Druck
                    Adafruit_BMP280::FILTER_X16,
                    Adafruit_BMP280::STANDBY_MS_500);
    Serial.println("[OK] BMP280 initialisiert");

    // --- WiFi im Station-Modus (für ESP-NOW benötigt, kein AP nötig) ---
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    Serial.print("[INFO] Sender-MAC: ");
    Serial.println(WiFi.macAddress());

    // --- ESP-NOW initialisieren ---
    if (esp_now_init() != ESP_OK) {
        Serial.println("[FEHLER] ESP-NOW Init fehlgeschlagen!");
        while (true) { delay(1000); }
    }
    esp_now_register_send_cb(onDataSent);

    // --- Peer (Empfänger) registrieren ---
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, RECEIVER_MAC, 6);
    peerInfo.channel = 0;   // 0 = aktueller Kanal
    peerInfo.encrypt = false;

    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("[FEHLER] Peer konnte nicht hinzugefügt werden!");
        while (true) { delay(1000); }
    }
    peerRegistered = true;
    Serial.println("[OK] ESP-NOW bereit");
}

// ---------------------------------------------------------------------------
// Hauptschleife
// ---------------------------------------------------------------------------

void loop() {
    uint32_t now = millis();
    if (now - lastSendTime < SEND_INTERVAL_MS) {
        return;  // Noch nicht Zeit für die nächste Messung
    }
    lastSendTime = now;

    // --- Sensordaten lesen ---
    float temperature = bmp.readTemperature();  // °C
    float pressure    = bmp.readPressure() / 100.0F;  // Pa → hPa
    int   hallRaw     = digitalRead(PIN_HALL);   // 0 = Magnet aktiv (Low-aktiv)
    bool  magnet      = (hallRaw == LOW);        // Magnet erkannt wenn LOW

    // Plausibilitätsprüfung: BMP280 liefert bei Ausfall 0 oder NaN
    if (isnan(temperature) || isnan(pressure) || pressure < 800.0F || pressure > 1100.0F) {
        Serial.println("[WARNUNG] Ungültige Sensordaten – Paket wird nicht gesendet");
        return;
    }

    // --- Payload befüllen ---
    payload.temp    = temperature;
    payload.press   = pressure;
    payload.hallRaw = hallRaw;
    payload.magnet  = magnet;

    // --- Debug-Ausgabe ---
    Serial.printf("[MESSUNG] Temp: %.2f°C  |  Druck: %.2f hPa  |  Magnet: %s\n",
                  temperature, pressure, magnet ? "JA" : "NEIN");

    // --- Senden ---
    esp_err_t result = esp_now_send(RECEIVER_MAC,
                                    reinterpret_cast<const uint8_t*>(&payload),
                                    sizeof(payload));
    if (result != ESP_OK) {
        Serial.printf("[FEHLER] esp_now_send: 0x%X\n", result);
    }
}
