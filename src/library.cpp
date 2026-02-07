#include "common.h"

// Color palette optimized for e-paper
#define COVER_ACCENT     COL_DARK
#define COVER_BORDER     COL_BLACK
#define COVER_BG         COL_WHITE
#define SHADOW_COLOR     0xCC  // Medium gray for subtle shadow

void drawEnhancedBookCover(int x, int y, String title, int progress, int bookIndex) {
    // Generate pseudo-random but consistent accent color based on title hash
    int hash = 0;
    for (int i = 0; i < title.length(); i++) {
        hash = (hash * 31 + title[i]) % 256;
    }
    
    // Use hash to create varied visual elements
    bool usePattern = (hash % 3) == 0;
    bool useBorder = (hash % 2) == 0;
    
    // 1. Soft Drop Shadow (Premium "Floating" Effect)
    // Draw offset shadow first so it appears behind the card
    fill_rect_rotated(x + 4, y + 4, BOOK_W, BOOK_H, COL_GRAY); // 2px equivalent shadow (4px in hi-res?) - using block for efficiency

    // 2. Main Cover Background
    fill_rect_rotated(x, y, BOOK_W, BOOK_H, COVER_BG);
    
    // 3. Decorative Border with Double-Line Effect
    draw_rounded_rect(x, y, BOOK_W, BOOK_H, 6, COVER_BORDER);
    draw_rounded_rect(x + 2, y + 2, BOOK_W - 4, BOOK_H - 4, 5, COVER_BORDER);
    
    if (useBorder) {
        draw_rounded_rect(x + 8, y + 8, BOOK_W - 16, BOOK_H - 16, 4, COL_LIGHT);
    }

    // 4. Decorative Header Bar (Top accent)
    int headerH = 50;
    fill_rect_rotated(x + 12, y + 12, BOOK_W - 24, headerH, COL_BLACK);
    
    // Add subtle pattern to header
    if (usePattern) {
        for (int py = 0; py < headerH; py += 8) {
            draw_line_rotated(x + 12, y + 12 + py, x + BOOK_W - 12, y + 12 + py, COL_LIGHT);
        }
    }

    // 5. Enhanced Book Icon
    int iconSize = 60; 
    int icX = x + (BOOK_W - iconSize) / 2;
    int icY = y + 70;  
    
    fill_rect_rotated(icX + 3, icY + 3, iconSize - 6, iconSize + 10, COL_LIGHT);
    fill_rect_rotated(icX, icY, iconSize - 6, iconSize + 10, COL_WHITE);
    draw_rounded_rect(icX, icY, iconSize - 6, iconSize + 10, 3, COL_BLACK);
    fill_rect_rotated(icX, icY, 12, iconSize + 10, COL_BLACK);
    for (int i = 1; i <= 3; i++) {
        draw_line_rotated(icX + 18, icY + iconSize - 15 - (i * 8), 
                         icX + iconSize - 10, icY + iconSize - 15 - (i * 8), COL_LIGHT);
    }

    // 6. Title Text with Clipping
    int dotIdx = title.lastIndexOf('.');
    if (dotIdx > 0) title = title.substring(0, dotIdx);
    title.replace("_", " ");
    title.replace("-", " ");
    
    // Capitalize
    bool capNext = true;
    for (int i = 0; i < title.length(); i++) {
        if (capNext && title[i] >= 'a' && title[i] <= 'z') {
            title[i] = title[i] - 32;
        }
        capNext = (title[i] == ' ');
    }
    
    // Simple clipping for cleaner look
    if (title.length() > 30) {
        title = title.substring(0, 27) + "...";
    }
    
    // Draw title (multiline if needed, but strictly clipped)
    int tX = x + 15;
    int tY = y + 180;
    int maxW = BOOK_W - 30;
    
    String line1 = title;
    String line2 = "";
    int w1 = get_text_width_scaled(line1.c_str(), 0.55);
    
    if (w1 > maxW) {
        // Find split point
        int splitIdx = title.length() / 2;
        while (splitIdx < title.length() && title[splitIdx] != ' ') splitIdx++;
        if (splitIdx < title.length()) {
             line1 = title.substring(0, splitIdx);
             line2 = title.substring(splitIdx + 1);
        } else {
             // Force split
             splitIdx = title.length() / 2;
             line1 = title.substring(0, splitIdx);
             line2 = title.substring(splitIdx);
        }
    }
    
    writeln_scaled(line1.c_str(), tX, tY, 0.55, true, COL_BLACK);
    if (line2.length() > 0) {
        if (line2.length() > 12) line2 = line2.substring(0, 10) + "...";
        writeln_scaled(line2.c_str(), tX, tY + 24, 0.55, true, COL_BLACK);
    }

    // 7. Visual Progress Bar
    int barW = 40;
    int barH = 6;
    int barX = x + (BOOK_W - barW) / 2;
    int barY = y + BOOK_H - 25;
    
    draw_rect_rotated(barX, barY, barW, barH, COL_BLACK);
    if (progress > 0) {
        int fillW = (progress * barW) / 100;
        if (fillW < 1 && progress > 0) fillW = 1;
        fill_rect_rotated(barX, barY, fillW, barH, COL_BLACK);
    }

    char numStr[8];
    sprintf(numStr, "#%d", bookIndex + 1);
    int numW = get_text_width_scaled(numStr, 0.35);
    writeln_scaled(numStr, x + BOOK_W - numW - 15, y + 35, 0.35, true, COL_WHITE);

    // 8. Format Tag
    String fullPath = books[bookIndex];
    String ext = fullPath.substring(fullPath.lastIndexOf('.') + 1);
    ext.toUpperCase();
    if (ext == "TXT") {
        int tagW = get_text_width_scaled(ext.c_str(), 0.35) + 10;
        fill_rect_rotated(x + 12, y + 12, tagW, 20, COL_WHITE);
        draw_rounded_rect(x + 12, y + 12, tagW, 20, 3, COL_BLACK);
        writeln_scaled(ext.c_str(), x + 17, y + 26, 0.35, true, COL_BLACK);
    }
}

void updateLibrary() {
    if (DEBUG_ON) Serial.println(F("[UI] Rendering Enhanced Bookshelf Library..."));
    
    epd_poweron();
    epd_clear(); 
    memset(framebuffer, COL_WHITE, L_WIDTH * L_HEIGHT / 2);

    const char* header = "MY LIBRARY";
    int headerY = 65;
    fill_rect_rotated(0, 0, P_WIDTH, 100, COL_BLACK);
    int hw = get_text_width_scaled(header, 1.2);
    writeln_scaled(header, (P_WIDTH - hw) / 2, headerY, 1.2, true, COL_WHITE);
    
    // Header Separator Line
    draw_line_rotated(0, 100, P_WIDTH, 100, COL_BLACK); // Full width line below header
    
    int lineY = 85;
    int lineLen = 200;
    draw_line_rotated((P_WIDTH - lineLen) / 2, lineY, (P_WIDTH + lineLen) / 2, lineY, COL_WHITE);
    
    float batt = getBatteryVoltage();
    char battStr[16]; sprintf(battStr, "%.2fV", batt);
    int battW = get_text_width_scaled(battStr, 0.6);
    writeln_scaled(battStr, P_WIDTH - 30 - battW, headerY, 0.6, true, COL_WHITE);
    
    if (!books.empty()) {
        char countStr[32];
        sprintf(countStr, "%d books", books.size());
        writeln_scaled(countStr, 35, headerY, 0.5, false, COL_WHITE);
    }

    if (books.empty()) {
        int msgY = 350;
        int emptyIconSize = 100;
        int emptyIconX = (P_WIDTH - emptyIconSize) / 2;
        int emptyIconY = msgY - 150;
        fill_rect_rotated(emptyIconX, emptyIconY, emptyIconSize, emptyIconSize + 20, COL_LIGHT);
        draw_rounded_rect(emptyIconX, emptyIconY, emptyIconSize, emptyIconSize + 20, 5, COL_DARK);
        fill_rect_rotated(emptyIconX, emptyIconY, 18, emptyIconSize + 20, COL_DARK);
        const char* msg = "Your Library is Empty";
        const char* sub = "Add .txt files to your SD card";
        int mw = get_text_width_scaled(msg, 0.9);
        int sw = get_text_width_scaled(sub, 0.6);
        writeln_scaled(msg, (P_WIDTH - mw)/2, msgY, 0.9, true, COL_BLACK);
        writeln_scaled(sub, (P_WIDTH - sw)/2, msgY + 45, 0.6, false, COL_DARK);
    } else {
        int page = librarySelection / SHELF_BOOKS_PER_PAGE;
        int startIdx = page * SHELF_BOOKS_PER_PAGE;
        for (int i = 0; i < SHELF_BOOKS_PER_PAGE; i++) {
            int idx = startIdx + i;
            if (idx >= (int)books.size()) break;
            int col = i % SHELF_COLS;
            int row = i / SHELF_COLS;
            int x = GAP_X + col * (BOOK_W + GAP_X);
            int y = SHELF_START_Y + row * (BOOK_H + GAP_Y);
            long savedPos = prefs.getLong(getPrefKey(books[idx]).c_str(), 0);
            long totalSize = 1;
            File f = SD.open(books[idx]);
            if (f) { totalSize = f.size(); f.close(); }
            int pct = (totalSize > 0) ? (savedPos * 100 / totalSize) : 0;
            drawEnhancedBookCover(x, y, books[idx].substring(books[idx].lastIndexOf('/')+1), pct, idx);
            if (idx == librarySelection) {
                draw_rounded_rect(x - 6, y - 6, BOOK_W + 12, BOOK_H + 12, 10, COL_BLACK);
                draw_rounded_rect(x - 5, y - 5, BOOK_W + 10, BOOK_H + 10, 9, COL_BLACK);
                draw_rounded_rect(x - 4, y - 4, BOOK_W + 8, BOOK_H + 8, 8, COL_DARK);
            }
        }
        int footerY = P_HEIGHT - 55;
        int totalPages = (books.size() + SHELF_BOOKS_PER_PAGE - 1) / SHELF_BOOKS_PER_PAGE;
        char pgStr[32];
        sprintf(pgStr, "Page %d of %d", page + 1, totalPages);
        int pillW = 160;
        int pillH = 40;
        int pillX = (P_WIDTH - pillW) / 2;
        fill_rect_rotated(pillX, footerY, pillW, pillH, COL_BLACK);
        draw_rounded_rect(pillX, footerY, pillW, pillH, 20, COL_DARK);
        int tw = get_text_width_scaled(pgStr, 0.55);
        writeln_scaled(pgStr, pillX + (pillW - tw)/2, footerY + 27, 0.55, true, COL_WHITE);
    }
    epd_draw_grayscale_image(epd_full_screen(), framebuffer);
    epd_poweroff();
}

void openBook() {
    if (books.empty() || librarySelection >= (int)books.size()) return;
    currentFileIndex = librarySelection;
    textPos = prefs.getLong(getPrefKey(books[currentFileIndex]).c_str(), 0);
    appState = STATE_READING;
    pageHistory.clear();
    epd_poweron(); epd_clear(); epd_poweroff();
    updateReader();
}