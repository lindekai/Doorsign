#pragma once
#include <Arduino.h>

// ============================================================
//  DISPLAY-TREIBER AUSWAHL  [DISPLAY-ABHÄNGIG]
// ============================================================
#include <GxEPD2_BW.h>
// #include <epd/GxEPD2_750_T7.h> // Waveshare 7.5" V2 (800x480) — NICHT dieses
#include <epd/GxEPD2_750.h>       // [DISPLAY-ABHÄNGIG] Waveshare 7.5" V1 (640x384) ← aktiv

#include <PNGdec.h>
#include <Fonts/FreeMonoBold18pt7b.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include <LittleFS.h>
#include <SPI.h>
#include "config.h"

// ============================================================
//  Globaler Kontext für PNGdec-Callback
//  rowBuf: einmal alloziert, für alle 480 Zeilen wiederverwendet
// ============================================================
struct PngRenderContext {
    uint8_t*  pixBuf;         // 1-Bit Zielpuffer  (48 000 Bytes)
    uint16_t* rowBuf;         // RGB565-Zeilenpuffer (1 600 Bytes, wiederverwendet)
    int32_t   displayWidth;
    int32_t   displayHeight;
    bool      invert;
};
extern PngRenderContext g_pngCtx;

// ============================================================
//  DisplayManager
// ============================================================
class DisplayManager {
public:
    DisplayManager();
    void begin();
    bool showImageFromFile(const char* path);
    void showStartup(const String& statusLine = "");
    void showFallback();
    void hibernate();
    bool isInitialized() const { return _initialized; }

private:
    // [DISPLAY-ABHÄNGIG] Treiber-Typ hier anpassen wenn Treiber wechselt
    GxEPD2_BW<GxEPD2_750, GxEPD2_750::HEIGHT> _display;
    bool _initialized;

    bool renderPng(const char* path);
    void drawCenteredText(const String& text, int16_t y);
    void drawBorder();
};
