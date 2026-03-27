#include "DisplayManager.h"
#include "Logger.h"

// ============================================================
//  DisplayManager — Implementierung (PNG-Version)
//
//  PNG-Decode-Strategie:
//    PNGdec ruft für jede Bildzeile einen Callback auf.
//    Im Callback konvertieren wir jeden Pixel nach Graustufe
//    und schreiben das Ergebnis als 1-Bit in einen Heap-Puffer.
//    Nach dem vollständigen Decode übergeben wir den Puffer
//    einmalig an GxEPD2 → ein vollständiger E-Ink Full-Refresh.
//
//  Speicherbedarf:
//    - GxEPD2-Puffer (Stack/intern): 48 000 Bytes
//    - PNG-Pixel-Puffer (malloc):     48 000 Bytes
//    - PNGdec-Arbeitsspeicher:        ~40 KB (intern allokiert)
//    → Gesamt ca. 136 KB — passt in den ESP32-Heap (320 KB SRAM)
// ============================================================

// Globaler Kontext für PNGdec-Callback (kein this-Zeiger möglich)
PngRenderContext g_pngCtx = {nullptr, 0, 0, false};

// ============================================================
//  PNGdec-Callback: wird für jede dekodierte Zeile aufgerufen.
//  pDraw enthält Zeiger auf RGB-Pixel der aktuellen Zeile.
// ============================================================
static void pngDrawCallback(PNGDRAW* pDraw) {
    if (!g_pngCtx.pixBuf) return;

    int32_t y = pDraw->y;
    if (y >= g_pngCtx.displayHeight) return;

    int32_t lineWidth = pDraw->iWidth;

    // Temporäre Zeilenpuffer für RGB888-Konvertierung
    // PNGdec liefert immer RGB888 (3 Bytes pro Pixel) nach Konvertierung
    uint8_t* rgbRow = (uint8_t*)malloc((size_t)lineWidth * 3);
    if (!rgbRow) return;  // Speicher knapp — Zeile überspringen

    // PNGdec in RGB888 konvertieren (funktioniert für alle PNG-Typen:
    // 1-Bit, Graustufen, Palette, RGB, RGBA)
    PNG* png = (PNG*)pDraw->pUser;
    png->getLineAsRGB888(pDraw, rgbRow, PNG_RGB888, 0xFFFFFF);

    // Pixel-Byte-Index im 1-Bit-Puffer für diese Zeile
    int32_t bytesPerRow = (g_pngCtx.displayWidth + 7) / 8;
    uint8_t* rowDest    = g_pngCtx.pixBuf + (size_t)y * (size_t)bytesPerRow;

    // Alle Pixel dieser Zeile in 1-Bit umwandeln
    for (int32_t x = 0; x < lineWidth && x < g_pngCtx.displayWidth; x++) {
        uint8_t r = rgbRow[x * 3 + 0];
        uint8_t g = rgbRow[x * 3 + 1];
        uint8_t b = rgbRow[x * 3 + 2];

        // Wahrgenommene Helligkeit (BT.601-Koeffizienten, ganzzahlig)
        // Formel: Y = (299*R + 587*G + 114*B) / 1000
        uint16_t brightness = ((uint16_t)r * 299u +
                                (uint16_t)g * 587u +
                                (uint16_t)b * 114u) / 1000u;

        bool isWhite = (brightness >= IMG_GRAY_THRESHOLD);
        if (g_pngCtx.invert) isWhite = !isWhite;

        // GxEPD2 writeImage: Bit=0 → weißer Pixel, Bit=1 → schwarzer Pixel
        // Daher: isWhite → Bit=0 (clear), !isWhite → Bit=1 (set)
        uint8_t byteIdx = (uint8_t)(x / 8);
        uint8_t bitMask = (uint8_t)(0x80 >> (x % 8));
        if (!isWhite) {
            rowDest[byteIdx] |=  bitMask;  // schwarz → Bit setzen
        } else {
            rowDest[byteIdx] &= ~bitMask;  // weiß   → Bit löschen
        }
    }

    free(rgbRow);
}

// ============================================================
//  DisplayManager — Konstruktor und Initialisierung
// ============================================================

// [DISPLAY-ABHÄNGIG] Konstruktor: Pins aus config.h
DisplayManager::DisplayManager()
    : _display(GxEPD2_750_T7(PIN_EPD_CS, PIN_EPD_DC, PIN_EPD_RST, PIN_EPD_BUSY)),
      _initialized(false) {}

void DisplayManager::begin() {
    logInfo("DISP", "Initialisiere Display...");
    _display.init(SERIAL_BAUD_RATE, true, 2, false);
    _display.setRotation(0);  // [DISPLAY-ABHÄNGIG] 0=landscape, 1=portrait, usw.
    _initialized = true;
    logInfo("DISP", "Display OK: " +
                    String(_display.width()) + " x " +
                    String(_display.height()) + " Pixel");
}

bool DisplayManager::showImageFromFile(const char* path) {
    if (!_initialized) {
        logError("DISP", "Display nicht initialisiert");
        return false;
    }
    if (!LittleFS.exists(path)) {
        logError("DISP", "Datei nicht gefunden: " + String(path));
        return false;
    }
    return renderPng(path);
}

bool DisplayManager::renderPng(const char* path) {
    logInfo("DISP", "Starte PNG-Decode: " + String(path));
    logHeap("DISP");

    // ---- Pixel-Puffer allokieren (1-Bit, IMG_WIDTH × IMG_HEIGHT) ----
    int32_t bytesPerRow = (IMG_WIDTH + 7) / 8;
    size_t  bufSize     = (size_t)bytesPerRow * (size_t)IMG_HEIGHT;

    uint8_t* pixBuf = (uint8_t*)malloc(bufSize);
    if (!pixBuf) {
        logError("DISP", "malloc fehlgeschlagen: " + String(bufSize) + " Bytes");
        return false;
    }
    // Alles auf weiß initialisieren (0x00 = alle Bits 0 = alle Pixel weiß in GxEPD2)
    memset(pixBuf, 0x00, bufSize);

    // ---- Globalen Kontext für Callback befüllen ----
    g_pngCtx.pixBuf       = pixBuf;
    g_pngCtx.displayWidth = IMG_WIDTH;
    g_pngCtx.displayHeight= IMG_HEIGHT;
    g_pngCtx.invert       = (DISPLAY_INVERT_IMAGE != 0);

    // ---- PNG aus LittleFS öffnen und dekodieren ----
    File pngFile = LittleFS.open(path, "r");
    if (!pngFile) {
        logError("DISP", "Datei nicht oeffenbar: " + String(path));
        free(pixBuf);
        g_pngCtx.pixBuf = nullptr;
        return false;
    }

    // PNGdec benötigt die Datei als Byte-Array im Speicher ODER
    // via openRAM(). Da LittleFS-Dateien bis 200 KB passen in den Heap
    // (ESP32 hat ~200–280 KB freien Heap nach GxEPD2), laden wir sie direkt.
    size_t  pngSize = pngFile.size();
    uint8_t* pngData = (uint8_t*)malloc(pngSize);
    if (!pngData) {
        logError("DISP", "malloc fuer PNG-Daten fehlgeschlagen: " + String(pngSize) + " Bytes");
        pngFile.close();
        free(pixBuf);
        g_pngCtx.pixBuf = nullptr;
        return false;
    }

    size_t bytesRead = pngFile.read(pngData, pngSize);
    pngFile.close();

    if (bytesRead != pngSize) {
        logError("DISP", "Lesefehler: " + String(bytesRead) + "/" + String(pngSize));
        free(pngData);
        free(pixBuf);
        g_pngCtx.pixBuf = nullptr;
        return false;
    }

    logDebug("DISP", "PNG in RAM geladen: " + String(pngSize) + " Bytes");
    logHeap("DISP");

    // PNG dekodieren
    PNG png;
    int rc = png.openRAM(pngData, (int)pngSize, pngDrawCallback);
    if (rc != PNG_SUCCESS) {
        logError("DISP", "png.openRAM fehlgeschlagen: " + String(rc));
        free(pngData);
        free(pixBuf);
        g_pngCtx.pixBuf = nullptr;
        return false;
    }

    // Bilddimensionen prüfen
    int32_t pngW = png.getWidth();
    int32_t pngH = png.getHeight();
    logInfo("DISP", "PNG-Dimensionen: " + String(pngW) + " x " + String(pngH));

    if (pngW != IMG_WIDTH || pngH != IMG_HEIGHT) {
        logError("DISP", "Falsche Dimensionen (erwartet: " +
                         String(IMG_WIDTH) + "x" + String(IMG_HEIGHT) + ")");
        png.close();
        free(pngData);
        free(pixBuf);
        g_pngCtx.pixBuf = nullptr;
        return false;
    }

    // Dekodierung starten — ruft pngDrawCallback für jede Zeile auf
    // pUser wird an den Callback weitergereicht (für getLineAsRGB888)
    rc = png.decode((void*)&png, 0);
    png.close();
    free(pngData);

    if (rc != PNG_SUCCESS) {
        logError("DISP", "PNG-Decode fehlgeschlagen: " + String(rc));
        free(pixBuf);
        g_pngCtx.pixBuf = nullptr;
        return false;
    }

    logInfo("DISP", "PNG-Decode erfolgreich — schreibe auf Display...");

    // ---- Puffer auf E-Ink-Display ausgeben ----
    // Full-Update: löscht Ghosting, aktualisiert das gesamte Display.
    // PNG ist top-down gespeichert → mirror_y = false.
    _display.setFullWindow();
    _display.writeImage(pixBuf,
                        0, 0,
                        (int16_t)IMG_WIDTH, (int16_t)IMG_HEIGHT,
                        false,   // invert: bereits im Callback verarbeitet
                        false,   // mirror_y: PNG ist top-down (kein Spiegeln nötig)
                        false);  // pgm: false (RAM, kein PROGMEM)
    _display.display(false);     // false = Full-Update

    free(pixBuf);
    g_pngCtx.pixBuf = nullptr;

    logInfo("DISP", "Display-Update abgeschlossen");
    return true;
}

void DisplayManager::showFallback() {
    if (!_initialized) return;
    logInfo("DISP", "Zeige Fallback-Bildschirm");

    _display.setFullWindow();
    _display.firstPage();
    do {
        _display.fillScreen(GxEPD_WHITE);
        drawBorder();

        _display.setFont(&FreeMonoBold18pt7b);
        _display.setTextColor(GxEPD_BLACK);
        drawCenteredText(String(ROOM_NAME), 80);

        _display.drawFastHLine(30, 110, IMG_WIDTH - 60, GxEPD_BLACK);
        _display.drawFastHLine(30, 112, IMG_WIDTH - 60, GxEPD_BLACK);

        _display.setFont(&FreeMonoBold18pt7b);
        drawCenteredText("Keine Daten verfugbar", 210);

        _display.setFont(&FreeMonoBold9pt7b);
        drawCenteredText("Verbindung wird hergestellt...", 280);
        drawCenteredText("Bitte warten.", 310);

        _display.setFont(nullptr);
        _display.setTextSize(1);
        _display.setTextColor(GxEPD_BLACK);
        _display.setCursor(20, IMG_HEIGHT - 20);
        _display.print(DEVICE_NAME);

    } while (_display.nextPage());
}

void DisplayManager::hibernate() {
    if (_initialized) {
        _display.hibernate();
        logInfo("DISP", "Display in Ruhemodus versetzt");
    }
}

void DisplayManager::drawCenteredText(const String& text, int16_t y) {
    int16_t x1, y1;
    uint16_t w, h;
    _display.getTextBounds(text.c_str(), 0, y, &x1, &y1, &w, &h);
    int16_t x = ((int16_t)_display.width() - (int16_t)w) / 2;
    _display.setCursor(x, y);
    _display.print(text);
}

void DisplayManager::drawBorder() {
    const int16_t m = 8;
    _display.drawRect(m,     m,     IMG_WIDTH - 2*m,     IMG_HEIGHT - 2*m,     GxEPD_BLACK);
    _display.drawRect(m + 3, m + 3, IMG_WIDTH - 2*(m+3), IMG_HEIGHT - 2*(m+3), GxEPD_BLACK);
}
