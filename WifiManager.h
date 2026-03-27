#pragma once
#include <Arduino.h>
#include <WiFi.h>

// ============================================================
//  WifiManager — WLAN-Verbindung mit Auto-Reconnect
// ============================================================
class WifiManager {
public:
    WifiManager(const char* ssid, const char* password);

    // Verbindung herstellen. Blockiert bis verbunden oder Timeout.
    // Gibt true zurück bei erfolgreicher Verbindung.
    bool connect(unsigned long timeoutMs = WIFI_CONNECT_TIMEOUT_MS);

    // true wenn aktuell verbunden
    bool isConnected() const;

    // Reconnect wenn Verbindung verloren. Rate-limited durch
    // WIFI_RECONNECT_INTERVAL_MS — kehrt sofort zurück wenn zu früh.
    // Gibt true zurück wenn verbunden (auch wenn bereits verbunden war).
    bool reconnectIfNeeded();

    // Verbindung sauber trennen
    void disconnect();

    // IP-Adresse als String (z.B. "192.168.1.42")
    String getIPAddress() const;

    // Signalstärke in dBm
    int getRSSI() const;

private:
    const char*   _ssid;
    const char*   _password;
    unsigned long _lastReconnectAttemptMs;
};
