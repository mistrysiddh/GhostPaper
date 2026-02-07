#include "common.h"

// Counter for periodic full refresh to prevent ghosting
static int partialUpdateCounter = 0;
const int MAX_PARTIAL_BEFORE_FULL = 5;  // Full refresh every 5 partial updates

void partialUpdateHeader() {
    if (appState != STATE_READING) return;
    
    // Define the header area to update in PHYSICAL landscape coordinates
    // y=0 in portrait (top) is the RIGHT side in physical landscape if not swapped, 
    // but the driver handles the mapping. Let's use the same logic as library/reader.
    Rect_t area = {
        .x = 0,
        .y = 0,
        .width = (uint32_t)L_WIDTH,
        .height = 70
    };

    epd_poweron();
    
    // No epd_clear_area - we draw the whole stripe to avoid flashing
    
    uint8_t black = COL_BLACK;

    // Redraw Battery Voltage into the framebuffer
    float batt = getBatteryVoltage();
    char battStr[16]; 
    sprintf(battStr, "%.2fV", batt);
    int battW = get_text_width_scaled(battStr, 0.5);
    
    // We update the framebuffer first, then push to screen
    writeln_scaled(battStr, P_WIDTH - 35 - battW, 40, 0.5, true, black);
    
    // Push the grayscale stripe to avoid the B&W flash
    epd_draw_grayscale_image(area, framebuffer);
    
    epd_poweroff();
}

void updateReader(bool partial_refresh) {
    if (DEBUG_ON) Serial.printf("[UI] Rendering Reader Page... Partial: %s\n", partial_refresh ? "YES" : "NO");
    epd_poweron();
    
    // Decide whether to force full refresh based on counter
    bool doFullRefresh = !partial_refresh || (partialUpdateCounter >= MAX_PARTIAL_BEFORE_FULL);
    
    if (doFullRefresh) {
        epd_clear(); 
        partialUpdateCounter = 0;
    }
    
    memset(framebuffer, COL_BG, L_WIDTH * L_HEIGHT / 2);
    uint8_t black = COL_BLACK, gray = COL_GRAY;

    if (books.empty() || currentFileIndex < 0 || currentFileIndex >= (int)books.size()) {
        epd_poweroff();
        return;
    }

    File f = SD.open(books[currentFileIndex]);
    if (f) {
        // --- 1. Header (Clean & Modern) ---
        int headerH = 70;
        draw_line_rotated(30, headerH, P_WIDTH - 30, headerH, black);

        // Title
        String title = books[currentFileIndex].substring(books[currentFileIndex].lastIndexOf('/') + 1);
        int dotIdx = title.lastIndexOf('.');
        if (dotIdx > 0) title = title.substring(0, dotIdx);
        title.replace("_", " ");
        if (title.length() > 25) title = title.substring(0, 22) + "...";
        writeln_scaled(title.c_str(), 35, 40, 0.5, true, black);

        // Battery Voltage
        float batt = getBatteryVoltage();
        char battStr[16]; sprintf(battStr, "%.2fV", batt);
        int battW = get_text_width_scaled(battStr, 0.5);
        writeln_scaled(battStr, P_WIDTH - 35 - battW, 40, 0.5, true, black);

        // --- 2. Main Text Area (Enhanced Card Style) ---
        int boxTop = 80;
        int boxBottom = 820;
        int boxMargin = 15;
        int boxW = P_WIDTH - 2 * boxMargin;
        int boxH = boxBottom - boxTop;

        // 1. Soft Drop Shadow
        fill_rect_rotated(boxMargin + 4, boxTop + 4, boxW, boxH, COL_GRAY);

        // 2. Main Background (White card)
        fill_rect_rotated(boxMargin, boxTop, boxW, boxH, COL_WHITE);

        // 3. Decorative Double Border
        draw_rounded_rect(boxMargin, boxTop, boxW, boxH, 8, black);
        draw_rounded_rect(boxMargin + 2, boxTop + 2, boxW - 4, boxH - 4, 7, black);

        // Text rendering coordinates (nested inside the border)
        int textStartX = boxMargin + 20;
        int textStartY = boxTop + 45; 
        int textMaxWidth = P_WIDTH - boxMargin - 20;
        int textMaxHeight = boxBottom - 20; 

        f.seek(textPos);
        const int bufSize = 5001; 
        char *buf = (char*)malloc(bufSize);
        if (buf) {
            int r = f.readBytes(buf, bufSize - 1); 
            buf[r] = '\0';
            lastPageByteCount = renderPage(buf, textStartX, textStartY, textMaxWidth, textMaxHeight); 
            free(buf);
        }

        // --- 3. Footer (Visual Progress & Controls) ---
        int footerY = P_HEIGHT - 115;
        draw_line_rotated(30, footerY - 15, P_WIDTH - 30, footerY - 15, black);

        long totalSize = f.size();
        int progress = (totalSize > 0) ? (textPos * 100 / totalSize) : 0;
        
        // Time (Left)
        String timeS = getTimeString();
        writeln_scaled(timeS.c_str(), 35, footerY + 10, 0.45, false, gray);

        // Progress Text (Right)
        char progStr[32]; 
        sprintf(progStr, "%d%% completed", progress);
        int progW = get_text_width_scaled(progStr, 0.45);
        writeln_scaled(progStr, P_WIDTH - 35 - progW, footerY + 10, 0.45, false, gray);

        // Visual Progress Bar
        int barX = 35;
        int barY = footerY + 25;
        int barW = P_WIDTH - 70;
        draw_rect_rotated(barX, barY, barW, 4, gray);
        if (progress > 0) {
            fill_rect_rotated(barX, barY, (int)(barW * (progress / 100.0)), 4, black);
        }

        // Navigation Buttons (Clean Style)
        int btnY = BTN_Y_POS;
        int mbx = (P_WIDTH - (3 * BUTTON_W + 2 * BUTTON_GAP)) / 2;

        const char* btnLabels[] = {"Back", "Prev", "Next"};
        for (int i = 0; i < 3; i++) {
            int bx = mbx + i * (BUTTON_W + BUTTON_GAP);
            draw_rounded_rect(bx, btnY, BUTTON_W, BUTTON_H, 8, black);
            int tw = get_text_width_scaled(btnLabels[i], 0.45);
            writeln_scaled(btnLabels[i], bx + (BUTTON_W - tw) / 2, btnY + 33, 0.45, true, black);
        }

        prefs.putLong(getPrefKey(books[currentFileIndex]).c_str(), textPos); 
        f.close();
    }
    
    if (partial_refresh && !doFullRefresh) {
        // Fast B&W partial update - minimal ghosting
        // Use BLACK_ON_WHITE mode for faster updates
        epd_draw_image(epd_full_screen(), framebuffer, BLACK_ON_WHITE);
        partialUpdateCounter++;
        
        if (DEBUG_ON) {
            Serial.printf("[PARTIAL] Count: %d/%d\n", partialUpdateCounter, MAX_PARTIAL_BEFORE_FULL);
        }
    } else {
        // High Quality Grayscale update - eliminates ghosting
        epd_draw_grayscale_image(epd_full_screen(), framebuffer); 
        partialUpdateCounter = 0;
        
        if (DEBUG_ON) {
            Serial.println(F("[FULL] Grayscale refresh completed"));
        }
    }
    
    epd_poweroff();
}

// Alternative: Manual full refresh trigger
void forceFullRefresh() {
    partialUpdateCounter = MAX_PARTIAL_BEFORE_FULL;
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
