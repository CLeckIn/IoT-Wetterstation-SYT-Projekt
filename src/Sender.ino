#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <Wire.h>
#include <Adafruit_BMP280.h>

// Zeit-Definitionen für den Stromsparmodus
#define uS_TO_S_FACTOR 1000000ULL
#define TIME_TO_SLEEP 15

// MAC-Adresse des Ziel-Empfängers
uint8_t receiverAddress[] = { 0x20, 0xE7, 0xC8, 0x67, 0x76, 0xB0 };

// Pin-Belegung für I2C und den Sensor
#define I2C_SDA 32
#define I2C_SCL 33
#define HALL_PIN 34

Adafruit_BMP280 bmp;

// Struktur für das Datenpaket
typedef struct struct_message {
  float temp;
  float press;
  bool magnet;
} struct_message;

struct_message myData;

// Wird aufgerufen, wenn das Paket verschickt wurde
void OnDataSent(const wifi_tx_info_t *info, esp_now_send_status_t status) {
  Serial.print("Sende Status: ");
  if(status == ESP_NOW_SEND_SUCCESS) { 
    Serial.println("OK"); 
  } else { 
    Serial.println("FEHLER"); 
  }

  // Nach dem Senden geht es direkt wieder in den Schlafmodus
  Serial.println("Deep Sleep startet...");
  delay(1000);
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  esp_deep_sleep_start();
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n===== SENDER WACH =====");

  pinMode(HALL_PIN, INPUT);
  Wire.begin(I2C_SDA, I2C_SCL);

  // Sensor auslesen
  if (bmp.begin(0x76)) {
    Serial.println("BMP280 gefunden");
    myData.temp = bmp.readTemperature();
    myData.press = bmp.readPressure() / 100.0F;
  } else {
    Serial.println("BMP280 NICHT gefunden");
    myData.temp = 0;
    myData.press = 0;
  }

  // Magnet-Zustand erfassen (LOW bedeutet Magnet ist nah)
  myData.magnet = (digitalRead(HALL_PIN) == LOW);

  // Debug-Ausgabe der Messwerte
  Serial.print("Temp: ");
  Serial.print(myData.temp);
  Serial.print(" C | Druck: ");
  Serial.print(myData.press);
  Serial.print(" hPa | Magnet: ");
  Serial.println(myData.magnet);

  // Funk-Konfiguration auf Kanal 11
  WiFi.mode(WIFI_STA);
  esp_wifi_set_channel(11, WIFI_SECOND_CHAN_NONE);

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP NOW Fehler");
    return;
  }

  esp_now_register_send_cb(OnDataSent);

  // Empfänger-Informationen registrieren
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, receiverAddress, 6);
  peerInfo.channel = 11;
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Peer Fehler");
    return;
  }

  // Daten abschicken
  Serial.println("Senden gestartet");
  esp_err_t result = esp_now_send(receiverAddress, (uint8_t *) &myData, sizeof(myData));

  if(result != ESP_OK) {
    Serial.println("Sende Fehler");
  }
}

void loop() {
  // Bleibt leer, da alles im Setup passiert und dann geschlafen wird
}
