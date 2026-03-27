#pragma once
#include <Arduino.h>
#include <time.h>

// ============================================================
//  TimeManager — NTP-Synchronisation, Zeitzone, Aktivfenster
// ============================================================
class TimeManager {
public:
    TimeManager();

    // Zeitzone konfigurieren (muss vor synchronize() aufgerufen werden)
    void begin();

    // NTP-Synchronisation starten.
    // Blockiert maximal NTP_SYNC_TIMEOUT_MS.
    // Gibt true zurück wenn Zeit erfolgreich synchronisiert wurde.
    bool synchronize();

    // true wenn die Zeit mindestens einmal erfolgreich synchronisiert wurde
    bool isSynced() const;

    // true wenn jetzt Montag–Freitag, 08:00–17:59 Uhr (Europe/Berlin)
    bool isInActiveWindow() const;

    // Aktuelle lokale Zeit als "HH:MM:SS"
    String getTimeString() const;

    // Aktuelles Datum + Zeit als "YYYY-MM-DD HH:MM:SS"
    String getTimestamp() const;

    // Millisekunden seit dem letzten erfolgreichen NTP-Sync
    unsigned long millisSinceLastSync() const;

private:
    bool          _synced;
    unsigned long _lastSyncMillis;
};
