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
RTC_DATA_ATTR float fontScale = 1.0;
RTC_DATA_ATTR long lastPageByteCount = 0;

uint8_t *framebuffer = NULL;
std::vector<String> books;
Preferences prefs;
Button2 smartBtn;
TouchDrvGT911 touch;
std::vector<long> pageHistory;
unsigned long lastInteraction = 0;
unsigned long lastTouchTime = 0;
PCF8563_Class rtc;

// --- Visual Feedback Functions ---

void showTapFeedback(int x, int y) {
    epd_poweron();
    draw_circle_rotated(x, y, 12, COL_BLACK);
    draw_circle_rotated(x, y, 11, COL_BLACK);
    fill_rect_rotated(x - 2, y - 2, 4, 4, COL_BLACK);
    epd_draw_grayscale_image(epd_full_screen(), framebuffer);
    delay(100);
}

void showTransitionEffect() {
    epd_poweron();
    memset(framebuffer, COL_LIGHT, L_WIDTH * L_HEIGHT / 2);
    epd_draw_grayscale_image(epd_full_screen(), framebuffer);
    delay(80);
}

// --- Direct Touch Action Logic ---

void handleTouchAction(int x, int y) {
    if (appState == STATE_READING) {
        // Bottom Menu Area (Buttons)
        if (y >= BTN_Y_POS - 10 && y <= BTN_Y_POS + BUTTON_H + 10) {
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
    } 
    else if (appState == STATE_LIBRARY) {
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
                        librarySelection = targetIdx;
                        showTransitionEffect();
                        openBook();
                    }
                    return;
                }
            }
            
            // Footer Tap for Page Navigation
            if (y > P_HEIGHT - 110) {
                int totalPages = (books.size() + SHELF_BOOKS_PER_PAGE - 1) / SHELF_BOOKS_PER_PAGE;
                
                // Left side - Previous page
                if (x < P_WIDTH * 0.3 && page > 0) {
                    showTapFeedback(x, y);
                    librarySelection = max(0, librarySelection - SHELF_BOOKS_PER_PAGE);
                    updateLibrary();
                }
                // Right side - Next page
                else if (x > P_WIDTH * 0.7 && page < totalPages - 1) {
                    showTapFeedback(x, y);
                    librarySelection += SHELF_BOOKS_PER_PAGE;
                    if (librarySelection >= (int)books.size()) {
                        librarySelection = ((int)books.size() - 1);
                    }
                    updateLibrary();
                }
            }
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
            updateReader(true);
        } else {
            updateReader(true);
        }
    }
}

void showEnhancedSplash() {
    epd_poweron(); 
    epd_clear(); 
    memset(framebuffer, COL_WHITE, L_WIDTH * L_HEIGHT / 2);
    
    int titleY = 350;
    const char* title = "GhostPage";
    int titleW = get_text_width_scaled(title, 2.2);
    writeln_scaled(title, (P_WIDTH - titleW) / 2, titleY, 2.2, true, COL_BLACK);
    
    const char* tagline = "Your Personal E-Reader";
    int tagW = get_text_width_scaled(tagline, 0.8);
    writeln_scaled(tagline, (P_WIDTH - tagW) / 2, titleY + 70, 0.8, false, COL_GRAY);
    
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
    }
    
    touch.setPins(-1, TOUCH_INT); 
    uint8_t touchAddress = 0;
    Wire.beginTransmission(0x14);
    if (Wire.endTransmission() == 0) touchAddress = 0x14;
    else {
        Wire.beginTransmission(0x5D);
        if (Wire.endTransmission() == 0) touchAddress = 0x5D;
    }

    if (touchAddress != 0) {
        touch.begin(Wire, touchAddress, TOUCH_SDA, TOUCH_SCL);
        touch.setMaxCoordinates(L_WIDTH, L_HEIGHT);
        touch.setSwapXY(true);
        touch.setMirrorXY(true, false);
    }

    smartBtn.begin(BUTTON_1);
    smartBtn.setReleasedHandler([](Button2& b) {
        lastInteraction = millis(); 
        unsigned long d = b.wasPressedFor();
        
        if (d < 500) {
            // Short press - next page or next book
            handleNext();
        }
        else {
            // Long press (any length over 500ms) - back to library
            if (appState == STATE_READING) { 
                showTransitionEffect();
                appState = STATE_LIBRARY; 
                updateLibrary(); 
            }
        }
    });

    appState = STATE_SPLASH;
    showEnhancedSplash();
    lastInteraction = millis();
}

void loop() { 
    
    if (appState == STATE_SPLASH && (millis() - lastInteraction > 5000)) {
        showTransitionEffect();
        appState = STATE_LIBRARY;
        updateLibrary();
        lastInteraction = millis();
    }

    // Header sync
    static unsigned long lastHeaderUpdate = 0;
    if (appState == STATE_READING && (millis() - lastHeaderUpdate > 60000)) {
        partialUpdateHeader();
        lastHeaderUpdate = millis();
    }

    // Direct Touch
    if (millis() > lastTouchTime + TOUCH_COOLDOWN) {
        int16_t tx, ty;
        if (touch.getPoint(&tx, &ty)) {
            handleTouchAction(tx, ty);
            lastTouchTime = millis();
            lastInteraction = millis();
        }
    }
}
