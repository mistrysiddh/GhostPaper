#include "common.h"
#include "default_book.h"
#include "store.h"
#include <WiFi.h>
#include <Button2.h>
#include <math.h> 
#include <WebServer.h>
#include <ESPmDNS.h>
#include <qrcode.h>
#include "driver/rtc_io.h"

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
RTC_DATA_ATTR bool useSerif = false;
RTC_DATA_ATTR bool isGuestMode = false;

uint8_t *framebuffer = NULL;
std::vector<String> books;
std::vector<String> filteredBooks;
Preferences prefs;
TouchDrvGT911 touch;
std::vector<long> pageHistory;
unsigned long lastInteraction = 0;
unsigned long lastTouchTime = 0;
PCF8563_Class rtc;
bool touchReleased = true; // Guard for double clicks
RTC_DATA_ATTR bool touchEnabled = true;
Button2 btn1(BUTTON_1);

// --- WiFi Setup State ---
std::vector<String> foundSSIDs;
String selectedSSID = "";
String typingPassword = "";
bool isScanning = false;
bool isTypingPass = false;
bool isShifted = false;
bool isSymbols = false; // New variable for symbol mode // New variable for keyboard state

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
        .width = (int32_t)size,
        .height = (int32_t)size
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
        .width = (int32_t)size,
        .height = (int32_t)size
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

        int bh = menuH / 6;
        int vCenterOffset = (bh / 2) + 15;
        const char* labels[] = {"NEXT PAGE", "PREV PAGE", "DASHBOARD", "SYNC", "WIFI", "BACK"};
        
        for (int i = 0; i < 6; i++) {
            float scale = 0.8;
            int tw = get_text_width_scaled(labels[i], scale);
            writeln_scaled(labels[i], menuX + (menuW - tw) / 2, menuY + (i * bh) + vCenterOffset, scale, true, COL_BLACK);
            if (i < 5) draw_line_rotated(menuX + 30, menuY + (i + 1) * bh, menuX + menuW - 30, menuY + (i + 1) * bh, COL_LIGHT);
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

        int bh = menuH / 6;
        int vCenterOffset = (bh / 2) + 15;

        const char* labels[] = {"FONT +", "FONT -", useSerif ? "SERIF ON" : "SANS SERIF", "REFRESH", "SYNC", "BACK"};
        for (int i = 0; i < 6; i++) {
            float scale = 1.0; // Larger text for larger menu
            int tw = get_text_width_scaled(labels[i], scale);
            writeln_scaled(labels[i], menuX + (menuW - tw) / 2, menuY + (i * bh) + vCenterOffset, scale, true, COL_BLACK);
            
            // Decorative separators
            if (i < 5) {
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
    const char* title = settingNew ? "SETUP SECURITY" : "PRIVACY SCREEN";
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
        .width = (int32_t)kh,
        .height = (int32_t)kw
    };
    epd_poweron();
    epd_push_pixels(btnArea, 30, 0); // Quick black flash
    
    // Targeted Physical Wash for Card
    Rect_t masterArea = {
        .x = (int32_t)(960 - 1 - (cardY + 820)), 
        .y = (int32_t)cardX,
        .width = (int32_t)820,
        .height = (int32_t)cardW
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
                appState = STATE_DASHBOARD;
                updateDashboard();
                return;
            }
        } else {
            if (enteredPin == activePin) {
                isGuestMode = false;
                showTransitionEffect();
                appState = STATE_DASHBOARD;
                updateDashboard();
                enteredPin = ""; 
                return;
            } else if (enteredPin == "999999") { // Duress PIN
                isGuestMode = true;
                showTransitionEffect();
                appState = STATE_DASHBOARD;
                updateDashboard();
                enteredPin = "";
                return;
            } else {
                enteredPin = "";
            }
        }
    } else {
        if (enteredPin.length() < 6) {
            enteredPin += val;
            
            // Auto-Unlock Check
            if (appState == STATE_PRIVACY && enteredPin.length() == 6) {
                if (enteredPin == activePin) {
                    isGuestMode = false;
                    showTransitionEffect();
                    appState = STATE_DASHBOARD;
                    updateDashboard();
                    enteredPin = ""; 
                    return;
                } else if (enteredPin == "999999") { // Duress PIN
                    isGuestMode = true;
                    showTransitionEffect();
                    appState = STATE_DASHBOARD;
                    updateDashboard();
                    enteredPin = "";
                    return;
                } else {
                    // Flash error or clear if wrong (Privacy Screen)
                    delay(200); 
                    enteredPin = "";
                }
            }
            // Auto-Save Check for Set PIN
            else if (appState == STATE_SET_PIN && enteredPin.length() == 6) {
                prefs.putString("saved_pin", enteredPin);
                activePin = enteredPin;
                enteredPin = "";
                showTransitionEffect();
                appState = STATE_DASHBOARD;
                updateDashboard();
                return;
            }
        }
    }

    drawPinPad(appState == STATE_SET_PIN);
}

void goToDeepSleep() {
    Serial.println(F("SYSTEM: Entering Deep Sleep Mode..."));
    
    // 1. Display Screensaver
    epd_poweron();
    memset(framebuffer, 0xFF, L_WIDTH * L_HEIGHT / 2); // Clear buffer to white
    // Center the 360x540 image on the 540x960 screen
    draw_bmp_rotated("/images/dashboard.bmp", 90, 210); 
    epd_draw_grayscale_image(epd_full_screen(), framebuffer);
    delay(500); // Give E-ink time to settle

    // 2. Fully shut down the EPD to save power and prevent noise
    epd_poweroff_all(); 
    
    // 2. Clear any existing wake-up sources to be safe
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
    
    // 3. Configure EXT1 Wakeup for BUTTON_1 (GPIO 21)
    // We use EXT1 because it's more stable on S3 and allows precise pin selection.
    // 1ULL << BUTTON_1 creates a bitmask for the pin.
    // ESP_EXT1_WAKEUP_NONZERO means wake when the pin is NOT zero (High)? 
    // Wait, BUTTON_1 is active LOW (pulled up). So we want to wake when it goes LOW (Zero).
    // On S3, esp_sleep_enable_ext1_wakeup takes a mask and a mode.
    // ESP_EXT1_WAKEUP_ANY_LOW wakes if any pin in the mask goes LOW.
    
    esp_sleep_enable_ext1_wakeup(1ULL << BUTTON_1, ESP_EXT1_WAKEUP_ALL_LOW);
    
    // 4. Force the RTC to maintain the pull-up during sleep to prevent floating pin noise
    rtc_gpio_pullup_en((gpio_num_t)BUTTON_1);
    rtc_gpio_pulldown_dis((gpio_num_t)BUTTON_1);
    
    // 5. Enter Deep Sleep
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
    if (appState == STATE_STORE) {
        handleStoreTouch(x, y);
        return;
    }
    if (appState == STATE_WIFI_SETUP) {
        handleWiFiTouch(x, y);
        return;
    }
    if (appState == STATE_DASHBOARD) {
        handleDashboardTouch(x, y);
        return;
    }
    if (appState == STATE_PRIVACY || appState == STATE_SET_PIN) {
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
            int bh = menuH / 6;
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
            else if (slot == 2) { // TYPOGRAPHY TOGGLE
                useSerif = !useSerif;
                prefs.putBool("serif", useSerif);
                appState = STATE_READING;
                updateReader(false); // Triggers redraw with new font
                return;
            }
            else if (slot == 3) { // REFRESH
                appState = STATE_READING;
                forceFullRefresh();
                return;
            }
            else if (slot == 4) { // SYNC
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
                int tabW = P_WIDTH / 5; // 5 tabs now
                int newFilter = x / tabW;
                if (newFilter == 4) { // STORE tab
                    appState = STATE_STORE;
                    showTransitionEffect();
                    updateStore();
                    if (storeCatalog.empty()) { // Trigger sync if store is empty
                         syncStore();
                         updateStore();
                    }
                    return;
                }
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
                    
                    appState = STATE_LIBRARY;
                    redrawBookCover(focusedBookIndex);
                    updateBookCardMenu(focusedBookIndex, false);
                    focusedBookIndex = -1;
                    return;
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
            int bh = menuH / 6;
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
            else if (slot == 2) { // DASHBOARD
                appState = STATE_DASHBOARD;
                updateDashboard();
                return;
            }
            else if (slot == 3) { // SYNC
                startGhostDrop();
                return;
            }
            else if (slot == 4) { // WIFI
                appState = STATE_WIFI_SETUP;
                startWiFiScan();
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
        String fName = path.substring(path.lastIndexOf('/') + 1);
        if (isGuestMode && fName.startsWith("private_")) continue;

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
    useSerif = prefs.getBool("serif", false);
    
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
    String savedSSID = prefs.getString("wifi_ssid", "");
    
    // Check if we woke up from deep sleep or if it's a fresh boot
    if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_UNDEFINED) {
        if (activePin == "") {
            appState = STATE_SET_PIN;
            Serial.println(F("DEBUG: Fresh Boot - No PIN found. Starting SETUP."));
        } else if (savedSSID == "" && String(WIFI_SSID_1) == "YOUR_SSID") {
            // No credentials in NVS and no valid credentials in config.h
            appState = STATE_WIFI_SETUP;
            Serial.println(F("DEBUG: Fresh Boot - No WiFi found. Starting SETUP."));
        } else {
            appState = STATE_PRIVACY;
            Serial.println(F("DEBUG: Fresh Boot - PIN found. Starting LOCK."));
        }
    } else {
        Serial.println(F("DEBUG: Woke up from Deep Sleep. Restoring state."));
    }

    if (appState == STATE_PRIVACY || appState == STATE_SET_PIN) {
        drawPinPad(appState == STATE_SET_PIN);
    } else if (appState == STATE_WIFI_SETUP) {
        startWiFiScan();
    } else if (appState == STATE_LIBRARY) {
        updateLibrary();
    } else if (appState == STATE_READING) {
        updateReader(false);
    } else if (appState == STATE_STORE) {
        updateStore();
    } else if (appState == STATE_WIFI_SETUP) {
        updateWiFiSetup(false);
    } else if (appState == STATE_DASHBOARD) {
        updateDashboard();
    }
    
    lastInteraction = millis();

    // 4. Configure Hardware Button
    pinMode(BUTTON_1, INPUT_PULLUP);
    btn1.setClickHandler([](Button2& b) {
        lastInteraction = millis();
        if (appState == STATE_SCREENSAVER) {
            showTransitionEffect();
            if (activePin != "") {
                appState = STATE_PRIVACY;
                drawPinPad(false);
            } else {
                appState = STATE_DASHBOARD;
                updateDashboard();
            }
            return;
        } else if (appState == STATE_PRIVACY) {
            // If already on privacy screen and button pressed, refresh it
            drawPinPad(false);
        }
    });
    btn1.setLongClickTime(2000);
    btn1.setLongClickDetectedHandler([](Button2& b) {
        touchEnabled = !touchEnabled;
        Serial.printf("SYSTEM: Touch %s\n", touchEnabled ? "ENABLED" : "DISABLED");
        
        // Refresh UI to show state (optional, but cleaner than a black bar)
        if (appState == STATE_READING) {
            partialUpdateHeader();
        } else if (appState == STATE_LIBRARY) {
            partialUpdateLibraryHeader();
        }
    });
}

void loop() { 
    btn1.loop();

    static unsigned long lastBeat = 0;
    if (millis() - lastBeat > 1000) {
        lastBeat = millis();
    }

    // Screensaver Logic (5 Minutes total inactivity)
    if (appState != STATE_SCREENSAVER && (millis() - lastInteraction > 300000)) {
        appState = STATE_SCREENSAVER;
        epd_poweron();
        epd_clear(); // Full hardware refresh (flashing) to clear all ghosting
        memset(framebuffer, 0xFF, L_WIDTH * L_HEIGHT / 2);
        // Stretch the 360x540 image to 540x960
        draw_bmp_stretched_rotated("/images/dashboard.bmp", 0, 0, P_WIDTH, P_HEIGHT); 
        epd_draw_grayscale_image(epd_full_screen(), framebuffer);
        epd_poweroff();
        Serial.println(F("SYSTEM: Entering Screensaver Mode..."));
    }

    if (appState == STATE_GHOSTDROP) {
        server.handleClient();
    }

    // Check for Background Dashboard Update
    if (appState == STATE_DASHBOARD && dashboardDataReady) {
        dashboardDataReady = false;
        Serial.println(F("SYSTEM: New Dashboard Data Available. Refreshing UI."));
        updateDashboard();
    }

    // Direct Touch using TouchDrvGT911
    if (appState != STATE_SCREENSAVER && touchEnabled && (millis() > lastTouchTime + TOUCH_COOLDOWN)) {
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

// --- WiFi Setup UI Logic ---

void partialUpdateRegion(int x, int y, int w, int h) {
    Rect_t area = {
        .x = (int32_t)(960 - (y + h)),
        .y = (int32_t)x,
        .width = (uint32_t)h,
        .height = (uint32_t)w
    };
    epd_poweron();
    // 3. THE "PHYSICAL WASH" (To clear ghosting before redraw)
    for (int i = 0; i < 2; i++) epd_push_pixels(area, 50, 1);
    epd_draw_grayscale_image_area(area, framebuffer);
    epd_poweroff();
}

void partialUpdateWiFiPassword() {
    int boxX = 20, boxY = 150, boxW = P_WIDTH - 40, boxH = 50;
    fill_rect_rotated(boxX, boxY, boxW, boxH, COL_WHITE);
    draw_rect_rotated(boxX, boxY, boxW, boxH, COL_BLACK);
    String masked = "";
    for(int i=0; i<(int)typingPassword.length(); i++) masked += "*";
    writeln_scaled(masked.c_str(), 35, boxY + 35, 0.6, true, COL_BLACK);
    
    // Using partialUpdateRegion which now handles the physical wash
    partialUpdateRegion(boxX, boxY, boxW, boxH);
}

void startWiFiScan() {
    isScanning = true;
    updateWiFiSetup(false);
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    int n = WiFi.scanNetworks();
    foundSSIDs.clear();
    for (int i = 0; i < n && i < 8; ++i) foundSSIDs.push_back(WiFi.SSID(i));
    isScanning = false;
    updateWiFiSetup(false);
}

void updateWiFiSetup(bool partial) {
    epd_poweron();
    if (!partial) {
        // 1. FULL SCREEN REFRESH PATTERN
        epd_clear();
        memset(framebuffer, COL_WHITE, L_WIDTH * L_HEIGHT / 2);
        fill_rect_rotated(0, 0, P_WIDTH, 80, COL_BLACK);
        writeln_scaled("WiFi Setup", 25, 50, 0.8, true, COL_WHITE);
        
        // Back Button
        int btnW = 80, btnH = 40, btnX = P_WIDTH - 100, btnY = 20;
        draw_rounded_rect(btnX, btnY, btnW, btnH, 5, COL_WHITE);
        int tw = get_text_width_scaled("BACK", 0.4);
        writeln_scaled("BACK", btnX + (btnW - tw) / 2, btnY + 28, 0.4, true, COL_WHITE);
    }

    if (isScanning) {
        writeln_scaled("Scanning for networks...", 100, 400, 0.6, true, COL_BLACK);
    } else if (!isTypingPass) {
        writeln_scaled("Select your network:", 25, 120, 0.5, true, COL_BLACK);
        for (int i = 0; i < (int)foundSSIDs.size(); i++) {
            int y = 160 + i * 70;
            draw_rect_rotated(20, y, P_WIDTH - 40, 60, COL_DARK);
            writeln_scaled(foundSSIDs[i].c_str(), 40, y + 40, 0.5, false, COL_BLACK);
        }
        int rbtnW = 130, rbtnH = 45, rbtnX = P_WIDTH - 150, rbtnY = 95;
        draw_rounded_rect(rbtnX, rbtnY, rbtnW, rbtnH, 5, COL_BLACK);
        int rtw = get_text_width_scaled("RESCAN", 0.4);
        writeln_scaled("RESCAN", rbtnX + (rbtnW - rtw) / 2, rbtnY + 31, 0.4, true, COL_BLACK);
    } else {
        if (!partial) {
            writeln_scaled("Enter Password:", 25, 110, 0.5, false, COL_BLACK);
            writeln_scaled(selectedSSID.c_str(), 25, 140, 0.5, true, COL_GRAY);
            draw_rect_rotated(20, 150, P_WIDTH - 40, 50, COL_BLACK);
            String masked = "";
            for(int i=0; i<(int)typingPassword.length(); i++) masked += "*";
            writeln_scaled(masked.c_str(), 35, 185, 0.6, true, COL_BLACK);
            
            // Back to List Button
            int bbtnW = 80, bbtnH = 40, bbtnX = P_WIDTH - 100, bbtnY = 95;
            draw_rounded_rect(bbtnX, bbtnY, bbtnW, bbtnH, 5, COL_BLACK);
            int btw = get_text_width_scaled("BACK", 0.4);
            writeln_scaled("BACK", bbtnX + (bbtnW - btw) / 2, bbtnY + 28, 0.4, true, COL_BLACK);
        }

        const char* r1[] = {"1","2","3","4","5","6","7","8","9","0"};
        const char* r1S[] = {"!","@","#","$","%","^","&","*","(",")"};
        const char* r2[] = {"q","w","e","r","t","y","u","i","o","p"};
        const char* r2S[] = {"-","=","[","]","\\",";","'",",",".","/"};
        const char* r3[] = {"a","s","d","f","g","h","j","k","l"};
        const char* r3S[] = {"{","}","|",":","\"","<",">","?","_"};
        const char* r4[] = {"z","x","c","v","b","n","m"};
        const char* r4S[] = {"+","~","`"," "," "," "," "};

        int startY = 220;
        int keyH = 55; int gap = 8;
        int keyW = (P_WIDTH - 40 - (9 * gap)) / 10;
        
        if (partial) fill_rect_rotated(0, 220, P_WIDTH, 350, COL_WHITE);

        for (int i=0; i<10; i++) {
            int x = 20 + i * (keyW + gap);
            draw_rounded_rect(x, startY, keyW, keyH, 5, COL_BLACK);
            const char* label = isSymbols ? r1S[i] : r1[i];
            int tw = get_text_width_scaled(label, 0.5);
            writeln_scaled(label, x + (keyW - tw) / 2, startY + 38, 0.5, true, COL_BLACK);
        }
        startY += keyH + gap;
        for (int i=0; i<10; i++) {
            int x = 20 + i * (keyW + gap);
            draw_rounded_rect(x, startY, keyW, keyH, 5, COL_BLACK);
            String label = isSymbols ? r2S[i] : (isShifted ? String((char)toupper(r2[i][0])) : r2[i]);
            int tw = get_text_width_scaled(label.c_str(), 0.5);
            writeln_scaled(label.c_str(), x + (keyW - tw) / 2, startY + 38, 0.5, true, COL_BLACK);
        }
        startY += keyH + gap;
        int indent = keyW / 2;
        for (int i=0; i<9; i++) {
            int x = 20 + indent + i * (keyW + gap);
            draw_rounded_rect(x, startY, keyW, keyH, 5, COL_BLACK);
            String label = isSymbols ? r3S[i] : (isShifted ? String((char)toupper(r3[i][0])) : r3[i]);
            int tw = get_text_width_scaled(label.c_str(), 0.5);
            writeln_scaled(label.c_str(), x + (keyW - tw) / 2, startY + 38, 0.5, true, COL_BLACK);
        }
        startY += keyH + gap;
        int shW = keyW * 1.5;
        draw_rounded_rect(20, startY, shW, keyH, 5, isShifted ? COL_BLACK : COL_GRAY);
        if (isShifted && !isSymbols) fill_rect_rotated(20, startY, shW, keyH, COL_BLACK);
        const char* shLabel = isSymbols ? "ABC" : "SH";
        int shtw = get_text_width_scaled(shLabel, 0.4);
        writeln_scaled(shLabel, 20 + (shW - shtw) / 2, startY + 38, 0.4, true, (isShifted && !isSymbols) ? COL_WHITE : COL_BLACK);
        
        int xStart = 20 + shW + gap;
        for (int i=0; i<7; i++) { 
            int x = xStart + i * (keyW + gap);
            draw_rounded_rect(x, startY, keyW, keyH, 5, COL_BLACK);
            String label = isSymbols ? r4S[i] : (isShifted ? String((char)toupper(r4[i][0])) : r4[i]);
            if (label != " ") {
                int tw = get_text_width_scaled(label.c_str(), 0.5);
                writeln_scaled(label.c_str(), x + (keyW - tw) / 2, startY + 38, 0.5, true, COL_BLACK);
            }
        }
        int delX = xStart + 7 * (keyW + gap);
        int delW = P_WIDTH - 20 - delX;
        draw_rounded_rect(delX, startY, delW, keyH, 5, COL_BLACK);
        int dtw = get_text_width_scaled("DEL", 0.4);
        writeln_scaled("DEL", delX + (delW - dtw) / 2, startY + 38, 0.4, true, COL_BLACK);

        startY += keyH + gap;
        int symW = 100;
        draw_rounded_rect(20, startY, symW, keyH, 5, isSymbols ? COL_BLACK : COL_GRAY);
        if (isSymbols) fill_rect_rotated(20, startY, symW, keyH, COL_BLACK);
        const char* symLabel = isSymbols ? "123" : "SYM";
        int symtw = get_text_width_scaled(symLabel, 0.4);
        writeln_scaled(symLabel, 20 + (symW - symtw) / 2, startY + 38, 0.4, true, isSymbols ? COL_WHITE : COL_BLACK);
        
        int spcW = 260;
        draw_rounded_rect(130, startY, spcW, keyH, 5, COL_BLACK);
        int spctw = get_text_width_scaled("SPACE", 0.4);
        writeln_scaled("SPACE", 130 + (spcW - spctw) / 2, startY + 38, 0.4, true, COL_BLACK);
        
        int okW = 120;
        draw_rounded_rect(400, startY, okW, keyH, 5, COL_BLACK);
        int oktw = get_text_width_scaled("OK", 0.4);
        writeln_scaled("OK", 400 + (okW - oktw) / 2, startY + 38, 0.4, true, COL_BLACK);
    }

    if (partial) {
        // partialUpdateRegion now handles poweron/off and wash
        partialUpdateRegion(0, 220, P_WIDTH, 350);
    } else {
        epd_draw_grayscale_image(epd_full_screen(), framebuffer);
    }
    epd_poweroff();
}

void handleWiFiTouch(int x, int y) {
    if (isScanning) return;

    // Helper for instant feedback
    auto feedback = [&](int fx, int fy, int fw, int fh) {
        Rect_t area = { .x = (int32_t)(960 - (fy + fh)), .y = (int32_t)fx, .width = (uint32_t)fh, .height = (uint32_t)fw };
        epd_poweron(); epd_push_pixels(area, 30, 0); epd_poweroff();
    };

    // Header Back Button
    if (y > 10 && y < 70 && x > P_WIDTH - 110) {
        feedback(P_WIDTH - 100, 20, 80, 40);
        appState = STATE_LIBRARY; showTransitionEffect(); updateLibrary(); return;
    }

    if (!isTypingPass) {
        if (y > 95 && y < 140 && x > P_WIDTH - 150) { 
            feedback(P_WIDTH - 150, 95, 130, 45); // RESCAN button
            startWiFiScan(); return; 
        }
        for (int i = 0; i < (int)foundSSIDs.size(); i++) {
            int iy = 160 + i * 70;
            if (y > iy && y < iy + 60) {
                feedback(20, iy, P_WIDTH - 40, 60); // SSID row
                selectedSSID = foundSSIDs[i]; typingPassword = "";
                isTypingPass = true; isShifted = false; isSymbols = false;
                updateWiFiSetup(false); return;
            }
        }
    } else {
        // Back to list button when typing password
        if (y > 90 && y < 140 && x > P_WIDTH - 110) {
            feedback(P_WIDTH - 100, 95, 80, 40);
            isTypingPass = false; updateWiFiSetup(false); return;
        }

        int startY = 220; int keyH = 55; int gap = 8;
        int keyW = (P_WIDTH - 40 - (9 * gap)) / 10;

        auto keyFeedback = [&](int kx, int ky, int kw_val) {
            Rect_t btn = { .x = (int32_t)(960 - (ky + keyH)), .y = (int32_t)kx, .width = (uint32_t)keyH, .height = (uint32_t)kw_val };
            epd_poweron(); epd_push_pixels(btn, 30, 0); epd_poweroff();
        };

        if (y >= startY && y < startY + keyH) {
            int col = (x - 20) / (keyW + gap);
            if (col >= 0 && col < 10) {
                int kx = 20 + col * (keyW + gap);
                keyFeedback(kx, startY, keyW);
                const char* r1[] = {"1","2","3","4","5","6","7","8","9","0"};
                const char* r1S[] = {"!","@","#","$","%","^","&","*","(",")"};
                typingPassword += isSymbols ? r1S[col] : r1[col];
                partialUpdateWiFiPassword();
            }
            return;
        }
        startY += keyH + gap;
        if (y >= startY && y < startY + keyH) {
            int col = (x - 20) / (keyW + gap);
            if (col >= 0 && col < 10) {
                int kx = 20 + col * (keyW + gap);
                keyFeedback(kx, startY, keyW);
                const char* r2[] = {"q","w","e","r","t","y","u","i","o","p"};
                const char* r2S[] = {"-","=","[","]","\\",";","'",",",".","/"};
                if (isSymbols) typingPassword += r2S[col];
                else typingPassword += (isShifted ? (char)toupper(r2[col][0]) : r2[col][0]);
                partialUpdateWiFiPassword();
            }
            return;
        }
        startY += keyH + gap;
        int indent = keyW / 2;
        if (y >= startY && y < startY + keyH) {
            int col = (x - (20 + indent)) / (keyW + gap);
            if (col >= 0 && col < 9) {
                int kx = 20 + indent + col * (keyW + gap);
                keyFeedback(kx, startY, keyW);
                const char* r3[] = {"a","s","d","f","g","h","j","k","l"};
                const char* r3S[] = {"{","}","|",":","\"","<",">","?","_"};
                if (isSymbols) typingPassword += r3S[col];
                else typingPassword += (isShifted ? (char)toupper(r3[col][0]) : r3[col][0]);
                partialUpdateWiFiPassword();
            }
            return;
        }
        startY += keyH + gap;
        if (y >= startY && y < startY + keyH) {
            if (x >= 20 && x < 20 + keyW * 1.5) {
                keyFeedback(20, startY, keyW * 1.5);
                if (isSymbols) isSymbols = false; else isShifted = !isShifted;
                updateWiFiSetup(true); return;
            }
            int xStart = 20 + (keyW * 1.5) + gap;
            int delX = xStart + 7 * (keyW + gap);
            if (x >= delX) {
                keyFeedback(delX, startY, P_WIDTH - 20 - delX);
                if (typingPassword.length() > 0) typingPassword.remove(typingPassword.length() - 1);
                partialUpdateWiFiPassword(); return;
            }
            int col = (x - xStart) / (keyW + gap);
            if (col >= 0 && col < 7) {
                int kx = xStart + col * (keyW + gap);
                keyFeedback(kx, startY, keyW);
                const char* r4[] = {"z","x","c","v","b","n","m"};
                const char* r4S[] = {"+","~","`"," "," "," "," "};
                if (isSymbols) { if (String(r4S[col]) != " ") typingPassword += r4S[col]; }
                else typingPassword += (isShifted ? (char)toupper(r4[col][0]) : r4[col][0]);
                partialUpdateWiFiPassword();
            }
            return;
        }
        startY += keyH + gap;
        if (y >= startY && y < startY + keyH) {
            if (x < 120) { keyFeedback(20, startY, 100); isSymbols = !isSymbols; updateWiFiSetup(true); }
            else if (x > 400) {
                keyFeedback(400, startY, 120);
                prefs.putString("wifi_ssid", selectedSSID);
                prefs.putString("wifi_pass", typingPassword);
                appState = STATE_LIBRARY; showTransitionEffect(); updateLibrary();
            } else { keyFeedback(130, startY, 260); typingPassword += " "; partialUpdateWiFiPassword(); }
            return;
        }
    }
}