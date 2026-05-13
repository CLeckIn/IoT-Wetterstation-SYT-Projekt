# Komponenten-Liste – IoT Wetterstation

Alle verwendeten Bauteile und Materialien für das SYT-Projekt.

---

## Mikrocontroller

| Anzahl | Bezeichnung           | Beschreibung                                              |
|--------|-----------------------|-----------------------------------------------------------|
| 2×     | **ESP32 DevKit v1**   | WLAN/Bluetooth-Mikrocontroller; einer als Sender, einer als Empfänger |

---

## Sensoren

| Anzahl | Bezeichnung           | Schnittstelle | Beschreibung                              |
|--------|-----------------------|---------------|-------------------------------------------|
| 1×     | **BMP280**            | I2C (0x76)    | Temperatur- & Luftdrucksensor             |
| 1×     | **LM393 Hallsensor**  | Digital (DO)  | Erkennt Magnetfelder; Digitalausgang Low-aktiv |

---

## Anzeige / Aktoren

| Anzahl | Bezeichnung             | Beschreibung                                                |
|--------|-------------------------|-------------------------------------------------------------|
| 2×     | **RGB-LED (gemeinsame Kathode)** | LED 1: Magnetstatus (Grün/Rot); LED 2: Systemstatus (Blau/Rot/Cyan/Orange) |
| 6×     | **Widerstand 220 Ω**    | Vorwiderstände für die LED-Pins (je R, G, B pro LED)       |

---

## Verbindungsmaterial

| Anzahl | Bezeichnung              | Beschreibung                     |
|--------|--------------------------|----------------------------------|
| 2×     | **Breadboard**           | Steckplatine für den Prototypen  |
| ~30×   | **Jumper-Kabel M–M**     | Verbindungskabel                 |
| 1×     | **USB-Kabel (Micro-USB)**| Stromversorgung / Programmierung |

---

## Software / Bibliotheken

| Bibliothek              | Version   | Verwendung                           |
|-------------------------|-----------|--------------------------------------|
| Arduino ESP32 Core      | ≥ 2.0.x   | ESP32 Board-Support                  |
| Adafruit BMP280         | ≥ 2.6.x   | BMP280-Sensor-Treiber                |
| WiFiManager (tzapu)     | ≥ 2.0.x   | Auto-WLAN-Konfigurationsportal       |
| UniversalTelegramBot    | ≥ 1.3.x   | Telegram-Bot-Integration             |
| ArduinoJson             | ≥ 6.x     | JSON-Parsing (für Telegram)          |
| RemoteXY                | ≥ 3.x     | Smartphone-Steuerung via App         |

---

## Pinbelegung

### Sender-ESP32

| Pin       | Verbindung               |
|-----------|--------------------------|
| GPIO 21   | BMP280 SDA               |
| GPIO 22   | BMP280 SCL               |
| GPIO 34   | LM393 DO (Digitalausgang)|
| 3V3       | BMP280 VCC, LM393 VCC    |
| GND       | BMP280 GND, LM393 GND    |

### Empfänger-ESP32

| Pin       | Verbindung                         |
|-----------|------------------------------------|
| GPIO 25   | LED 1 – Rot (über 220 Ω)           |
| GPIO 26   | LED 1 – Grün (über 220 Ω)          |
| GPIO 27   | LED 1 – Blau (über 220 Ω)          |
| GPIO 14   | LED 2 – Rot (über 220 Ω)           |
| GPIO 12   | LED 2 – Grün (über 220 Ω)          |
| GPIO 13   | LED 2 – Blau (über 220 Ω)          |
| GND       | LED 1 Kathode, LED 2 Kathode       |
