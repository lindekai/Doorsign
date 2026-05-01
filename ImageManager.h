#pragma once
#include <Arduino.h>
#include <LittleFS.h>
#include <HTTPClient.h>
#include "config.h"

// ============================================================
//  Download-Ergebnis
// ============================================================
enum class DownloadResult {
    SUCCESS_CHANGED,    // Neues Bild heruntergeladen und gespeichert
    SUCCESS_UNCHANGED,  // Server: 304 Not Modified
    ERROR_NO_WIFI,      // Keine WLAN-Verbindung
    ERROR_HTTP,         // HTTP-Fehler
    ERROR_INVALID_IMAGE,// Ungültige Bilddatei (PNG oder BMP)
    ERROR_STORAGE       // LittleFS-Fehler
};

// ============================================================
//  ImageManager — HTTP-Download, ETag-Caching, LittleFS
//  Unterstützt PNG und BMP (umschaltbar via config.h)
// ============================================================
class ImageManager {
public:
    ImageManager();
    bool begin();
    DownloadResult downloadImage(const char* url);
    bool hasStoredImage() const;
    static String resultToString(DownloadResult r);

private:
    String readFile(const char* path) const;
    void   writeFile(const char* path, const String& content);

    // Format-spezifische Validierung
    bool validateImage(const char* path) const;
    bool validatePng(const char* path) const;
    bool validateBmp(const char* path) const;
};
