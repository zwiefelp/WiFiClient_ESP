# WiFiClient_ESP

MQTT-Smart-Home-Display auf Basis eines **ESP8266 (NodeMCU v2)** mit ILI9341-TFT.
Das Gerät dient als Bedien- und Anzeigeterminal für eine [openHAB](https://www.openhab.org/)-Installation:
Es zeigt schaltbare Elemente samt Status an, sendet Schaltbefehle per MQTT und blendet
im Ruhezustand eine Wetter- und Sensoranzeige (Netatmo-Daten) ein.

## Funktionen

- **Bedien-Screens** – mehrere konfigurierbare Seiten mit je bis zu 4 Elementen (linker/rechter Schalter).
- **Laufzeit-Konfiguration über MQTT** – die Oberfläche wird nicht fest einkompiliert, sondern beim
  Start per MQTT vom openHAB-Server geladen (siehe [MQTT-Protokoll](#mqtt-protokoll)).
- **Tastensteuerung** über ein 74HC595-Schieberegister (10 Tasten) oder direkte GPIOs.
- **Ruheanzeige** nach 10 s Inaktivität: Uhrzeit, Datum und Netatmo-Sensorwerte
  (Innen-/Außentemperatur, Luftfeuchte, Luftdruck, CO₂, Lärm).
- **OTA-Updates** via ArduinoOTA.

## Hardware

| Komponente | Beschreibung |
|---|---|
| Board | NodeMCU v2 (ESP8266) |
| Display | Adafruit ILI9341, 240×320, SPI |
| Eingabe | 10 Taster über 74HC595-Schieberegister |

### Pinbelegung (Standard, `HC595`-Modus)

| Signal | GPIO | Funktion |
|---|---|---|
| `TFT_DC` | 2 | Display Data/Command |
| `TFT_CS` | -1 | Display Chip Select (fest aktiv) |
| `DS` | 15 | 74HC595 Dateneingang |
| `SRCLK` | 4 | 74HC595 Shift-Clock |
| `RCLK` | 5 | 74HC595 Latch-Clock |
| `SENSE` | 16 | Tasten-Rückmeldung (Pulldown) |

## Software-Stack

- **PlatformIO** mit Arduino-Framework (`platform = espressif8266`, `board = nodemcuv2`)
- Bibliotheken:
  - [`knolleary/PubSubClient`](https://github.com/knolleary/pubsubclient) (MQTT)
  - [`adafruit/Adafruit GFX Library`](https://github.com/adafruit/Adafruit-GFX-Library)
  - `Adafruit_ILI9341esp` (lokal in `lib/`)

## Einrichtung

1. [PlatformIO](https://platformio.org/) installieren (z. B. als VS-Code-Erweiterung).
2. Zugangsdaten anlegen – die Datei `src/secrets.h` ist **nicht** im Repository enthalten:
   ```sh
   cp src/secrets.h.example src/secrets.h
   ```
   Anschließend `src/secrets.h` mit den eigenen Werten füllen:
   ```c
   #define SECRET_WIFI_SSID     "DEIN_WLAN_SSID"
   #define SECRET_WIFI_PASSWORD "DEIN_WLAN_PASSWORT"
   #define SECRET_MQTT_SERVER   "192.168.1.1"
   ```
3. Bauen und flashen:
   ```sh
   pio run --target upload
   ```
4. Serielle Ausgabe verfolgen (115200 Baud):
   ```sh
   pio device monitor
   ```

## Konfiguration (Compile-Time)

Wichtige Defines in `src/WiFiClient_ESP.ino`:

| Define | Standard | Bedeutung |
|---|---|---|
| `HC595` | gesetzt | Tasteneingabe über Schieberegister (statt direkter GPIOs) |
| `DEBUG` | gesetzt | Ausgaben über die serielle Schnittstelle |
| `TFTROT` | 2 | Display-Rotation (0 = Pinheader unten, 2 = oben) |
| `sleepmillis` | 10000 | Inaktivität in ms bis zur Ruheanzeige |

## MQTT-Protokoll

Das Gerät bezieht seine Oberflächen-Konfiguration zur Laufzeit. Ablauf:

1. Beim Start veröffentlicht das Gerät `getconfig:<ChipId>` auf `/openhab/configuration`.
2. Der Server antwortet auf `/openhab/configuration/<ChipId>` mit einer Folge `:`-getrennter Nachrichten:
   - `Screens:<Anzahl>`
   - je Screen: `StartScreen:<Name>`, dann pro Element
     `Member:<idx>:<name1>:<name2>:<type>:<topic>:<txtl>:<txtr>:<cmdl>:<cmdr>:<statetopic>`,
     abgeschlossen mit `EndScreen`
   - `EndConfig` schließt die Konfiguration ab.
3. Steuerkommandos auf `/openhab/configuration/<ChipId>`: `initialize`, `reconfigure`, `restart`,
   ab 1.0 Panel außerdem `getVersion` (Antwort `Version 1.0 Panel: RSSI=-71`) und `getIP`
   (Antwort `IP: 192.168.1.108 RSSI=-71`), beide auf demselben Topic, nicht retained.

**Diagnose** (ab 1.0 Panel, gleiches Format wie die Sonoff-Firmware, ausgewertet vom Debug-Tab
von HomeControl):

| Topic | Inhalt |
|---|---|
| `/openhab/debug/<ChipId>` | bei jeder MQTT-Verbindung: `Startup <ChipId> - Version 1.0 Panel: RSSI=-85 MQTTrc=-3 WiFiReason=4 LoopMax=140` (MQTTrc/WiFiReason/LoopMax nur nach einem Abbruch) |
| `/openhab/debug/<ChipId>/status` | jede Minute: `Uptime=600s Heap=34664 MinHeap=31624 RSSI=-88 Reset=Power_On` |

MQTT-Keepalive 60 s (der Broker trennt erst nach 90 s ohne Paket).

Statusaktualisierungen kommen über die `statetopic`-Pfade der Elemente; feste Sensor-Topics
(`/openhab/out/Netatmo_*`, `/openhab/DayDate`, `/openhab/Daytime`) speisen die Ruheanzeige.

## Projektstruktur

```
src/
  WiFiClient_ESP.ino     Haupt-Sketch
  netatmo_icons.h        Bitmap-Icons für die Sensoranzeige
  secrets.h.example      Vorlage für die Zugangsdaten (secrets.h ist gitignored)
lib/
  Adafruit_ILI9341esp/   Display-Treiber (lokal eingebunden)
platformio.ini           PlatformIO-Konfiguration
```

## Sicherheitshinweis

Die WLAN- und MQTT-Zugangsdaten liegen in der nicht versionierten Datei `src/secrets.h`.
Ältere Commits dieses Repositorys können noch Zugangsdaten im Klartext enthalten – beim Teilen
des Repositorys ggf. das WLAN-Passwort rotieren und die Git-Historie bereinigen.
