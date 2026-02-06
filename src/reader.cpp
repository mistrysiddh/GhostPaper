#include "common.h"

void updateReader() {
    if (DEBUG_ON) Serial.println(F("[UI] Rendering Reader Page..."));
    epd_poweron();
    epd_clear(); 
    memset(framebuffer, COL_BG, L_WIDTH * L_HEIGHT / 2);
    uint8_t black = COL_BLACK, gray = COL_GRAY;

    if (books.empty() || currentFileIndex < 0 || currentFileIndex >= (int)books.size()) return;

    File f = SD.open(books[currentFileIndex]);
            if (f) {
                String title = books[currentFileIndex].substring(books[currentFileIndex].lastIndexOf('/') + 1);
                int dotIdx = title.lastIndexOf('.');
                if (dotIdx > 0) title = title.substring(0, dotIdx);
                
                // Replace underscores with spaces
                title.replace("_", " ");
                
                if (title.length() > 30) title = title.substring(0, 27) + "...";            writeln_scaled(title.c_str(), 40, 45, 0.4, false, gray);
            
            int bx = P_WIDTH - 70, by = 30;
            draw_rect_rotated(bx, by, 30, 14, black);
            draw_line_rotated(bx + 30, by + 4, bx + 30, by + 10, black);
    
            String filename = books[currentFileIndex];
            filename.toLowerCase();
            
            if (filename.endsWith(".pdf")) {
                const char* msg = "PDF Viewer Not Supported";
                writeln_scaled(msg, (P_WIDTH - get_text_width_scaled(msg, 0.8))/2, 400, 0.8, true, COL_BLACK);
            } else {
                f.seek(textPos);
                const int bufSize = 5001; char *buf = (char*)malloc(bufSize);
                if (buf) {
                    int r = f.readBytes(buf, bufSize - 1); buf[r] = '\0';
                    lastPageByteCount = renderPage(buf, 40, 140, P_WIDTH - 40, P_HEIGHT - READER_MENU_HEIGHT - 20); 
                    free(buf);
                }
            }
    
            // --- Bottom Menu ---
            int mbx = (P_WIDTH - (3 * BUTTON_W + 2 * BUTTON_GAP)) / 2; // Center the group
            int mby = BTN_Y_POS;

            // Back Button
            draw_rounded_rect(mbx, mby, BUTTON_W, BUTTON_H, 10, COL_BLACK);
            writeln_scaled("Back", mbx + (BUTTON_W - get_text_width_scaled("Back", 0.4))/2, mby + 32, 0.4, false, COL_BLACK);
            
            // Prev Button
            draw_rounded_rect(mbx + BUTTON_W + BUTTON_GAP, mby, BUTTON_W, BUTTON_H, 10, COL_BLACK);
            writeln_scaled("Prev", mbx + BUTTON_W + BUTTON_GAP + (BUTTON_W - get_text_width_scaled("Prev", 0.4))/2, mby + 32, 0.4, false, COL_BLACK);

            // Next Button
            draw_rounded_rect(mbx + 2 * (BUTTON_W + BUTTON_GAP), mby, BUTTON_W, BUTTON_H, 10, COL_BLACK);
            writeln_scaled("Next", mbx + 2 * (BUTTON_W + BUTTON_GAP) + (BUTTON_W - get_text_width_scaled("Next", 0.4))/2, mby + 32, 0.4, false, COL_BLACK);

        long totalSize = f.size();
        int progress = (totalSize > 0) ? (textPos * 100 / totalSize) : 0;
        char progStr[16]; sprintf(progStr, "%d%%", progress);
        writeln_scaled(progStr, (P_WIDTH - get_text_width_scaled(progStr, 0.4)) / 2, 930, 0.4, false, black);
        
        String timeS = getTimeString();
        writeln_scaled(timeS.c_str(), P_WIDTH - 40 - get_text_width_scaled(timeS.c_str(), 0.4), 930, 0.4, false, black);

        prefs.putLong(getPrefKey(books[currentFileIndex]).c_str(), textPos); 
        f.close();
    }
    epd_draw_grayscale_image(epd_full_screen(), framebuffer); epd_poweroff();
}

long renderPage(const char* text, int startX, int startY, int maxWidth, int maxHeight) {
    int curX = startX; int curY = startY; const char* p = text;
    int lineHeight = (int)(52 * fontScale); 
    while (*p) {
        if (*p == '\r') { p++; continue; }
        if (*p == '\n') {
            curX = startX; curY += lineHeight;
            if (*(p+1) == '\n') { curY += lineHeight / 2; p++; }
            p++; if (curY > maxHeight) break; continue;
        }
        const char* next_p = p;
        while (*next_p && *next_p != ' ' && *next_p != '\n' && *next_p != '\r') decode_utf8(&next_p);
        int wordLen = next_p - p; 
        if(wordLen > 0) {
            char word[128]; if(wordLen > 127) wordLen = 127;
            strncpy(word, p, wordLen); word[wordLen] = '\0';
            int tW = get_text_width_scaled(word, fontScale);
            if (curX + tW > maxWidth) { curX = startX; curY += lineHeight; }
            if (curY > maxHeight) break;
            writeln_scaled(word, curX, curY, fontScale, false, 0x00);
            curX += tW;
        }
        p = next_p; 
        if (*p == ' ') { curX += (int)(12 * fontScale); p++; }
    }
    return (p - text);
}