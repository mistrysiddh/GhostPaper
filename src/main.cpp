#include "common.h"
#include <WiFi.h>
#include <Button2.h>
#include <math.h> 

// --- Configuration & Expert Toggles ---
#define SLEEP_TIMEOUT_MS  300000 

// --- Global Variable Definitions ---
const unsigned long TOUCH_COOLDOWN = 500; 
RTC_DATA_ATTR AppState appState = STATE_SPLASH;
RTC_DATA_ATTR int librarySelection = 0; 
RTC_DATA_ATTR int targetBookIndex = -1;
RTC_DATA_ATTR int targetBookX = 0;
RTC_DATA_ATTR int targetBookY = 0;
RTC_DATA_ATTR int currentFileIndex = -1;
RTC_DATA_ATTR long textPos = 0;
RTC_DATA_ATTR float fontScale = 1.0;
RTC_DATA_ATTR bool touchEnabled = true;
RTC_DATA_ATTR long lastPageByteCount = 0;
bool isTouchProcessed = false;
int lastTouchX = 0, lastTouchY = 0;

uint8_t *framebuffer = NULL;
std::vector<String> books;
Preferences prefs;
Button2 smartBtn;
TouchDrvGT911 touch;
std::vector<long> pageHistory;
unsigned long lastInteraction = 0;
unsigned long lastTouchTime = 0;
unsigned long pressStartTime = 0;
PCF8563_Class rtc;

// --- Enhanced Visual Feedback Functions ---

void showTapFeedback(int x, int y, int radius = 30) {
    // Style 1: Modern High-Contrast Target Dot
    epd_poweron();
    draw_circle_rotated(x, y, 12, COL_BLACK);
    draw_circle_rotated(x, y, 11, COL_BLACK); // Thicker ring
    fill_rect_rotated(x - 2, y - 2, 4, 4, COL_BLACK); // Center dot
    epd_draw_grayscale_image(epd_full_screen(), framebuffer);
    delay(100);
}

void showButtonPressFeedback(int btnX, int btnY, int btnW, int btnH, const char* label) {
    // Visual feedback disabled
}

void showTransitionEffect() {
    // Quick fade effect for state transitions
    epd_poweron();
    memset(framebuffer, COL_LIGHT, L_WIDTH * L_HEIGHT / 2);
    epd_draw_grayscale_image(epd_full_screen(), framebuffer);
    delay(80);
}

// --- Expert State Machine Logic ---

void handleTouchAction(int x, int y) {
    if (appState == STATE_READING) {
        // --- Reader Touch Zones (Buttons ONLY) ---
        
        // 1. Bottom Menu Area (Buttons)
        if (y >= BTN_Y_POS - 10 && y <= BTN_Y_POS + BUTTON_H + 10) {
            int mbx = (P_WIDTH - (3 * BUTTON_W + 2 * BUTTON_GAP)) / 2;
            
            // Back Button
            if (x >= mbx && x <= mbx + BUTTON_W) {
                if (DEBUG_ON) Serial.println(F("[TOUCH] Back to Library"));
                showButtonPressFeedback(mbx, BTN_Y_POS, BUTTON_W, BUTTON_H, "Back");
                showTransitionEffect();
                appState = STATE_LIBRARY; 
                updateLibrary();
                return;
            }
            // Prev Button
            else if (x >= mbx + BUTTON_W + BUTTON_GAP && x <= mbx + 2*BUTTON_W + BUTTON_GAP) {
                if (DEBUG_ON) Serial.println(F("[TOUCH] Previous Page"));
                int prevBtnX = mbx + BUTTON_W + BUTTON_GAP;
                showButtonPressFeedback(prevBtnX, BTN_Y_POS, BUTTON_W, BUTTON_H, "Prev");
                
                if (!pageHistory.empty()) { 
                    textPos = pageHistory.back(); 
                    pageHistory.pop_back(); 
                    updateReader(); 
                } else {
                    updateReader(); // Reload current if no history
                }
                return;
            }
            // Next Button
            else if (x >= mbx + 2*(BUTTON_W + BUTTON_GAP) && x <= mbx + 3*BUTTON_W + 2*BUTTON_GAP) {
                if (DEBUG_ON) Serial.println(F("[TOUCH] Next Page"));
                int nextBtnX = mbx + 2*(BUTTON_W + BUTTON_GAP);
                showButtonPressFeedback(nextBtnX, BTN_Y_POS, BUTTON_W, BUTTON_H, "Next");
                handleNext();
                return;
            }
        }
        
        // ALL OTHER TOUCHES IN STATE_READING ARE NOW IGNORED
        return;
    } 
    else if (appState == STATE_LIBRARY) {
        if (!books.empty()) {
            int page = librarySelection / SHELF_BOOKS_PER_PAGE;
            int startIdx = page * SHELF_BOOKS_PER_PAGE;

            // Check if touching a book cover
            bool bookTouched = false;
            for (int i = 0; i < SHELF_BOOKS_PER_PAGE; i++) {
                int col = i % SHELF_COLS;
                int row = i / SHELF_COLS;

                int xStart = GAP_X + col * (BOOK_W + GAP_X);
                int yStart = SHELF_START_Y + row * (BOOK_H + GAP_Y);

                if (x >= xStart - 10 && x <= xStart + BOOK_W + 10 && 
                    y >= yStart - 10 && y <= yStart + BOOK_H + 10) {
                    
                    int targetIdx = startIdx + i;
                    if (targetIdx < (int)books.size()) {
                        bookTouched = true;
                        if (DEBUG_ON) Serial.printf("[TOUCH] Card #%d -> Show Inline Menu\n", targetIdx);
                        
                        targetBookIndex = targetIdx;
                        targetBookX = xStart;
                        targetBookY = yStart;
                        librarySelection = targetIdx;
                        appState = STATE_BOOK_MENU;
                        updateLibrary();
                    }
                    return;
                }
            }
            
            // Footer Tap for Page Navigation
            if (!bookTouched && y > P_HEIGHT - 110) {
                int totalPages = (books.size() + SHELF_BOOKS_PER_PAGE - 1) / SHELF_BOOKS_PER_PAGE;
                
                // Left side - Previous page
                if (x < P_WIDTH * 0.3 && page > 0) {
                    if (DEBUG_ON) Serial.println(F("[TOUCH] Library Previous Page"));
                    showTapFeedback(x, y, 20);
                    librarySelection = max(0, librarySelection - SHELF_BOOKS_PER_PAGE);
                    updateLibrary();
                }
                // Right side - Next page
                else if (x > P_WIDTH * 0.7 && page < totalPages - 1) {
                    if (DEBUG_ON) Serial.println(F("[TOUCH] Library Next Page"));
                    showTapFeedback(x, y, 20);
                    librarySelection += SHELF_BOOKS_PER_PAGE;
                    if (librarySelection >= (int)books.size()) {
                        librarySelection = ((int)books.size() - 1);
                    }
                    updateLibrary();
                }
                // Center - tap page indicator to jump to first page
                else if (x >= P_WIDTH * 0.35 && x <= P_WIDTH * 0.65) {
                    if (DEBUG_ON) Serial.println(F("[TOUCH] Jump to first page"));
                    showTapFeedback(x, y, 20);
                    librarySelection = 0;
                    updateLibrary();
                }
            }
        }
    }
    else if (appState == STATE_BOOK_MENU) {
        // Check if tap is inside the target card
        if (x >= targetBookX && x <= targetBookX + BOOK_W && 
            y >= targetBookY && y <= targetBookY + BOOK_H) {
            
            // Check which button was clicked
            for (int i = 0; i < 3; i++) {
                int btnY = targetBookY + 30 + (i * 85);
                if (y >= btnY && y <= btnY + 65) {
                    if (i == 0) { // OPEN
                        if (DEBUG_ON) Serial.println(F("[MENU] Open"));
                        showTransitionEffect();
                        openBook();
                    }
                    else if (i == 1) { // RESET
                        if (DEBUG_ON) Serial.println(F("[MENU] Reset"));
                        prefs.putLong(getPrefKey(books[targetBookIndex]).c_str(), 0);
                        appState = STATE_LIBRARY;
                        updateLibrary(); 
                    }
                    else { // BACK
                        if (DEBUG_ON) Serial.println(F("[MENU] Back"));
                        appState = STATE_LIBRARY;
                        updateLibrary();
                    }
                    return;
                }
            }
        } else {
            // Tap outside -> Close menu smoothly
            if (DEBUG_ON) Serial.println(F("[MENU] Tap Outside -> Close"));
            appState = STATE_LIBRARY;
            updateLibrary();
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
            if (n.endsWith(".txt") || n.endsWith(".pdf") || n.endsWith(".epub")) books.push_back(String(file.path())); 
        }
        file = dir.openNextFile();
    }
}

String getPrefKey(String path) {
    String key = path.substring(path.lastIndexOf('/') + 1);
    if (key.length() > 15) key = key.substring(0, 15);
    return key;
}

void goToSleep() {
    if (DEBUG_ON) Serial.println(F("[SYS] Entering Deep Sleep..."));
    
    epd_poweron(); 
    epd_clear(); 
    memset(framebuffer, COL_WHITE, L_WIDTH * L_HEIGHT / 2);
    
    // Enhanced sleep screen with moon icon
    int centerX = P_WIDTH / 2;
    int centerY = P_HEIGHT / 2 - 50;
    
    // Moon crescent
    int moonSize = 80;
    // Outer circle
    for (int r = moonSize; r > moonSize - 6; r -= 2) {
        for (int angle = 0; angle < 360; angle += 15) {
            float rad = angle * 3.14159 / 180.0;
            int px = centerX + (int)(r * cos(rad));
            int py = centerY + (int)(r * sin(rad));
            fill_rect_rotated(px - 2, py - 2, 4, 4, COL_BLACK);
        }
    }
    
    // Inner shadow crescent (offset circle to create crescent)
    for (int r = moonSize - 10; r > moonSize - 16; r -= 2) {
        for (int angle = 0; angle < 360; angle += 15) {
            float rad = angle * 3.14159 / 180.0;
            int px = centerX + 25 + (int)(r * cos(rad));
            int py = centerY + (int)(r * sin(rad));
            fill_rect_rotated(px - 2, py - 2, 4, 4, COL_WHITE);
        }
    }
    
    // Stars around moon
    int stars[][2] = {{-120, -80}, {120, -60}, {-90, 70}, {100, 80}, {-140, 20}, {130, -20}};
    for (int i = 0; i < 6; i++) {
        int sx = centerX + stars[i][0];
        int sy = centerY + stars[i][1];
        fill_rect_rotated(sx - 3, sy, 7, 2, COL_BLACK);
        fill_rect_rotated(sx, sy - 3, 2, 7, COL_BLACK);
    }
    
    const char* msg1 = "Sleeping...";
    const char* msg2 = "Press button to wake";
    
    writeln_scaled(msg1, (P_WIDTH - get_text_width_scaled(msg1, 1.0)) / 2, centerY + 120, 1.0, true, COL_BLACK);
    writeln_scaled(msg2, (P_WIDTH - get_text_width_scaled(msg2, 0.6)) / 2, centerY + 165, 0.6, false, COL_DARK);
    
    epd_draw_grayscale_image(epd_full_screen(), framebuffer); 
    epd_poweroff();
    
    // Ensure pull-up is enabled for the wake-up pin
    gpio_pullup_en((gpio_num_t)BUTTON_1);
    gpio_pulldown_dis((gpio_num_t)BUTTON_1);
    
    esp_sleep_enable_ext0_wakeup((gpio_num_t)BUTTON_1, 0); 
    esp_deep_sleep_start();
}

void handleNext() {
    if (DEBUG_ON) Serial.println(F("[SYS] handleNext called"));
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
            updateReader();
        } else {
            updateReader();
        }
    }
}

void showEnhancedSplash() {
    epd_poweron(); 
    epd_clear(); 
    memset(framebuffer, COL_WHITE, L_WIDTH * L_HEIGHT / 2);
    
    // --- Text-Only Minimalist Splash ---
    
    int titleY = 350;
    const char* title = "GhostPage";
    
    // Reduced scale from 2.8 to 2.2 to fit screen
    int titleW = get_text_width_scaled(title, 2.2);
    writeln_scaled(title, (P_WIDTH - titleW) / 2, titleY, 2.2, true, COL_BLACK);
    
    // Tagline
    const char* tagline = "Your Personal E-Reader";
    int tagW = get_text_width_scaled(tagline, 0.8);
    writeln_scaled(tagline, (P_WIDTH - tagW) / 2, titleY + 70, 0.8, false, COL_GRAY);
    
    // Divider line
    draw_line_rotated(100, titleY + 120, P_WIDTH - 100, titleY + 120, COL_BLACK);

    epd_draw_grayscale_image(epd_full_screen(), framebuffer); 
    epd_poweroff();
}

void setup() {
    if (DEBUG_ON) { Serial.begin(115200); delay(1000); Serial.println(F("\n--- GhostPage BOOT ---")); }
    
    epd_init(); 
    framebuffer = (uint8_t *)heap_caps_malloc(L_WIDTH * L_HEIGHT / 2, MALLOC_CAP_SPIRAM);
    
    Wire.begin(TOUCH_SDA, TOUCH_SCL); 
    rtc.begin();
    
    prefs.begin("vellum", false); 
    fontScale = prefs.getFloat("fscale", 1.0);
    
    SPI.begin(SD_SCLK, SD_MISO, SD_MOSI, SD_CS); 
    if (SD.begin(SD_CS, SPI)) {
        books.clear();
        scanFiles("/");
        if (DEBUG_ON) Serial.printf("[SD] Found %d books\n", books.size());
    } else {
        if (DEBUG_ON) Serial.println(F("[SD] Card Mount Failed!"));
    }
    
    touch.setPins(TOUCH_RST, TOUCH_INT); 
    touch.begin(Wire, GT911_SLAVE_ADDRESS_H, TOUCH_SDA, TOUCH_SCL);

    smartBtn.begin(BUTTON_1);
    smartBtn.setPressedHandler([](Button2& b) { pressStartTime = millis(); });
    smartBtn.setReleasedHandler([](Button2& b) {
        lastInteraction = millis(); 
        unsigned long d = millis() - pressStartTime;
        
        if (d > 2000) {
            // Long press - toggle touch mode with feedback
            touchEnabled = !touchEnabled;
            if (DEBUG_ON) Serial.printf("Touch Mode: %s\n", touchEnabled ? "ON" : "OFF");
            
            // Show visual feedback
            epd_poweron();
            const char* msg = touchEnabled ? "Touch: ON" : "Touch: OFF";
            int msgW = get_text_width_scaled(msg, 0.7);
            
            // Notification banner
            fill_rect_rotated((P_WIDTH - msgW - 40) / 2, 50, msgW + 40, 50, COL_BLACK);
            writeln_scaled(msg, (P_WIDTH - msgW) / 2, 80, 0.7, true, COL_WHITE);
            epd_draw_grayscale_image(epd_full_screen(), framebuffer);
            delay(800);
            
            // Restore screen
            if (appState == STATE_READING) updateReader();
            else if (appState == STATE_LIBRARY) updateLibrary();
        }
        else if (d < 500) {
            // Short press - next
            handleNext();
        }
        else {
            // Medium press - back
            if (appState == STATE_READING) { 
                showTransitionEffect();
                appState = STATE_LIBRARY; 
                updateLibrary(); 
            }
        }
    });

    // Boot behavior
    if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_UNDEFINED) {
        // Fresh boot - show splash
        appState = STATE_SPLASH;
        showEnhancedSplash();
    } else {
        // Wake from sleep - resume
        if (DEBUG_ON) Serial.println(F("[WAKE] Resuming from sleep..."));
        if (appState == STATE_READING) {
            updateReader();
        } else { 
            appState = STATE_LIBRARY; 
            updateLibrary(); 
        }
    }
    
    lastInteraction = millis();
}

void loop() { 
    smartBtn.loop(); 
    
    // Auto-transition from Splash to Library after 5 seconds
    if (appState == STATE_SPLASH && (millis() - lastInteraction > 5000)) {
        if (DEBUG_ON) Serial.println(F("[SYS] Splash -> Library Auto-Transition"));
        showTransitionEffect();
        appState = STATE_LIBRARY;
        updateLibrary();
        lastInteraction = millis();
    }

    if (touchEnabled) {
        if (touch.isPressed()) {
            // Ignore touch if still in cooldown window
            if (millis() - lastTouchTime < TOUCH_COOLDOWN) return;

            int16_t tx[1], ty[1];
            if (touch.getPoint(tx, ty, 1)) {
                lastTouchX = 540 - tx[0]; 
                lastTouchY = 960 - ty[0];
                
                if (!isTouchProcessed) {
                    isTouchProcessed = true;
                }
                lastInteraction = millis();
            }
        } else {
            // Touch released
            if (isTouchProcessed) {
                handleTouchAction(lastTouchX, lastTouchY);
                isTouchProcessed = false;
                lastTouchTime = millis(); // lock input immediately on release
                lastInteraction = millis();
                if (DEBUG_ON) Serial.println(F("[TOUCH] Released & Cooldown Started"));
            }
        }
    }
    
    if (millis() - lastInteraction > SLEEP_TIMEOUT_MS) {
        goToSleep();
    }
}
