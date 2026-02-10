#include "dashboard.h"
#include "store.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

// Live Data
String weatherDesc = "Clear Sky";
String tempStr = "24C";
String onlineQuote = "Books are a uniquely portable magic.";
String onlineAuthor = "Stephen King";
unsigned long lastFetchTime = 0;
bool dashboardDataReady = false;
bool isFetchingData = false;
const unsigned long FETCH_INTERVAL = 1800000; // 30 minutes in ms

void taskFetchData(void * parameter) {
    isFetchingData = true;
    Serial.println(F("BG TASK: Starting Dashboard Sync..."));

    if (WiFi.status() != WL_CONNECTED) {
        WiFi.begin(WIFI_SSID_1, WIFI_PASS_1);
        int retries = 0;
        while (WiFi.status() != WL_CONNECTED && retries < 15) { 
            vTaskDelay(500 / portTICK_PERIOD_MS); 
            retries++; 
        }
    }

    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        WiFiClientSecure client;
        client.setInsecure();
        client.setTimeout(5000); 
        
        // 1. Weather
        if (http.begin(client, "https://wttr.in/?format=%25C+%25t")) {
            if (http.GET() == 200) {
                String p = http.getString(); p.trim();
                int s = p.indexOf(' ');
                if (s > 0) { 
                    weatherDesc = p.substring(0, s); 
                    tempStr = p.substring(s + 1); 
                    tempStr.replace("+", ""); 
                }
            }
            http.end();
        }

        // 2. Quote
        if (http.begin(client, "https://api.quotable.io/random?tags=literature|wisdom")) {
            if (http.GET() == 200) {
                String p = http.getString();
                int qS = p.indexOf("\"content\":\"") + 11; int qE = p.indexOf("\"", qS);
                if (qS > 10) onlineQuote = p.substring(qS, qE);
                int aS = p.indexOf("\"author\":\"") + 10; int aE = p.indexOf("\"", aS);
                if (aS > 9) onlineAuthor = p.substring(aS, aE);
            }
            http.end();
        }
        
        lastFetchTime = millis();
        dashboardDataReady = true; 
        Serial.println(F("BG TASK: Sync Complete."));
    } else {
        Serial.println(F("BG TASK: WiFi Failed."));
    }
    
    isFetchingData = false;
    vTaskDelete(NULL);
}

void drawThinDivider(int y) {
    draw_line_rotated(20, y, P_WIDTH - 20, y, COL_BLACK);
}

void drawMinimalHeader() {
    // 1. Large Digital Time (Left Aligned)
    String timeS = getTimeString(); 
    bool oldSerif = useSerif;
    useSerif = false;
    writeln_scaled(timeS.c_str(), 30, 65, 1.8, true, COL_BLACK);
    
    // 2. WiFi / Sync Status
    int dotX = P_WIDTH - 45;
    int dotY = 45;
    if (WiFi.status() == WL_CONNECTED) {
        draw_circle_rotated(dotX, dotY, 8, COL_BLACK);
        draw_circle_rotated(dotX, dotY, 4, COL_BLACK); 
    } else {
        draw_circle_rotated(dotX, dotY, 8, COL_GRAY); 
    }

    // Sync Freshness (Small dot near time if data is fresh < 1 min)
    if (lastFetchTime > 0 && (millis() - lastFetchTime < 60000)) {
        draw_circle_rotated(30 + get_text_width_scaled(timeS.c_str(), 1.8) + 20, 35, 4, COL_BLACK);
    }
    
    useSerif = oldSerif;
}

void drawDateSection(int y) {
    drawThinDivider(y);
    useSerif = false;
    RTC_Date date = rtc.getDateTime();
    const char* days[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
    const char* months[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
    
    char dateBuf[32];
    sprintf(dateBuf, "%s | %d %s", days[rtc.getDayOfWeek(date.day, date.month, date.year)], date.day, months[date.month-1]);
    
    int tw = get_text_width_scaled(dateBuf, 0.5);
    writeln_scaled(dateBuf, (P_WIDTH - tw)/2, y + 40, 0.5, true, COL_BLACK);
    drawThinDivider(y + 60);
}

void drawWeatherIcon(int x, int y, String desc) {
    desc.toLowerCase();
    if (desc.indexOf("clear") >= 0 || desc.indexOf("sun") >= 0) {
        // Enhanced Sun
        draw_circle_rotated(x, y, 22, COL_BLACK);
        draw_circle_rotated(x, y, 21, COL_BLACK);
        for(int i=0; i<360; i+=30) {
            float r = i * 0.01745;
            draw_line_rotated(x + cos(r)*28, y + sin(r)*28, x + cos(r)*38, y + sin(r)*38, COL_BLACK);
        }
    } else if (desc.indexOf("cloud") >= 0 || desc.indexOf("overcast") >= 0) {
        // Minimalist Cloud
        draw_circle_rotated(x-10, y+5, 12, COL_BLACK);
        draw_circle_rotated(x+5, y-5, 15, COL_BLACK);
        draw_circle_rotated(x+15, y+5, 10, COL_BLACK);
        fill_rect_rotated(x-12, y+10, 30, 5, COL_WHITE); // Flatten bottom
        draw_line_rotated(x-12, y+15, x+18, y+15, COL_BLACK);
    } else if (desc.indexOf("rain") >= 0 || desc.indexOf("drizzle") >= 0) {
        // Rain Cloud
        draw_circle_rotated(x, y-5, 15, COL_BLACK);
        draw_line_rotated(x-10, y+15, x-15, y+25, COL_BLACK);
        draw_line_rotated(x, y+15, x-5, y+25, COL_BLACK);
        draw_line_rotated(x+10, y+15, x+5, y+25, COL_BLACK);
    } else {
        // Default Circle
        draw_circle_rotated(x, y, 20, COL_BLACK);
    }
}

void drawWeatherSection(int y) {
    // Large Temp
    useSerif = false;
    String t = tempStr;
    if (!t.endsWith("C") && !t.endsWith("F")) t += "C";
    writeln_scaled(t.c_str(), 30, y + 80, 1.6, true, COL_BLACK);

    // Dynamic Weather Icon
    drawWeatherIcon(P_WIDTH - 80, y + 55, weatherDesc);

    // Condition & City
    useSerif = true;
    String city = "DELHI"; // Could be made dynamic
    String displayDesc = weatherDesc;
    displayDesc.toUpperCase();
    writeln_scaled(displayDesc.c_str(), 35, y + 120, 0.5, false, COL_BLACK);
    writeln_scaled(city.c_str(), 35, y + 150, 0.4, true, COL_GRAY);
}

void drawStatBar(const char* label, int pct, int x, int y, int w) {
    useSerif = false;
    writeln_scaled(label, x, y + 15, 0.35, true, COL_BLACK);
    
    int barX = x + 80;
    int barW = w - 120;
    int barH = 10;
    
    // Background bar
    fill_rect_rotated(barX, y + 4, barW, barH, COL_LIGHT);
    // Progress bar
    fill_rect_rotated(barX, y + 4, (int)(barW * (pct / 100.0)), barH, COL_BLACK);
    
    char pStr[8]; sprintf(pStr, "%d%%", pct);
    writeln_scaled(pStr, barX + barW + 15, y + 15, 0.35, false, COL_BLACK);
}

void drawSystemStats(int y) {
    int curY = y;
    int w = P_WIDTH - 60;
    
    float batt = getBatteryVoltage();
    int bPct = map((int)(batt * 100), 330, 420, 0, 100);
    if (bPct > 100) bPct = 100;
    if (bPct < 0) bPct = 0;
    
    drawStatBar("BAT", bPct, 35, curY, w);
    drawStatBar("STR", 68, 35, curY + 40, w); // Storage
}

void drawQuoteSection(int y) {
    // Decorative Quote Mark
    useSerif = true;
    writeln_scaled("\"", 30, y + 45, 1.2, true, COL_GRAY);

    String q = onlineQuote;
    int startY = y + 60;
    float qScale = 0.45;
    
    int maxW = P_WIDTH - 80;
    // Simple line wrap logic
    if (get_text_width_scaled(q.c_str(), qScale) > maxW) {
        int split = q.indexOf(' ', q.length()/2);
        if (split == -1) split = q.length()/2;
        String q1 = q.substring(0, split);
        String q2 = q.substring(split + 1);
        writeln_scaled(q1.c_str(), 45, startY, qScale, false, COL_BLACK);
        writeln_scaled(q2.c_str(), 45, startY + 40, qScale, false, COL_BLACK);
        startY += 40;
    } else {
        writeln_scaled(q.c_str(), 45, startY, qScale, false, COL_BLACK);
    }
    
    String auth = "— " + onlineAuthor;
    int aw = get_text_width_scaled(auth.c_str(), 0.35);
    writeln_scaled(auth.c_str(), P_WIDTH - 45 - aw, startY + 50, 0.35, true, COL_DARK);
}

void drawReadingWidget(int y) {
    useSerif = false;
    writeln_scaled("CURRENTLY READING", 35, y, 0.35, true, COL_GRAY);
    
    int curY = y + 45;
    if (currentFileIndex < 0 || currentFileIndex >= (int)books.size()) {
        writeln_scaled("No book in progress.", 40, curY, 0.45, false, COL_DARK);
        return;
    }

    String path = books[currentFileIndex];
    String title = path.substring(path.lastIndexOf('/') + 1);
    if (path.startsWith("internal:")) title = "Echoes of the Code";
    
    // Get Progress
    long savedPos = prefs.getLong(getPrefKey(path).c_str(), 0);
    long totalSize = 0;
    if (path.startsWith("internal:")) {
        totalSize = 25000; // Approximate for internal asset
    } else {
        File f = SD.open(path);
        if (f) { totalSize = f.size(); f.close(); }
    }
    int pct = (totalSize > 0) ? (savedPos * 100 / totalSize) : 0;

    // Draw Title
    useSerif = true;
    if (title.length() > 25) title = title.substring(0, 22) + "...";
    writeln_scaled(title.c_str(), 40, curY, 0.55, true, COL_BLACK);

    // Draw Progress Bar
    int barX = 40;
    int barY = curY + 30;
    int barW = P_WIDTH - 80;
    int barH = 12;
    fill_rect_rotated(barX, barY, barW, barH, COL_LIGHT);
    fill_rect_rotated(barX, barY, (int)(barW * (pct / 100.0)), barH, COL_BLACK);

    // Percent Text
    useSerif = false;
    char pStr[16];
    sprintf(pStr, "%d%% COMPLETED", pct);
    writeln_scaled(pStr, barX, barY + 35, 0.35, true, COL_DARK);

    // Resume Prompt (visual hint)
    const char* prompt = "TAP TO RESUME >";
    int pw = get_text_width_scaled(prompt, 0.35);
    writeln_scaled(prompt, P_WIDTH - 40 - pw, barY + 35, 0.35, true, COL_BLACK);
}

void drawGlobalNav() {
    int y = P_HEIGHT - 85;
    draw_line_rotated(0, y, P_WIDTH, y, COL_BLACK);
    const char* navLabels[] = {"LIBRARY", "STORE", "SYNC", "WIFI"};
    int btnW = P_WIDTH / 4;
    for (int i = 0; i < 4; i++) {
        int bx = i * btnW;
        if (i > 0) draw_line_rotated(bx, y + 15, bx, P_HEIGHT - 15, COL_GRAY);
        int tw = get_text_width_scaled(navLabels[i], 0.35);
        writeln_scaled(navLabels[i], bx + (btnW - tw)/2, y + 50, 0.35, true, COL_BLACK);
    }
}

void updateDashboard() {
    // 1. Trigger background fetch (will be ignored if cached)
    fetchDashboardData(false);

    // 2. Render UI immediately with current data
    epd_poweron();
    epd_clear();
    memset(framebuffer, COL_WHITE, L_WIDTH * L_HEIGHT / 2);

    drawMinimalHeader();
    drawDateSection(85);
    drawWeatherSection(160);
    drawSystemStats(340);
    drawQuoteSection(480);
    drawReadingWidget(720);
    drawGlobalNav();

    if (isFetchingData) {
        // Draw small "Syncing..." indicator
        writeln_scaled("Syncing...", P_WIDTH - 80, 20, 0.35, false, COL_BLACK);
    }

    epd_draw_grayscale_image(epd_full_screen(), framebuffer);
    epd_poweroff();
}

void handleDashboardTouch(int x, int y) {
    if (y > P_HEIGHT - 85) {
        int btnW = P_WIDTH / 4;
        int slot = x / btnW;
        showTransitionEffect();
        if (slot == 0) { appState = STATE_LIBRARY; updateLibrary(); }
        else if (slot == 1) { appState = STATE_STORE; updateStore(); }
        else if (slot == 2) { startGhostDrop(); }
        else if (slot == 3) { appState = STATE_WIFI_SETUP; startWiFiScan(); }
    } else if (y > 720 && y < P_HEIGHT - 85) {
        if (currentFileIndex >= 0 && currentFileIndex < (int)books.size()) {
            showTransitionEffect();
            appState = STATE_READING;
            updateReader();
        }
    } else {
        fetchDashboardData(true);
        updateDashboard();
    }
}

void fetchDashboardData(bool force) {
    if (isFetchingData) {
        Serial.println(F("DASHBOARD: Sync already in progress."));
        return;
    }

    if (!force && lastFetchTime > 0 && (millis() - lastFetchTime < FETCH_INTERVAL)) {
        Serial.println(F("DASHBOARD: Using cached data."));
        return;
    }

    Serial.println(F("DASHBOARD: Launching Background Task..."));
    
    xTaskCreate(
        taskFetchData,    
        "TaskFetchData",  
        8192,             
        NULL,             
        1,                
        NULL);            
}