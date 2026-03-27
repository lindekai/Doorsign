#include "ImageManager.h"
#include "config.h"
#include "Logger.h"
#include <WiFi.h>

// ============================================================
//  ImageManager — Implementierung (PNG-Version)
// ============================================================

ImageManager::ImageManager() {}

bool ImageManager::begin() {
    if (!LittleFS.begin(true)) {
        logError("IMG", "LittleFS konnte nicht initialisiert werden");
        return false;
    }
    size_t total = LittleFS.totalBytes();
    size_t used  = LittleFS.usedBytes();
    logInfo("IMG", "LittleFS OK — belegt: " + String(used) +
                   " / " + String(total) + " Bytes");
    String etag = readFile(FS_ETAG_PATH);
    if (etag.length() > 0)
        logInfo("IMG", "Gespeicherter ETag: " + etag);
    String lm = readFile(FS_LASTMOD_PATH);
    if (lm.length() > 0)
        logInfo("IMG", "Gespeichertes Last-Modified: " + lm);
    return true;
}

DownloadResult ImageManager::downloadImage(const char* url) {
    if (!WiFi.isConnected()) {
        logError("IMG", "Kein WLAN — Download abgebrochen");
        return DownloadResult::ERROR_NO_WIFI;
    }

    logInfo("IMG", "HTTP GET: " + String(url));
    logHeap("IMG");

    HTTPClient http;
    http.begin(url);
    http.setTimeout((int)HTTP_TIMEOUT_MS);
    http.setUserAgent("ESP32-DoorSign/1.0 (" + String(DEVICE_NAME) + ")");
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

    // Conditional GET via ETag
    String savedEtag = readFile(FS_ETAG_PATH);
    if (savedEtag.length() > 0) {
        http.addHeader("If-None-Match", savedEtag);
        logDebug("IMG", "If-None-Match: " + savedEtag);
    } else {
        String savedLastMod = readFile(FS_LASTMOD_PATH);
        if (savedLastMod.length() > 0) {
            http.addHeader("If-Modified-Since", savedLastMod);
            logDebug("IMG", "If-Modified-Since: " + savedLastMod);
        }
    }

    int httpCode = http.GET();
    logInfo("IMG", "HTTP Response: " + String(httpCode));

    if (httpCode == HTTP_CODE_NOT_MODIFIED) {
        logInfo("IMG", "Bild unverändert (304 Not Modified)");
        http.end();
        return DownloadResult::SUCCESS_UNCHANGED;
    }

    if (httpCode != HTTP_CODE_OK) {
        logError("IMG", "HTTP-Fehler: " + String(httpCode));
        http.end();
        return DownloadResult::ERROR_HTTP;
    }

    int serverContentLength = http.getSize();
    logInfo("IMG", "Content-Length: " + String(serverContentLength) + " Bytes");

    if (serverContentLength > 0 && (size_t)serverContentLength > IMG_MAX_BYTES) {
        logError("IMG", "Datei zu groß (" + String(serverContentLength) + " Bytes)");
        http.end();
        return DownloadResult::ERROR_INVALID_PNG;
    }

    String newEtag    = http.header("ETag");
    String newLastMod = http.header("Last-Modified");

    // Download in temporäre Datei
    File tmpFile = LittleFS.open(FS_IMAGE_TMP, "w");
    if (!tmpFile) {
        logError("IMG", "Kann temporäre Datei nicht anlegen");
        http.end();
        return DownloadResult::ERROR_STORAGE;
    }

    WiFiClient*   stream   = http.getStreamPtr();
    uint8_t       buf[512];
    size_t        written  = 0;
    unsigned long dlStart  = millis();

    while (http.connected()) {
        size_t avail = stream->available();
        if (avail > 0) {
            size_t toRead  = min(avail, sizeof(buf));
            int bytesRead  = stream->readBytes(buf, toRead);
            if (bytesRead > 0) {
                tmpFile.write(buf, (size_t)bytesRead);
                written += (size_t)bytesRead;
            }
        } else {
            if (serverContentLength > 0 && written >= (size_t)serverContentLength) break;
            delay(1);
        }
        if (millis() - dlStart > DOWNLOAD_TIMEOUT_MS) {
            logError("IMG", "Download-Timeout");
            tmpFile.close();
            LittleFS.remove(FS_IMAGE_TMP);
            http.end();
            return DownloadResult::ERROR_HTTP;
        }
        if (written > IMG_MAX_BYTES) {
            logError("IMG", "Download überschreitet IMG_MAX_BYTES");
            tmpFile.close();
            LittleFS.remove(FS_IMAGE_TMP);
            http.end();
            return DownloadResult::ERROR_INVALID_PNG;
        }
    }

    tmpFile.close();
    http.end();
    logInfo("IMG", "Download abgeschlossen: " + String(written) + " Bytes");

    // PNG-Signatur prüfen
    if (!validatePng(FS_IMAGE_TMP)) {
        LittleFS.remove(FS_IMAGE_TMP);
        logError("IMG", "PNG-Validierung fehlgeschlagen — temporäre Datei gelöscht");
        return DownloadResult::ERROR_INVALID_PNG;
    }

    // Atomarer Austausch: temp → final
    LittleFS.remove(FS_IMAGE_PATH);
    if (!LittleFS.rename(FS_IMAGE_TMP, FS_IMAGE_PATH)) {
        logError("IMG", "Rename temporäre Datei fehlgeschlagen");
        LittleFS.remove(FS_IMAGE_TMP);
        return DownloadResult::ERROR_STORAGE;
    }

    // Metadaten persistieren
    if (newEtag.length() > 0) {
        writeFile(FS_ETAG_PATH, newEtag);
        logDebug("IMG", "ETag gespeichert: " + newEtag);
    } else {
        LittleFS.remove(FS_ETAG_PATH);
    }
    if (newLastMod.length() > 0) {
        writeFile(FS_LASTMOD_PATH, newLastMod);
        logDebug("IMG", "Last-Modified gespeichert: " + newLastMod);
    } else {
        LittleFS.remove(FS_LASTMOD_PATH);
    }

    logInfo("IMG", "Neues Bild erfolgreich gespeichert: " + String(FS_IMAGE_PATH));
    return DownloadResult::SUCCESS_CHANGED;
}

bool ImageManager::hasStoredImage() const {
    return LittleFS.exists(FS_IMAGE_PATH);
}

String ImageManager::resultToString(DownloadResult r) {
    switch (r) {
        case DownloadResult::SUCCESS_CHANGED:    return "Neues Bild";
        case DownloadResult::SUCCESS_UNCHANGED:  return "Unverändert (304)";
        case DownloadResult::ERROR_NO_WIFI:      return "Fehler: kein WLAN";
        case DownloadResult::ERROR_HTTP:         return "Fehler: HTTP";
        case DownloadResult::ERROR_INVALID_PNG:  return "Fehler: ungueltige PNG";
        case DownloadResult::ERROR_STORAGE:      return "Fehler: Speicher";
        default:                                 return "Unbekannt";
    }
}

// ============================================================
//  Private Hilfsfunktionen
// ============================================================

bool ImageManager::validatePng(const char* path) const {
    File f = LittleFS.open(path, "r");
    if (!f) {
        logError("IMG", "validatePng: Datei nicht oeffenbar");
        return false;
    }

    size_t fileSize = f.size();
    logDebug("IMG", "validatePng: Dateigroesse = " + String(fileSize) + " Bytes");

    // PNG-Signatur: 8 Bytes: 0x89 0x50 0x4E 0x47 0x0D 0x0A 0x1A 0x0A
    if (fileSize < 8) {
        logError("IMG", "validatePng: Datei zu klein");
        f.close();
        return false;
    }

    uint8_t sig[8];
    f.seek(0);
    f.read(sig, 8);
    f.close();

    const uint8_t pngSig[8] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
    for (int i = 0; i < 8; i++) {
        if (sig[i] != pngSig[i]) {
            logError("IMG", "validatePng: Keine PNG-Signatur");
            return false;
        }
    }

    logInfo("IMG", "validatePng: OK — " + String(fileSize) + " Bytes");
    return true;
}

String ImageManager::readFile(const char* path) const {
    if (!LittleFS.exists(path)) return "";
    File f = LittleFS.open(path, "r");
    if (!f) return "";
    String content = f.readString();
    f.close();
    content.trim();
    return content;
}

void ImageManager::writeFile(const char* path, const String& content) {
    File f = LittleFS.open(path, "w");
    if (!f) {
        logError("IMG", "writeFile: Kann Datei nicht schreiben: " + String(path));
        return;
    }
    f.print(content);
    f.close();
}
