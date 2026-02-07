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
    epd_poweron();
    draw_circle_rotated(x, y, 12, COL_BLACK);
    draw_circle_rotated(x, y, 11, COL_BLACK);
    fill_rect_rotated(x - 2, y - 2, 4, 4, COL_BLACK);
    epd_draw_grayscale_image(epd_full_screen(), framebuffer);
    epd_poweroff();
    delay(100);
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
                        // --- INTERACTION FEEDBACK: FLASH HIGHLIGHT ---
                        // Physically flash the card black briefly to register touch
                        Rect_t cardArea = {
                            .x = (int32_t)(960 - 1 - (yStart + BOOK_H)),
                            .y = (int32_t)xStart,
                            .width = (int32_t)BOOK_H,
                            .height = (int32_t)BOOK_W
                        };
                        epd_poweron();
                        epd_push_pixels(cardArea, 30, 0); // 0 = Darken (Black flash)
                        epd_poweroff();

                        focusedBookIndex = targetIdx;
                        appState = STATE_BOOK_OPTIONS;
                        updateBookCardMenu(focusedBookIndex, true);
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
                    return;
                }
                // Right side - Next page
                else if (x > P_WIDTH * 0.7 && page < totalPages - 1) {
                    showTapFeedback(x, y);
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
                showTransitionEffect();
                openBook();
                return;
            }
            else if (localY < 2 * bh) {
                // RESET: Delete progress and open
                String key = getPrefKey(books[focusedBookIndex]);
                prefs.remove(key.c_str());
                if (DEBUG_ON) Serial.println(F("DEBUG: Progress Reset"));
                librarySelection = focusedBookIndex;
                showTransitionEffect();
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

    appState = STATE_SPLASH;
    showEnhancedSplash();
    lastInteraction = millis();
    Serial.println(F("DEBUG: Setup complete. Starting loop."));
}

void loop() { 
    static unsigned long lastBeat = 0;
    if (millis() - lastBeat > 1000) {
        // Serial.printf("DEBUG: System Running. State: %d, Time: %lu\n", appState, millis());
        lastBeat = millis();
    }

    if (appState == STATE_SPLASH) {
        unsigned long timeElapsed = millis() - lastInteraction;
        if (timeElapsed > 5000) {
            Serial.println(F("DEBUG: Splash timeout reached. Transitioning..."));
            appState = STATE_LIBRARY; // Update state first
            showTransitionEffect();
            updateLibrary();
            lastInteraction = millis();
        }
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

                if (appState != STATE_SPLASH) {
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