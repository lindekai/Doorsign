#pragma once
#include <Arduino.h>

// ============================================================
//  DISPLAY-TREIBER AUSWAHL
//  [DISPLAY-ABHÄNGIG] Passenden Treiber einkommentieren.
//
//  Waveshare 7.5" V2 (GDEW075T7): GxEPD2_750_T7  ← Standard
//  Waveshare 7.5" V1 (GDEW075T8): GxEPD2_750
// ============================================================
#include <GxEPD2_BW.h>
#include <GxEPD2_750_T7.h>    // [DISPLAY-ABHÄNGIG] Waveshare 7.5" V2
// #include <GxEPD2_750.h>    // [DISPLAY-ABHÄNGIG] Waveshare 7.5" V1

// PNG-Decoder: PNGdec von Larry Bank
// Arduino Library Manager: Suche nach "PNGdec"
#include <PNGdec.h>

#include <Fonts/FreeMonoBold18pt7b.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include <LittleFS.h>
#include "config.h"

// ============================================================
//  DisplayManager
// ============================================================
class DisplayManager {
public:
    DisplayManager();
    void begin();
    bool showImageFromFile(const char* path);
    void showFallback();
    void hibernate();
    bool isInitialized() const { return _initialized; }

private:
    // [DISPLAY-ABHÄNGIG] Treiber-Typ hier anpassen wenn Treiber wechselt.
    GxEPD2_BW<GxEPD2_750_T7, GxEPD2_750_T7::HEIGHT> _display;
    bool _initialized;

    bool renderPng(const char* path);
    void drawCenteredText(const String& text, int16_t y);
    void drawBorder();
};

// ============================================================
//  Globale Hilfsvariablen für PNGdec-Callback
//  (PNGdec erwartet freie Callback-Funktionen, kein this-Zeiger)
// ============================================================
struct PngRenderContext {
    uint8_t* pixBuf;       // Zielpuffer: 1-Bit, 800×480 = 48000 Bytes
    int32_t  displayWidth;
    int32_t  displayHeight;
    bool     invert;
};

extern PngRenderContext g_pngCtx;
