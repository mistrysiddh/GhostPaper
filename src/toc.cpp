#include "common.h"

int tocScrollOffset = 0;

void updateTOC() {
    epd_poweron();
    epd_clear();
    memset(framebuffer, COL_BG, L_WIDTH * L_HEIGHT / 2);
    
    // Header
    fill_rect_rotated(0, 0, P_WIDTH, 85, COL_BLACK);
    const char* title = "Table of Contents";
    int tw = get_text_width_scaled(title, 0.8);
    writeln_scaled(title, (P_WIDTH - tw)/2, 55, 0.8, true, COL_WHITE);
    
    // Back Button
    int bbtnW = 80, bbtnH = 40, bbtnX = P_WIDTH - 100, bbtnY = 20;
    draw_rounded_rect(bbtnX, bbtnY, bbtnW, bbtnH, 5, COL_WHITE);
    int btw = get_text_width_scaled("BACK", 0.4);
    writeln_scaled("BACK", bbtnX + (bbtnW - btw) / 2, bbtnY + 28, 0.4, true, COL_WHITE);

    int startY = 110;
    int itemH = 75;
    int itemsPerPage = 10;
    
    if (chapters.empty()) {
        writeln_scaled("No chapters found.", 50, 200, 0.5, false, COL_BLACK);
    } else {
        for (int i = 0; i < itemsPerPage; i++) {
            int idx = tocScrollOffset + i;
            if (idx >= (int)chapters.size()) break;
            
            int y = startY + i * itemH;
            draw_rect_rotated(15, y, P_WIDTH - 30, itemH - 5, COL_GRAY);
            
            String label = chapters[idx].title;
            writeln_scaled(label.c_str(), 35, y + 45, 0.55, true, COL_BLACK);
            
            // Percentage of book
            long totalSize = 0;
            String path = books[currentFileIndex];
            if (path.startsWith("internal:")) totalSize = 25000;
            else {
                File f = SD.open(path);
                if (f) { totalSize = f.size(); f.close(); }
            }
            if (totalSize > 0) {
                int pct = chapters[idx].offset * 100 / totalSize;
                char pStr[16]; sprintf(pStr, "%d%%", pct);
                int pw = get_text_width_scaled(pStr, 0.4);
                writeln_scaled(pStr, P_WIDTH - 50 - pw, y + 45, 0.4, false, COL_DARK);
            }
        }
    }
    
    // Footer Navigation
    int footerY = P_HEIGHT - 80;
    draw_line_rotated(0, footerY, P_WIDTH, footerY, COL_BLACK);
    
    if (tocScrollOffset > 0) {
        writeln_scaled("< PREV", 40, footerY + 50, 0.5, true, COL_BLACK);
    }
    
    if (tocScrollOffset + itemsPerPage < (int)chapters.size()) {
        int nw = get_text_width_scaled("NEXT >", 0.5);
        writeln_scaled("NEXT >", P_WIDTH - 40 - nw, footerY + 50, 0.5, true, COL_BLACK);
    }

    epd_draw_grayscale_image(epd_full_screen(), framebuffer);
    epd_poweroff();
}

void handleTOCTouch(int x, int y) {
    if (y < 85) { // Header
        if (x > P_WIDTH - 110) {
            appState = STATE_READING;
            updateReader(false);
        }
        return;
    }
    
    if (y > P_HEIGHT - 80) { // Footer
        if (x < P_WIDTH / 2) {
            if (tocScrollOffset >= 10) {
                tocScrollOffset -= 10;
                updateTOC();
            }
        } else {
            if (tocScrollOffset + 10 < (int)chapters.size()) {
                tocScrollOffset += 10;
                updateTOC();
            }
        }
        return;
    }
    
    // Items
    int startY = 110;
    int itemH = 75;
    int itemsPerPage = 10;
    
    for (int i = 0; i < itemsPerPage; i++) {
        int itemY = startY + i * itemH;
        if (y >= itemY && y < itemY + itemH - 5) {
            int idx = tocScrollOffset + i;
            if (idx < (int)chapters.size()) {
                textPos = chapters[idx].offset;
                pageHistory.clear();
                showTransitionEffect();
                appState = STATE_READING;
                updateReader(false);
            }
            return;
        }
    }
}
