#include "DisplayManager.h"
#include "config.h"
#include "Logger.h"
#include "esp_task_wdt.h"

// ============================================================
//  DisplayManager — Implementierung
//
//  Format wird zur Compile-Zeit gewählt (IMAGE_FORMAT_PNG):
//    PNG: PNGdec-Decoder, RGB565→1-Bit-Konvertierung
//    BMP: Direktes Bit-Mapping aus LittleFS-Datei
//
//  SPI-Reihenfolge (KRITISCH für Waveshare ESP32 Driver Board):
//    1. SPI.begin() mit Board-Pins
//    2. _display.init()  — GxEPD2 überschreibt ggf. SPI-Pins
//    3. SPI.begin() nochmals — stellt Board-Pins sicher
// ============================================================

// ============================================================
//  WDT-Hilfsfunktionen
// ============================================================
static void wdtSuspend() {
    esp_task_wdt_config_t cfg = {
        .timeout_ms     = 60000,
        .idle_core_mask = 0,
        .trigger_panic  = false
    };
    esp_task_wdt_reconfigure(&cfg);
}

static void wdtResume() {
    esp_task_wdt_config_t cfg = {
        .timeout_ms     = 10000,
        .idle_core_mask = 0,
        .trigger_panic  = false
    };
    esp_task_wdt_reconfigure(&cfg);
}

// ============================================================
//  PNG-spezifisch
// ============================================================
#if IMAGE_FORMAT_PNG

PngRenderContext g_pngCtx = {nullptr, nullptr, 0, 0, false};

static int pngDrawCallback(PNGDRAW* pDraw) {
    if (!g_pngCtx.pixBuf || !g_pngCtx.rowBuf) return 1;
    int32_t y = pDraw->y;
    if (y >= g_pngCtx.displayHeight) return 1;

    PNG* png = (PNG*)pDraw->pUser;
    png->getLineAsRGB565(pDraw, g_pngCtx.rowBuf, PNG_RGB565_BIG_ENDIAN, 0xFFFFFF);

    int32_t lineWidth   = min((int32_t)pDraw->iWidth, g_pngCtx.displayWidth);
    int32_t bytesPerRow = (g_pngCtx.displayWidth + 7) / 8;
    uint8_t* rowDest    = g_pngCtx.pixBuf + (size_t)y * (size_t)bytesPerRow;

    for (int32_t x = 0; x < lineWidth; x++) {
        uint16_t px = g_pngCtx.rowBuf[x];
        uint8_t r = (uint8_t)(((px >> 11) & 0x1F) * 255 / 31);
        uint8_t g = (uint8_t)(((px >>  5) & 0x3F) * 255 / 63);
        uint8_t b = (uint8_t)(((px      ) & 0x1F) * 255 / 31);
        uint16_t brightness = ((uint16_t)r * 299u +
                               (uint16_t)g * 587u +
                               (uint16_t)b * 114u) / 1000u;
        bool isWhite = (brightness >= IMG_GRAY_THRESHOLD);
        if (g_pngCtx.invert) isWhite = !isWhite;
        uint8_t bitMask = (uint8_t)(0x80 >> (x % 8));
        if (!isWhite) rowDest[x / 8] |=  bitMask;
        else          rowDest[x / 8] &= ~bitMask;
    }
    if ((y & 31) == 0) yield();
    return 1;
}

#endif // IMAGE_FORMAT_PNG

// ============================================================
//  Konstruktor + begin()
// ============================================================
DisplayManager::DisplayManager()
    // [DISPLAY-ABHÄNGIG] Treiber wird aus DISPLAY_TYPE in config.h gewählt
#if DISPLAY_TYPE == DISPLAY_V3
    : _display(GxEPD2_750c_Z90(PIN_EPD_CS, PIN_EPD_DC, PIN_EPD_RST, PIN_EPD_BUSY)),
#elif DISPLAY_TYPE == DISPLAY_V2
    : _display(GxEPD2_750_T7(PIN_EPD_CS, PIN_EPD_DC, PIN_EPD_RST, PIN_EPD_BUSY)),
#else
    : _display(GxEPD2_750(PIN_EPD_CS, PIN_EPD_DC, PIN_EPD_RST, PIN_EPD_BUSY)),
#endif
      _initialized(false) {}

void DisplayManager::begin() {
    logInfo("DISP", "Initialisiere Display...");
    logInfo("DISP", "Format: " + String(IMAGE_FORMAT_PNG ? "PNG" : "BMP 1-Bit"));
    logInfo("DISP", "Treiber: " + String(
        DISPLAY_TYPE == DISPLAY_V3 ? "V3 S/W/Rot (GxEPD2_750c_Z90)" :
        DISPLAY_TYPE == DISPLAY_V2 ? "V2 S/W (GxEPD2_750_T7)" :
                                     "V1 S/W (GxEPD2_750)"));
    wdtSuspend();

    // SPI vor und nach init() setzen (GxEPD2 überschreibt ggf.)
    SPI.begin(PIN_SPI_SCK, PIN_SPI_MISO, PIN_SPI_MOSI, PIN_EPD_CS);
    _display.init(SERIAL_BAUD_RATE, true, 2, false);
    SPI.begin(PIN_SPI_SCK, PIN_SPI_MISO, PIN_SPI_MOSI, PIN_EPD_CS);

    wdtResume();
    _display.setRotation(0); // [DISPLAY-ABHÄNGIG] 0=landscape
    _initialized = true;

    logInfo("DISP", "Display OK: " +
                    String(_display.width()) + "x" +
                    String(_display.height()) + " Pixel");
}

// ============================================================
//  showImageFromFile — wählt PNG oder BMP Renderer
// ============================================================
bool DisplayManager::showImageFromFile(const char* path) {
    if (!_initialized) { logError("DISP", "Nicht initialisiert"); return false; }
    if (!LittleFS.exists(path)) { logError("DISP", "Nicht gefunden: " + String(path)); return false; }

#if IMAGE_FORMAT_PNG
    return renderPng(path);
#else
    return renderBmp(path);
#endif
}

// ============================================================
//  renderPng() — nur kompiliert wenn IMAGE_FORMAT_PNG=1
// ============================================================
#if IMAGE_FORMAT_PNG

bool DisplayManager::renderPng(const char* path) {
    logInfo("DISP", "PNG-Decode: " + String(path));
    logHeap("DISP");

    int32_t bytesPerRow = (IMG_WIDTH + 7) / 8;
    size_t  bufSize     = (size_t)bytesPerRow * (size_t)IMG_HEIGHT;

    uint8_t* pixBuf = (uint8_t*)malloc(bufSize);
    if (!pixBuf) { logError("DISP", "malloc pixBuf fehlgeschlagen"); return false; }
    memset(pixBuf, 0x00, bufSize);

    uint16_t* rowBuf = (uint16_t*)malloc((size_t)IMG_WIDTH * 2);
    if (!rowBuf) {
        logError("DISP", "malloc rowBuf fehlgeschlagen");
        free(pixBuf); return false;
    }

    g_pngCtx = {pixBuf, rowBuf, IMG_WIDTH, IMG_HEIGHT, (DISPLAY_INVERT_IMAGE != 0)};

    File pngFile = LittleFS.open(path, "r");
    if (!pngFile) {
        logError("DISP", "Nicht oeffenbar: " + String(path));
        free(rowBuf); free(pixBuf); g_pngCtx.pixBuf = nullptr; return false;
    }

    size_t pngSize = pngFile.size();
    logDebug("DISP", "PNG: " + String(pngSize) + " Bytes");
    logHeap("DISP");

    uint8_t* pngData = (uint8_t*)malloc(pngSize);
    if (!pngData) {
        logError("DISP", "malloc pngData fehlgeschlagen (" + String(pngSize) + " Bytes)");
        pngFile.close(); free(rowBuf); free(pixBuf); g_pngCtx.pixBuf = nullptr; return false;
    }

    if (pngFile.read(pngData, pngSize) != pngSize) {
        logError("DISP", "Lesefehler PNG");
        pngFile.close(); free(pngData); free(rowBuf); free(pixBuf);
        g_pngCtx.pixBuf = nullptr; return false;
    }
    pngFile.close();

    // PNG-Objekt auf Heap — NICHT als Stack-Variable!
    // tinfl_decompressor ist ~15 KB — würde 8 KB ESP32-Stack sofort sprengen.
    PNG* pPng = new PNG();
    if (!pPng) {
        logError("DISP", "new PNG() fehlgeschlagen");
        free(pngData); free(rowBuf); free(pixBuf); g_pngCtx.pixBuf = nullptr; return false;
    }

    int rc = pPng->openRAM(pngData, (int)pngSize, pngDrawCallback);
    if (rc != PNG_SUCCESS) {
        logError("DISP", "openRAM rc=" + String(rc));
        delete pPng; free(pngData); free(rowBuf); free(pixBuf);
        g_pngCtx.pixBuf = nullptr; return false;
    }

    int32_t pngW = pPng->getWidth(), pngH = pPng->getHeight();
    logInfo("DISP", "PNG: " + String(pngW) + "x" + String(pngH));

    if (pngW != IMG_WIDTH || pngH != IMG_HEIGHT) {
        logError("DISP", "Falsche Dimensionen — erwartet " +
                 String(IMG_WIDTH) + "x" + String(IMG_HEIGHT));
        pPng->close(); delete pPng; free(pngData); free(rowBuf); free(pixBuf);
        g_pngCtx.pixBuf = nullptr; return false;
    }

    rc = pPng->decode((void*)pPng, 0);
    pPng->close(); delete pPng; pPng = nullptr;
    free(pngData); free(rowBuf); g_pngCtx.rowBuf = nullptr;

    if (rc != PNG_SUCCESS) {
        logError("DISP", "Decode rc=" + String(rc));
        free(pixBuf); g_pngCtx.pixBuf = nullptr; return false;
    }

    logInfo("DISP", "Decode OK — schreibe auf Display...");
    logHeap("DISP");

    wdtSuspend();
    _display.setFullWindow();
    _display.firstPage();
    do {
        _display.fillScreen(GxEPD_WHITE);
        _display.drawBitmap(0, 0, pixBuf,
                            (int16_t)IMG_WIDTH, (int16_t)IMG_HEIGHT,
                            GxEPD_BLACK, GxEPD_WHITE);
    } while (_display.nextPage());
    wdtResume();

    free(pixBuf); g_pngCtx.pixBuf = nullptr;
    logInfo("DISP", "Display-Update abgeschlossen");
    return true;
}

// ============================================================
//  renderBmp() — nur kompiliert wenn IMAGE_FORMAT_BMP=1
// ============================================================
#else

bool DisplayManager::renderBmp(const char* path) {
    logInfo("DISP", "BMP-Render: " + String(path));
    logHeap("DISP");

    File f = LittleFS.open(path, "r");
    if (!f) { logError("DISP", "Nicht oeffenbar: " + String(path)); return false; }

    // BMP-Header parsen
    uint32_t dataOffset = 0;
    f.seek(10); f.read((uint8_t*)&dataOffset, 4);

    int32_t  bmpW = 0, bmpH = 0;
    uint16_t bitDepth = 0;
    f.seek(18); f.read((uint8_t*)&bmpW, 4); f.read((uint8_t*)&bmpH, 4);
    f.seek(28); f.read((uint8_t*)&bitDepth, 2);
    bool topDown = (bmpH < 0);
    if (topDown) bmpH = -bmpH;

    logInfo("DISP", "BMP: " + String(bmpW) + "x" + String(bmpH) +
                    ", " + String(bitDepth) + "-Bit" +
                    (topDown ? " (top-down)" : " (bottom-up)"));

    // ----------------------------------------------------------------
    //  Farbtabelle einlesen — bei 1-Bit: 2 Eintraege, bei 4-Bit: 16
    //  Jeder Eintrag = 4 Bytes BGRA. Wir werten Helligkeit + Rot aus.
    //  Mapping pro Palette-Index:
    //    palToBlack[i] = true  → Pixel mit diesem Index wird SCHWARZ
    //    palToRed[i]   = true  → Pixel mit diesem Index wird ROT (nur V3)
    //    sonst                  → WEISS
    // ----------------------------------------------------------------
    uint16_t numColors = (bitDepth == 1) ? 2 : 16;
    bool palToBlack[16] = {false};
    bool palToRed[16]   = {false};

    f.seek(54);  // Standardposition der Palette nach 40-Byte DIB-Header
    for (uint16_t i = 0; i < numColors; i++) {
        uint8_t bgra[4];
        f.read(bgra, 4);
        uint8_t b = bgra[0], g = bgra[1], r = bgra[2];
        uint16_t bright = ((uint16_t)r * 299u + (uint16_t)g * 587u + (uint16_t)b * 114u) / 1000u;
        // Rot-Erkennung: dominanter R-Anteil, wenig G/B
        bool isRed = (r >= 150 && g < 120 && b < 120);
        if (isRed) {
            palToRed[i]   = true;
        } else if (bright < 128) {
            palToBlack[i] = true;
        }
    }
    if (DISPLAY_INVERT_IMAGE) {
        // Schwarz und Weiss tauschen — Rot bleibt Rot
        for (int i = 0; i < numColors; i++) {
            if (!palToRed[i]) palToBlack[i] = !palToBlack[i];
        }
    }

    // ----------------------------------------------------------------
    //  Pixelpuffer allozieren — IMMER 1-Bit fuer Schwarz-Anteil.
    //  Bei V3 zusaetzlich 1-Bit fuer Rot-Anteil.
    // ----------------------------------------------------------------
    int32_t  pixBytesPerRow = (bmpW + 7) / 8;
    size_t   blackBufSize   = (size_t)pixBytesPerRow * (size_t)bmpH;

    uint8_t* blackBuf = (uint8_t*)malloc(blackBufSize);
    if (!blackBuf) {
        logError("DISP", "malloc blackBuf fehlgeschlagen (" + String(blackBufSize) + " Bytes)");
        f.close(); return false;
    }
    memset(blackBuf, 0x00, blackBufSize);  // 0 = weiss in drawBitmap-Konvention

#if DISPLAY_TYPE == DISPLAY_V3
    uint8_t* redBuf = (uint8_t*)malloc(blackBufSize);
    if (!redBuf) {
        logError("DISP", "malloc redBuf fehlgeschlagen");
        free(blackBuf); f.close(); return false;
    }
    memset(redBuf, 0x00, blackBufSize);
#endif

    // ----------------------------------------------------------------
    //  Zeilen lesen und konvertieren.
    //  BMP-Zeilenstride ist 4-Byte-aligned.
    // ----------------------------------------------------------------
    uint32_t bmpStride;
    if (bitDepth == 1) {
        bmpStride = (((uint32_t)bmpW + 31) / 32) * 4;
    } else {  // 4-Bit
        bmpStride = (((uint32_t)bmpW * 4 + 31) / 32) * 4;
    }
    uint8_t* rowBuf = (uint8_t*)malloc(bmpStride);
    if (!rowBuf) {
        logError("DISP", "malloc rowBuf fehlgeschlagen");
        free(blackBuf);
#if DISPLAY_TYPE == DISPLAY_V3
        free(redBuf);
#endif
        f.close(); return false;
    }

    for (int32_t row = 0; row < bmpH; row++) {
        // BMP ist bottom-up: Datei-Zeile 0 = unterste Bildzeile.
        // Wir wollen top-down im Puffer → Ziel-Zeile = (bmpH-1-row) wenn bottom-up,
        //                                  Ziel-Zeile = row             wenn top-down.
        int32_t fileRowIdx = row;
        int32_t destRow    = topDown ? row : (bmpH - 1 - row);

        f.seek(dataOffset + (uint32_t)fileRowIdx * bmpStride);
        size_t n = f.read(rowBuf, bmpStride);
        if (n != bmpStride) {
            logError("DISP", "Lesefehler Zeile " + String(row));
            free(rowBuf); free(blackBuf);
#if DISPLAY_TYPE == DISPLAY_V3
            free(redBuf);
#endif
            f.close(); return false;
        }

        uint8_t* destBlackRow = blackBuf + destRow * pixBytesPerRow;
#if DISPLAY_TYPE == DISPLAY_V3
        uint8_t* destRedRow   = redBuf   + destRow * pixBytesPerRow;
#endif

        // Pixel der Zeile auswerten
        for (int32_t x = 0; x < bmpW; x++) {
            uint8_t palIdx;
            if (bitDepth == 1) {
                uint8_t byte = rowBuf[x / 8];
                palIdx = (byte >> (7 - (x & 7))) & 0x01;
            } else {  // 4-Bit
                uint8_t byte = rowBuf[x / 2];
                palIdx = (x & 1) ? (byte & 0x0F) : ((byte >> 4) & 0x0F);
            }

            uint8_t bitMask = (uint8_t)(0x80 >> (x & 7));
            if (palToBlack[palIdx]) {
                destBlackRow[x / 8] |= bitMask;
            }
#if DISPLAY_TYPE == DISPLAY_V3
            else if (palToRed[palIdx]) {
                destRedRow[x / 8] |= bitMask;
            }
#endif
        }

        if ((row & 31) == 0) yield();  // Watchdog freihalten
    }

    free(rowBuf);
    f.close();

    logInfo("DISP", "BMP-Decode OK — schreibe auf Display...");
    logHeap("DISP");

    // ----------------------------------------------------------------
    //  An GxEPD2 uebergeben
    // ----------------------------------------------------------------
    wdtSuspend();
    _display.setFullWindow();
    _display.firstPage();
    do {
        _display.fillScreen(GxEPD_WHITE);
#if DISPLAY_TYPE == DISPLAY_V3
        // Rot zuerst, damit Schwarz darueber liegt falls beide gesetzt waeren
        _display.drawBitmap(0, 0, redBuf,
                            (int16_t)bmpW, (int16_t)bmpH,
                            GxEPD_RED, GxEPD_WHITE);
        _display.drawBitmap(0, 0, blackBuf,
                            (int16_t)bmpW, (int16_t)bmpH,
                            GxEPD_BLACK, GxEPD_WHITE);
#else
        _display.drawBitmap(0, 0, blackBuf,
                            (int16_t)bmpW, (int16_t)bmpH,
                            GxEPD_BLACK, GxEPD_WHITE);
#endif
    } while (_display.nextPage());
    wdtResume();

    free(blackBuf);
#if DISPLAY_TYPE == DISPLAY_V3
    free(redBuf);
#endif

    logInfo("DISP", "Display-Update abgeschlossen");
    return true;
}

#endif // IMAGE_FORMAT_BMP

// ============================================================
//  Startbildschirm
// ============================================================
void DisplayManager::showStartup(const String& statusLine) {
    if (!_initialized) return;
    logInfo("DISP", "Zeige Startbildschirm...");
    wdtSuspend();

    _display.setFullWindow();
    _display.firstPage();
    do {
        _display.fillScreen(GxEPD_WHITE);
        drawBorder();

        _display.setFont(&FreeMonoBold18pt7b);
        _display.setTextColor(GxEPD_BLACK);
        drawCenteredText(String(ROOM_NAME), 90);
        _display.drawFastHLine(30, 115, IMG_WIDTH - 60, GxEPD_BLACK);
        _display.drawFastHLine(30, 117, IMG_WIDTH - 60, GxEPD_BLACK);

        _display.setFont(&FreeMonoBold18pt7b);
        drawCenteredText("DoorSign startet...", 210);

        _display.setFont(&FreeMonoBold9pt7b);
        drawCenteredText(statusLine.length() > 0 ? statusLine : "Bitte warten.", 275);
        drawCenteredText("WLAN: " + String(WIFI_SSID), 310);

        _display.setFont(nullptr);
        _display.setTextSize(1);
        _display.setCursor(20, IMG_HEIGHT - 20);
        _display.print(String(DEVICE_NAME) + "  |  " +
                       String(IMAGE_FORMAT_PNG ? "PNG" : "BMP") + "  |  " +
                       String(__DATE__) + " " + String(__TIME__));
    } while (_display.nextPage());

    wdtResume();
    logInfo("DISP", "Startbildschirm angezeigt");
}

// ============================================================
//  Fallback-Bildschirm
// ============================================================
void DisplayManager::showFallback() {
    if (!_initialized) return;
    logInfo("DISP", "Zeige Fallback...");
    wdtSuspend();

    _display.setFullWindow();
    _display.firstPage();
    do {
        _display.fillScreen(GxEPD_WHITE);
        drawBorder();

        _display.setFont(&FreeMonoBold18pt7b);
        _display.setTextColor(GxEPD_BLACK);
        drawCenteredText(String(ROOM_NAME), 90);
        _display.drawFastHLine(30, 115, IMG_WIDTH - 60, GxEPD_BLACK);
        _display.drawFastHLine(30, 117, IMG_WIDTH - 60, GxEPD_BLACK);

        _display.setFont(&FreeMonoBold18pt7b);
        drawCenteredText("Keine Daten verfugbar", 220);

        _display.setFont(&FreeMonoBold9pt7b);
        drawCenteredText("Verbindung wird hergestellt...", 285);

        _display.setFont(nullptr);
        _display.setTextSize(1);
        _display.setCursor(20, IMG_HEIGHT - 20);
        _display.print(DEVICE_NAME);
    } while (_display.nextPage());

    wdtResume();
    logInfo("DISP", "Fallback angezeigt");
}

// ============================================================
//  hibernate()
// ============================================================
void DisplayManager::hibernate() {
    if (_initialized) {
        _display.hibernate();
        logInfo("DISP", "Display in Ruhemodus");
    }
}

// ============================================================
//  Private Hilfsfunktionen
// ============================================================
void DisplayManager::drawCenteredText(const String& text, int16_t y) {
    int16_t x1, y1; uint16_t w, h;
    _display.getTextBounds(text.c_str(), 0, y, &x1, &y1, &w, &h);
    _display.setCursor(((int16_t)_display.width() - (int16_t)w) / 2, y);
    _display.print(text);
}

void DisplayManager::drawBorder() {
    const int16_t m = 8;
    _display.drawRect(m,     m,     IMG_WIDTH-2*m,     IMG_HEIGHT-2*m,     GxEPD_BLACK);
    _display.drawRect(m+3,   m+3,   IMG_WIDTH-2*(m+3), IMG_HEIGHT-2*(m+3), GxEPD_BLACK);
}
