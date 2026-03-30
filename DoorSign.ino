// ============================================================
//  DoorSign.ino — Digitales Türschild für Konferenzräume
//
//  Hardware:
//    - ESP32 DevKit (oder kompatibles Board)
//    - Waveshare 7.5" E-Ink Display (800×480, Schwarz/Weiß)
//
// PIN	ESP32	Description
// VCC	3V3	Power positive (3.3V power supply input)
// GND	GND	Ground
// DIN	P14	SPI's MOSI, data input
// SCLK	P13	SPI's CLK, clock signal input
// CS	P15	Chip selection, low active
// DC	P27	Data/Command, low for command, high for data
// RST	P26	Reset, low active
// BUSY	P25	Busy status output pin (indicating busy)
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
#include <ArduinoOTA.h>

#include "config.h"
#include "Logger.h"
#include "WifiManager.h"
#include "TimeManager.h"
#include "ImageManager.h"
#include "DisplayManager.h"
#include "StateMachine.h"

// ============================================================
//  Vorwärts-Deklarationen
// ============================================================
void registerOTAHandlers();

// ============================================================
//  Globale Objekte
// ============================================================
WifiManager    wifiManager(WIFI_SSID, WIFI_PASSWORD);
TimeManager    timeManager;
ImageManager   imageManager;
DisplayManager displayManager;
StateMachine   stateMachine(&wifiManager, &timeManager,
                            &imageManager, &displayManager);

// OTA nur einmalig starten, NACHDEM WLAN verbunden ist
static bool _otaStarted = false;

// ============================================================
//  setup()
// ============================================================
void setup() {
    Serial.begin(SERIAL_BAUD_RATE);
    delay(500);

    Serial.println();
    Serial.println("=========================================");
    Serial.println("  DoorSign -- Digitales Konferenzschild  ");
    Serial.println("=========================================");
    logInfo("MAIN", "Geraet:    " + String(DEVICE_NAME));
    logInfo("MAIN", "Raum:      " + String(ROOM_NAME));
    logInfo("MAIN", "Firmware:  " + String(__DATE__) + " " + String(__TIME__));
    logHeap("MAIN");

    if (!imageManager.begin()) {
        logError("MAIN", "KRITISCH: LittleFS Init fehlgeschlagen!");
    }

    timeManager.begin();
    displayManager.begin();

#if OTA_ENABLED
    // Nur Callbacks registrieren — begin() erst nach WLAN-Connect (in loop)
    registerOTAHandlers();
#endif

    stateMachine.begin();
    logInfo("MAIN", "setup() abgeschlossen");
}

// ============================================================
//  loop()
// ============================================================
void loop() {
    stateMachine.update();

#if OTA_ENABLED
    // ArduinoOTA.begin() erfordert aktives WLAN (mDNS-Stack).
    // Deshalb hier lazy initialisieren, sobald WLAN das erste Mal verbunden ist.
    if (!_otaStarted && WiFi.isConnected()) {
        ArduinoOTA.begin();
        _otaStarted = true;
        logInfo("OTA", "ArduinoOTA gestartet (IP: " + WiFi.localIP().toString() + ")");
    }
    if (_otaStarted) {
        ArduinoOTA.handle();
    }
#endif

    delay(10);
}

// ============================================================
//  OTA-Callbacks registrieren (kein begin() hier!)
// ============================================================
void registerOTAHandlers() {
    ArduinoOTA.setPort(OTA_PORT);
    ArduinoOTA.setHostname(DEVICE_NAME);
    ArduinoOTA.setPassword(OTA_PASSWORD);

    ArduinoOTA.onStart([]() {
        logInfo("OTA", "Update gestartet");
    });

    ArduinoOTA.onEnd([]() {
        logInfo("OTA", "Update abgeschlossen -- Neustart...");
    });

    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        static uint8_t lastPct = 255;
        uint8_t pct = (uint8_t)(progress * 100 / total);
        if (pct != lastPct && pct % 10 == 0) {
            logInfo("OTA", "Fortschritt: " + String(pct) + "%");
            lastPct = pct;
        }
    });

    ArduinoOTA.onError([](ota_error_t error) {
        logError("OTA", "Fehler: " + String(error));
    });

    logInfo("OTA", "OTA-Callbacks registriert (startet nach WLAN-Verbindung)");
}
