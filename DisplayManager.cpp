#include "DisplayManager.h"
#include "config.h"
#include "Logger.h"
#include "esp_task_wdt.h"

// ============================================================
//  DisplayManager — Implementierung
//
//  SPI-Reihenfolge (KRITISCH):
//    _display.init()  →  GxEPD2 setzt SPI auf Default-Pins (18/23)
//    SPI.begin(...)   →  wir überschreiben DANACH mit Board-Pins
//
//  PNG-Decode:
//    Callback wird 480× aufgerufen (1× pro Bildzeile).
//    rowBuf wird einmal alloziert und wiederverwendet → kein
//    malloc/free-Overhead pro Zeile.
//    yield() alle 32 Zeilen → FreeRTOS-Scheduler bleibt aktiv →
//    kein TG1WDT-Reset.
// ============================================================

PngRenderContext g_pngCtx = {nullptr, nullptr, 0, 0, false};

// ============================================================
//  WDT-Hilfsfunktionen (ESP32 Arduino 3.x / IDF 5.x)
//  Erhöhen nur den Timeout — reset() wird bewusst weggelassen
//  da der loopTask u.U. nicht beim TWDT registriert ist.
// ============================================================
static void wdtSuspend() {
    esp_task_wdt_config_t cfg = {
        .timeout_ms     = 60000,   // 60 s für E-Ink-Refresh
        .idle_core_mask = 0,
        .trigger_panic  = false
    };
    esp_task_wdt_reconfigure(&cfg);
}

static void wdtResume() {
    esp_task_wdt_config_t cfg = {
        .timeout_ms     = 10000,   // 10 s Normalbetrieb
        .idle_core_mask = 0,
        .trigger_panic  = false
    };
    esp_task_wdt_reconfigure(&cfg);
}

// ============================================================
//  PNGdec-Callback — einmal pro Bildzeile aufgerufen
//  Rückgabe: 1 = weitermachen, 0 = abbrechen
// ============================================================
static int pngDrawCallback(PNGDRAW* pDraw) {
    if (!g_pngCtx.pixBuf || !g_pngCtx.rowBuf) return 1;

    int32_t y = pDraw->y;
    if (y >= g_pngCtx.displayHeight) return 1;

    // Pre-allozierter rowBuf aus dem Kontext — kein malloc/free hier!
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

    // FreeRTOS-Scheduler alle 32 Zeilen yielden lassen.
    // Verhindert TG1WDT durch langen blockierenden Decode-Loop.
    if ((y & 31) == 0) yield();

    return 1;
}

// ============================================================
//  Konstruktor
// ============================================================
DisplayManager::DisplayManager()
    : _display(GxEPD2_750(PIN_EPD_CS, PIN_EPD_DC, PIN_EPD_RST, PIN_EPD_BUSY)),
      _initialized(false) {}

// ============================================================
//  begin()
// ============================================================
void DisplayManager::begin() {
    logInfo("DISP", "Initialisiere Display...");

    wdtSuspend();

    // SPI VOR init() konfigurieren: falls GxEPD2 intern SPI.begin() aufruft
    // und SPI bereits initialisiert ist, wird der Aufruf ggf. uebersprungen.
    // [DISPLAY-ABHÄNGIG] Pins aus config.h
    SPI.begin(PIN_SPI_SCK, PIN_SPI_MISO, PIN_SPI_MOSI, PIN_EPD_CS);
    logInfo("DISP", "SPI vor init(): SCK=" + String(PIN_SPI_SCK) +
                    " MOSI=" + String(PIN_SPI_MOSI) +
                    " CS="   + String(PIN_EPD_CS));

    // GxEPD2 init — sendet Initialisierungssequenz an das Display
    _display.init(SERIAL_BAUD_RATE, true, 2, false);

    // SPI NOCHMALS setzen: sicherheitshalber falls GxEPD2 intern
    // SPI.begin() mit Default-Pins aufgerufen hat.
    SPI.begin(PIN_SPI_SCK, PIN_SPI_MISO, PIN_SPI_MOSI, PIN_EPD_CS);
    logInfo("DISP", "SPI nach init(): SCK=" + String(PIN_SPI_SCK) +
                    " MOSI=" + String(PIN_SPI_MOSI) +
                    " CS="   + String(PIN_EPD_CS));

    wdtResume();

    _display.setRotation(0);  // [DISPLAY-ABHÄNGIG] 0=landscape 800x480
    _initialized = true;

    logInfo("DISP", "Display OK: " +
                    String(_display.width()) + "x" +
                    String(_display.height()) + " Pixel");
}

// ============================================================
//  showStartup()
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
        drawCenteredText(
            statusLine.length() > 0 ? statusLine : "Bitte warten.",
            275);
        drawCenteredText("WLAN: " + String(WIFI_SSID), 310);

        _display.setFont(nullptr);
        _display.setTextSize(1);
        _display.setCursor(20, IMG_HEIGHT - 20);
        _display.print(String(DEVICE_NAME) + "  |  " +
                       String(__DATE__) + " " + String(__TIME__));

    } while (_display.nextPage());

    wdtResume();
    logInfo("DISP", "Startbildschirm angezeigt");
}

// ============================================================
//  showImageFromFile / renderPng
// ============================================================
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
    logInfo("DISP", "PNG-Decode: " + String(path));
    logHeap("DISP");

    // ---- Puffer allozieren ----
    int32_t bytesPerRow = (IMG_WIDTH + 7) / 8;
    size_t  bufSize     = (size_t)bytesPerRow * (size_t)IMG_HEIGHT;

    uint8_t* pixBuf = (uint8_t*)malloc(bufSize);
    if (!pixBuf) {
        logError("DISP", "malloc pixBuf fehlgeschlagen (" + String(bufSize) + " Bytes)");
        return false;
    }
    memset(pixBuf, 0x00, bufSize);

    // Zeilenpuffer: einmal allozieren, in allen 480 Callback-Aufrufen wiederverwenden
    uint16_t* rowBuf = (uint16_t*)malloc((size_t)IMG_WIDTH * 2);
    if (!rowBuf) {
        logError("DISP", "malloc rowBuf fehlgeschlagen");
        free(pixBuf);
        return false;
    }

    g_pngCtx.pixBuf        = pixBuf;
    g_pngCtx.rowBuf        = rowBuf;
    g_pngCtx.displayWidth  = IMG_WIDTH;
    g_pngCtx.displayHeight = IMG_HEIGHT;
    g_pngCtx.invert        = (DISPLAY_INVERT_IMAGE != 0);

    // ---- PNG in RAM laden ----
    File pngFile = LittleFS.open(path, "r");
    if (!pngFile) {
        logError("DISP", "Datei nicht oeffenbar");
        free(rowBuf);
        free(pixBuf);
        g_pngCtx.pixBuf = nullptr;
        g_pngCtx.rowBuf = nullptr;
        return false;
    }

    size_t pngSize = pngFile.size();
    logDebug("DISP", "PNG-Dateigroesse: " + String(pngSize) + " Bytes");
    logHeap("DISP");

    uint8_t* pngData = (uint8_t*)malloc(pngSize);
    if (!pngData) {
        logError("DISP", "malloc pngData fehlgeschlagen (" + String(pngSize) + " Bytes)");
        pngFile.close();
        free(rowBuf);
        free(pixBuf);
        g_pngCtx.pixBuf = nullptr;
        g_pngCtx.rowBuf = nullptr;
        return false;
    }

    size_t bytesRead = pngFile.read(pngData, pngSize);
    pngFile.close();

    if (bytesRead != pngSize) {
        logError("DISP", "Lesefehler: " + String(bytesRead) + "/" + String(pngSize));
        free(pngData);
        free(rowBuf);
        free(pixBuf);
        g_pngCtx.pixBuf = nullptr;
        g_pngCtx.rowBuf = nullptr;
        return false;
    }

    // ---- PNG dekodieren ----
    // WICHTIG: PNG-Objekt auf dem HEAP anlegen (nicht als lokale Variable)!
    // Die interne tinfl_decompressor-Struktur ist ~11-15 KB gross.
    // Als Stack-Variable wuerde das den 8KB ESP32-Stack sofort ueberlaufen.
    PNG* pPng = new PNG();
    if (!pPng) {
        logError("DISP", "Kein Heap fuer PNG-Objekt");
        free(pngData);
        free(rowBuf);
        free(pixBuf);
        g_pngCtx.pixBuf = nullptr;
        g_pngCtx.rowBuf = nullptr;
        return false;
    }
    logDebug("DISP", "PNG-Objekt auf Heap: " + String(sizeof(PNG)) + " Bytes");

    int rc = pPng->openRAM(pngData, (int)pngSize, pngDrawCallback);
    if (rc != PNG_SUCCESS) {
        logError("DISP", "png.openRAM: rc=" + String(rc));
        delete pPng;
        free(pngData);
        free(rowBuf);
        free(pixBuf);
        g_pngCtx.pixBuf = nullptr;
        g_pngCtx.rowBuf = nullptr;
        return false;
    }

    int32_t pngW = pPng->getWidth();
    int32_t pngH = pPng->getHeight();
    logInfo("DISP", "PNG: " + String(pngW) + "x" + String(pngH));

    if (pngW != IMG_WIDTH || pngH != IMG_HEIGHT) {
        logError("DISP", "Falsche Dimensionen — erwartet " +
                 String(IMG_WIDTH) + "x" + String(IMG_HEIGHT));
        pPng->close();
        delete pPng;
        free(pngData);
        free(rowBuf);
        free(pixBuf);
        g_pngCtx.pixBuf = nullptr;
        g_pngCtx.rowBuf = nullptr;
        return false;
    }

    // Dekodierung — 480 Callback-Aufrufe, je mit yield() alle 32 Zeilen
    rc = pPng->decode((void*)pPng, 0);
    pPng->close();
    delete pPng;
    pPng = nullptr;
    free(pngData);
    free(rowBuf);
    g_pngCtx.rowBuf = nullptr;

    if (rc != PNG_SUCCESS) {
        logError("DISP", "PNG-Decode: rc=" + String(rc));
        free(pixBuf);
        g_pngCtx.pixBuf = nullptr;
        return false;
    }

    logInfo("DISP", "Decode OK — schreibe auf Display...");
    logHeap("DISP");

    // ---- E-Ink-Refresh via firstPage/nextPage ----
    // Dieses Muster ist zuverlässiger als writeImage()+display():
    //   • fillScreen(WHITE) stellt sicher dass das volle 800×480 Display
    //     weiß ist, auch wenn das Bild kleiner ist (z.B. 640×384)
    //   • drawBitmap() nutzt GxEPD2-interne Konvention:
    //     bit=1 → Vordergrundfarbe (schwarz), bit=0 → Hintergrundfarbe (weiß)
    //     Das passt exakt zu unserem pixBuf aus dem PNG-Decoder.
    //   • firstPage()/nextPage() initialisiert das Display korrekt
    //     und vermeidet Bit-Polaritätsprobleme von writeImage().
    wdtSuspend();
    _display.setFullWindow();
    _display.firstPage();
    do {
        _display.fillScreen(GxEPD_WHITE);
        // [DISPLAY-ABHÄNGIG] Bildposition: (0,0) = oben links.
        // Für zentriertes Bild: x=(800-IMG_WIDTH)/2, y=(480-IMG_HEIGHT)/2
        _display.drawBitmap(0, 0, pixBuf,
                            (int16_t)IMG_WIDTH, (int16_t)IMG_HEIGHT,
                            GxEPD_BLACK, GxEPD_WHITE);
    } while (_display.nextPage());
    wdtResume();

    free(pixBuf);
    g_pngCtx.pixBuf = nullptr;

    logInfo("DISP", "Display-Update abgeschlossen");
    return true;
}

// ============================================================
//  showFallback()
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
        logInfo("DISP", "Display in Ruhemodus versetzt");
    }
}

// ============================================================
//  Private Hilfsfunktionen
// ============================================================
void DisplayManager::drawCenteredText(const String& text, int16_t y) {
    int16_t x1, y1;
    uint16_t w, h;
    _display.getTextBounds(text.c_str(), 0, y, &x1, &y1, &w, &h);
    _display.setCursor(((int16_t)_display.width() - (int16_t)w) / 2, y);
    _display.print(text);
}

void DisplayManager::drawBorder() {
    const int16_t m = 8;
    _display.drawRect(m,     m,     IMG_WIDTH - 2*m,     IMG_HEIGHT - 2*m,     GxEPD_BLACK);
    _display.drawRect(m + 3, m + 3, IMG_WIDTH - 2*(m+3), IMG_HEIGHT - 2*(m+3), GxEPD_BLACK);
}
