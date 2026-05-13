#include <esp_now.h>
#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_BMP280.h>

// --- SLEEP SETTINGS ---
#define uS_TO_S_FACTOR 1000000ULL 
#define TIME_TO_SLEEP  15          // Schläft 15 Sekunden für den Test

uint8_t receiverAddress[] = {0x20, 0xE7, 0xC8, 0x67, 0x76, 0xB0}; // MAC prüfen!
const char* ssid = "Compact";
const char* password = "kroatien1000";

#define I2C_SDA 32
#define I2C_SCL 33
#define HALL_PIN 34

Adafruit_BMP280 bmp;
typedef struct struct_message { float temp; float press; int hallRaw; bool magnet; } struct_message;
struct_message myData;

// Callback wenn gesendet wurde (Hier kommt das "Funk: OK")
void OnDataSent(const wifi_tx_info_t *info, esp_now_send_status_t status) {
  Serial.print(" >>> Funk-Status: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "OK (Empfangen!)" : "FEHLER");
  
  // Erst wenn der Status gedruckt wurde, schlafen gehen
  Serial.println("Gehe jetzt schlafen...");
  Serial.flush(); 
  esp_deep_sleep_start();
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\nSENDER WACH");

  // 1. Sensoren lesen (n=10 Mittelwert)
  Wire.begin(I2C_SDA, I2C_SCL);
  pinMode(HALL_PIN, INPUT_PULLUP);
  
  if (bmp.begin(0x76)) {
    float t_sum = 0, p_sum = 0;
    for(int i=0; i<10; i++) {
      t_sum += bmp.readTemperature();
      p_sum += bmp.readPressure() / 100.0F;
      delay(20);
    }
    myData.temp = t_sum / 10.0;
    myData.press = p_sum / 10.0;
  }
  myData.magnet = (digitalRead(HALL_PIN) == LOW);

  // 2. Alles drucken (Temperatur, Druck, Magnet)
  Serial.printf("MESSUNG: %.2f C | %.1f hPa | Magnet: %d", myData.temp, myData.press, myData.magnet);

  // 3. WiFi & ESP-NOW
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 5000) { delay(100); }

  if (esp_now_init() == ESP_OK) {
    esp_now_register_send_cb(OnDataSent);
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, receiverAddress, 6);
    peerInfo.channel = WiFi.channel(); 
    peerInfo.encrypt = false;
    esp_now_add_peer(&peerInfo);
    
    // Senden auslösen
    esp_now_send(receiverAddress, (uint8_t *) &myData, sizeof(myData));
  } else {
    esp_deep_sleep_start();
  }

  // Falls der Callback nicht kommt, nach 5 Sek trotzdem schlafen
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  delay(5000);
  esp_deep_sleep_start();
}

void loop() {}
