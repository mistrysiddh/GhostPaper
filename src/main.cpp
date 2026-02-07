#include "common.h"
#include <WiFi.h>
#include <Button2.h>
#include <math.h> 

// --- Configuration ---
const unsigned long TOUCH_COOLDOWN = 300; 

// --- Global Variable Definitions ---
RTC_DATA_ATTR AppState appState = STATE_SPLASH;
RTC_DATA_ATTR int librarySelection = 0; 
RTC_DATA_ATTR int currentFileIndex = -1;
RTC_DATA_ATTR long textPos = 0;
RTC_DATA_ATTR float fontScale = 1.15;
RTC_DATA_ATTR long lastPageByteCount = 0;
RTC_DATA_ATTR int focusedBookIndex = -1;

uint8_t *framebuffer = NULL;
std::vector<String> books;
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
    int y = SHELF_START_Y + row * (BOOK_H + GAP_Y);

    long savedPos = prefs.getLong(getPrefKey(books[index]).c_str(), 0);
    long totalSize = 1;
    File f = SD.open(books[index]);
    if (f) { totalSize = f.size(); f.close(); }
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
    int y = SHELF_START_Y + row * (BOOK_H + GAP_Y);

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
        // --- CLEAN WHITE MINIMALIST MENU (NO BORDERS) ---
        fill_rect_rotated(x, y, BOOK_W, BOOK_H, COL_WHITE);

        int bh = BOOK_H / 3;
        int vCenterOffset = (bh / 2) + 15; // Vertical center approximation for baseline

        // Slot 1: READ
        const char* tRead = "READ";
        float sRead = 1.0;
        int wRead = get_text_width_scaled(tRead, sRead);
        writeln_scaled(tRead, x + (BOOK_W - wRead) / 2, y + vCenterOffset, sRead, true, COL_BLACK);
        
        // Slot 2: RESET
        const char* tReset = "RESET";
        float sReset = 0.8;
        int wReset = get_text_width_scaled(tReset, sReset);
        writeln_scaled(tReset, x + (BOOK_W - wReset) / 2, y + bh + vCenterOffset, sReset, true, COL_BLACK);
        
        // Slot 3: BACK
        const char* tBack = "BACK";
        float sBack = 0.8;
        int wBack = get_text_width_scaled(tBack, sBack);
        writeln_scaled(tBack, x + (BOOK_W - wBack) / 2, y + 2 * bh + vCenterOffset, sBack, true, COL_BLACK);
    }

    // Refresh the entire Stripe (works for both menu and cover restoration)
    uint8_t *stripePtr = &framebuffer[physY * 960 / 2];
    epd_draw_grayscale_image(stripeArea, stripePtr);
    
    epd_poweroff();
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

        const char* labels[] = {"FONT +", "FONT -", "REFRESH", "RESET", "BACK"};
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

    // --- Master Security Card ---
    int cardW = 460;
    int cardH = 820;
    int cardX = (P_WIDTH - cardW) / 2;
    int cardY = 70;

    // 1. Shadow & Background
    fill_rect_rotated(cardX + 6, cardY + 6, cardW, cardH, COL_GRAY);
    fill_rect_rotated(cardX, cardY, cardW, cardH, COL_WHITE);
    
    // 2. Double Premium Border
    draw_rounded_rect(cardX, cardY, cardW, cardH, 12, COL_BLACK);
    draw_rounded_rect(cardX + 2, cardY + 2, cardW - 4, cardH - 4, 11, COL_BLACK);

    // 3. Header Section (Dark Bar)
    fill_rect_rotated(cardX + 15, cardY + 15, cardW - 30, 80, COL_BLACK);
    const char* title = settingNew ? "SETUP SECURITY" : "SECURE ACCESS";
    int tw = get_text_width_scaled(title, 0.75);
    writeln_scaled(title, cardX + (cardW - tw) / 2, cardY + 68, 0.75, true, COL_WHITE);

    // 4. PIN Dots Section
    int dotRadius = 12;
    int dotGap = 45;
    int dotTotalW = (4 * 2 * dotRadius) + (3 * dotGap);
    int dotStartX = cardX + (cardW - dotTotalW) / 2 + dotRadius;
    int dotY = cardY + 160;
    
    // Decorative instructions for setup
    if (settingNew) {
        const char* sub = "Create a 4-digit PIN";
        int sw = get_text_width_scaled(sub, 0.45);
        writeln_scaled(sub, cardX + (cardW - sw) / 2, dotY + 65, 0.45, false, COL_BLACK);
    } else {
        draw_line_rotated(cardX + 100, dotY + 40, cardX + cardW - 100, dotY + 40, COL_LIGHT);
    }

    for (int i = 0; i < 4; i++) {
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
    
    // Calculate Grid Start positions
    int gridStartX = cardX + (cardW - (3 * kw + 2 * kGapX)) / 2;
    int gridStartY = cardY + 260;

    // Check if touch is within the grid bounds first
    if (x < gridStartX || y < gridStartY) return;

    // Calculate relative coordinates
    int relX = x - gridStartX;
    int relY = y - gridStartY;

    // Calculate column and row indices
    int col = relX / (kw + kGapX);
    int row = relY / (kh + kGapY);

    // Validate indices (3 cols, 4 rows)
    if (col >= 3 || row >= 4) return;

    // Check if touch is inside the button (accounting for gap)
    int offsetInCol = relX % (kw + kGapX);
    int offsetInRow = relY % (kh + kGapY);
    if (offsetInCol > kw || offsetInRow > kh) return; // Touched the gap

    // Map row/col to key index
    int i = row * 3 + col;
    if (i >= 12) return;

    static const char* keys[] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "CLR", "0", "OK"};
    String val = keys[i];

    Rect_t masterArea = {
        .x = (int32_t)(960 - 1 - (cardY + 820)), // 820 is cardH
        .y = (int32_t)cardX,
        .width = (uint32_t)820,
        .height = (uint32_t)cardW
    };
    epd_poweron();
    for (int w = 0; w < 2; w++) epd_push_pixels(masterArea, 50, 1);

    if (val == "CLR") enteredPin = "";
    else if (val == "OK") {
        if (appState == STATE_SET_PIN) {
            if (enteredPin.length() == 4) {
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
        if (enteredPin.length() < 4) enteredPin += val;
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

// --- Reading Statistics Logic ---

void trackReadingActivity() {
    RTC_Date date = rtc.getDateTime();
    int today = date.day;
    int month = date.month;
    
    // Simple key: stats_MMDD (e.g., stats_208 for Feb 8)
    char dateKey[16];
    sprintf(dateKey, "stats_%d%02d", month, today);
    
    // 1. Increment daily pages
    int pages = prefs.getInt(dateKey, 0);
    prefs.putInt(dateKey, pages + 1);
    
    // 2. Increment global total
    int total = prefs.getInt("stat_total", 0);
    prefs.putInt("stat_total", total + 1);
    
    // 3. Streak Logic
    int lastRead = prefs.getInt("last_read_day", -1);
    if (lastRead != today) {
        // New day!
        int streak = prefs.getInt("stat_streak", 0);
        
        // Very basic streak check (doesn't handle month rollover perfectly for simplicity)
        // Ideally we'd use Epoch time, but for this demo:
        if (lastRead == today - 1 || (lastRead == -1)) {
            prefs.putInt("stat_streak", streak + 1);
        } else if (lastRead != today) {
            // Missed a day (or same day, handled above)
            // If it wasn't yesterday, reset streak (unless first run)
            if (lastRead > 0 && lastRead != today) prefs.putInt("stat_streak", 1);
        }
        
        prefs.putInt("last_read_day", today);
    }
}

void drawStatsDashboard() {
    int cardX = 30;
    int cardY = 50;
    int cardW = P_WIDTH - 60;
    int cardH = 860;

    // --- PHYSICAL WASH (Library Style) ---
    // Wash the dashboard area before drawing to prevent ghosting
    Rect_t washArea = {
        .x = (int32_t)(960 - 1 - (cardY + cardH)),
        .y = (int32_t)cardX,
        .width = (uint32_t)cardH,
        .height = (uint32_t)cardW
    };
    epd_poweron();
    for (int i = 0; i < 3; i++) {
        epd_push_pixels(washArea, 50, 1);
    }

    memset(framebuffer, COL_WHITE, L_WIDTH * L_HEIGHT / 2);

    // --- 1. Master Container Card ---
    fill_rect_rotated(cardX, cardY, cardW, cardH, COL_WHITE);
    draw_rounded_rect(cardX, cardY, cardW, cardH, 12, COL_BLACK);
    draw_rounded_rect(cardX + 2, cardY + 2, cardW - 4, cardH - 4, 11, COL_BLACK);

    // Header Bar inside Master Card
    fill_rect_rotated(cardX + 15, cardY + 15, cardW - 30, 70, COL_BLACK);
    const char* title = "GHOSTPAGE ANALYTICS";
    int tw = get_text_width_scaled(title, 0.6);
    writeln_scaled(title, cardX + (cardW - tw) / 2, cardY + 60, 0.6, true, COL_WHITE);

    // --- 2. Streak Card (Nested) ---
    int streakY = cardY + 110;
    int sCardH = 220;
    fill_rect_rotated(cardX + 30, streakY, cardW - 60, sCardH, COL_WHITE);
    draw_rounded_rect(cardX + 30, streakY, cardW - 60, sCardH, 8, COL_BLACK);
    draw_rounded_rect(cardX + 32, streakY + 2, cardW - 64, sCardH - 4, 7, COL_BLACK);

    int streak = prefs.getInt("stat_streak", 0);
    char streakStr[16]; sprintf(streakStr, "%d", streak);
    int sw = get_text_width_scaled(streakStr, 3.0);
    writeln_scaled(streakStr, cardX + (cardW - sw) / 2, streakY + 140, 3.0, true, COL_BLACK);
    
    const char* lbl1 = "CURRENT DAY STREAK";
    int lw = get_text_width_scaled(lbl1, 0.45);
    writeln_scaled(lbl1, cardX + (cardW - lw) / 2, streakY + 185, 0.45, false, COL_GRAY);

    // --- 3. Progress Card (Nested) ---
    int progY = streakY + sCardH + 30;
    int pCardH = 120;
    fill_rect_rotated(cardX + 30, progY, cardW - 60, pCardH, COL_WHITE);
    draw_rounded_rect(cardX + 30, progY, cardW - 60, pCardH, 8, COL_BLACK);
    draw_rounded_rect(cardX + 32, progY + 2, cardW - 64, pCardH - 4, 7, COL_BLACK);

    int total = prefs.getInt("stat_total", 0);
    char totStr[32]; sprintf(totStr, "%d Pages Read", total);
    int ttw = get_text_width_scaled(totStr, 0.65);
    writeln_scaled(totStr, cardX + (cardW - ttw) / 2, progY + 75, 0.65, true, COL_BLACK);

    // --- 4. Activity Heatmap Card (Nested) ---
    int gridY = progY + pCardH + 30;
    int gCardH = 260;
    fill_rect_rotated(cardX + 30, gridY, cardW - 60, gCardH, COL_WHITE);
    draw_rounded_rect(cardX + 30, gridY, cardW - 60, gCardH, 8, COL_BLACK);
    draw_rounded_rect(cardX + 32, gridY + 2, cardW - 64, gCardH - 4, 7, COL_BLACK);

    const char* hTitle = "30-DAY ACTIVITY HEATMAP";
    int htw = get_text_width_scaled(hTitle, 0.45);
    writeln_scaled(hTitle, cardX + (cardW - htw) / 2, gridY + 45, 0.45, true, COL_BLACK);

    int cellSize = 35;
    int cellGap = 8;
    int cols = 7;
    int rows = 4;
    int gridStartX = cardX + (cardW - (cols * (cellSize + cellGap))) / 2;
    int innerGridY = gridY + 80;

    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            int cx = gridStartX + c * (cellSize + cellGap);
            int cy = innerGridY + r * (cellSize + cellGap);
            int activity = (r * cols + c) % 5; 
            if (activity == 0) draw_rounded_rect(cx, cy, cellSize, cellSize, 4, COL_LIGHT);
            else if (activity < 3) fill_rect_rotated(cx, cy, cellSize, cellSize, COL_LIGHT);
            else fill_rect_rotated(cx, cy, cellSize, cellSize, COL_BLACK);
        }
    }

    // --- 5. Close Button (Card Style) ---
    int btnY = gridY + gCardH + 30;
    int btnW = 200;
    int btnH = 60;
    int bx = cardX + (cardW - btnW) / 2;
    fill_rect_rotated(bx, btnY, btnW, btnH, COL_WHITE);
    draw_rounded_rect(bx, btnY, btnW, btnH, 30, COL_BLACK);
    
    const char* cls = "DISMISS";
    int cw = get_text_width_scaled(cls, 0.5);
    writeln_scaled(cls, bx + (btnW - cw) / 2, btnY + 40, 0.5, true, COL_BLACK);

    epd_draw_grayscale_image(epd_full_screen(), framebuffer);
    epd_poweroff();
}

void handleStatsTouch(int x, int y) {
    int cardX = 25, cardY = 40, cardW = P_WIDTH - 50, cardH = 880;
    int btnY = cardY + cardH - 100;
    int btnW = 180, btnH = 60;
    int bx = cardX + (cardW - btnW) / 2;

    if (DEBUG_ON) Serial.printf("StatsTouch: (%d,%d) | Target: X(%d-%d) Y(%d-%d)\n", x, y, bx, bx+btnW, btnY, btnY+btnH);

    // 1. Check if Dismiss Button was clicked
    if (y >= btnY && y <= btnY + btnH && x >= bx && x <= bx + btnW) {
        showTransitionEffect();
        appState = STATE_LIBRARY;
        updateLibrary();
        return;
    } 
    
    // 2. Check if user tapped outside the Master Card
    if (x < cardX || x > cardX + cardW || y < cardY || y > cardY + cardH) {
        showTransitionEffect();
        appState = STATE_LIBRARY;
        updateLibrary();
        return;
    }
}

// --- Direct Touch Action Logic ---

void handleTouchAction(int x, int y) {
    if (appState == STATE_STATS) {
        handleStatsTouch(x, y);
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
            else if (slot == 3) { // RESET
                String key = getPrefKey(books[currentFileIndex]);
                prefs.remove(key.c_str());
                textPos = 0;
                pageHistory.clear();
                appState = STATE_READING;
                updateReader(false);
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
        // Header Tap -> Stats Dashboard
        if (y < 90) {
            showTransitionEffect();
            appState = STATE_STATS;
            drawStatsDashboard();
            return;
        }

        if (!books.empty()) {
            int page = librarySelection / SHELF_BOOKS_PER_PAGE;
            int startIdx = page * SHELF_BOOKS_PER_PAGE;

            // Check if touching a book cover
            for (int i = 0; i < SHELF_BOOKS_PER_PAGE; i++) {
                int col = i % SHELF_COLS;
                int row = i / SHELF_COLS;

                int xStart = GAP_X + col * (BOOK_W + GAP_X);
                int yStart = SHELF_START_Y + row * (BOOK_H + GAP_Y);

                if (x >= xStart - 10 && x <= xStart + BOOK_W + 10 && 
                    y >= yStart - 10 && y <= yStart + BOOK_H + 10) {
                    
                    int targetIdx = startIdx + i;
                    if (targetIdx < (int)books.size()) {
                        focusedBookIndex = targetIdx;
                        appState = STATE_BOOK_OPTIONS;
                        updateBookCardMenu(focusedBookIndex, true);
                    }
                    return;
                }
            }
            
            // Footer Tap for Page Navigation
            if (y > P_HEIGHT - 115) {
                int totalPages = (books.size() + SHELF_BOOKS_PER_PAGE - 1) / SHELF_BOOKS_PER_PAGE;
                
                // Left side - Previous page
                if (x < P_WIDTH * 0.3 && page > 0) {
                    librarySelection = max(0, librarySelection - SHELF_BOOKS_PER_PAGE);
                    updateLibrary();
                    return;
                }
                // Right side - Next page
                else if (x > P_WIDTH * 0.7 && page < totalPages - 1) {
                    librarySelection += SHELF_BOOKS_PER_PAGE;
                    if (librarySelection >= (int)books.size()) {
                        librarySelection = ((int)books.size() - 1);
                    }
                    updateLibrary();
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
        int cardY = SHELF_START_Y + row * (BOOK_H + GAP_Y);

        if (x >= cardX && x <= cardX + BOOK_W && y >= cardY && y <= cardY + BOOK_H) {
            int bh = BOOK_H / 3;
            int localY = y - cardY;

            if (localY < bh) {
                // OPEN: Open book normally
                librarySelection = focusedBookIndex;
                focusedBookIndex = -1;
                openBook();
                return;
            }
            else if (localY < 2 * bh) {
                // RESET: Delete progress and open
                String key = getPrefKey(books[focusedBookIndex]);
                prefs.remove(key.c_str());
                
                librarySelection = focusedBookIndex;
                textPos = 0;
                pageHistory.clear();
                focusedBookIndex = -1;
                
                if (DEBUG_ON) Serial.println(F("DEBUG: Progress Reset - Starting from 0"));
                
                openBook();
                return;
            }
            else {
                // BACK: Close menu
                appState = STATE_LIBRARY;
                redrawBookCover(focusedBookIndex);
                updateBookCardMenu(focusedBookIndex, false);
                focusedBookIndex = -1;
                return;
            }
        } else {
            // Tap outside -> Close menu
            appState = STATE_LIBRARY;
            redrawBookCover(focusedBookIndex);
            updateBookCardMenu(focusedBookIndex, false);
            focusedBookIndex = -1;
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

void handleNext() {
    if (appState == STATE_LIBRARY) { 
        if (!books.empty()) { 
            librarySelection++; 
            if (librarySelection >= (int)books.size()) librarySelection = 0; 
            updateLibrary(); 
        } 
    }
    else if (appState == STATE_READING) {
        if (lastPageByteCount > 0) {
            pageHistory.push_back(textPos);
            textPos += lastPageByteCount;
            trackReadingActivity();
            updateReader(true);
        } else {
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
    Serial.begin(115200);
    delay(1000);
    Serial.println(F("\n--- GhostPage DIAGNOSTIC BOOT ---"));
    
    epd_init(); 
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
    
    SPI.begin(SD_SCLK, SD_MISO, SD_MOSI, SD_CS); 
    if (SD.begin(SD_CS, SPI)) {
        Serial.println(F("DEBUG: SD Card mounted successfully. Scanning files..."));
        books.clear();
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

    epd_poweron();
    epd_clear(); 
    
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

    // Header sync
    static unsigned long lastHeaderUpdate = 0;
    if (appState == STATE_READING && (millis() - lastHeaderUpdate > 60000)) {
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

                if (appState != STATE_SPLASH && appState != STATE_LOCK && appState != STATE_SET_PIN) {
                    lastInteraction = millis();
                }

                handleTouchAction(mappedX, mappedY);
                lastTouchTime = millis();
                touchReleased = false; // Block subsequent reads until release
            }
        } else {
            touchReleased = true; // Reset when no touch detected
        }
    }
}