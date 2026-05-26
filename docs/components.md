# Komponenten-Liste – IoT Wetterstation

Alle verwendeten Bauteile und Materialien für das SYT-Projekt.

---

## Mikrocontroller

| Anzahl | Bezeichnung           | Beschreibung                                              |
|--------|-----------------------|-----------------------------------------------------------|
| 2×     | **ESP32 DevKit v1**   | WLAN/Bluetooth-Mikrocontroller; einer als Sender, einer als Empfänger |

---

## Sensoren

| Anzahl | Bezeichnung           | Beschreibung                              |
|--------|-----------------------|-------------------------------------------|
| 1×     | **BMP280**            | Temperatur- & Luftdrucksensor             |
| 1×     | **LM393 Hallsensor**  | Erkennt Magnetfelder                      |

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
| 2×     | **USB-Kabel**            | Stromversorgung / Programmierung |

---

## Software / Bibliotheken

| Bibliothek              | Version   | Verwendung                           |
|-------------------------|-----------|--------------------------------------|
| Arduino ESP32 Core      | ≥ 2.0.x   | ESP32 Board-Support                  |
| Adafruit BMP280         | ≥ 2.6.x   | BMP280-Sensor-Treiber                |
| WiFiManager (tzapu)     | ≥ 2.0.x   | Auto-WLAN-Konfigurationsportal       |
| UniversalTelegramBot    | ≥ 1.3.x   | Telegram-Bot-Integration             |
| ArduinoJson             | ≥ 6.x     | JSON-Parsing (für Telegram)          |
