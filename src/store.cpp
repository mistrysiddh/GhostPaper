#include "store.h"
#include "opds_client.h"
#include <WiFi.h>

// --- Store State ---
std::vector<OpdsEntry> storeCatalog;
int storeScrollOffset = 0;
bool isDownloading = false;
String downloadStatus = "";

OpdsClient opds;
String OPDS_URL = "https://www.gutenberg.org/ebooks/search.opds/?sort_order=downloads"; 

void syncStore() {
    Serial.println("Store: Syncing...");
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("Store: Connecting to WiFi...");
        WiFi.begin(WIFI_SSID_1, WIFI_PASS_1);
        int retries = 0;
        while (WiFi.status() != WL_CONNECTED && retries < 20) {
            delay(500);
            retries++;
        }
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("Store: WiFi Connected");
        downloadStatus = "Syncing Catalog...";
        // Render status
        epd_poweron();
        int cx = P_WIDTH/2; int cy = P_HEIGHT/2;
        fill_rect_rotated(cx - 150, cy - 40, 300, 80, COL_WHITE);
        draw_rounded_rect(cx - 150, cy - 40, 300, 80, 10, COL_BLACK);
        writeln_scaled("Fetching Catalog...", cx - 100, cy + 10, 0.6, true, COL_BLACK);
        epd_draw_grayscale_image(epd_full_screen(), framebuffer);
        epd_poweroff();

        storeCatalog = opds.fetchCatalog(OPDS_URL);
        storeScrollOffset = 0;
        
        if (storeCatalog.empty()) {
            downloadStatus = opds.getLastError();
            if (downloadStatus == "") downloadStatus = "No .txt files found";
        } else {
            downloadStatus = "";
        }
    } else {
        downloadStatus = "WiFi Connect Failed";
    }
}

void drawStoreItem(int index, int y) {
    if (index >= storeCatalog.size()) return;
    
    OpdsEntry& item = storeCatalog[index];
    int cardH = 135;
    int cardW = P_WIDTH - 40;
    int x = 20;
    
    draw_rect_rotated(x, y, cardW, cardH, COL_LIGHT);
    
    String title = item.title;
    if (title.length() > 40) title = title.substring(0, 37) + "...";
    writeln_scaled(title.c_str(), x + 15, y + 45, 0.65, true, COL_BLACK);
    
    String author = (item.author != "") ? item.author : "Unknown Author";
    if (author.length() > 45) author = author.substring(0, 42) + "...";
    writeln_scaled(author.c_str(), x + 15, y + 85, 0.45, false, COL_DARK);
    
    int btnW = 110;
    int btnH = 45;
    int btnX = x + cardW - btnW - 15;
    int btnY = y + cardH - btnH - 15;
    
    draw_rounded_rect(btnX, btnY, btnW, btnH, 5, COL_BLACK);
    const char* dLabel = "GET";
    int dw = get_text_width_scaled(dLabel, 0.5);
    writeln_scaled(dLabel, btnX + (btnW - dw)/2, btnY + 31, 0.5, true, COL_BLACK);
}

void updateStore() {
    epd_poweron();
    epd_clear(); // High quality full refresh to eliminate ghosting
    memset(framebuffer, COL_WHITE, L_WIDTH * L_HEIGHT / 2);
    
    // Header
    fill_rect_rotated(0, 0, P_WIDTH, 90, COL_BLACK);
    const char* title = "GHOSTSTORE";
    int tw = get_text_width_scaled(title, 0.9);
    writeln_scaled(title, (P_WIDTH - tw)/2, 45, 0.9, true, COL_WHITE);
    
    char status[64];
    if (downloadStatus != "") {
        strncpy(status, downloadStatus.c_str(), 63);
    } else {
        sprintf(status, "%d books found", (int)storeCatalog.size());
    }
    int sw = get_text_width_scaled(status, 0.4);
    writeln_scaled(status, (P_WIDTH - sw)/2, 75, 0.4, false, COL_WHITE);
    
    draw_rounded_rect(P_WIDTH - 100, 20, 80, 45, 5, COL_WHITE);
    writeln_scaled("SYNC", P_WIDTH - 88, 50, 0.35, true, COL_WHITE);

    // List
    int startY = 110;
    int itemH = 145; 
    int itemsPerPage = 5;
    
    if (storeCatalog.empty() && downloadStatus == "") {
        const char* emptyMsg = "SYNCING CATALOG...";
        int ew = get_text_width_scaled(emptyMsg, 0.6);
        writeln_scaled(emptyMsg, (P_WIDTH - ew)/2, 400, 0.6, true, COL_GRAY);
    } else {
        for (int i = 0; i < itemsPerPage; i++) {
            int idx = storeScrollOffset + i;
            if (idx < storeCatalog.size()) {
                drawStoreItem(idx, startY + i * itemH);
            }
        }
    }
    
    // Footer - Moved UP
    int footerY = P_HEIGHT - 110; 
    fill_rect_rotated(0, footerY - 20, P_WIDTH, 130, COL_WHITE);
    draw_line_rotated(20, footerY - 20, P_WIDTH - 20, footerY - 20, COL_BLACK);

    int totalPages = (storeCatalog.size() + itemsPerPage - 1) / itemsPerPage;
    int currentPage = (storeScrollOffset / itemsPerPage) + 1;
    char pgStr[32];
    if (totalPages > 0) sprintf(pgStr, "Page %d of %d", currentPage, totalPages);
    else sprintf(pgStr, "Gutenberg Feed");
    
    int ptw = get_text_width_scaled(pgStr, 0.5);
    writeln_scaled(pgStr, (P_WIDTH - ptw)/2, footerY + 25, 0.5, true, COL_BLACK);

    if (storeScrollOffset > 0) {
        writeln_scaled("< PREV", 30, footerY + 25, 0.5, true, COL_BLACK);
    }
    if (storeScrollOffset + itemsPerPage < (int)storeCatalog.size()) {
        int nw = get_text_width_scaled("NEXT >", 0.5);
        writeln_scaled("NEXT >", P_WIDTH - 30 - nw, footerY + 25, 0.5, true, COL_BLACK);
    }
    
    int homeW = 140;
    int homeX = (P_WIDTH - homeW) / 2;
    draw_rounded_rect(homeX, footerY + 55, homeW, 35, 15, COL_BLACK);
    int hw = get_text_width_scaled("EXIT", 0.4);
    writeln_scaled("EXIT", homeX + (homeW - hw)/2, footerY + 78, 0.4, true, COL_BLACK);

    epd_draw_grayscale_image(epd_full_screen(), framebuffer);
    epd_poweroff();
}

void handleStoreTouch(int x, int y) {
    if (DEBUG_ON) Serial.printf("StoreTouch: (%d,%d)\n", x, y);

    if (y < 90 && x > P_WIDTH - 120) {
        syncStore();
        updateStore();
        return;
    }
    
    if (y > P_HEIGHT - 140) { // Threshold adjusted for footer being higher
        if (x < P_WIDTH * 0.3) {
            if (storeScrollOffset >= 5) {
                storeScrollOffset -= 5;
                updateStore();
            }
            return;
        }
        if (x > P_WIDTH * 0.7) {
            if (storeScrollOffset + 5 < (int)storeCatalog.size()) {
                storeScrollOffset += 5;
                updateStore();
            }
            return;
        }
        if (x > P_WIDTH * 0.3 && x < P_WIDTH * 0.7 && y > P_HEIGHT - 65) {
            WiFi.disconnect(true);
            WiFi.mode(WIFI_OFF);
            appState = STATE_LIBRARY;
            showTransitionEffect();
            updateLibrary();
            return;
        }
    }
    
    int startY = 110;
    int itemH = 145;
    for (int i = 0; i < 5; i++) {
        int itemY = startY + i * itemH;
        if (y >= itemY && y < itemY + 135) {
            if (x > P_WIDTH - 150) {
                int idx = storeScrollOffset + i;
                if (idx < (int)storeCatalog.size()) {
                    OpdsEntry& item = storeCatalog[idx];
                    downloadStatus = "Preparing " + item.title + "...";
                    updateStore(); 
                    
                    String finalUrl = item.downloadUrl;
                    if (finalUrl.startsWith("resolve:")) {
                        finalUrl = opds.resolveBookUrl(finalUrl.substring(8));
                    }

                    if (finalUrl == "" || finalUrl.startsWith("resolve:")) {
                        downloadStatus = "Format not available";
                        updateStore();
                        return;
                    }

                    downloadStatus = "Downloading...";
                    updateStore();

                    String fileName = "/" + item.title + ".txt";
                    fileName.replace(" ", "_");
                    fileName.replace(";", "");
                    fileName.replace(":", "");
                    
                    if (opds.downloadBook(finalUrl, fileName)) {
                        downloadStatus = "Done! Saved to SD.";
                        bool exists = false;
                        for(auto& b : books) if(b == fileName) exists = true;
                        if(!exists) books.push_back(fileName);
                    } else {
                        downloadStatus = "Download Failed";
                    }
                    updateStore();
                }
                return;
            }
        }
    }
}
