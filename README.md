# 🌡 IoT Wetterstation – SYT Projekt

> **Schüler:** *Toni Gugic, Luka Milanovic*  
> **Klasse / Jahrgang:** *2CHIT*  
> **Schule:** *TGM*  
> **Datum:** *27.03.2027*

---

## Inhaltsverzeichnis

1. [Einleitung](#1-einleitung)
2. [Projektbeschreibung](#2-projektbeschreibung)
3. [Theorie](#3-theorie)
4. [Hardware-Aufbau](#4-hardware-aufbau)
5. [Arbeitsschritte](#5-arbeitsschritte)
6. [Code-Erklärung](#6-code-erklärung)
7. [LED-Logik](#7-led-logik)
8. [Mögliche Verbesserungen](#8-mögliche-verbesserungen)
9. [Zusammenfassung](#9-zusammenfassung)

---

## 1. Einleitung

Dieses Projekt entstand im Rahmen des SYT-Unterrichts (Systemtechnik) und demonstriert eine vollständige IoT-Anwendung mit zwei ESP32-Mikrocontrollern. Ziel war es, ein eingebettetes System zu entwickeln, das Sensordaten kabellos überträgt, visuell anzeigt und über mehrere Schnittstellen (Webseite, Telegram, Smartphone-App) abrufbar macht.

---

## 2. Projektbeschreibung

Das System besteht aus zwei ESP32-Boards, die über das **ESP-NOW**-Protokoll miteinander kommunizieren:

### Sender-ESP32

* Liest **Temperatur** und **Luftdruck** vom *BMP280*-Sensor (I²C)
* Liest den **Magnetstatus** vom *LM393*-Hallsensor (Digital-I/O)
* Sendet alle 2 Sekunden ein Datenpaket per ESP-NOW

```cpp
typedef struct struct_message {
    float temp;      // Temperatur in °C
    float press;     // Luftdruck in hPa
    int   hallRaw;   // Rohwert des Hall-Sensors
    bool  magnet;    // true = Magnet in der Nähe
} struct_message;
```

### Empfänger-ESP32

* Empfängt die ESP-NOW-Datenpakete
* Verbindet sich mit WLAN (Auto-Konfiguration via **WiFiManager**)
* Synchronisiert die Uhrzeit per **NTP**
* Betreibt ein **Web-Dashboard** (Dark Mode, Port 80)
* Unterstützt einen **Telegram-Bot** (`/info`-Befehl)
* Unterstützt **RemoteXY** zur Smartphone-Steuerung
* Steuert **zwei RGB-LEDs** zur Statusanzeige

---

## 3. Theorie

### ESP-NOW

ESP-NOW ist ein von Espressif entwickeltes Peer-to-Peer-Kommunikationsprotokoll für ESP8266 und ESP32. Es ermöglicht die direkte Datenübertragung zwischen zwei Geräten ohne einen WLAN-Router. Die Übertragung erfolgt auf dem 2,4-GHz-Band mit einer Latenz von wenigen Millisekunden.

Vorteile gegenüber klassischem WiFi:
- Sehr geringe Latenz (< 10 ms)
- Kein Router notwendig
- Geringer Stromverbrauch
- Einfache Implementierung mit der `esp_now`-Bibliothek

### BMP280

Der *Bosch BMP280* ist ein digitaler Umgebungssensor, der Temperatur (−40 bis +85 °C) und absoluten Luftdruck (300–1100 hPa) misst. Die Kommunikation erfolgt über I²C oder SPI. Im Projekt wird die I²C-Variante mit der Adafruit-Bibliothek verwendet.

### LM393 Hallsensor

Das LM393-Modul enthält einen Halleffekt-Sensor und einen Komparatorchip. Der Digitalausgang (DO) wechselt auf LOW, sobald ein Magnet in die Nähe des Sensors gebracht wird. Damit lässt sich z. B. das Öffnen und Schließen einer Tür oder eines Fensters erkennen.

### WiFiManager

Die *WiFiManager*-Bibliothek (by tzapu) ermöglicht es, WLAN-Zugangsdaten ohne Hardcodierung zu konfigurieren. Bei der ersten Inbetriebnahme (oder wenn das gespeicherte Netzwerk nicht erreichbar ist) öffnet der ESP32 einen eigenen WLAN-Accesspoint mit einem Konfigurationsportal. Der Nutzer verbindet sich damit und gibt die Zugangsdaten ein.

---

## 4. Hardware-Aufbau

### Benötigte Bauteile

Siehe [`docs/components.md`](docs/components.md) für die vollständige Komponenten- und Kostenliste.

### Pinbelegung Sender

| ESP32-Pin | Verbindung               |
|-----------|--------------------------|
| GPIO 21   | BMP280 SDA               |
| GPIO 22   | BMP280 SCL               |
| GPIO 34   | LM393 DO (Digitalausgang)|
| 3V3       | BMP280 VCC, LM393 VCC    |
| GND       | Gemeinsame Masse         |

### Pinbelegung Empfänger

| ESP32-Pin | Verbindung                     |
|-----------|--------------------------------|
| GPIO 25   | LED 1 Rot (220 Ω)              |
| GPIO 26   | LED 1 Grün (220 Ω)             |
| GPIO 27   | LED 1 Blau (220 Ω)             |
| GPIO 14   | LED 2 Rot (220 Ω)              |
| GPIO 12   | LED 2 Grün (220 Ω)             |
| GPIO 13   | LED 2 Blau (220 Ω)             |
| GND       | LED-Kathoden (gemeinsame Masse)|

### Schaltplan

> 📷 Schaltplan-Bild: [`images/wiring_diagram.png`](images/wiring_diagram.png)

### Hardware-Foto

> 📷 Foto des aufgebauten Prototyps: [`images/hardware_photo.jpg`](images/hardware_photo.jpg)

---

## 5. Arbeitsschritte

1. **Repository klonen**
   ```bash
   git clone https://github.com/CLeckIn/IoT-Wetterstation-SYT-Projekt.git
   cd IoT-Wetterstation-SYT-Projekt
   ```

2. **`secrets.h` anlegen**
   ```bash
   cp src/secrets.h.example src/secrets.h
   # Datei öffnen und eigene Werte eintragen
   ```

3. **Bibliotheken in der Arduino IDE installieren**  
   Über *Sketch → Bibliotheken verwalten*:
   - Adafruit BMP280
   - WiFiManager (by tzapu)
   - UniversalTelegramBot
   - ArduinoJson (≥ 6.x)
   - RemoteXY

4. **MAC-Adresse des Empfängers ermitteln**  
   Sketch auf den Empfänger-ESP32 laden, den Serial Monitor öffnen und die angezeigte MAC-Adresse notieren.

5. **MAC-Adresse im Sender eintragen**  
   In `src/Sender.ino` die Konstante `RECEIVER_MAC` mit der notierten Adresse befüllen.

6. **Sender-Sketch hochladen** auf den Sender-ESP32

7. **Empfänger-Sketch hochladen** auf den Empfänger-ESP32

8. **WLAN konfigurieren**  
   Beim ersten Start öffnet der Empfänger den Accesspoint `Wetterstation-Setup`. Verbinde dich damit und gib deine WLAN-Daten ein.

9. **Dashboard aufrufen**  
   Öffne im Browser die IP-Adresse des Empfängers (z. B. `http://192.168.1.42`).

---

## 6. Code-Erklärung

### Sender (`src/Sender.ino`)

| Abschnitt           | Erklärung                                                                 |
|---------------------|---------------------------------------------------------------------------|
| `struct_message`    | Datenstruktur, die per ESP-NOW übertragen wird                            |
| `RECEIVER_MAC`      | MAC-Adresse des Empfängers – muss manuell eingetragen werden              |
| `setup()`           | Initialisiert BMP280, Hall-Pin, WiFi (nur Station-Modus) und ESP-NOW      |
| `loop()`            | Liest Sensoren alle 2 s, prüft Plausibilität und sendet per ESP-NOW       |
| `onDataSent()`      | Callback nach erfolgtem Sendeversuch – gibt Status im Serial aus          |

**Wichtige Robustheits-Maßnahme:** Vor dem Senden werden die BMP280-Werte auf `NaN` und plausible Druckbereiche (800–1100 hPa) geprüft. Ungültige Messwerte werden verworfen.

### Empfänger (`src/Receiver.ino`)

| Abschnitt              | Erklärung                                                                    |
|------------------------|------------------------------------------------------------------------------|
| `onDataReceived()`     | ESP-NOW Callback – prüft Paketgröße vor `memcpy`, setzt `newDataAvailable`   |
| `handleRoot()`         | Erzeugt dynamisches HTML-Dashboard (Dark Mode) mit aktuellen Sensordaten     |
| `updateLEDs()`         | Setzt LED 1 und LED 2 basierend auf Magnet- und Temperaturstatus             |
| `handleTelegramMessages()` | Verarbeitet `/info`- und `/start`-Befehle des Telegram-Bots             |
| `setup()`              | WiFiManager, NTP, Webserver, Telegram, RemoteXY und ESP-NOW werden initialisiert |
| `loop()`               | Abwechselndes Abarbeiten von Webserver, RemoteXY, LED-Update, Telegram       |

**Wichtige Robustheits-Maßnahme:** Der ESP-NOW-Callback prüft `len == sizeof(struct_message)` bevor `memcpy` aufgerufen wird. Pakete falscher Größe werden verworfen.

### Datenflussbeschreibung

```
[BMP280 / LM393]
      │
      ▼
[Sender ESP32] ──ESP-NOW──► [Empfänger ESP32]
                                    │
                    ┌───────────────┼───────────────┐
                    ▼               ▼               ▼
              [Webserver]     [Telegram Bot]   [RGB LEDs]
                (Port 80)       (/info)        (Status)
                    │
                    ▼
              [RemoteXY App]
```

---

## 7. LED-Logik

### LED 1 – Magnetstatus

| Zustand               | Farbe          |
|-----------------------|----------------|
| Magnet erkannt        | 🟢 Grün         |
| Kein Magnet           | 🔴 Rot          |
| Sender offline        | 🔴 Rot          |

### LED 2 – Systemstatus

| Zustand                          | Farbe                  |
|----------------------------------|------------------------|
| Temperatur normal (18–28 °C)     | 🔵 Blau                |
| Temperatur hoch (> 28 °C)        | 🔴 Rot                 |
| Temperatur niedrig (< 18 °C)     | 🩵 Cyan (Blau + Grün)  |
| Sender seit > 10 s offline       | 🟠 Orange (blinkend)   |

> 📷 Dashboard-Screenshot: [`images/dashboard_screenshot.png`](images/dashboard_screenshot.png)  
> 📱 Telegram-Screenshot: [`images/telegram_screenshot.png`](images/telegram_screenshot.png)

---

## 8. Mögliche Verbesserungen

| Verbesserung                           | Beschreibung                                                              |
|----------------------------------------|---------------------------------------------------------------------------|
| **HTTPS für Web-Dashboard**            | TLS/SSL-Zertifikat für sichere Übertragung der Messdaten                  |
| **Datenpersistenz (Datenbank)**        | Speicherung von Messwerten in InfluxDB + Grafana-Dashboard                |
| **Mehrere Sender**                     | ESP-NOW unterstützt bis zu 20 Peers – einfache Erweiterung auf mehrere Sensoren |
| **OTA-Updates**                        | Over-the-Air Firmware-Updates via ArduinoOTA oder ESP-IDF                 |
| **Batteriebetrieb + Deep Sleep**       | Sender mit LiPo-Akku und Deep-Sleep-Modus für energieeffizienten Betrieb  |
| **Alarmbenachrichtigung**              | Automatische Telegram-Benachrichtigung bei Überschreitung von Schwellwerten |
| **Luftfeuchtigkeit**                   | Austausch des BMP280 gegen einen BME280 für zusätzliche Feuchtigkeitsmessung |
| **PCB-Design**                         | Übergang vom Breadboard-Prototyp auf eine eigene Leiterplatte             |

---

## 9. Zusammenfassung

In diesem Projekt wurde eine vollständige IoT-Wetterstation mit zwei ESP32-Mikrocontrollern entwickelt. Der Sender erfasst Temperatur, Luftdruck und Magnetstatus und überträgt die Daten drahtlos per ESP-NOW. Der Empfänger verarbeitet die Daten, stellt sie über ein Web-Dashboard bereit, ermöglicht Telegram-Abfragen, RemoteXY-Steuerung und zeigt den Systemstatus über RGB-LEDs an.

Das Projekt demonstriert praxisrelevante Konzepte der eingebetteten Systemtechnik:
- Drahtlose Kommunikation (ESP-NOW)
- Sensorintegration (I²C, Digital-I/O)
- Netzwerkdienste (WiFi, NTP, HTTP)
- Sichere Konfigurationsverwaltung (Secrets-Datei, .gitignore)
- Robuste Fehlerbehandlung (Paketgrößenvalidierung, Plausibilitätsprüfung)

---

## Lizenz

Dieses Projekt ist für Bildungszwecke freigegeben.  
Quellcode und Dokumentation dürfen für schulische Arbeiten verwendet und verändert werden.

---

## Repository-Struktur

```
IoT-Wetterstation-SYT-Projekt/
├── README.md                    ← Diese Datei
├── .gitignore                   ← Schließt secrets.h aus
├── src/
│   ├── Sender.ino               ← Sender-Sketch (BMP280 + LM393 + ESP-NOW)
│   ├── Receiver.ino             ← Empfänger-Sketch (Web, Telegram, LEDs)
│   └── secrets.h.example        ← Vorlage für Zugangsdaten
├── images/
│   ├── README.md                ← Anleitung für Bilder
│   ├── wiring_diagram.png       ← Schaltplan (bitte hinzufügen)
│   ├── hardware_photo.jpg       ← Hardwarefoto (bitte hinzufügen)
│   ├── dashboard_screenshot.png ← Dashboard (bitte hinzufügen)
│   └── telegram_screenshot.png  ← Telegram (bitte hinzufügen)
└── docs/
    └── components.md            ← Komponenten- und Kostenliste
```
