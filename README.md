# Digitales Türschild – ESP32 + E-Ink Display

Firmware für digitale Konferenzraum-Türschilder auf Basis von ESP32 und
Waveshare 7.5" E-Ink Display (800×480).

---

## 1. Architekturübersicht

```
┌─────────────────────────────────────────────────────────────┐
│                     Server (extern)                         │
│  Erzeugt pro Raum ein PNG-Bild (800×480, Graustufen 8-Bit) │
│  Stellt es per HTTP/HTTPS bereit, optional mit ETag-Header  │
└────────────────────────┬────────────────────────────────────┘
                         │ HTTP GET (alle 5 min, Mo–Fr 08–18)
                         ▼
┌─────────────────────────────────────────────────────────────┐
│                      ESP32 Firmware                          │
│                                                              │
│  ┌──────────┐  ┌───────────┐  ┌──────────┐  ┌───────────┐  │
│  │ wifi_mgr │  │ time_mgr  │  │image_mgr │  │display_mgr│  │
│  │          │  │           │  │          │  │           │  │
│  │ Auto-    │  │ NTP-Sync  │  │ Download │  │ GxEPD2    │  │
│  │ Reconnect│  │ Zeitzone  │  │ ETag/MD5 │  │ SPI       │  │
│  │          │  │ Zeitfens- │  │ PNG→Mono │  │ Full/Part │  │
│  │          │  │ ter-Logik │  │ Dithering│  │ Refresh   │  │
│  └──────────┘  └───────────┘  │ LittleFS │  └───────────┘  │
│                               └──────────┘                   │
│                                                              │
│  DoorSign.ino: Zustandsmaschine, millis()-Scheduler          │
└──────────────────────────────────────────────────────────────┘
                         │ SPI
                         ▼
              ┌──────────────────┐
              │ Waveshare 7.5"   │
              │ E-Ink Display    │
              │ 800×480 mono     │
              └──────────────────┘
```

### Zustandsmaschine

```
BOOT → WIFI_CONNECT → NTP_SYNC → INITIAL_DISPLAY
                                        │
         ┌──────────────────────────────┘
         ▼
  CHECK_SCHEDULE ──(aktiv)──→ IMAGE_UPDATE ──(neu)──→ DISPLAY_UPDATE
         │                        │                        │
         │                   (unverändert)                  │
         │                        │                        │
         └────────── IDLE ←───────┴────────────────────────┘
                      │
               (Intervall abgelaufen)
                      │
                      └──→ CHECK_SCHEDULE (Schleife)
```

---

## 2. Bildformat-Entscheidung

**Empfehlung: Graustufen-PNG (8-Bit) vom Server, Monochrom-Konvertierung auf dem ESP32.**

### Begründung

| Kriterium         | Mono-BMP           | Graustufen-PNG         |
|-------------------|--------------------|------------------------|
| Serverseite       | Muss selbst dithern| Liefert Graustufen     |
| Dateigröße        | ~48 KB unkomprimiert| ~20–60 KB komprimiert |
| Textqualität      | OK                 | Besser (Dithering auf ESP) |
| Robustheit        | Sehr hoch          | Hoch                   |
| Flexibilität      | Nur S/W            | Schwellwert anpassbar  |

Die Firmware empfängt **Graustufen-PNG (800×480, 8-Bit Farbtiefe)** und
konvertiert on-device mit **Floyd-Steinberg-Dithering** zu Monochrom.
Dadurch entsteht die bestmögliche Darstellung auf dem E-Ink-Display, und
der Server muss sich nicht um Display-spezifisches Rendering kümmern.

### Anforderungen an das Server-Bild

- Format: PNG
- Farbmodell: Grayscale 8-Bit (empfohlen) oder RGB
- Auflösung: exakt 800×480 Pixel
- Die Firmware verarbeitet auch RGB-PNGs (automatische Luminanz-Berechnung)

---

## 3. Verwendete Bibliotheken

| Bibliothek       | Zweck                              | Installation              |
|------------------|------------------------------------|---------------------------|
| **GxEPD2**       | E-Ink Display-Treiber              | Arduino Library Manager   |
| **PNGdec**       | PNG-Dekodierung auf ESP32          | Arduino Library Manager   |
| **Adafruit GFX** | Grafik-Basis (Abhängigkeit GxEPD2) | wird automatisch installiert |
| WiFi             | WLAN (ESP32 built-in)              | im ESP32-Core enthalten   |
| HTTPClient       | HTTP/HTTPS Client                  | im ESP32-Core enthalten   |
| LittleFS         | Flash-Dateisystem                  | im ESP32-Core enthalten   |
| mbedtls          | MD5-Hashberechnung                 | im ESP32-Core enthalten   |
| time.h           | NTP + Zeitzone                     | im ESP32-Core enthalten   |

---

## 4. Pin-Belegung: ESP32 ↔ E-Ink Display

```
 ESP32-Pin  │  E-Ink-Signal  │  Beschreibung
────────────┼────────────────┼──────────────────────────
 GPIO 23    │  DIN (MOSI)    │  SPI Master Out, Slave In
 GPIO 18    │  CLK (SCLK)    │  SPI Clock
 GPIO 5     │  CS            │  Chip Select
 GPIO 17    │  DC            │  Data/Command
 GPIO 16    │  RST           │  Reset
 GPIO 4     │  BUSY          │  Busy-Signal vom Display
────────────┼────────────────┼──────────────────────────
 3.3V       │  VCC           │  Stromversorgung
 GND        │  GND           │  Masse
```

Die Pin-Zuordnung kann in `config.h` unter **SPI-Pinbelegung** angepasst werden.

**Hinweis:** Wenn ein Waveshare ESP32 Driver Board verwendet wird, sind die
Pins bereits fest verdrahtet. In diesem Fall die Pins in config.h an die
Board-Dokumentation anpassen.

---

## 5. Konfiguration für zwei Räume

### Gerät 1 – Konferenzraum 1

In `config.h`:
```cpp
#define ROOM_KONFERENZ_1
// #define ROOM_KONFERENZ_2
```

### Gerät 2 – Konferenzraum 2

In `config.h`:
```cpp
// #define ROOM_KONFERENZ_1
#define ROOM_KONFERENZ_2
```

### WLAN-Zugangsdaten (für beide gleich)

```cpp
#define WIFI_SSID       "MeinWLAN"
#define WIFI_PASSWORD   "MeinPasswort"
```

### Server-URLs anpassen

```cpp
#ifdef ROOM_KONFERENZ_1
  #define IMAGE_URL "http://dein-server.local/api/room/konferenz1/display.png"
#endif

#ifdef ROOM_KONFERENZ_2
  #define IMAGE_URL "http://dein-server.local/api/room/konferenz2/display.png"
#endif
```

---

## 6. Stellen im Code, die displayabhängig sind

Alle displayspezifischen Stellen sind in den Dateien mit
**`>>> DISPLAYABHÄNGIG <<<`** markiert:

1. **`config.h`** – Treiber-Auswahl (`USE_GxEPD2_750_T7` etc.)
2. **`config.h`** – SPI-Pinbelegung
3. **`config.h`** – Display-Auflösung (`DISPLAY_WIDTH`, `DISPLAY_HEIGHT`)
4. **`config.h`** – Invertierung (`DISPLAY_INVERT`)
5. **`display_mgr.cpp`** – GxEPD2-Instanziierung (Treiber-Klasse)

---

## 7. Update-Logik

### Ablauf eines Update-Zyklus

1. **Zeitfenster prüfen:** Mo–Fr, 08:00–18:00 Uhr (Europe/Berlin)
2. **HTTP GET** an die konfigurierte URL
   - Header `If-None-Match` wird mitgesendet (falls ETag vorhanden)
3. **304 Not Modified?** → Kein Update, zurück in IDLE
4. **200 OK:** PNG wird in LittleFS heruntergeladen
5. **MD5-Prüfsumme** wird berechnet und mit gespeicherter verglichen
   - Identisch? → Kein Update (Server lieferte gleiches Bild trotz 200)
6. **PNG dekodieren:** PNGdec liest zeilenweise, Floyd-Steinberg-Dithering
   konvertiert Graustufen zu Monochrom
7. **Display aktualisieren:** Monochrom-Bitmap via GxEPD2 anzeigen
8. **Zustand speichern:** ETag, MD5, Monochrom-Bitmap in LittleFS

### Änderungserkennung (dreistufig)

| Priorität | Methode           | Beschreibung                          |
|-----------|-------------------|---------------------------------------|
| 1         | HTTP ETag         | Server sendet ETag, Client nutzt If-None-Match |
| 2         | MD5-Hash          | Prüfsumme der heruntergeladenen Datei |
| 3         | Implizit          | Neues Bild wird nur bei Änderung angezeigt |

### Display-Refresh-Strategie

- **Full-Refresh** alle 6 Updates (konfigurierbar): Entfernt Ghosting
- **Partial-Refresh** dazwischen: Schneller, schonender
- **Kein Refresh** wenn Bild unverändert
- Display-Controller wird nach Update ausgeschaltet (Stromsparen)

---

## 8. Fehlerbehandlung

| Fehler                    | Verhalten                                  |
|---------------------------|--------------------------------------------|
| WLAN nicht erreichbar     | Auto-Reconnect alle 10s, Fallback-Anzeige  |
| NTP-Sync fehlgeschlagen   | Retry, Updates werden vorerst blockiert     |
| HTTP-Fehler (4xx, 5xx)    | Geloggt, Retry nach 60s, letztes Bild bleibt |
| Leere Server-Antwort      | Ignoriert, letztes Bild bleibt             |
| PNG ungültig / korrupt    | Ignoriert, letztes Bild bleibt             |
| Falsche PNG-Auflösung     | Ignoriert mit Fehlermeldung                |
| LittleFS-Fehler           | Automatische Formatierung beim ersten Start |
| Speicher nicht allokierbar| System-Halt (kritischer Fehler)             |
| Stromausfall              | Sauberer Neustart, letztes Bild aus LittleFS |

---

## 9. Kompilieren und Flashen

### Voraussetzungen

1. **Arduino IDE 2.x** installieren
2. **ESP32 Board-Paket** hinzufügen:
   - Datei → Einstellungen → Zusätzliche Boardverwalter-URLs:
     `https://espressif.github.io/arduino-esp32/package_esp32_index.json`
   - Werkzeuge → Board → Boardverwalter → "esp32" installieren
3. **Bibliotheken installieren** (Sketch → Bibliothek einbinden → Verwalten):
   - `GxEPD2` (by Jean-Marc Zingg)
   - `PNGdec` (by Larry Bank)
   - `Adafruit GFX Library` (wird ggf. als Abhängigkeit automatisch installiert)

### Board-Einstellungen

| Einstellung        | Wert                              |
|--------------------|-----------------------------------|
| Board              | ESP32 Dev Module                  |
| Upload Speed       | 921600                            |
| CPU Frequency      | 240 MHz                           |
| Flash Frequency    | 80 MHz                            |
| Flash Mode         | QIO                               |
| Flash Size         | 4MB (32Mb)                        |
| Partition Scheme   | Default 4MB with spiffs (1.2MB APP/1.5MB SPIFFS) |
| PSRAM              | Enabled (falls Board PSRAM hat)   |

### Kompilieren und Hochladen

1. `config.h` anpassen (Raum, WLAN, Server-URL, Pins)
2. Sketch → Überprüfen/Kompilieren
3. ESP32 per USB verbinden
4. Port auswählen
5. Sketch → Hochladen

### Für das zweite Gerät

1. In `config.h` die Raumauswahl ändern:
   ```cpp
   // #define ROOM_KONFERENZ_1
   #define ROOM_KONFERENZ_2
   ```
2. Erneut kompilieren und auf den zweiten ESP32 flashen

---

## 10. Anpassung für andere E-Ink Displays

### Anderes Waveshare-Panel (andere Auflösung)

1. In `config.h` den passenden Treiber eintragen:
   ```cpp
   // Beispiel: 4.2" Display (400×300)
   // #define USE_GxEPD2_750_T7
   #define USE_GxEPD2_420      // muss ggf. in display_mgr.cpp ergänzt werden
   #define DISPLAY_WIDTH  400
   #define DISPLAY_HEIGHT 300
   ```

2. In `display_mgr.cpp` die Treiber-Instanziierung ergänzen:
   ```cpp
   #elif defined(USE_GxEPD2_420)
     GxEPD2_BW<GxEPD2_420, GxEPD2_420::HEIGHT>
         display(GxEPD2_420(PIN_CS, PIN_DC, PIN_RST, PIN_BUSY));
   ```

3. Server-Bilder an die neue Auflösung anpassen

### Vollständige Liste unterstützter Displays

Siehe: https://github.com/ZinggJM/GxEPD2#supported-e-paper-panels

### Dreifarbiges Display (z.B. Schwarz/Weiß/Rot)

Für Displays mit drei Farben (z.B. `GxEPD2_750c`) müsste die Dithering-Logik
erweitert werden, um den Rot-Kanal separat zu behandeln. Grundsätzlich ist die
Architektur dafür vorbereitet, erfordert aber Anpassungen in `display_mgr.cpp`
und `image_mgr.cpp`.

---

## 11. OTA-Updates (optional)

Für Over-the-Air-Updates kann der Arduino OTA-Mechanismus ergänzt werden.

### Einfache Integration

In `DoorSign.ino` ergänzen:

```cpp
#include <ArduinoOTA.h>

// In setup(), nach WLAN-Verbindung:
ArduinoOTA.setHostname(DEVICE_NAME);
ArduinoOTA.setPassword("ota-passwort");
ArduinoOTA.begin();

// In loop(), am Anfang:
ArduinoOTA.handle();
```

### Hinweise

- OTA benötigt ausreichend Flash-Platz (Partition Scheme anpassen)
- Empfohlen: "Minimal SPIFFS (1.9MB APP with OTA/190KB SPIFFS)"
- Achtung: LittleFS-Speicher wird dadurch kleiner
- Alternativ: HTTP-basiertes OTA über den gleichen Server

---

## 12. Dateien im Projekt

```
DoorSign/
├── DoorSign.ino       Hauptsketch (Zustandsmaschine, Setup, Loop)
├── config.h           Zentrale Konfiguration
├── wifi_mgr.h         WLAN-Management (Header)
├── wifi_mgr.cpp       WLAN-Management (Implementierung)
├── time_mgr.h         NTP + Zeitlogik (Header)
├── time_mgr.cpp       NTP + Zeitlogik (Implementierung)
├── display_mgr.h      Display-Steuerung (Header)
├── display_mgr.cpp    Display-Steuerung (Implementierung)
├── image_mgr.h        Bild-Download + Dekodierung (Header)
├── image_mgr.cpp      Bild-Download + Dekodierung (Implementierung)
└── README.md          Diese Dokumentation
```

---

## 13. Serial-Monitor-Ausgabe (Beispiel)

```
=============================================
 Digitales Tuerschild – Tuerschild-K1
 Raum: Konferenzraum 1
=============================================

[     523][SYS   ] Kein PSRAM, Puffer in RAM allokiert
[     524][SYS   ] Freier Heap: 267432 Bytes
[     530][FS    ] LittleFS bereit: 12288 / 1507328 Bytes belegt
[     545][DISP  ] Display initialisiert: 800x480
[     546][SYS   ] Zustand: BOOT -> WIFI_CONNECT
[     547][WIFI  ] Verbinde mit SSID: MeinWLAN
.........
[    3012][WIFI  ] Verbunden! IP: 192.168.1.42
[    3013][WIFI  ] RSSI: -54 dBm
[    3014][SYS   ] Zustand: WIFI_CONNECT -> NTP_SYNC
[    3015][TIME  ] NTP initialisiert: TZ=CET-1CEST,M3.5.0,M10.5.0/3
[    3016][TIME  ] Warte auf NTP-Synchronisation...
[    4521][TIME  ] Zeit synchronisiert: 2026-03-31 10:15:23 (Tuesday)
[    4522][SYS   ] Zustand: NTP_SYNC -> INITIAL_DISPLAY
[    4523][FS    ] Gespeichertes Bitmap geladen (48000 Bytes)
[    5200][DISP  ] Full-Refresh (#1)
[    5201][DISP  ] Display aktualisiert und ausgeschaltet
[    5202][SYS   ] Zustand: INITIAL_DISPLAY -> CHECK_SCHEDULE
[    5203][SYS   ] Aktives Zeitfenster – Update wird gestartet
[    5204][SYS   ] Zustand: CHECK_SCHEDULE -> IMAGE_UPDATE
[    5205][HTTP  ] Lade Bild von: http://server.local/api/room/konferenz1/display.png
[    5206][HTTP  ] Gespeicherter ETag: "abc123"
[    5207][HTTP  ] HTTP Status: 200
[    5208][HTTP  ] Neuer ETag: "def456"
[    5209][HTTP  ] Content-Length: 34521 Bytes
[    5890][HTTP  ] Download abgeschlossen: 34521 Bytes in 681 ms
[    5891][IMG   ] Neue MD5: a1b2c3d4e5f6...
[    5892][IMG   ] PNG-Info: 800x480, 8 bpp, Typ: 0
[    5893][IMG   ] Starte PNG-Dekodierung mit Floyd-Steinberg-Dithering...
[    6234][IMG   ] PNG erfolgreich dekodiert und zu Monochrom konvertiert
[    6235][IMG   ] Bild-Update erfolgreich abgeschlossen
[    6236][SYS   ] Neues Bild empfangen – Display wird aktualisiert
[    6237][SYS   ] Zustand: IMAGE_UPDATE -> DISPLAY_UPDATE
[    6238][DISP  ] Partial-Refresh (#2 von 6)
[    8100][DISP  ] Display aktualisiert und ausgeschaltet
[    8101][SYS   ] Zustand: DISPLAY_UPDATE -> IDLE
[    8102][SYS   ] Display-Update abgeschlossen. Naechstes Update in 300 s
```

---

## 14. Häufige Probleme

### Bild wird invertiert dargestellt (Schwarz↔Weiß vertauscht)

In `config.h` ändern:
```cpp
#define DISPLAY_INVERT  true
```

### Display bleibt weiß / zeigt nichts

- SPI-Pins prüfen (stimmen mit Verdrahtung überein?)
- Richtigen GxEPD2-Treiber gewählt?
- BUSY-Pin korrekt verbunden?

### Ghosting / Nachbilder

- `FULL_REFRESH_EVERY` auf niedrigeren Wert setzen (z.B. 3)
- Oder: `displayForceFullRefresh()` vor jedem Update aufrufen

### PNG wird nicht erkannt

- Auflösung muss exakt 800×480 sein
- Unterstützte Farbmodelle: Grayscale 8-Bit, RGB, RGBA, Palette
- Keine progressive PNGs verwenden
