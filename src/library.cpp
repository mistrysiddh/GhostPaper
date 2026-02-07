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
    
    // 1. Soft Drop Shadow
    for (int i = 0; i < 3; i++) {
        draw_rounded_rect(x + 4 + i, y + 4 + i, BOOK_W, BOOK_H, 6, SHADOW_COLOR);
    }

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
    int iconSize = 60; // Reduced slightly
    int icX = x + (BOOK_W - iconSize) / 2;
    int icY = y + 70;  // Moved up
    
    // Draw book icon with pages effect
    fill_rect_rotated(icX + 3, icY + 3, iconSize - 6, iconSize + 10, COL_LIGHT);
    fill_rect_rotated(icX, icY, iconSize - 6, iconSize + 10, COL_WHITE);
    draw_rounded_rect(icX, icY, iconSize - 6, iconSize + 10, 3, COL_BLACK);
    
    // Spine
    fill_rect_rotated(icX, icY, 12, iconSize + 10, COL_BLACK);
    
    // Pages lines
    for (int i = 1; i <= 3; i++) {
        draw_line_rotated(icX + 18, icY + iconSize - 15 - (i * 8), 
                         icX + iconSize - 10, icY + iconSize - 15 - (i * 8), COL_LIGHT);
    }

    // 6. Title Text with Better Formatting
    int dotIdx = title.lastIndexOf('.');
    if (dotIdx > 0) title = title.substring(0, dotIdx);
    title.replace("_", " ");
    title.replace("-", " ");
    
    // Capitalize first letter of each word
    bool capNext = true;
    for (int i = 0; i < title.length(); i++) {
        if (capNext && title[i] >= 'a' && title[i] <= 'z') {
            title[i] = title[i] - 32;
        }
        capNext = (title[i] == ' ');
    }
    
    // Advanced Word Wrap Logic
    String lines[3] = {"", "", ""};
    int lineCount = 0;
    String words[20];
    int wordCount = 0;
    
    // Split into words
    int start = 0;
    for (int i = 0; i <= title.length(); i++) {
        if (i == title.length() || title[i] == ' ') {
            if (i > start) {
                words[wordCount++] = title.substring(start, i);
                if (wordCount >= 20) break;
            }
            start = i + 1;
        }
    }
    
    // Distribute words across lines
    String currentLine = "";
    for (int i = 0; i < wordCount; i++) {
        String test = currentLine.length() > 0 ? currentLine + " " + words[i] : words[i];
        if (test.length() <= 12 || currentLine.length() == 0) {
            currentLine = test;
        } else {
            if (lineCount < 3) lines[lineCount++] = currentLine;
            currentLine = words[i];
        }
    }
    if (currentLine.length() > 0 && lineCount < 3) {
        lines[lineCount++] = currentLine;
    }
    
    // Truncate last line if needed
    if (lineCount == 3 && lines[2].length() > 10) {
        lines[2] = lines[2].substring(0, 8) + "..";
    }

    // Draw title lines centered
    int tX = x + BOOK_W / 2;
    int tY = y + 185; // Shifted down
    for (int i = 0; i < lineCount; i++) {
        int w = get_text_width_scaled(lines[i].c_str(), 0.55); // Smaller font
        writeln_scaled(lines[i].c_str(), tX - w/2, tY + (i * 24), 0.55, true, COL_BLACK);
    }

    // 7. Enhanced Progress Indicator (Removed Progress Bar, only showing NEW badge if 0%)
    if (progress == 0) {
        int badgeX = x + BOOK_W - 50;
        int badgeY = y + BOOK_H - 25;
        fill_rect_rotated(badgeX, badgeY, 35, 16, COL_BLACK);
        writeln_scaled("NEW", badgeX + 4, badgeY + 12, 0.35, true, COL_WHITE);
    }

    char numStr[8];
    sprintf(numStr, "#%d", bookIndex + 1);
    int numW = get_text_width_scaled(numStr, 0.35);
    writeln_scaled(numStr, x + BOOK_W - numW - 15, y + 35, 0.35, true, COL_WHITE);

    // 8. Format Tag (Top Left)
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