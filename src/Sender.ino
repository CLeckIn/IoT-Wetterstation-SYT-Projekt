#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <Wire.h>
#include <Adafruit_BMP280.h>

#define uS_TO_S_FACTOR 1000000ULL
#define TIME_TO_SLEEP 10

uint8_t receiverAddress[] = { 0x20, 0xE7, 0xC8, 0x67, 0x76, 0xB0 };

#define I2C_SDA 32
#define I2C_SCL 33
#define HALL_PIN 34

Adafruit_BMP280 bmp;

typedef struct struct_message {
  float temp;
  float press;
  bool magnet;
} struct_message;

struct_message myData;

void OnDataSent(const wifi_tx_info_t *info, esp_now_send_status_t status) {
  Serial.print("Sende Status: ");
  if(status == ESP_NOW_SEND_SUCCESS) { Serial.println("OK"); } 
  else { Serial.println("FEHLER"); }
  Serial.println("Deep Sleep startet...");
  delay(1000);
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  esp_deep_sleep_start();
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n SENDER WACH");
  pinMode(HALL_PIN, INPUT);
  Wire.begin(I2C_SDA, I2C_SCL);
  if (bmp.begin(0x76)) {
    myData.temp = bmp.readTemperature();
    myData.press = bmp.readPressure() / 100.0F;
  } else {
    myData.temp = 0;
    myData.press = 0;
  }
  myData.magnet = (digitalRead(HALL_PIN) == LOW);

  WiFi.mode(WIFI_STA);
  
  // Kanal 11 erzwingen (muss zum AP passen)
  esp_wifi_set_channel(11, WIFI_SECOND_CHAN_NONE);

  if (esp_now_init() != ESP_OK) return;
  esp_now_register_send_cb(OnDataSent);

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, receiverAddress, 6);
  peerInfo.channel = 11;
  peerInfo.encrypt = false;
  esp_now_add_peer(&peerInfo);
  
  esp_now_send(receiverAddress, (uint8_t *) &myData, sizeof(myData));
}

void loop() {}
