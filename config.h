#pragma once

// ============================================================
//  DOORSIGN — GERÄTEKONFIGURATION
//  Für jedes Gerät exakt EINEN Block aktivieren.
//  Alle anderen Blöcke auskommentiert lassen.
// ============================================================

// --- Gerät 1: Konferenzraum Alpha ---
#define DEVICE_NAME    "DoorSign-Alpha"
#define ROOM_NAME      "Konferenzraum Alpha"
#define IMAGE_URL      "http://192.168.1.100/api/room/alpha/image.png"
#define WIFI_SSID      "MeinNetzwerk"
#define WIFI_PASSWORD  "MeinPasswort"

/*
// --- Gerät 2: Konferenzraum Beta ---
#define DEVICE_NAME    "DoorSign-Beta"
#define ROOM_NAME      "Konferenzraum Beta"
#define IMAGE_URL      "http://192.168.1.100/api/room/beta/image.png"
#define WIFI_SSID      "MeinNetzwerk"
#define WIFI_PASSWORD  "MeinPasswort"
*/

// ============================================================
//  ZEITKONFIGURATION
// ============================================================
// POSIX-Zeitzonenstring für Europe/Berlin (CET/CEST inkl. Sommerzeit)
// Erklärung: CET-1 = UTC+1 im Winter, CEST = UTC+2 im Sommer,
//   M3.5.0 = letzter Sonntag im März (Umstellung auf Sommer),
//   M10.5.0/3 = letzter Sonntag im Oktober 03:00 (Umstellung auf Winter)
#define NTP_SERVER_PRIMARY    "pool.ntp.org"
#define NTP_SERVER_SECONDARY  "europe.pool.ntp.org"
#define TIMEZONE_POSIX        "CET-1CEST,M3.5.0,M10.5.0/3"

// Aktive Wochentage (0=Sonntag, 1=Montag, ..., 6=Samstag)
#define ACTIVE_WEEKDAY_FROM   1    // Montag
#define ACTIVE_WEEKDAY_TO     5    // Freitag
#define ACTIVE_HOUR_FROM      8    // 08:00 Uhr (inklusiv)
#define ACTIVE_HOUR_TO       18    // 18:00 Uhr (exklusiv → aktiv bis 17:59:59)

// ============================================================
//  INTERVALLE (Millisekunden)
// ============================================================
#define UPDATE_INTERVAL_MS          (5UL  * 60UL * 1000UL)  // 5 Minuten
#define WIFI_CONNECT_TIMEOUT_MS     (20UL * 1000UL)          // 20 Sek. für initialen Connect
#define WIFI_RECONNECT_INTERVAL_MS  (30UL * 1000UL)          // 30 Sek. zwischen Reconnect-Versuchen
#define NTP_SYNC_TIMEOUT_MS         (15UL * 1000UL)          // 15 Sek. NTP-Timeout
#define NTP_RETRY_INTERVAL_MS       (60UL * 1000UL)          // 60 Sek. NTP-Retry
#define HTTP_TIMEOUT_MS             (15UL * 1000UL)          // 15 Sek. HTTP-Timeout
#define DOWNLOAD_TIMEOUT_MS         (30UL * 1000UL)          // 30 Sek. maximale Download-Zeit

// ============================================================
//  DISPLAY-PINBELEGUNG  (ESP32  ↔  Waveshare 7.5" E-Paper)
// ============================================================
//  ┌────────────────┬────────────┬──────────────────────────────┐
//  │ E-Paper Pin    │ ESP32 GPIO │ Beschreibung                 │
//  ├────────────────┼────────────┼──────────────────────────────┤
//  │ VCC            │ 3.3 V      │ Versorgungsspannung          │
//  │ GND            │ GND        │ Masse                        │
//  │ DIN / MOSI     │ GPIO 23    │ SPI Data In  (Hardware SPI)  │
//  │ CLK / SCK      │ GPIO 18    │ SPI Clock    (Hardware SPI)  │
//  │ CS  / NSS      │ GPIO  5    │ Chip Select  (aktiv LOW)     │
//  │ DC             │ GPIO 17    │ Data / Command Select        │
//  │ RST            │ GPIO 16    │ Hardware Reset  (aktiv LOW)  │
//  │ BUSY           │ GPIO  4    │ Busy-Signal     (aktiv HIGH) │
//  └────────────────┴────────────┴──────────────────────────────┘
//  MOSI (GPIO 23) und SCK (GPIO 18) werden automatisch vom
//  Hardware-SPI des ESP32 verwendet — keine weitere Konfiguration nötig.
// [DISPLAY-ABHÄNGIG] Pins hier anpassen, falls abweichende Verdrahtung:
#define PIN_EPD_CS    5
#define PIN_EPD_DC   17
#define PIN_EPD_RST  16
#define PIN_EPD_BUSY  4

// ============================================================
//  BILDFORMAT — PNG
// ============================================================
// Erwartetes Format: 1-Bit monochrom, 800 × 480 Pixel,
//   Windows BMP (BITMAPINFOHEADER, Standard-Farbtabelle).
// Dateigröße: 14 (BMP-Header) + 40 (DIB-Header) + 8 (Farbtabelle)
//   + 100 × 480 (Pixeldaten, 100 Bytes/Zeile für 800 Px) = 48062 Bytes.
// [DISPLAY-ABHÄNGIG] Bei anderem Display Breite/Höhe hier anpassen:
#define IMG_WIDTH     800
#define IMG_HEIGHT    480
#define IMG_MAX_BYTES (200UL * 1024UL)  // 64 KB Sicherheits-Obergrenze

// Wenn das Display nach dem ersten Flash invertierte Farben zeigt,
// diesen Wert auf 1 setzen:
#define DISPLAY_INVERT_IMAGE  0

// ============================================================
//  DATEIPFADE (LittleFS)
// ============================================================
#define FS_IMAGE_PATH    "/image.png"
#define FS_IMAGE_TMP     "/image_tmp.png"
#define FS_ETAG_PATH     "/etag.txt"
#define FS_LASTMOD_PATH  "/lastmod.txt"

// ============================================================
//  OTA-UPDATE (auf 0 setzen um OTA zu deaktivieren)
// ============================================================
#define OTA_ENABLED       1                 // 1 = aktiviert, 0 = deaktiviert
#define OTA_PASSWORD      "doorsign_ota_pw"  // Sicheres Passwort setzen!
#define OTA_PORT          3232

// ============================================================
//  BMP-INVERSION
// ============================================================
// Wenn das Display nach dem ersten Flash invertierte Farben zeigt,
// diesen Wert auf 1 setzen:
#define DISPLAY_INVERT_IMAGE  0             // 0 = normal, 1 = invertiert

// ============================================================
//  SERIELLE AUSGABE
// ============================================================
#define SERIAL_BAUD_RATE  115200
