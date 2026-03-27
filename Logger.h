#pragma once
#include <Arduino.h>

// ============================================================
//  Logger — Leichtgewichtiges Logging für Serial-Ausgabe
//  Alle Funktionen sind inline, kein Overhead bei Release.
// ============================================================

static inline void logInfo(const char* tag, const String& msg) {
    Serial.print("[INFO ] [");
    Serial.print(tag);
    Serial.print("] ");
    Serial.println(msg);
}

static inline void logWarn(const char* tag, const String& msg) {
    Serial.print("[WARN ] [");
    Serial.print(tag);
    Serial.print("] ");
    Serial.println(msg);
}

static inline void logError(const char* tag, const String& msg) {
    Serial.print("[ERROR] [");
    Serial.print(tag);
    Serial.print("] ");
    Serial.println(msg);
}

static inline void logDebug(const char* tag, const String& msg) {
    Serial.print("[DEBUG] [");
    Serial.print(tag);
    Serial.print("] ");
    Serial.println(msg);
}

// Heap-Verbrauch ausgeben (nützlich zur Speicherdiagnose)
static inline void logHeap(const char* tag) {
    Serial.print("[DEBUG] [");
    Serial.print(tag);
    Serial.print("] Free heap: ");
    Serial.print(ESP.getFreeHeap());
    Serial.print(" bytes, min free: ");
    Serial.print(ESP.getMinFreeHeap());
    Serial.println(" bytes");
}
