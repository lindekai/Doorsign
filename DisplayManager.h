#pragma once
#include <Arduino.h>
#include "config.h"

// ============================================================
//  DISPLAY-TREIBER  [DISPLAY-ABHÄNGIG]
//  Wird automatisch aus DISPLAY_TYPE in config.h gewählt.
//
//  V1: GxEPD2_BW  + GxEPD2_750      (640x384, S/W)
//  V2: GxEPD2_BW  + GxEPD2_750_T7   (800x480, S/W)
//  V3: GxEPD2_3C  + GxEPD2_750c_Z90 (800x480, S/W/ROT)
//      → anderer Include-Ordner: epd3c/ statt epd/
//      → andere Basisklasse: GxEPD2_3C statt GxEPD2_BW
// ============================================================

#if DISPLAY_TYPE == DISPLAY_V3
  // 3-Farb-Display: GxEPD2_3C als Basisklasse
  #include <GxEPD2_3C.h>
  #include <epd3c/GxEPD2_750c_Z90.h>
#elif DISPLAY_TYPE == DISPLAY_V2
  #include <GxEPD2_BW.h>
  #include <epd/GxEPD2_750_T7.h>
#else
  // V1 (Standard)
  #include <GxEPD2_BW.h>
  #include <epd/GxEPD2_750.h>
#endif

// PNG-Decoder (nur wenn PNG-Modus aktiv)
#if IMAGE_FORMAT_PNG
  #include <PNGdec.h>
#endif

#include <Fonts/FreeMonoBold18pt7b.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include <LittleFS.h>
#include <SPI.h>

// ============================================================
//  Kontext für PNGdec-Callback
// ============================================================
#if IMAGE_FORMAT_PNG
struct PngRenderContext {
    uint8_t*  pixBuf;
    uint16_t* rowBuf;
    int32_t   displayWidth;
    int32_t   displayHeight;
    bool      invert;
};
extern PngRenderContext g_pngCtx;
#endif

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
    // [DISPLAY-ABHÄNGIG] Treiber-Typ aus DISPLAY_TYPE
#if DISPLAY_TYPE == DISPLAY_V3
    GxEPD2_3C<GxEPD2_750c_Z90, GxEPD2_750c_Z90::HEIGHT> _display;
#elif DISPLAY_TYPE == DISPLAY_V2
    GxEPD2_BW<GxEPD2_750_T7,   GxEPD2_750_T7::HEIGHT>   _display;
#else
    GxEPD2_BW<GxEPD2_750,      GxEPD2_750::HEIGHT>       _display;
#endif

    bool _initialized;

#if IMAGE_FORMAT_PNG
    bool renderPng(const char* path);
#else
    bool renderBmp(const char* path);
#endif

    void drawCenteredText(const String& text, int16_t y);
    void drawBorder();
};
