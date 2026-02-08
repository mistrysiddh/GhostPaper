#include "common.h"
#include "default_book.h"
#include <WiFi.h>
#include <Button2.h>
#include <math.h> 
#include <WebServer.h>
#include <ESPmDNS.h>
#include <qrcode.h>

// --- Configuration ---
const unsigned long TOUCH_COOLDOWN = 300; 

// --- Global Variable Definitions ---
WebServer server(80);
File uploadFile;
RTC_DATA_ATTR AppState appState = STATE_SPLASH;
RTC_DATA_ATTR LibFilter libraryFilter = FILTER_ALL;
RTC_DATA_ATTR int librarySelection = 0; 
RTC_DATA_ATTR int currentFileIndex = -1;
RTC_DATA_ATTR long textPos = 0;
RTC_DATA_ATTR float fontScale = 1.15;
RTC_DATA_ATTR long lastPageByteCount = 0;
RTC_DATA_ATTR int focusedBookIndex = -1;

uint8_t *framebuffer = NULL;
std::vector<String> books;
std::vector<String> filteredBooks;
Preferences prefs;
Button2 smartBtn;
TouchDrvGT911 touch;
std::vector<long> pageHistory;
unsigned long lastInteraction = 0;
unsigned long lastTouchTime = 0;
PCF8563_Class rtc;
bool touchReleased = true; // Guard for double clicks

// --- Visual Feedback Functions ---

void showTapFeedback(int x, int y) {
#if TAP_INDICATOR_STYLE == 1
    // Style 1: Small circular dot for subtle feedback
    int radius = 10;
    int size = (radius * 2) + 4;
    
    // 1. Prepare a small localized buffer for the circle
    // We use a small portion of the framebuffer temporarily or a stack-allocated one
    uint8_t circleBuf[size * size / 2]; 
    memset(circleBuf, COL_WHITE, sizeof(circleBuf));
    
    // Draw circle into our small buffer (coordinates relative to buffer)
    // We can use epd_fill_circle but it expects the full framebuffer size logic
    // So we manually draw a few lines/pixels for speed and control
    for (int dy = -radius; dy <= radius; dy++) {
        for (int dx = -radius; dx <= radius; dx++) {
            if (dx*dx + dy*dy <= radius*radius) {
                int lx = dx + radius + 2;
                int ly = dy + radius + 2;
                int idx = (ly * size + lx) / 2;
                if (lx % 2 == 0) circleBuf[idx] = (circleBuf[idx] & 0x0F) | (COL_BLACK << 4);
                else circleBuf[idx] = (circleBuf[idx] & 0xF0) | (COL_BLACK & 0x0F);
            }
        }
    }

    // 2. Physical Mapping (Portrait -> Landscape)
    Rect_t area = {
        .x = (int32_t)(960 - 1 - (y + size/2)),
        .y = (int32_t)(x - size/2),
        .width = (uint32_t)size,
        .height = (uint32_t)size
    };

    epd_poweron();
    epd_draw_grayscale_image(area, circleBuf);
    epd_poweroff();
#else
    // Default Style: Fast localized dark flash
    int size = 40;
    Rect_t area = {
        .x = (int32_t)(960 - 1 - (y + size/2)),
        .y = (int32_t)(x - size/2),
        .width = (uint32_t)size,
        .height = (uint32_t)size
    };
    epd_poweron();
    epd_push_pixels(area, 20, 0); // Quick darken
    epd_poweroff();
#endif
}

void showTransitionEffect() {
    epd_poweron();
    memset(framebuffer, COL_LIGHT, L_WIDTH * L_HEIGHT / 2);
    epd_draw_grayscale_image(epd_full_screen(), framebuffer);
    epd_poweroff();
    delay(80);
}

// --- Menu Logic ---

void redrawBookCover(int index) {
    int localIdx = index % SHELF_BOOKS_PER_PAGE;
    int col = localIdx % SHELF_COLS;
    int row = localIdx / SHELF_COLS;
    int x = GAP_X + col * (BOOK_W + GAP_X);
    int y = (SHELF_START_Y + 40) + row * (BOOK_H + GAP_Y);

    long savedPos = prefs.getLong(getPrefKey(books[index]).c_str(), 0);
    long totalSize = 1;
    if (books[index].startsWith("internal:")) {
        totalSize = strlen(DEFAULT_BOOK_TEXT);
    } else {
        File f = SD.open(books[index]);
        if (f) { totalSize = f.size(); f.close(); }
    }
    int pct = (totalSize > 0) ? (savedPos * 100 / totalSize) : 0;
    
    // Draw the cover into the framebuffer memory
    // Uses the new style defined in library.cpp automatically
    drawEnhancedBookCover(x, y, books[index].substring(books[index].lastIndexOf('/') + 1), pct, index);
}

void updateBookCardMenu(int index, bool showMenu) {
    // 1. Calculate Logical Portrait Position (540x960)
    int localIdx = index % SHELF_BOOKS_PER_PAGE;
    int col = localIdx % SHELF_COLS;
    int row = localIdx / SHELF_COLS;
    int x = GAP_X + col * (BOOK_W + GAP_X);
    int y = (SHELF_START_Y + 40) + row * (BOOK_H + GAP_Y);

    // 2. Physical Mapping (Portrait -> Landscape 960x540)
    int32_t physY = x;
    int32_t physH = BOOK_W;

    // 3. Define the Physical Stripe (Full Width 960)
    Rect_t stripeArea = {
        .x = 0,
        .y = physY,
        .width = 960,
        .height = physH
    };

    // Card-only area for the physical "wash"
    Rect_t cardArea = {
        .x = (int32_t)(960 - 1 - (y + BOOK_H)),
        .y = (int32_t)x,
        .width = (int32_t)BOOK_H,
        .height = (int32_t)BOOK_W
    };

    epd_poweron();

    // --- CRITICAL: Universal Physical Wash ---
    for (int i = 0; i < 3; i++) {
        epd_push_pixels(cardArea, 50, 1);
    }

    if (showMenu) {
        // --- CLEAN WHITE MINIMALIST MENU WITH BORDER ---
        fill_rect_rotated(x, y, BOOK_W, BOOK_H, COL_WHITE);
        draw_rect_rotated(x, y, BOOK_W, BOOK_H, COL_BLACK);
        draw_rect_rotated(x + 1, y + 1, BOOK_W - 2, BOOK_H - 2, COL_BLACK);

        int bh = BOOK_H / 4; 
        int vCenterOffset = (bh / 2) + 12; // Adjusted for precise vertical center
        float optScale = 0.75;

        // Slot 1: READ
        const char* tRead = "READ";
        int wRead = get_text_width_scaled(tRead, optScale);
        writeln_scaled(tRead, x + (BOOK_W - wRead) / 2, y + vCenterOffset, optScale, true, COL_BLACK);
        
        // Slot 2: RESET
        const char* tReset = "RESET";
        int wReset = get_text_width_scaled(tReset, optScale);
        writeln_scaled(tReset, x + (BOOK_W - wReset) / 2, y + bh + vCenterOffset, optScale, true, COL_BLACK);

        // Slot 3: DELETE
        const char* tDel = "DELETE";
        uint8_t delCol = COL_BLACK;
        if (filteredBooks[index].startsWith("internal:")) {
            tDel = "SYSTEM";
            delCol = COL_GRAY;
        }
        int wDel = get_text_width_scaled(tDel, optScale);
        writeln_scaled(tDel, x + (BOOK_W - wDel) / 2, y + 2 * bh + vCenterOffset, optScale, true, delCol);
        
        // Slot 4: BACK
        const char* tBack = "BACK";
        int wBack = get_text_width_scaled(tBack, optScale);
        writeln_scaled(tBack, x + (BOOK_W - wBack) / 2, y + 3 * bh + vCenterOffset, optScale, true, COL_BLACK);
    }

    // Refresh the entire Stripe (works for both menu and cover restoration)
    uint8_t *stripePtr = &framebuffer[physY * 960 / 2];
    epd_draw_grayscale_image(stripeArea, stripePtr);
    
    epd_poweroff();
}

void updateLibraryMenu(bool showMenu) {
    int boxTop = 150;
    int boxBottom = 750;
    int boxMargin = 60;
    int menuX = boxMargin;
    int menuY = boxTop;
    int menuW = P_WIDTH - 2 * boxMargin;
    int menuH = boxBottom - boxTop;

    if (showMenu) {
        Rect_t menuArea = {
            .x = (int32_t)(960 - 1 - (menuY + menuH)),
            .y = (int32_t)menuX,
            .width = (int32_t)menuH,
            .height = (int32_t)menuW
        };

        epd_poweron();
        for (int i = 0; i < 3; i++) epd_push_pixels(menuArea, 50, 1);

        fill_rect_rotated(menuX, menuY, menuW, menuH, COL_WHITE);
        draw_rect_rotated(menuX, menuY, menuW, menuH, COL_BLACK);
        draw_rect_rotated(menuX + 2, menuY + 2, menuW - 4, menuH - 4, COL_BLACK);

        int bh = menuH / 5;
        int vCenterOffset = (bh / 2) + 15;
        const char* labels[] = {"NEXT PAGE", "PREV PAGE", "REFRESH", "SYNC", "BACK"};
        
        for (int i = 0; i < 5; i++) {
            float scale = 0.8;
            int tw = get_text_width_scaled(labels[i], scale);
            writeln_scaled(labels[i], menuX + (menuW - tw) / 2, menuY + (i * bh) + vCenterOffset, scale, true, COL_BLACK);
            if (i < 4) draw_line_rotated(menuX + 30, menuY + (i + 1) * bh, menuX + menuW - 30, menuY + (i + 1) * bh, COL_LIGHT);
        }
        epd_draw_grayscale_image(epd_full_screen(), framebuffer);
        epd_poweroff();
    } else {
        updateLibrary();
    }
}

void redrawReaderText() {
    updateReader(false);
}

void updateReaderMenu(bool showMenu) {
    // 1. Calculate Full Main Body Area (matching reader.cpp)
    int boxTop = 80;
    int boxBottom = 820;
    int boxMargin = 15;
    int menuX = boxMargin;
    int menuY = boxTop;
    int menuW = P_WIDTH - 2 * boxMargin;
    int menuH = boxBottom - boxTop;

    if (showMenu) {
        // 2. Physical Mapping for Full Area Flash
        Rect_t menuArea = {
            .x = (int32_t)(960 - 1 - (menuY + menuH)),
            .y = (int32_t)menuX,
            .width = (int32_t)menuH,
            .height = (int32_t)menuW
        };

        epd_poweron();
        // --- PHYSICAL WASH (Library Style) ---
        for (int i = 0; i < 3; i++) {
            epd_push_pixels(menuArea, 50, 1);
        }

        // --- CLEAN WHITE CARD MENU (Full Size) ---
        fill_rect_rotated(menuX, menuY, menuW, menuH, COL_WHITE);
        
        // Double Border to match reader style
        draw_rounded_rect(menuX, menuY, menuW, menuH, 8, COL_BLACK);
        draw_rounded_rect(menuX + 2, menuY + 2, menuW - 4, menuH - 4, 7, COL_BLACK);

        int bh = menuH / 5;
        int vCenterOffset = (bh / 2) + 15;

        const char* labels[] = {"FONT +", "FONT -", "REFRESH", "SYNC", "BACK"};
        for (int i = 0; i < 5; i++) {
            float scale = 1.0; // Larger text for larger menu
            int tw = get_text_width_scaled(labels[i], scale);
            writeln_scaled(labels[i], menuX + (menuW - tw) / 2, menuY + (i * bh) + vCenterOffset, scale, true, COL_BLACK);
            
            // Decorative separators
            if (i < 4) {
                draw_line_rotated(menuX + 40, menuY + (i + 1) * bh, menuX + menuW - 40, menuY + (i + 1) * bh, COL_LIGHT);
            }
        }

        epd_draw_grayscale_image(epd_full_screen(), framebuffer);
        epd_poweroff();
    } else {
        redrawReaderText();
    }
}

String enteredPin = "";
String activePin = ""; // Loaded from prefs

void drawPinPad(bool settingNew) {
    epd_poweron();
    memset(framebuffer, COL_WHITE, L_WIDTH * L_HEIGHT / 2);

    // --- Master Security Card (Borderless) ---
    int cardW = 460;
    int cardH = 820;
    int cardX = (P_WIDTH - cardW) / 2;
    int cardY = 70;

    // 1. Pure White Background (No Shadow, No Border)
    fill_rect_rotated(cardX, cardY, cardW, cardH, COL_WHITE);
    
    // 2. Header Section (Dark Bar)
    fill_rect_rotated(cardX + 15, cardY + 15, cardW - 30, 80, COL_BLACK);
    const char* title = settingNew ? "SETUP SECURITY" : "SECURE ACCESS";
    int tw = get_text_width_scaled(title, 0.75);
    writeln_scaled(title, cardX + (cardW - tw) / 2, cardY + 68, 0.75, true, COL_WHITE);

    // 3. PIN Dots Section
    int dotRadius = 12;
    int dotGap = 45;
    int dotTotalW = (6 * 2 * dotRadius) + (5 * dotGap);
    int dotStartX = cardX + (cardW - dotTotalW) / 2 + dotRadius;
    int dotY = cardY + 160;
    
    // Instructions for setup (without line)
    if (settingNew) {
        const char* sub = "Create a 6-digit PIN";
        int sw = get_text_width_scaled(sub, 0.45);
        writeln_scaled(sub, cardX + (cardW - sw) / 2, dotY + 65, 0.45, false, COL_BLACK);
    }

    for (int i = 0; i < 6; i++) {
        int dx = dotStartX + i * (2 * dotRadius + dotGap);
        if (i < (int)enteredPin.length()) {
            for (int r = 0; r < dotRadius; r++) draw_circle_rotated(dx, dotY, r, COL_BLACK);
        } else {
            draw_circle_rotated(dx, dotY, dotRadius, COL_BLACK);
            draw_circle_rotated(dx, dotY, dotRadius - 1, COL_BLACK);
        }
    }

    // 5. Keypad Section
    int kw = 110, kh = 100;
    int kGapX = 25, kGapY = 20;
    int gridStartX = cardX + (cardW - (3 * kw + 2 * kGapX)) / 2;
    int gridStartY = cardY + 260;

    const char* keys[] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "CLR", "0", "OK"};
    for (int i = 0; i < 12; i++) {
        int col = i % 3;
        int row = i / 3;
        int bx = gridStartX + col * (kw + kGapX);
        int by = gridStartY + row * (kh + kGapY);

                        // Borderless flat style

                        fill_rect_rotated(bx, by, kw, kh, COL_WHITE); // BG

                        

                        int textW = get_text_width_scaled(keys[i], 0.8);

                        writeln_scaled(keys[i], bx + (kw - textW) / 2, by + 65, 0.8, true, COL_BLACK);

                

        
    }

    const char* brand = "GHOSTPAGE OS SECURITY";
    int bw = get_text_width_scaled(brand, 0.35);
    writeln_scaled(brand, cardX + (cardW - bw) / 2, cardY + cardH - 30, 0.35, false, COL_GRAY);

    epd_draw_grayscale_image(epd_full_screen(), framebuffer);
    epd_poweroff();
}

void handlePinTouch(int x, int y) {
    int cardW = 460;
    int cardX = (P_WIDTH - cardW) / 2;
    int cardY = 70;
    int kw = 110, kh = 100;
    int kGapX = 25, kGapY = 20;
    
    int gridStartX = cardX + (cardW - (3 * kw + 2 * kGapX)) / 2;
    int gridStartY = cardY + 260;

    if (DEBUG_ON) Serial.printf("PinTouch: (%d,%d) | GridStart: (%d,%d)\n", x, y, gridStartX, gridStartY);

    if (x < gridStartX || y < gridStartY) return;

    int relX = x - gridStartX;
    int relY = y - gridStartY;
    int col = relX / (kw + kGapX);
    int row = relY / (kh + kGapY);

    if (col >= 3 || row >= 4) return;

    int offsetInCol = relX % (kw + kGapX);
    int offsetInRow = relY % (kh + kGapY);
    if (offsetInCol > kw || offsetInRow > kh) return; 

    int i = row * 3 + col;
    if (i >= 12) return;

    static const char* keys[] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "CLR", "0", "OK"};
    String val = keys[i];
    
    if (DEBUG_ON) Serial.printf("Pin Key: %s (Row:%d Col:%d)\n", val.c_str(), row, col);

    // Flash Feedback for specific button
    Rect_t btnArea = {
        .x = (int32_t)(960 - 1 - (gridStartY + row * (kh + kGapY) + kh)),
        .y = (int32_t)(gridStartX + col * (kw + kGapX)),
        .width = (uint32_t)kh,
        .height = (uint32_t)kw
    };
    epd_poweron();
    epd_push_pixels(btnArea, 30, 0); // Quick black flash
    
    // Targeted Physical Wash for Card
    Rect_t masterArea = {
        .x = (int32_t)(960 - 1 - (cardY + 820)), 
        .y = (int32_t)cardX,
        .width = (uint32_t)820,
        .height = (uint32_t)cardW
    };
    for (int w = 0; w < 2; w++) epd_push_pixels(masterArea, 50, 1);
    epd_poweroff();

    if (val == "CLR") enteredPin = "";
    else if (val == "OK") {
        if (appState == STATE_SET_PIN) {
            if (enteredPin.length() == 6) {
                prefs.putString("saved_pin", enteredPin);
                activePin = enteredPin;
                enteredPin = "";
                showTransitionEffect();
                appState = STATE_LIBRARY;
                updateLibrary();
                return;
            }
        } else {
            if (enteredPin == activePin) {
                showTransitionEffect();
                appState = STATE_LIBRARY;
                updateLibrary();
                enteredPin = ""; 
                return;
            } else {
                enteredPin = "";
            }
        }
    } else {
        if (enteredPin.length() < 6) enteredPin += val;
    }

    drawPinPad(appState == STATE_SET_PIN);
}

void goToDeepSleep() {
    Serial.println(F("SYSTEM: Entering Deep Sleep Mode..."));
    epd_poweroff_all(); // Ensure display is fully off
    
    // Enable wake up on BUTTON_1 (GPIO 21) being pressed (Low)
    esp_sleep_enable_ext0_wakeup((gpio_num_t)BUTTON_1, 0); 
    
    esp_deep_sleep_start();
}

// --- GhostDrop (WiFi Transfer) Logic ---

void drawGhostDropUI(String statusMsg) {
    epd_poweron();
    memset(framebuffer, COL_WHITE, L_WIDTH * L_HEIGHT / 2);

    int cardX = 30, cardY = 50, cardW = P_WIDTH - 60, cardH = 860;
    draw_rounded_rect(cardX, cardY, cardW, cardH, 12, COL_BLACK);
    draw_rounded_rect(cardX+2, cardY+2, cardW-4, cardH-4, 11, COL_BLACK);

    // Header
    fill_rect_rotated(cardX + 15, cardY + 15, cardW - 30, 80, COL_BLACK);
    const char* title = "GHOSTDROP SYNC";
    int tw = get_text_width_scaled(title, 0.8);
    writeln_scaled(title, cardX + (cardW - tw) / 2, cardY + 70, 0.8, true, COL_WHITE);

    // Status Message
    int sw = get_text_width_scaled(statusMsg.c_str(), 0.55);
    writeln_scaled(statusMsg.c_str(), cardX + (cardW - sw) / 2, cardY + 160, 0.55, true, COL_BLACK);

    if (WiFi.status() == WL_CONNECTED) {
        String url = "http://" + WiFi.localIP().toString();
        int uw = get_text_width_scaled(url.c_str(), 0.6);
        writeln_scaled(url.c_str(), cardX + (cardW - uw) / 2, cardY + 210, 0.6, true, COL_BLACK);

        // QR Code
        QRCode qrcode;
        uint8_t qrbits[qrcode_getBufferSize(3)];
        qrcode_initText(&qrcode, qrbits, 3, ECC_LOW, url.c_str());
        
        int qrSize = 300;
        int qrX = cardX + (cardW - qrSize) / 2;
        int qrY = cardY + 280;
        int scale = qrSize / qrcode.size;

        for (uint8_t y = 0; y < qrcode.size; y++) {
            for (uint8_t x = 0; x < qrcode.size; x++) {
                if (qrcode_getModule(&qrcode, x, y)) {
                    fill_rect_rotated(qrX + x * scale, qrY + y * scale, scale, scale, COL_BLACK);
                }
            }
        }
        
        const char* inst = "Scan to upload .txt files";
        int iw = get_text_width_scaled(inst, 0.45);
        writeln_scaled(inst, cardX + (cardW - iw) / 2, qrY + qrSize + 40, 0.45, false, COL_GRAY);
    }

    // Stop Button
    int btnY = cardY + cardH - 100;
    int btnW = 220;
    int bx = cardX + (cardW - btnW) / 2;
    draw_rounded_rect(bx, btnY, btnW, 60, 30, COL_BLACK);
    const char* stp = "STOP SYNC";
    int stw = get_text_width_scaled(stp, 0.6);
    writeln_scaled(stp, bx + (btnW - stw) / 2, btnY + 40, 0.6, true, COL_BLACK);

    epd_draw_grayscale_image(epd_full_screen(), framebuffer);
    epd_poweroff();
}

void startGhostDrop() {
    appState = STATE_GHOSTDROP;
    
    // --- PHYSICAL WASH (Consistent OS Style) ---
    Rect_t fullArea = epd_full_screen();
    epd_poweron();
    for (int i = 0; i < 3; i++) {
        epd_push_pixels(fullArea, 50, 1);
    }
    
    drawGhostDropUI("Connecting to WiFi...");
    
    WiFi.begin(WIFI_SSID_1, WIFI_PASS_1);
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
        delay(500);
    }

    if (WiFi.status() == WL_CONNECTED) {
        server.on("/", HTTP_GET, []() {
            String html = "<html><body style='font-family:sans-serif;padding:40px;'>";
            html += "<h1>GhostDrop</h1><p>Select a .txt file to send to LilyGo:</p>";
            html += "<form method='POST' action='/upload' enctype='multipart/form-data'>";
            html += "<input type='file' name='upload' accept='.txt'><br><br>";
            html += "<input type='submit' value='Upload' style='padding:10px 20px;'>";
            html += "</form></body></html>";
            server.send(200, "text/html", html);
        });

        server.on("/upload", HTTP_POST, []() {
            server.send(200, "text/plain", "Upload Successful! You can send another or stop sync on the device.");
        }, []() {
            HTTPUpload& upload = server.upload();
            if (upload.status == UPLOAD_FILE_START) {
                String filename = "/" + upload.filename;
                uploadFile = SD.open(filename, FILE_WRITE);
            } else if (upload.status == UPLOAD_FILE_WRITE) {
                if (uploadFile) uploadFile.write(upload.buf, upload.currentSize);
            } else if (upload.status == UPLOAD_FILE_END) {
                if (uploadFile) uploadFile.close();
            }
        });

        server.begin();
        drawGhostDropUI("ONLINE - READY");
    } else {
        drawGhostDropUI("Connection Failed!");
        delay(2000);
        stopGhostDrop();
    }
}

void stopGhostDrop() {
    server.stop();
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    
    // --- PHYSICAL WASH ---
    Rect_t fullArea = epd_full_screen();
    epd_poweron();
    for (int i = 0; i < 3; i++) {
        epd_push_pixels(fullArea, 50, 1);
    }

    // Refresh the books list to include new uploads
    books.clear();
    books.push_back("internal:Echoes_of_the_Code.txt");
    scanFiles("/");

    showTransitionEffect();
    appState = STATE_LIBRARY;
    updateLibrary();
}

void handleGhostDropTouch(int x, int y) {
    int cardX = 30, cardY = 50, cardW = P_WIDTH - 60, cardH = 860;
    int btnY = cardY + cardH - 100;
    int btnW = 220;
    int bx = cardX + (cardW - btnW) / 2;

    if (y > btnY && y < btnY + 60 && x > bx && x < bx + btnW) {
        stopGhostDrop();
    }
}

// --- Direct Touch Action Logic ---

void handleTouchAction(int x, int y) {
    if (appState == STATE_GHOSTDROP) {
        handleGhostDropTouch(x, y);
        return;
    }
    if (appState == STATE_LOCK || appState == STATE_SET_PIN) {
        handlePinTouch(x, y);
        return;
    }
    if (appState == STATE_READING) {
        // Bottom Menu Area (Buttons)
        if (y >= BTN_Y_POS - 10 && y <= BTN_Y_POS + BUTTON_H + 10) {
            // ... (keep button logic)
            int mbx = (P_WIDTH - (3 * BUTTON_W + 2 * BUTTON_GAP)) / 2;
            
            // Back Button
            if (x >= mbx && x <= mbx + BUTTON_W) {
                showTransitionEffect();
                appState = STATE_LIBRARY; 
                updateLibrary();
                return;
            }
            // Prev Button
            else if (x >= mbx + BUTTON_W + BUTTON_GAP && x <= mbx + 2*BUTTON_W + BUTTON_GAP) {
                if (!pageHistory.empty()) { 
                    textPos = pageHistory.back(); 
                    pageHistory.pop_back(); 
                    updateReader(true); 
                } else {
                    updateReader(true);
                }
                return;
            }
            // Next Button
            else if (x >= mbx + 2*(BUTTON_W + BUTTON_GAP) && x <= mbx + 3*BUTTON_W + 2*BUTTON_GAP) {
                handleNext();
                return;
            }
        }
        // Main Body Area - Trigger Menu
        else if (y > 80 && y < 820) {
            appState = STATE_READER_OPTIONS;
            updateReaderMenu(true);
            return;
        }
    } 
    else if (appState == STATE_READER_OPTIONS) {
        int boxTop = 80;
        int boxBottom = 820;
        int boxMargin = 15;
        int menuX = boxMargin;
        int menuY = boxTop;
        int menuW = P_WIDTH - 2 * boxMargin;
        int menuH = boxBottom - boxTop;

        if (x >= menuX && x <= menuX + menuW && y >= menuY && y <= menuY + menuH) {
            int bh = menuH / 5;
            int localY = y - menuY;
            int slot = localY / bh;

            if (slot == 0) { // FONT +
                fontScale += 0.1;
                if (fontScale > 2.5) fontScale = 2.5;
                prefs.putFloat("fscale", fontScale);
                appState = STATE_READING;
                updateReader(false);
                return;
            }
            else if (slot == 1) { // FONT -
                fontScale -= 0.1;
                if (fontScale < 0.4) fontScale = 0.4;
                prefs.putFloat("fscale", fontScale);
                appState = STATE_READING;
                updateReader(false);
                return;
            }
            else if (slot == 2) { // REFRESH
                appState = STATE_READING;
                forceFullRefresh();
                return;
            }
            else if (slot == 3) { // SYNC
                startGhostDrop();
                return;
            }
            else { // BACK
                appState = STATE_READING;
                updateReaderMenu(false);
                return;
            }
        } else {
            // Tap outside -> Close menu
            appState = STATE_READING;
            updateReaderMenu(false);
            return;
        }
    }
    else if (appState == STATE_LIBRARY) {
        // --- Header Tap -> Library Options Menu ---
        if (y < 90) {
            appState = STATE_LIBRARY_OPTIONS;
            updateLibraryMenu(true);
            return;
        }

        // --- 1. Tab Bar Touch Detection ---
        
            if (y > 90 && y < 145) {
                int tabW = P_WIDTH / 4;
                int newFilter = x / tabW;
                if (newFilter >= 0 && newFilter < 4) {
                    libraryFilter = (LibFilter)newFilter;
                    librarySelection = 0; // Reset scroll/selection
                    updateLibrary();
                    return;
                }
            }
    
            if (!filteredBooks.empty()) {
                int page = librarySelection / SHELF_BOOKS_PER_PAGE;
                int startIdx = page * SHELF_BOOKS_PER_PAGE;
    
                // Check if touching a book cover
                for (int i = 0; i < SHELF_BOOKS_PER_PAGE; i++) {
                    int col = i % SHELF_COLS;
                    int row = i / SHELF_COLS;
    
                    int xStart = GAP_X + col * (BOOK_W + GAP_X);
                    int yStart = (SHELF_START_Y + 40) + row * (BOOK_H + GAP_Y);
    
                    if (x >= xStart - 10 && x <= xStart + BOOK_W + 10 && 
                        y >= yStart - 10 && y <= yStart + BOOK_H + 10) {
                        
                        int targetIdx = startIdx + i;
                        if (targetIdx < (int)filteredBooks.size()) {
                            focusedBookIndex = targetIdx;
                            appState = STATE_BOOK_OPTIONS;
                            updateBookCardMenu(focusedBookIndex, true);
                        }
                        return;
                    }
                }
                
                // Footer Tap for Page Navigation (Generous touch areas)
                if (y > P_HEIGHT - 100) {
                    int totalPages = (filteredBooks.size() + SHELF_BOOKS_PER_PAGE - 1) / SHELF_BOOKS_PER_PAGE;
                    
                    // Left 40% - Previous page
                    if (x < P_WIDTH * 0.4) {
                        if (page > 0) {
                            librarySelection = max(0, librarySelection - SHELF_BOOKS_PER_PAGE);
                            updateLibrary();
                        }
                        return;
                    }
                    // Right 40% - Next page
                    else if (x > P_WIDTH * 0.6) {
                        if (page < totalPages - 1) {
                            librarySelection += SHELF_BOOKS_PER_PAGE;
                            if (librarySelection >= (int)filteredBooks.size()) {
                                librarySelection = ((int)filteredBooks.size() - 1);
                            }
                            updateLibrary();
                        }
                        return;
                    }
                }
            }
        }
        else if (appState == STATE_BOOK_OPTIONS) {
    
            int localIdx = focusedBookIndex % SHELF_BOOKS_PER_PAGE;
            int col = localIdx % SHELF_COLS;
            int row = localIdx / SHELF_COLS;
    
            int cardX = GAP_X + col * (BOOK_W + GAP_X);
            int cardY = (SHELF_START_Y + 40) + row * (BOOK_H + GAP_Y);
    
            if (x >= cardX && x <= cardX + BOOK_W && y >= cardY && y <= cardY + BOOK_H) {
                int bh = BOOK_H / 4;
                int localY = y - cardY;
                int slot = localY / bh;
    
                if (slot == 0) { // READ
                    librarySelection = focusedBookIndex;
                    focusedBookIndex = -1;
                    openBook();
                    return;
                }
                else if (slot == 1) { // RESET
                    String path = filteredBooks[focusedBookIndex];
                    String key = getPrefKey(path);
                    prefs.remove(key.c_str());
                    
                    // Find correct index in master list for openBook
                    for(int i=0; i < (int)books.size(); i++) {
                        if (books[i] == path) {
                            librarySelection = focusedBookIndex;
                            focusedBookIndex = -1;
                            openBook();
                            return;
                        }
                    }
                }
                else if (slot == 2) { // DELETE
                    String path = filteredBooks[focusedBookIndex];
                    if (path.startsWith("internal:")) {
                        // Cannot delete internal books
                        appState = STATE_LIBRARY;
                        redrawBookCover(focusedBookIndex);
                        updateBookCardMenu(focusedBookIndex, false);
                        focusedBookIndex = -1;
                        return;
                    }
                    if (SD.exists(path)) {
                        SD.remove(path);
                        String key = getPrefKey(path);
                        prefs.remove(key.c_str());
                        
                        // Remove from master list
                        for(auto it = books.begin(); it != books.end(); ++it) {
                            if (*it == path) {
                                books.erase(it);
                                break;
                            }
                        }
                        
                        librarySelection = 0;
                        focusedBookIndex = -1;
                        
                        showTransitionEffect();
                        appState = STATE_LIBRARY;
                        updateLibrary();
                        return;
                    }
                }
                else { // BACK
                    appState = STATE_LIBRARY;
                    redrawBookCover(focusedBookIndex);
                    updateBookCardMenu(focusedBookIndex, false);
                    focusedBookIndex = -1;
                    return;
                }
            }
     else {
            // Tap outside -> Close menu
            appState = STATE_LIBRARY;
            redrawBookCover(focusedBookIndex);
            updateBookCardMenu(focusedBookIndex, false);
            focusedBookIndex = -1;
            return;
        }
    }
    else if (appState == STATE_LIBRARY_OPTIONS) {
        int boxTop = 150;
        int boxBottom = 750;
        int boxMargin = 60;
        int menuX = boxMargin;
        int menuY = boxTop;
        int menuW = P_WIDTH - 2 * boxMargin;
        int menuH = boxBottom - boxTop;

        if (x >= menuX && x <= menuX + menuW && y >= menuY && y <= menuY + menuH) {
            int bh = menuH / 5;
            int localY = y - menuY;
            int slot = localY / bh;
            int totalPages = (filteredBooks.size() + SHELF_BOOKS_PER_PAGE - 1) / SHELF_BOOKS_PER_PAGE;
            int currentPage = librarySelection / SHELF_BOOKS_PER_PAGE;

            if (slot == 0) { // NEXT PAGE
                if (currentPage < totalPages - 1) {
                    librarySelection += SHELF_BOOKS_PER_PAGE;
                    if (librarySelection >= (int)filteredBooks.size()) librarySelection = filteredBooks.size() - 1;
                }
                appState = STATE_LIBRARY;
                updateLibrary();
                return;
            }
            else if (slot == 1) { // PREV PAGE
                if (currentPage > 0) {
                    librarySelection = max(0, librarySelection - SHELF_BOOKS_PER_PAGE);
                }
                appState = STATE_LIBRARY;
                updateLibrary();
                return;
            }
            else if (slot == 2) { // REFRESH
                appState = STATE_LIBRARY;
                epd_poweron(); epd_clear(); epd_poweroff();
                updateLibrary();
                return;
            }
            else if (slot == 3) { // SYNC
                startGhostDrop();
                return;
            }
            else { // BACK
                appState = STATE_LIBRARY;
                updateLibraryMenu(false);
                return;
            }
        } else {
            appState = STATE_LIBRARY;
            updateLibraryMenu(false);
            return;
        }
    }
}

// --- System Helpers ---

float getBatteryVoltage() {
    return (analogRead(BATT_PIN) / 4095.0) * 2.0 * 3.3 * 1.1;
}

String getTimeString() {
    RTC_Date date = rtc.getDateTime();
    char buf[16];
    snprintf(buf, sizeof(buf), "%d:%02d", date.hour % 12 == 0 ? 12 : date.hour % 12, date.minute);
    return String(buf);
}

void scanFiles(String path) {
    File dir = SD.open(path);
    if (!dir || !dir.isDirectory()) return;
    File file = dir.openNextFile();
    while (file) {
        String fileName = String(file.name());
        if (file.isDirectory()) { 
            if (!fileName.startsWith(".") && fileName != "System Volume Information") {
                scanFiles(path + (path.endsWith("/") ? "" : "/") + fileName); 
            }
        } else { 
            String n = fileName; n.toLowerCase(); 
            if (n.endsWith(".txt")) books.push_back(String(file.path())); 
        }
        file = dir.openNextFile();
    }
}

String getPrefKey(String path) {
    String key = path.substring(path.lastIndexOf('/') + 1);
    if (key.length() > 15) key = key.substring(0, 15);
    return key;
}

void applyLibraryFilter() {
    filteredBooks.clear();
    for (const auto& path : books) {
        if (libraryFilter == FILTER_ALL) {
            filteredBooks.push_back(path);
            continue;
        }

        long savedPos = prefs.getLong(getPrefKey(path).c_str(), 0);
        long totalSize = 1;
        if (path.startsWith("internal:")) {
            totalSize = strlen(DEFAULT_BOOK_TEXT);
        } else {
            File f = SD.open(path);
            if (f) { totalSize = f.size(); f.close(); }
        }

        if (libraryFilter == FILTER_NEW) {
            if (savedPos == 0) filteredBooks.push_back(path);
        } else if (libraryFilter == FILTER_READING) {
            if (savedPos > 0 && savedPos < (totalSize * 0.95)) filteredBooks.push_back(path);
        } else if (libraryFilter == FILTER_FINISHED) {
            if (savedPos >= (totalSize * 0.95)) filteredBooks.push_back(path);
        }
    }
}

void showFinishedScreen() {
    epd_poweron();
    memset(framebuffer, COL_WHITE, L_WIDTH * L_HEIGHT / 2);
    
    int cardW = 500;
    int cardH = 340;
    int cardX = (P_WIDTH - cardW) / 2;
    int cardY = (P_HEIGHT - cardH) / 2;
    
    // Minimalist Card
    draw_rounded_rect(cardX, cardY, cardW, cardH, 20, COL_BLACK);
    fill_rect_rotated(cardX + 20, cardY + 20, cardW - 40, 80, COL_BLACK);
    
    const char* t = "BOOK FINISHED";
    int tw = get_text_width_scaled(t, 0.85);
    writeln_scaled(t, cardX + (cardW - tw)/2, cardY + 75, 0.85, true, COL_WHITE);
    
    const char* m1 = "You've reached the final page.";
    int mw1 = get_text_width_scaled(m1, 0.55);
    writeln_scaled(m1, cardX + (cardW - mw1)/2, cardY + 160, 0.55, true, COL_BLACK);

    const char* m2 = "Well read.";
    int mw2 = get_text_width_scaled(m2, 0.55);
    writeln_scaled(m2, cardX + (cardW - mw2)/2, cardY + 205, 0.55, false, COL_BLACK);
    
    const char* b = "Returning to Library...";
    int bw = get_text_width_scaled(b, 0.45);
    writeln_scaled(b, cardX + (cardW - bw)/2, cardY + 280, 0.45, false, COL_GRAY);
    
    epd_draw_grayscale_image(epd_full_screen(), framebuffer);
    epd_poweroff();
    
    delay(3000);
    
    showTransitionEffect();
    appState = STATE_LIBRARY;
    updateLibrary();
}

void handleNext() {
    if (appState == STATE_LIBRARY) { 
        if (!books.empty()) { 
            librarySelection++; 
            if (librarySelection >= (int)books.size()) librarySelection = 0; 
            updateLibrary(); 
        } 
    }
    else if (appState == STATE_READING) {
        if (currentFileIndex < 0 || currentFileIndex >= (int)books.size()) return;

        long totalSize = 0;
        String path = books[currentFileIndex];
        if (path.startsWith("internal:")) {
            totalSize = strlen(DEFAULT_BOOK_TEXT);
        } else {
            File f = SD.open(path);
            if (f) { totalSize = f.size(); f.close(); }
        }

        if (textPos + lastPageByteCount >= totalSize) {
            showFinishedScreen();
        } else {
            pageHistory.push_back(textPos);
            textPos += lastPageByteCount;
            updateReader(true);
        }
    }
}

void showEnhancedSplash() {
    epd_poweron(); 
    epd_clear(); 
    memset(framebuffer, COL_BLACK, L_WIDTH * L_HEIGHT / 2);
    
    int startY = 60;
    int lineH = 30;
    float fs = 0.45;

    // Penguin ASCII (Simple)
    writeln_scaled("    .--.", 40, startY, fs, true, COL_WHITE);
    writeln_scaled("   |o_o |", 40, startY + 25, fs, true, COL_WHITE);
    writeln_scaled("   |:_/ |", 40, startY + 50, fs, true, COL_WHITE);
    writeln_scaled("  //   \\ \\", 40, startY + 75, fs, true, COL_WHITE);
    writeln_scaled(" (|     | )", 40, startY + 100, fs, true, COL_WHITE);
    writeln_scaled("/'\\_   _/`\\", 40, startY + 125, fs, true, COL_WHITE);
    writeln_scaled("\\___)=(___/", 40, startY + 150, fs, true, COL_WHITE);

    const char* logs[] = {
        "[    0.000000] GhostPage Kernel 1.0.0-gp-esp32s3",
        "[    0.000000] CPU: ESP32-S3 (revision v0.2) 240MHz",
        "[    0.042183] mem: PSRAM detected, initializing allocator",
        "[    0.152910] vfs: Mounting SD card (FAT32) ... [ OK ]",
        "[    0.284102] input: GT911 Capacitive Touch Driver active",
        "[    0.410293] display: LilyGo EPD 4.7-Inch initialized",
        "[    0.592811] rtc: PCF8563 external clock synchronized",
        "[    0.712003] ghost: Loading user preferences (vellum)",
        "[    0.854192] ghost: Checking partitions ... [ OK ]",
        "[    1.102938] ghost: Starting bookshelf-daemon",
        "[    1.254102] ghost: Security subsystem active",
        "",
        "GhostPage Login: ghostpage (automatic login)",
        "Password: * * * *",
        "Last login: Sun Feb 08 2026 on tty1"
    };

    for (int i = 0; i < 15; i++) {
        writeln_scaled(logs[i], 180, startY + (i * lineH), fs, false, COL_WHITE);
        // Partial push for each log to simulate terminal scrolling
        epd_draw_grayscale_image(epd_full_screen(), framebuffer);
        delay(80);
    }

    epd_poweroff();
}

void setup() {
    // 1. Immediate EPD Initialization (Ensures screen shows content after upload)
    epd_init(); 
    epd_poweron();
    delay(200);
    epd_clear(); // Initial physical reset of the display

    Serial.begin(115200);
    delay(500);
    Serial.println(F("\n--- GhostPage DIAGNOSTIC BOOT ---"));
    
    framebuffer = (uint8_t *)heap_caps_malloc(L_WIDTH * L_HEIGHT / 2, MALLOC_CAP_SPIRAM);
    if (framebuffer == NULL) {
        Serial.println(F("CRITICAL ERROR: PSRAM allocation failed for framebuffer!"));
    } else {
        Serial.println(F("DEBUG: Framebuffer allocated in PSRAM."));
    }
    
    // 1. Hardware Reset (Crucial for S3)
#ifdef TOUCH_RST
    if (TOUCH_RST > -1) {
        pinMode(TOUCH_RST, OUTPUT);
        digitalWrite(TOUCH_RST, LOW);
        delay(50);
        digitalWrite(TOUCH_RST, HIGH);
        delay(100);
    }
#endif

    // 2. I2C Initialization with Pull-ups
    pinMode(TOUCH_SDA, INPUT_PULLUP);
    pinMode(TOUCH_SCL, INPUT_PULLUP);
    Wire.begin(TOUCH_SDA, TOUCH_SCL); 
    
    rtc.begin();
    
    prefs.begin("vellum", false); 
    fontScale = prefs.getFloat("fscale", 1.0);
    
    books.clear();
    books.push_back("internal:Echoes_of_the_Code.txt");

    SPI.begin(SD_SCLK, SD_MISO, SD_MOSI, SD_CS); 
    if (SD.begin(SD_CS, SPI)) {
        Serial.println(F("DEBUG: SD Card mounted successfully. Scanning files..."));
        scanFiles("/");
        Serial.printf("DEBUG: Found %d books\n", (int)books.size());
    } else {
        Serial.println(F("WARNING: SD Card failed to mount. App may hang in updateLibrary."));
    }
    
    // 3. Initialize GT911 Touch
    touch.setPins(TOUCH_RST, TOUCH_INT);
    if (touch.begin(Wire, 0x5D, TOUCH_SDA, TOUCH_SCL)) {
        touch.setMaxCoordinates(P_WIDTH, P_HEIGHT);
        touch.setSwapXY(false); // Explicitly disable swap
        touch.setMirrorXY(false, false); 
        Serial.println(F("GT911 Touch Initialized Successfully."));
    } else {
        Serial.println(F("GT911 Touch NOT found."));
    }

    // PIN and State initialization
    activePin = prefs.getString("saved_pin", "");
    
    // Check if we woke up from deep sleep or if it's a fresh boot
    if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_UNDEFINED) {
        if (activePin == "") {
            appState = STATE_SET_PIN;
            Serial.println(F("DEBUG: Fresh Boot - No PIN found. Starting SETUP."));
        } else {
            appState = STATE_LOCK;
            Serial.println(F("DEBUG: Fresh Boot - PIN found. Starting LOCK."));
        }
    } else {
        Serial.println(F("DEBUG: Woke up from Deep Sleep. Restoring state."));
    }

    if (appState == STATE_LOCK || appState == STATE_SET_PIN) {
        drawPinPad(appState == STATE_SET_PIN);
    } else if (appState == STATE_LIBRARY) {
        updateLibrary();
    } else if (appState == STATE_READING) {
        updateReader(false);
    }
    
    lastInteraction = millis();
}

void loop() { 
    static unsigned long lastBeat = 0;
    if (millis() - lastBeat > 1000) {
        lastBeat = millis();
    }

    // Deep Sleep Logic (10 Minutes total inactivity)
    if (millis() - lastInteraction > 600000) {
        goToDeepSleep();
    }

    // Adaptive Auto-Lock Logic (5 Minutes)
    if ((appState == STATE_LIBRARY || appState == STATE_READING) && (millis() - lastInteraction > 300000)) {
        appState = STATE_LOCK;
        drawPinPad(false);
    }

    if (appState == STATE_GHOSTDROP) {
        server.handleClient();
    }

    // Header sync (Reduced to 5 minutes to minimize flashing)
    static unsigned long lastHeaderUpdate = 0;
    if (appState == STATE_READING && (millis() - lastHeaderUpdate > 300000)) {
        partialUpdateHeader();
        lastHeaderUpdate = millis();
    }

    // Direct Touch using TouchDrvGT911
    if (millis() > lastTouchTime + TOUCH_COOLDOWN) {
        int16_t tx[5], ty[5];
        uint8_t n = touch.getPoint(tx, ty); // Returns number of points

        if (n > 0) {
            if (touchReleased) { // Only act if previously released
                // Manual Mapping: Inverted Portrait
                int mappedX = 539 - tx[0];
                int mappedY = 959 - ty[0];
                
                if (DEBUG_ON) Serial.printf("Touch: Raw(%d,%d) -> Mapped(%d,%d)\n", tx[0], ty[0], mappedX, mappedY);

                lastInteraction = millis(); // Update for ALL states

                handleTouchAction(mappedX, mappedY);
                lastTouchTime = millis();
                touchReleased = false; // Block subsequent reads until release
            }
        } else {
            touchReleased = true; // Reset when no touch detected
        }
    }
}