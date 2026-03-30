#pragma once
#include <Arduino.h>

// Vorwärts-Deklarationen um zirkuläre Includes zu vermeiden
class WifiManager;
class TimeManager;
class ImageManager;
class DisplayManager;

// ============================================================
//  Systemzustände
// ============================================================
enum class State : uint8_t {
    BOOT,              // Startzustand: Hardware-Init
    WIFI_CONNECTING,   // Wartet auf WLAN-Verbindung
    NTP_SYNCING,       // Wartet auf NTP-Zeitsynchronisation
    IDLE,              // Betriebsbereit, wartet auf nächsten Update-Zeitpunkt
    DOWNLOADING,       // Bild wird heruntergeladen
    UPDATING_DISPLAY,  // Display wird aktualisiert
    ERROR_RECOVERY     // Fehlerbehandlung / Wiederherstellung
};

// ============================================================
//  StateMachine — millis()-basierte, nicht-blockierende
//  Steuerlogik für das DoorSign-System
// ============================================================
class StateMachine {
public:
    StateMachine(WifiManager*    wifi,
                 TimeManager*    time,
                 ImageManager*   image,
                 DisplayManager* display);

    // Initialisierung: Fallback anzeigen falls kein Bild vorhanden,
    // dann in BOOT-Zustand wechseln.
    void begin();

    // Regelmäßig aus loop() aufrufen — kehrt immer sofort zurück.
    void update();

    // Aktuellen Zustand abfragen
    State getCurrentState() const { return _state; }

    // Zustand als lesbarer String
    const char* getStateString() const;

private:
    WifiManager*    _wifi;
    TimeManager*    _time;
    ImageManager*   _image;
    DisplayManager* _display;

    State         _state;
    bool          _displayShowingContent;      // Hat Display einen sinnvollen Inhalt?
    bool          _pendingDisplayUpdate;       // Neues Bild wartet auf Anzeige
    bool          _pendingStoredImageDisplay;  // Gespeichertes Bild nach NTP zeigen
    unsigned long _lastUpdateMs;            // millis() des letzten Download-Versuchs
    unsigned long _ntpLastAttemptMs;        // millis() des letzten NTP-Versuchs
    unsigned long _stateEnteredMs;          // millis() beim letzten Zustandswechsel
    unsigned long _errorRecoveryDelayMs;    // Wartezeit im ERROR_RECOVERY-Zustand

    // Zustandswechsel mit Logging
    void setState(State next);

    // Handler für jeden Zustand
    void handleBoot();
    void handleWifiConnecting();
    void handleNtpSyncing();
    void handleIdle();
    void handleDownloading();
    void handleUpdatingDisplay();
    void handleErrorRecovery();

    // true wenn jetzt ein Update gestartet werden soll
    bool isUpdateDue() const;
};
