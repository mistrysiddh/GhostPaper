#include "common.h"
#include "default_book.h"

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
    int tY = y + 175; 
    int maxW = BOOK_W - 30;
    float titleScale = 0.65; // Slightly smaller to prevent overflow (was 0.7)
    
    String line1 = title;
    String line2 = "";
    int w1 = get_text_width_scaled(line1.c_str(), titleScale);
    
    if (w1 > maxW) {
        // Find best split point (last space before limit)
        int splitIdx = -1;
        for (int j = 0; j < (int)title.length(); j++) {
            if (title[j] == ' ') {
                if (get_text_width_scaled(title.substring(0, j).c_str(), titleScale) < maxW) {
                    splitIdx = j;
                } else break;
            }
        }

        if (splitIdx != -1) {
             line1 = title.substring(0, splitIdx);
             line2 = title.substring(splitIdx + 1);
        } else {
             // Force split if no space found
             int charLimit = 12; 
             line1 = title.substring(0, charLimit);
             line2 = title.substring(charLimit);
        }
    }
    
    // Strict clipping for both lines
    if (get_text_width_scaled(line1.c_str(), titleScale) > maxW) line1 = line1.substring(0, 10) + "..";
    
    writeln_scaled(line1.c_str(), tX, tY, titleScale, true, COL_BLACK);
    if (line2.length() > 0) {
        if (get_text_width_scaled(line2.c_str(), titleScale) > maxW) line2 = line2.substring(0, 10) + "..";
        writeln_scaled(line2.c_str(), tX, tY + 28, titleScale, true, COL_BLACK);
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

void partialUpdateLibraryHeader() {
    if (appState != STATE_LIBRARY) return;

    // Header area (0, 0) to (P_WIDTH, 90)
    Rect_t area = {
        .x = (int32_t)(960 - 1 - 90),
        .y = 0,
        .width = 90,
        .height = (uint32_t)P_WIDTH
    };

    epd_poweron();
    
    // Redraw the black header background for this area in the framebuffer
    fill_rect_rotated(0, 0, P_WIDTH, 90, COL_BLACK);

    const char* header = "MY LIBRARY";
    float titleScale = 0.9;
    int hw = get_text_width_scaled(header, titleScale);
    writeln_scaled(header, (P_WIDTH - hw) / 2, 45, titleScale, true, COL_WHITE);
            
    float infoScale = 0.4;
    float batt = getBatteryVoltage();
    char infoStr[64];
    sprintf(infoStr, "%d items  |  %.2fV", (int)filteredBooks.size(), batt);
    if (!touchEnabled) {
        strcat(infoStr, "  |  [LOCKED]");
    }
    int iw = get_text_width_scaled(infoStr, infoScale);
    writeln_scaled(infoStr, (P_WIDTH - iw) / 2, 75, infoScale, false, COL_WHITE);

    epd_draw_grayscale_image(area, framebuffer);
    epd_poweroff();
}

void updateLibrary() {

    if (DEBUG_ON) Serial.println(F("[UI] Rendering Enhanced Bookshelf Library..."));

    

    // Refresh the filtered list before drawing

    applyLibraryFilter();



    epd_poweron();

    epd_clear(); 

    memset(framebuffer, COL_WHITE, L_WIDTH * L_HEIGHT / 2);



    // --- 1. Header (Centered Design) ---

    const char* header = "MY LIBRARY";

    fill_rect_rotated(0, 0, P_WIDTH, 90, COL_BLACK); 

    

    float titleScale = 0.9;

            int hw = get_text_width_scaled(header, titleScale);

            writeln_scaled(header, (P_WIDTH - hw) / 2, 45, titleScale, true, COL_WHITE);

            

            float infoScale = 0.4;

    float batt = getBatteryVoltage();

    char infoStr[64];

        sprintf(infoStr, "%d items  |  %.2fV", (int)filteredBooks.size(), batt);

        if (!touchEnabled) {

            strcat(infoStr, "  |  [LOCKED]");

        }

        int iw = get_text_width_scaled(infoStr, infoScale);

    writeln_scaled(infoStr, (P_WIDTH - iw) / 2, 75, infoScale, false, COL_WHITE);

    

    // --- 2. Filter Tabs (ALL | NEW | READING | FINISHED) ---

    int tabY = 125;

    const char* tabs[] = {"ALL", "NEW", "READING", "FINISHED"};

    int tabW = P_WIDTH / 4;

    for (int i = 0; i < 4; i++) {

        int tx = i * tabW;

        if (libraryFilter == i) {

            fill_rect_rotated(tx + 5, tabY - 25, tabW - 10, 40, COL_BLACK);

            int tw = get_text_width_scaled(tabs[i], 0.4);

            writeln_scaled(tabs[i], tx + (tabW - tw) / 2, tabY, 0.4, true, COL_WHITE);

        } else {

            draw_rounded_rect(tx + 5, tabY - 25, tabW - 10, 40, 5, COL_BLACK);

            int tw = get_text_width_scaled(tabs[i], 0.4);

            writeln_scaled(tabs[i], tx + (tabW - tw) / 2, tabY, 0.4, false, COL_BLACK);

        }

    }



    if (filteredBooks.empty()) {

        int msgY = 450;

        const char* msg = "No books in this category";

        int mw = get_text_width_scaled(msg, 0.7);

        writeln_scaled(msg, (P_WIDTH - mw)/2, msgY, 0.7, true, COL_BLACK);

    } else {

        // Adjust selection if it goes out of bounds for the filtered list

        if (librarySelection >= (int)filteredBooks.size()) librarySelection = filteredBooks.size() - 1;

        if (librarySelection < 0) librarySelection = 0;



        int page = librarySelection / SHELF_BOOKS_PER_PAGE;

        int startIdx = page * SHELF_BOOKS_PER_PAGE;

        

        // Push the shelf start lower to account for tabs

        int shelfStartY = SHELF_START_Y + 40; 



        for (int i = 0; i < SHELF_BOOKS_PER_PAGE; i++) {

            int idx = startIdx + i;

            if (idx >= (int)filteredBooks.size()) break;

            int col = i % SHELF_COLS;

            int row = i / SHELF_COLS;

            int x = GAP_X + col * (BOOK_W + GAP_X);

            int y = shelfStartY + row * (BOOK_H + GAP_Y);

            

                                    long savedPos = prefs.getLong(getPrefKey(filteredBooks[idx]).c_str(), 0);

            

                                    long totalSize = 1;

            

                                    if (filteredBooks[idx].startsWith("internal:")) {

            

                                        totalSize = strlen(DEFAULT_BOOK_TEXT);

            

                                    } else {

            

                                        File f = SD.open(filteredBooks[idx]);

            

                                        if (f) { totalSize = f.size(); f.close(); }

            

                                    }

            

                        

            

            

            int pct = (totalSize > 0) ? (savedPos * 100 / totalSize) : 0;

            

            String displayTitle = filteredBooks[idx].substring(filteredBooks[idx].lastIndexOf('/')+1);
            if (filteredBooks[idx].startsWith("internal:")) {
                displayTitle = "Echoes of the Code";
            }
            
            drawEnhancedBookCover(x, y, displayTitle, pct, idx);

            if (idx == librarySelection) {

                draw_rounded_rect(x - 6, y - 6, BOOK_W + 12, BOOK_H + 12, 10, COL_BLACK);

                draw_rounded_rect(x - 4, y - 4, BOOK_W + 8, BOOK_H + 8, 8, COL_DARK);

            }

        }

        

        // --- 3. Footer Navigation ---
        int footerY = P_HEIGHT - 55;
        int totalPages = (filteredBooks.size() + SHELF_BOOKS_PER_PAGE - 1) / SHELF_BOOKS_PER_PAGE;
        char pgStr[32];
        sprintf(pgStr, "Page %d of %d", page + 1, totalPages);
        
        // Central Page Pill
        int pillW = 160;
        int pillH = 40;
        int pillX = (P_WIDTH - pillW) / 2;
        fill_rect_rotated(pillX, footerY, pillW, pillH, COL_BLACK);
        draw_rounded_rect(pillX, footerY, pillW, pillH, 20, COL_DARK);
        int tw = get_text_width_scaled(pgStr, 0.55);
        writeln_scaled(pgStr, pillX + (pillW - tw)/2, footerY + 27, 0.55, true, COL_WHITE);

        // Visual Navigation Arrows
        if (page > 0) {
            const char* prev = "< PREV";
            writeln_scaled(prev, 30, footerY + 27, 0.5, true, COL_BLACK);
        }
        if (page < totalPages - 1) {
            const char* next = "NEXT >";
            int nw = get_text_width_scaled(next, 0.5);
            writeln_scaled(next, P_WIDTH - 30 - nw, footerY + 27, 0.5, true, COL_BLACK);
        }
    }
    epd_draw_grayscale_image(epd_full_screen(), framebuffer);
    epd_poweroff();
}



void openBook() {

    if (filteredBooks.empty() || librarySelection >= (int)filteredBooks.size()) return;

    

    // Find the actual path from the filtered list

    String path = filteredBooks[librarySelection];

    

    // Find its index in the master 'books' list for consistent reference

    for (int i=0; i < (int)books.size(); i++) {

        if (books[i] == path) {

            currentFileIndex = i;

            break;

        }

    }



    textPos = prefs.getLong(getPrefKey(books[currentFileIndex]).c_str(), 0);

    appState = STATE_READING;

    pageHistory.clear();

    epd_poweron(); epd_clear(); epd_poweroff();

    updateReader();

}
