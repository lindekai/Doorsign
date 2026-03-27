// ============================================================
//  DoorSign.ino — Digitales Türschild für Konferenzräume
//
//  Hardware:
//    - ESP32 DevKit (oder kompatibles Board)
//    - Waveshare 7.5" E-Ink Display (800×480, Schwarz/Weiß)
//
//  Bibliotheken (in Arduino Library Manager installieren):
//    - GxEPD2 by ZinggJM (getestet mit v1.5.x)
//    - Adafruit GFX Library (Abhängigkeit von GxEPD2)
//
//  Eingebaute ESP32-Bibliotheken (kein separates Installieren nötig):
//    - WiFi
//    - HTTPClient
//    - LittleFS
//    - ArduinoOTA
//
//  Board-Einstellungen in Arduino IDE:
//    Board:        "ESP32 Dev Module" (oder passendes ESP32-Board)
//    Upload Speed: 921600
//    Flash Size:   4MB (32Mb)
//    Partition:    "Default 4MB with spiffs" oder "Default 4MB with ffat"
//      → Für LittleFS: "Default 4MB with spiffs" reicht aus
//    PSRAM:        Disabled (oder Enable falls verbaut)
//
//  Erste Inbetriebnahme:
//    1. config.h öffnen und WLAN, Gerätename, Image-URL eintragen
//    2. Kompilieren und flashen
//    3. Seriellen Monitor öffnen (115200 Baud)
//    4. Startsequenz und Statusmeldungen beobachten
// ============================================================

#include <Arduino.h>
#include <WiFi.h>

#if OTA_ENABLED
  #include <ArduinoOTA.h>
#endif

#include "config.h"
#include "Logger.h"
#include "WifiManager.h"
#include "TimeManager.h"
#include "ImageManager.h"
#include "DisplayManager.h"
#include "StateMachine.h"

// ============================================================
//  Globale Objekte
//  Reihenfolge beachten: Manager-Objekte werden vor StateMachine
//  erstellt, StateMachine hält nur Zeiger darauf.
// ============================================================
WifiManager    wifiManager(WIFI_SSID, WIFI_PASSWORD);
TimeManager    timeManager;
ImageManager   imageManager;
DisplayManager displayManager;
StateMachine   stateMachine(&wifiManager, &timeManager,
                            &imageManager, &displayManager);

// ============================================================
//  setup()
// ============================================================
void setup() {
    Serial.begin(SERIAL_BAUD_RATE);
    delay(500);  // Kurze Pause damit der serielle Monitor sich verbinden kann

    Serial.println();
    Serial.println("=========================================");
    Serial.println("  DoorSign — Digitales Konferenzschild  ");
    Serial.println("=========================================");
    logInfo("MAIN", "Gerät:    " + String(DEVICE_NAME));
    logInfo("MAIN", "Raum:     " + String(ROOM_NAME));
    logInfo("MAIN", "Firmware: " + String(__DATE__) + " " + String(__TIME__));
    logInfo("MAIN", "ESP32 Chip-ID: " + String((uint32_t)(ESP.getEfuseMac() >> 32), HEX));
    logHeap("MAIN");

    // ---- Hardware initialisieren ----

    // 1. LittleFS für persistenten Speicher
    if (!imageManager.begin()) {
        logError("MAIN", "KRITISCH: LittleFS Init fehlgeschlagen!");
        // System läuft weiter, aber ohne persistenten Speicher
    }

    // 2. Zeitzone konfigurieren (vor NTP-Sync)
    timeManager.begin();

    // 3. Display initialisieren
    displayManager.begin();

    // 4. OTA-Update vorbereiten (optional)
#if OTA_ENABLED
    setupOTA();
#endif

    // 5. State Machine starten (zeigt ggf. gespeichertes Bild oder Fallback)
    stateMachine.begin();

    logInfo("MAIN", "setup() abgeschlossen");
}

// ============================================================
//  loop()
//  Wird kontinuierlich aufgerufen. Enthält keine blocking delays.
//  Alle Wartelogik ist in der StateMachine via millis() realisiert.
// ============================================================
void loop() {
    // State Machine vorantreiben
    stateMachine.update();

#if OTA_ENABLED
    // OTA-Update-Handler regelmäßig aufrufen
    ArduinoOTA.handle();
#endif

    // Sehr kurze Pause um Watchdog zu bedienen und CPU-Last zu reduzieren.
    // Die StateMachine kehrt bei nicht-blockierenden Zuständen sofort zurück.
    delay(10);
}

// ============================================================
//  OTA-Setup (optional)
// ============================================================
#if OTA_ENABLED
void setupOTA() {
    ArduinoOTA.setPort(OTA_PORT);
    ArduinoOTA.setHostname(DEVICE_NAME);
    ArduinoOTA.setPassword(OTA_PASSWORD);

    ArduinoOTA.onStart([]() {
        String type = (ArduinoOTA.getCommand() == U_FLASH) ? "Firmware" : "LittleFS";
        logInfo("OTA", "Update gestartet: " + type);
        // Display während OTA nicht anfassen
    });

    ArduinoOTA.onEnd([]() {
        logInfo("OTA", "Update abgeschlossen — Neustart...");
    });

    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        static uint8_t lastPct = 0;
        uint8_t pct = (uint8_t)(progress * 100 / total);
        if (pct != lastPct && pct % 10 == 0) {
            logInfo("OTA", "Fortschritt: " + String(pct) + "%");
            lastPct = pct;
        }
    });

    ArduinoOTA.onError([](ota_error_t error) {
        String errMsg;
        switch (error) {
            case OTA_AUTH_ERROR:    errMsg = "Authentifizierung fehlgeschlagen"; break;
            case OTA_BEGIN_ERROR:   errMsg = "Begin fehlgeschlagen";             break;
            case OTA_CONNECT_ERROR: errMsg = "Verbindungsfehler";                break;
            case OTA_RECEIVE_ERROR: errMsg = "Empfangsfehler";                   break;
            case OTA_END_ERROR:     errMsg = "End fehlgeschlagen";               break;
            default:                errMsg = "Unbekannter Fehler";               break;
        }
        logError("OTA", "Fehler [" + String(error) + "]: " + errMsg);
    });

    ArduinoOTA.begin();
    logInfo("OTA", "OTA bereit auf Port " + String(OTA_PORT) +
                   ", Hostname: " + String(DEVICE_NAME));
}
#endif
