#include "store.h"
#include "opds_client.h"
#include "common.h"
#include <WiFi.h>

// Helper to generate consistent filenames
String getSanitizedFilename(String title) {
    String name = "/" + title + ".txt";
    name.replace(" ", "_");
    name.replace(";", "");
    name.replace(":", "");
    name.replace(",", "");
    return name;
}

// --- Store State ---
std::vector<OpdsEntry> storeCatalog;
int storeScrollOffset = 0;
String downloadStatus = "";

OpdsClient opds;
String OPDS_URL = "https://www.gutenberg.org/ebooks/search.opds/?sort_order=downloads"; 

void drawProgressBar(int percent) {
    int footerY = P_HEIGHT - 90;
    int barW = P_WIDTH - 120;
    int barH = 10;
    int barX = 60;
    int barY = footerY + 10; 

    epd_poweron();
    // Targeted wash
    Rect_t area = {
        .x = (int32_t)(960 - 1 - (barY + barH)),
        .y = (int32_t)barX,
        .width = (int32_t)barH,
        .height = (int32_t)barW
    };
    epd_push_pixels(area, 40, 1); 
    
    fill_rect_rotated(barX, barY, barW, barH, COL_WHITE);
    draw_rect_rotated(barX, barY, barW, barH, COL_BLACK);
    int fillW = (barW * percent) / 100;
    if (fillW > 0) fill_rect_rotated(barX, barY, fillW, barH, COL_BLACK);
    
    epd_draw_grayscale_image(area, framebuffer);
    epd_poweroff();
}

void syncStore() {
    if (WiFi.status() != WL_CONNECTED) {
        String savedSSID = prefs.getString("wifi_ssid", WIFI_SSID_1);
        String savedPass = prefs.getString("wifi_pass", WIFI_PASS_1);
        
        Serial.printf("Store: Connecting to %s...\n", savedSSID.c_str());
        WiFi.begin(savedSSID.c_str(), savedPass.c_str());
        
        int retries = 0;
        while (WiFi.status() != WL_CONNECTED && retries < 20) {
            delay(500);
            retries++;
        }
    }

    if (WiFi.status() == WL_CONNECTED) {
        epd_poweron(); epd_clear();
        int cx = P_WIDTH/2; int cy = P_HEIGHT/2;
        fill_rect_rotated(cx - 150, cy - 40, 300, 80, COL_WHITE);
        draw_rounded_rect(cx - 150, cy - 40, 300, 80, 10, COL_BLACK);
        writeln_scaled("Updating Feed...", cx - 90, cy + 10, 0.6, true, COL_BLACK);
        epd_draw_grayscale_image(epd_full_screen(), framebuffer);
        epd_poweroff();

        storeCatalog = opds.fetchCatalog(OPDS_URL);
        storeScrollOffset = 0;
        downloadStatus = storeCatalog.empty() ? opds.getLastError() : "";
    } else {
        downloadStatus = "WiFi Failed";
    }
}

void drawStoreItem(int index, int y) {
    if (index >= (int)storeCatalog.size()) return;
    
    OpdsEntry& item = storeCatalog[index];
    int cardH = 140; // Slightly shorter
    int cardW = P_WIDTH - 30;
    int x = 15;
    
    draw_rect_rotated(x, y, cardW, cardH, COL_DARK);
    
    float titleScale = 0.65;
    int maxTextW = cardW - 150; 
    
    String line1 = item.title;
    String line2 = "";
    
    if (get_text_width_scaled(line1.c_str(), titleScale) > maxTextW) {
        int splitIdx = -1;
        for (int j = 0; j < (int)line1.length(); j++) {
            if (line1[j] == ' ') {
                if (get_text_width_scaled(line1.substring(0, j).c_str(), titleScale) < maxTextW) splitIdx = j;
                else break;
            }
        }
        if (splitIdx != -1) {
            line2 = line1.substring(splitIdx + 1);
            line1 = line1.substring(0, splitIdx);
        }
    }

    writeln_scaled(line1.c_str(), x + 15, y + 40, titleScale, true, COL_BLACK);
    int authorY = y + 80;
    if (line2 != "") {
        if (get_text_width_scaled(line2.c_str(), titleScale) > maxTextW) line2 = line2.substring(0, 22) + "...";
        writeln_scaled(line2.c_str(), x + 15, y + 70, titleScale, true, COL_BLACK);
        authorY = y + 105;
    }
    
    String author = (item.author != "") ? item.author : "Gutenberg Author";
    if (author.length() > 35) author = author.substring(0, 32) + "...";
    writeln_scaled(author.c_str(), x + 15, authorY, 0.45, false, COL_GRAY);
    
    int btnW = 110;
    int btnH = 45;
    int btnX = x + cardW - btnW - 15;
    int btnY = y + (cardH - btnH) / 2;
    
    String targetFile = getSanitizedFilename(item.title);
    bool alreadyDownloaded = false;
    for (auto& b : books) if (b == targetFile) alreadyDownloaded = true;

    if (alreadyDownloaded) {
        draw_rounded_rect(btnX, btnY, btnW, btnH, 8, COL_DARK);
        const char* dLabel = "READ";
        int dw = get_text_width_scaled(dLabel, 0.5);
        writeln_scaled(dLabel, btnX + (btnW - dw)/2, btnY + 31, 0.5, true, COL_WHITE);
    } else {
        draw_rounded_rect(btnX, btnY, btnW, btnH, 8, COL_BLACK);
        const char* dLabel = "GET";
        int dw = get_text_width_scaled(dLabel, 0.5);
        writeln_scaled(dLabel, btnX + (btnW - dw)/2, btnY + 31, 0.5, true, COL_BLACK);
    }
}

void updateStore() {
    epd_poweron();
    epd_clear();
    memset(framebuffer, COL_WHITE, L_WIDTH * L_HEIGHT / 2);
    
    fill_rect_rotated(0, 0, P_WIDTH, 85, COL_BLACK);
    const char* title = "GhostStore";
    writeln_scaled(title, 25, 55, 0.85, true, COL_WHITE);
    
    // Back Button
    int bbtnW = 80, bbtnH = 40, bbtnX = P_WIDTH - 100, bbtnY = 20;
    draw_rounded_rect(bbtnX, bbtnY, bbtnW, bbtnH, 5, COL_WHITE);
    int btw = get_text_width_scaled("BACK", 0.4);
    writeln_scaled("BACK", bbtnX + (bbtnW - btw) / 2, bbtnY + 28, 0.4, true, COL_WHITE);
    
    char status[64];
    if (downloadStatus != "") strncpy(status, downloadStatus.c_str(), 63);
    else sprintf(status, "%d Books Online", (int)storeCatalog.size());
    int sw = get_text_width_scaled(status, 0.4);
    writeln_scaled(status, P_WIDTH - sw - 120, 52, 0.4, false, COL_WHITE); // Moved status left to accommodate BACK button

    int startY = 95;
    int itemH = 145; // Tighter layout
    int itemsPerPage = 5;
    
    for (int i = 0; i < itemsPerPage; i++) {
        int idx = storeScrollOffset + i;
        if (idx < (int)storeCatalog.size()) drawStoreItem(idx, startY + i * itemH);
    }
    
    int footerY = P_HEIGHT - 90;
    fill_rect_rotated(0, footerY, P_WIDTH, 90, COL_WHITE);
    draw_line_rotated(0, footerY, P_WIDTH, footerY, COL_BLACK);

    int totalPages = (storeCatalog.size() + itemsPerPage - 1) / itemsPerPage;
    int currentPage = (storeScrollOffset / itemsPerPage) + 1;
    if (totalPages > 0) {
        char pgStr[32];
        sprintf(pgStr, "%d / %d", currentPage, totalPages);
        int ptw = get_text_width_scaled(pgStr, 0.5);
        writeln_scaled(pgStr, (P_WIDTH - ptw)/2, footerY + 60, 0.5, true, COL_BLACK);
    }

    if (storeScrollOffset > 0) writeln_scaled("< BACK", 30, footerY + 60, 0.5, true, COL_BLACK);
    if (storeScrollOffset + itemsPerPage < (int)storeCatalog.size()) {
        int nw = get_text_width_scaled("NEXT >", 0.5);
        writeln_scaled("NEXT >", P_WIDTH - 30 - nw, footerY + 60, 0.5, true, COL_BLACK);
    }
    
    if (opds.downloadProgress >= 0) {
        int barW = P_WIDTH - 120;
        int barH = 10;
        int barX = 60;
        int barY = footerY + 10;
        draw_rect_rotated(barX, barY, barW, barH, COL_BLACK);
        int fillW = (barW * opds.downloadProgress) / 100;
        if (fillW > 0) fill_rect_rotated(barX, barY, fillW, barH, COL_BLACK);
    }

    epd_draw_grayscale_image(epd_full_screen(), framebuffer);
    epd_poweroff();
}

void handleStoreTouch(int x, int y) {
    if (y < 85) {
        if (x > P_WIDTH - 110) {
            WiFi.disconnect(true); WiFi.mode(WIFI_OFF);
            appState = STATE_LIBRARY; showTransitionEffect(); updateLibrary();
        } else {
            syncStore(); updateStore();
        }
        return;
    }
    if (y > P_HEIGHT - 90) {
        if (x < P_WIDTH * 0.3 && storeScrollOffset >= 5) {
            storeScrollOffset -= 5; updateStore();
        } else if (x > P_WIDTH * 0.7 && storeScrollOffset + 5 < (int)storeCatalog.size()) {
            storeScrollOffset += 5; updateStore();
        } else if (x > P_WIDTH * 0.3 && x < P_WIDTH * 0.7) {
            WiFi.disconnect(true); WiFi.mode(WIFI_OFF);
            appState = STATE_LIBRARY; showTransitionEffect(); updateLibrary();
        }
        return;
    }
    
    int startY = 95;
    int itemH = 145;
    for (int i = 0; i < 5; i++) {
        int itemY = startY + i * itemH;
        if (y >= itemY && y < itemY + 140) {
            if (x > P_WIDTH - 150) {
                int idx = storeScrollOffset + i;
                if (idx < (int)storeCatalog.size()) {
                    OpdsEntry& item = storeCatalog[idx];
                    String fileName = getSanitizedFilename(item.title);
                    int foundIdx = -1;
                    for (int j=0; j < (int)books.size(); j++) if (books[j] == fileName) { foundIdx = j; break; }

                    if (foundIdx != -1) {
                        WiFi.disconnect(true); WiFi.mode(WIFI_OFF);
                        currentFileIndex = foundIdx;
                        textPos = prefs.getLong(getPrefKey(books[currentFileIndex]).c_str(), 0);
                        appState = STATE_READING;
                        pageHistory.clear();
                        epd_poweron(); epd_clear(); epd_poweroff();
                        showTransitionEffect(); updateReader();
                        return;
                    }

                    downloadStatus = "Resolving..."; updateStore();
                    String finalUrl = item.downloadUrl;
                    if (finalUrl.startsWith("resolve:")) finalUrl = opds.resolveBookUrl(finalUrl.substring(8));
                    if (finalUrl == "" || finalUrl.startsWith("resolve:")) {
                        downloadStatus = "Format Error"; updateStore(); return;
                    }

                    downloadStatus = "Downloading..."; updateStore();
                    bool success = opds.downloadBook(finalUrl, fileName, [](int pct) { drawProgressBar(pct); });
                    if (success) {
                        downloadStatus = "Done!";
                        bool exists = false;
                        for(auto& b : books) if(b == fileName) exists = true;
                        if(!exists) books.push_back(fileName);
                    } else {
                        downloadStatus = "Failed";
                    }
                    updateStore(); 
                }
                return;
            }
        }
    }
}
