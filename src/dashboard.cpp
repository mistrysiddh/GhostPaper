#include "dashboard.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

// Live Data
String weatherDesc = "Clear Sky";
String tempStr = "24C";
String onlineQuote = "Books are a uniquely portable magic.";
String onlineAuthor = "Stephen King";
struct CalendarEvent { String time; String title; };
std::vector<CalendarEvent> events = {
    {"09:00", "Daily Standup"},
    {"13:00", "Deep Work Session"},
    {"17:00", "Gym & Health"}
};

void drawHorizontalDivider(int y) {
    draw_line_rotated(40, y, P_WIDTH - 40, y, COL_BLACK);
}

void drawHeroHeader() {
    // 1. Large Minimal Clock
    String timeS = getTimeString();
    bool oldSerif = useSerif;
    useSerif = false; // Sans for time
    int timeW = get_text_width_scaled(timeS.c_str(), 2.4);
    writeln_scaled(timeS.c_str(), (P_WIDTH - timeW)/2, 100, 2.4, true, COL_BLACK);
    
    // 2. Elegant Date & Battery
    useSerif = true; // Serif for date
    const char* dateS = "TUESDAY, FEBRUARY 10"; 
    int dateW = get_text_width_scaled(dateS, 0.45);
    writeln_scaled(dateS, (P_WIDTH - dateW)/2, 140, 0.45, true, COL_DARK);
    
    // Battery Percentage (Minimal)
    float batt = getBatteryVoltage();
    int pct = map((int)(batt * 100), 330, 420, 0, 100);
    if (pct > 100) pct = 100; if (pct < 0) pct = 0;
    char bStr[16]; sprintf(bStr, "%d%% PWR", pct);
    int bw = get_text_width_scaled(bStr, 0.35);
    writeln_scaled(bStr, (P_WIDTH - bw)/2, 165, 0.35, false, COL_GRAY);
    useSerif = oldSerif;
}

void drawWeatherStrip(int y) {
    int icX = 80;
    int icY = y + 40;
    
    // Minimal Icon
    String desc = weatherDesc; desc.toLowerCase();
    if (desc.indexOf("clear") != -1 || desc.indexOf("sun") != -1) {
        draw_circle_rotated(icX, icY, 12, COL_BLACK);
    } else {
        draw_circle_rotated(icX - 6, icY + 3, 8, COL_BLACK);
        draw_circle_rotated(icX + 6, icY + 3, 8, COL_BLACK);
        draw_circle_rotated(icX, icY - 3, 8, COL_BLACK);
    }

    // Temp and Condition
    useSerif = false;
    writeln_scaled(tempStr.c_str(), icX + 40, icY + 12, 0.7, true, COL_BLACK);
    
    useSerif = true;
    String cond = weatherDesc; cond.toUpperCase();
    writeln_scaled(cond.c_str(), icX + 110, icY + 10, 0.4, false, COL_DARK);
}

void drawElegantQuote(int y, int h) {
    bool oldSerif = useSerif;
    useSerif = true;
    
    String q = onlineQuote;
    float qScale = 0.55;
    if (q.length() > 60) qScale = 0.45;
    if (q.length() > 100) qScale = 0.35;

    // Center Vertical
    int curY = y + (h/2);

    if (get_text_width_scaled(q.c_str(), qScale) > P_WIDTH - 100) {
        int split = q.indexOf(' ', q.length()/2);
        if (split == -1) split = q.length()/2;
        String q1 = "\"" + q.substring(0, split);
        String q2 = q.substring(split + 1) + "\"";
        int tw1 = get_text_width_scaled(q1.c_str(), qScale);
        int tw2 = get_text_width_scaled(q2.c_str(), qScale);
        writeln_scaled(q1.c_str(), (P_WIDTH - tw1)/2, curY - 10, qScale, false, COL_BLACK);
        writeln_scaled(q2.c_str(), (P_WIDTH - tw2)/2, curY + 30, qScale, false, COL_BLACK);
    } else {
        String fullQ = "\"" + q + "\"";
        int tw = get_text_width_scaled(fullQ.c_str(), qScale);
        writeln_scaled(fullQ.c_str(), (P_WIDTH - tw)/2, curY + 10, qScale, false, COL_BLACK);
    }

    String auth = "— " + onlineAuthor;
    int aw = get_text_width_scaled(auth.c_str(), 0.35);
    writeln_scaled(auth.c_str(), (P_WIDTH - aw)/2, curY + 70, 0.35, true, COL_GRAY);
    
    useSerif = oldSerif;
}

void drawAgenda(int y) {
    useSerif = false;
    writeln_scaled("TODAY'S AGENDA", 40, y + 20, 0.35, true, COL_GRAY);
    
    int curY = y + 55;
    for(auto& e : events) {
        writeln_scaled(e.time.c_str(), 45, curY, 0.45, true, COL_BLACK);
        writeln_scaled(e.title.c_str(), 120, curY, 0.45, false, COL_BLACK);
        curY += 40;
    }
}

void drawReaderFooter() {
    int y = P_HEIGHT - 100;
    draw_line_rotated(0, y, P_WIDTH, y, COL_BLACK);
    
    useSerif = true;
    writeln_scaled("CURRENTLY READING", 40, y + 35, 0.35, false, COL_GRAY);
    
    useSerif = false;
    const char* book = "Echoes of the Code";
    writeln_scaled(book, 40, y + 70, 0.55, true, COL_BLACK);
    
    // Progress Bar
    int barW = 120;
    int barX = P_WIDTH - barW - 40;
    int barY = y + 60;
    draw_rect_rotated(barX, barY, barW, 6, COL_BLACK);
    fill_rect_rotated(barX, barY, barW * 0.42, 6, COL_BLACK);
    writeln_scaled("42%", barX + barW - 35, barY - 10, 0.35, true, COL_BLACK);
}

void updateDashboard() {
    fetchDashboardData();
    epd_poweron();
    epd_clear();
    memset(framebuffer, COL_WHITE, L_WIDTH * L_HEIGHT / 2);

    drawHeroHeader();
    
    drawHorizontalDivider(200);
    drawWeatherStrip(210);
    
    drawHorizontalDivider(300);
    drawElegantQuote(320, 300);
    
    drawHorizontalDivider(620);
    drawAgenda(640);
    
    drawReaderFooter();

    epd_draw_grayscale_image(epd_full_screen(), framebuffer);
    epd_poweroff();
}

void handleDashboardTouch(int x, int y) {
    if (y > P_HEIGHT - 100) {
         appState = STATE_LIBRARY;
         showTransitionEffect();
         updateLibrary();
    } else {
        updateDashboard();
    }
}

void fetchDashboardData() {
    if (WiFi.status() != WL_CONNECTED) {
        WiFi.begin(WIFI_SSID_1, WIFI_PASS_1);
        int retries = 0;
        while (WiFi.status() != WL_CONNECTED && retries < 15) { delay(500); retries++; }
    }

    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        WiFiClientSecure client;
        client.setInsecure();
        
        // 1. Weather
        if (http.begin(client, "https://wttr.in/?format=%25C+%25t")) {
            int code = http.GET();
            if (code == 200) {
                String payload = http.getString(); payload.trim();
                int spaceIdx = payload.indexOf(' ');
                if (spaceIdx > 0) {
                    weatherDesc = payload.substring(0, spaceIdx);
                    tempStr = payload.substring(spaceIdx + 1);
                    tempStr.replace("+", "");
                }
            }
            http.end();
        }

        // 2. Quote
        if (http.begin(client, "https://api.quotable.io/random?tags=literature|wisdom")) {
            int code = http.GET();
            if (code == 200) {
                String payload = http.getString();
                int qS = payload.indexOf("\"content\":\"") + 11;
                int qE = payload.indexOf("\"", qS);
                if (qS > 10) onlineQuote = payload.substring(qS, qE);
                int aS = payload.indexOf("\"author\":\"") + 10;
                int aE = payload.indexOf("\"", aS);
                if (aS > 9) onlineAuthor = payload.substring(aS, aE);
            }
            http.end();
        }
    }
}