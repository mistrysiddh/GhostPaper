#include "common.h"
#include "default_book.h"

void partialUpdateHeader() {
    if (appState != STATE_READING) return;
    
    // 1. Refresh Header Area (Top 70px)
    Rect_t headerArea = {
        .x = (int32_t)(960 - 1 - 70),
        .y = 0,
        .width = 70,
        .height = (int32_t)P_WIDTH
    };

    epd_poweron();
    uint8_t black = COL_BLACK, gray = COL_GRAY;

    // Redraw Header Background/Base in framebuffer
    fill_rect_rotated(0, 0, P_WIDTH, 70, COL_BG);
    draw_line_rotated(30, 70, P_WIDTH - 30, 70, black);

    // Title
    String path = books[currentFileIndex];
    String title = path.substring(path.lastIndexOf('/') + 1);
    int dotIdx = title.lastIndexOf('.');
    if (dotIdx > 0) title = title.substring(0, dotIdx);
    title.replace("_", " ");
    if (title.length() > 25) title = title.substring(0, 22) + "...";
    writeln_scaled(title.c_str(), 35, 40, 0.5, true, black);

    // Battery
    float batt = getBatteryVoltage();
    char battStr[16]; 
    sprintf(battStr, "%.2fV", batt);
    int battW = get_text_width_scaled(battStr, 0.5);
    writeln_scaled(battStr, P_WIDTH - 35 - battW, 40, 0.5, true, black);

    if (!touchEnabled) {
        const char* lockMsg = "[LOCKED]";
        int lockW = get_text_width_scaled(lockMsg, 0.5);
        writeln_scaled(lockMsg, (P_WIDTH - lockW) / 2, 40, 0.5, true, black);
    }
    
    epd_draw_grayscale_image(headerArea, framebuffer);

    // 2. Refresh Footer Area (Bottom 120px)
    int footerStart = P_HEIGHT - 130;
    Rect_t footerArea = {
        .x = 0, // Starts at bottom in physical landscape
        .y = 0,
        .width = 130,
        .height = (int32_t)P_WIDTH
    };

    // Redraw Footer Base
    fill_rect_rotated(0, footerStart, P_WIDTH, 130, COL_BG);
    draw_line_rotated(30, footerStart, P_WIDTH - 30, footerStart, black);

    // Progress Logic
    long totalSize = 0;
    if (path.startsWith("internal:")) totalSize = strlen(DEFAULT_BOOK_TEXT);
    else {
        File f = SD.open(path);
        if (f) { totalSize = f.size(); f.close(); }
    }
    int progress = (totalSize > 0) ? (textPos * 100 / totalSize) : 0;
    
    // Time & Progress Text
    int footerTextY = P_HEIGHT - 105;
    String timeS = getTimeString();
    writeln_scaled(timeS.c_str(), 35, footerTextY + 10, 0.45, false, gray);

    char progStr[32]; 
    sprintf(progStr, "%d%% completed", progress);
    int progW = get_text_width_scaled(progStr, 0.45);
    writeln_scaled(progStr, P_WIDTH - 35 - progW, footerTextY + 10, 0.45, false, gray);

    // Progress Bar
    int barX = 35;
    int barY = footerTextY + 25;
    int barW = P_WIDTH - 70;
    draw_rect_rotated(barX, barY, barW, 4, gray);
    if (progress > 0) {
        fill_rect_rotated(barX, barY, (int)(barW * (progress / 100.0)), 4, black);
    }

    epd_draw_grayscale_image(footerArea, framebuffer);
    epd_poweroff();
}

void updateReader(bool partial_refresh) {
    if (DEBUG_ON) Serial.printf("[UI] Rendering Reader Page... Partial: %s\n", partial_refresh ? "YES" : "NO");
    epd_poweron();
    
    // Full refresh only when explicitly requested (e.g., opening book or manual refresh)
    if (!partial_refresh) {
        epd_clear(); 
    }
    
    memset(framebuffer, COL_BG, L_WIDTH * L_HEIGHT / 2);
    uint8_t black = COL_BLACK, gray = COL_GRAY;

    if (books.empty() || currentFileIndex < 0 || currentFileIndex >= (int)books.size()) {
        epd_poweroff();
        return;
    }

    String path = books[currentFileIndex];
    bool isInternal = path.startsWith("internal:");
    String title = "";
    long totalSize = 0;
    
    // OPTIMIZATION: Persistent buffer to avoid heap fragmentation
    static char buf[5001]; 

    int boxTop = 80;
    int boxBottom = 820;
    int boxMargin = 25; 
    int boxW = P_WIDTH - 2 * boxMargin;
    int boxH = boxBottom - boxTop;

    // Text rendering coordinates (nested inside the border)
    int textStartX = boxMargin + 15;
    int textStartY = boxTop + 45; 
    int textMaxWidth = P_WIDTH - boxMargin - 15;
    int textMaxHeight = boxBottom - 15;

    if (isInternal) {
        title = DEFAULT_BOOK_TITLE;
        totalSize = strlen(DEFAULT_BOOK_TEXT);
    } else {
        File f = SD.open(path);
        if (f) {
            title = path.substring(path.lastIndexOf('/') + 1);
            int dotIdx = title.lastIndexOf('.');
            if (dotIdx > 0) title = title.substring(0, dotIdx);
            title.replace("_", " ");
            totalSize = f.size();
            f.close();
        }
    }

    if (title != "") {
        // --- 1. Header (Clean & Modern) ---
        int headerH = 70;
        draw_line_rotated(30, headerH, P_WIDTH - 30, headerH, black);

        String displayTitle = title;
        if (displayTitle.length() > 25) displayTitle = displayTitle.substring(0, 22) + "...";
        writeln_scaled(displayTitle.c_str(), 35, 40, 0.5, true, black);

        // Battery Voltage
        float batt = getBatteryVoltage();
        char battStr[16]; sprintf(battStr, "%.2fV", batt);
        int battW = get_text_width_scaled(battStr, 0.5);
        writeln_scaled(battStr, P_WIDTH - 35 - battW, 40, 0.5, true, black);

        if (!touchEnabled) {
            const char* lockMsg = "[LOCKED]";
            int lockW = get_text_width_scaled(lockMsg, 0.5);
            writeln_scaled(lockMsg, (P_WIDTH - lockW) / 2, 40, 0.5, true, black);
        }

        // --- 2. Main Text Area ---
        if (partial_refresh) {
            Rect_t textCardArea = {
                .x = (int32_t)(960 - 1 - (boxTop + boxH)),
                .y = (int32_t)boxMargin,
                .width = (int32_t)boxH,
                .height = (int32_t)boxW
            };
            for (int i = 0; i < 3; i++) {
                epd_push_pixels(textCardArea, 50, 1);
            }
        }

        fill_rect_rotated(boxMargin, boxTop, boxW, boxH, COL_WHITE);
        draw_rect_rotated(boxMargin, boxTop, boxW, boxH, black);

        // --- RENDER TEXT CONTENT ---
        if (isInternal) {
            if (textPos >= totalSize) textPos = 0;
            int toRead = min((long)sizeof(buf) - 1, totalSize - textPos);
            if (toRead > 0) {
                memcpy(buf, DEFAULT_BOOK_TEXT + textPos, toRead);
                buf[toRead] = '\0';
                lastPageByteCount = renderPage(buf, textStartX, textStartY, textMaxWidth, textMaxHeight);
            } else {
                lastPageByteCount = 0;
            }
        } else {
            File f = SD.open(path);
            if (f) {
                f.seek(textPos);
                int r = f.readBytes(buf, sizeof(buf) - 1); 
                buf[r] = '\0';
                lastPageByteCount = renderPage(buf, textStartX, textStartY, textMaxWidth, textMaxHeight);
                f.close();
            }
        }

        // --- 3. Footer ---
        int footerY = P_HEIGHT - 115;
        draw_line_rotated(30, footerY - 15, P_WIDTH - 30, footerY - 15, black);

        int progress = (totalSize > 0) ? (textPos * 100 / totalSize) : 0;
        
        String timeS = getTimeString();
        writeln_scaled(timeS.c_str(), 35, footerY + 10, 0.45, false, gray);

        char progStr[32]; 
        sprintf(progStr, "%d%% completed", progress);
        int progW = get_text_width_scaled(progStr, 0.45);
        writeln_scaled(progStr, P_WIDTH - 35 - progW, footerY + 10, 0.45, false, gray);

        int barX = 35;
        int barY = footerY + 25;
        int barW = P_WIDTH - 70;
        draw_rect_rotated(barX, barY, barW, 4, gray);
        if (progress > 0) {
            fill_rect_rotated(barX, barY, (int)(barW * (progress / 100.0)), 4, black);
        }

        // Navigation Buttons
        int btnY = BTN_Y_POS;
        int mbx = (P_WIDTH - (3 * BUTTON_W + 2 * BUTTON_GAP)) / 2;

        const char* btnLabels[] = {"Back", "Prev", "Next"};
        for (int i = 0; i < 3; i++) {
            int bx = mbx + i * (BUTTON_W + BUTTON_GAP);
            draw_rounded_rect(bx, btnY, BUTTON_W, BUTTON_H, 8, black);
            int tw = get_text_width_scaled(btnLabels[i], 0.45);
            writeln_scaled(btnLabels[i], bx + (BUTTON_W - tw) / 2, btnY + 33, 0.45, true, black);
        }

        prefs.putLong(getPrefKey(path).c_str(), textPos); 
    }
    
    if (partial_refresh) {
        // Fast B&W partial update - minimal ghosting
        epd_draw_image(epd_full_screen(), framebuffer, BLACK_ON_WHITE);
    } else {
        // High Quality Grayscale update - eliminates ghosting
        epd_draw_grayscale_image(epd_full_screen(), framebuffer); 
        if (DEBUG_ON) {
            Serial.println(F("[FULL] Grayscale refresh completed"));
        }
    }
    
    epd_poweroff();
}

// Alternative: Manual full refresh trigger
void forceFullRefresh() {
    updateReader(false);
}

long renderPage(const char* text, int startX, int startY, int maxWidth, int maxHeight) {
    int curX = startX; 
    int curY = startY; 
    const char* p = text;
    int lineHeight = (int)(52 * fontScale); 
    
    while (*p) {
        if (*p == '\r') { 
            p++; 
            continue; 
        }
        
        if (*p == '\n') {
            curX = startX; 
            curY += lineHeight;
            if (*(p+1) == '\n') { 
                curY += lineHeight / 2; 
                p++; 
            }
            p++; 
            if (curY > maxHeight) break; 
            continue;
        }
        
        const char* next_p = p;
        while (*next_p && *next_p != ' ' && *next_p != '\n' && *next_p != '\r') {
            decode_utf8(&next_p);
        }
        
        int wordLen = next_p - p; 
        if(wordLen > 0) {
            char word[128]; 
            if(wordLen > 127) wordLen = 127;
            strncpy(word, p, wordLen); 
            word[wordLen] = '\0';
            
            int tW = get_text_width_scaled(word, fontScale);
            
            if (curX + tW > maxWidth) { 
                curX = startX; 
                curY += lineHeight; 
            }
            
            if (curY > maxHeight) break;
            
            writeln_scaled(word, curX, curY, fontScale, false, 0x00);
            curX += tW;
        }
        
        p = next_p; 
        if (*p == ' ') { 
            curX += (int)(12 * fontScale); 
            p++; 
        }
    }
    
    return (p - text);
}