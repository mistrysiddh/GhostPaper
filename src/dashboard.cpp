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
struct CalendarEvent { String time; String title; };
std::vector<CalendarEvent> events;

void drawThinDivider(int y) {
    draw_line_rotated(20, y, P_WIDTH - 20, y, COL_BLACK);
}

void drawMinimalHeader() {
    // 1. Large Digital Time (Left Aligned)
    String timeS = getTimeString(); // e.g. "09:41"
    bool oldSerif = useSerif;
    useSerif = false;
    writeln_scaled(timeS.c_str(), 30, 65, 1.8, true, COL_BLACK);
    
    // 2. WiFi Status Dot (Right Aligned)
    int dotX = P_WIDTH - 45;
    int dotY = 45;
    if (WiFi.status() == WL_CONNECTED) {
        draw_circle_rotated(dotX, dotY, 8, COL_BLACK);
        draw_circle_rotated(dotX, dotY, 4, COL_BLACK); // Filled look
    } else {
        draw_circle_rotated(dotX, dotY, 8, COL_BLACK); // Empty circle
    }
    useSerif = oldSerif;
}

void drawDateSection(int y) {
    drawThinDivider(y);
    useSerif = false;
    const char* dateS = "MON | 10 FEB"; // Hardcoded for style or dynamic if RTC updated
    int tw = get_text_width_scaled(dateS, 0.5);
    writeln_scaled(dateS, (P_WIDTH - tw)/2, y + 40, 0.5, true, COL_BLACK);
    drawThinDivider(y + 60);
}

void drawWeatherSection(int y) {
    // Large Temp
    useSerif = false;
    String t = tempStr;
    if (!t.endsWith("C")) t += "C";
    writeln_scaled(t.c_str(), 30, y + 80, 1.6, true, COL_BLACK);

    // Sun Outline Icon
    int icX = P_WIDTH - 80;
    int icY = y + 55;
    draw_circle_rotated(icX, icY, 20, COL_BLACK);
    for(int i=0; i<360; i+=45) {
        float r = i * 0.01745;
        draw_line_rotated(icX + cos(r)*25, icY + sin(r)*25, icX + cos(r)*35, icY + sin(r)*35, COL_BLACK);
    }

    // Condition & City
    useSerif = true;
    String city = "DELHI";
    writeln_scaled(weatherDesc.c_str(), 35, y + 120, 0.5, false, COL_BLACK);
    writeln_scaled(city.c_str(), 35, y + 150, 0.4, true, COL_GRAY);
}

void drawStatBar(const char* label, int pct, int x, int y, int w) {
    useSerif = false;
    writeln_scaled(label, x, y + 15, 0.35, true, COL_BLACK);
    
    int barX = x + 100;
    int barW = w - 100;
    int barH = 12;
    draw_rect_rotated(barX, y, barW, barH, COL_BLACK);
    fill_rect_rotated(barX + 2, y + 2, (int)((barW - 4) * (pct / 100.0)), barH - 4, COL_BLACK);
    
    char pStr[8]; sprintf(pStr, "%d%%", pct);
    writeln_scaled(pStr, barX + barW + 10, y + 15, 0.35, false, COL_BLACK);
}

void drawSystemStats(int y) {
    int curY = y;
    int w = P_WIDTH - 150;
    
    float batt = getBatteryVoltage();
    int bPct = map((int)(batt * 100), 330, 420, 0, 100);
    if (bPct > 100) bPct = 100;
    
    drawStatBar("CPU", 32, 35, curY, w);
    drawStatBar("RAM", 41, 35, curY + 35, w);
    drawStatBar("BAT", bPct, 35, curY + 70, w);
}

void drawQuoteSection(int y) {
    useSerif = true;
    String q = "\"" + onlineQuote + "\"";
    
    // Generous whitespace and vertical centering
    int startY = y + 60;
    float qScale = 0.5;
    
    // Manual wrap for serif aesthetic
    int maxW = P_WIDTH - 80;
    if (get_text_width_scaled(q.c_str(), qScale) > maxW) {
        int split = q.indexOf(' ', q.length()/2);
        if (split == -1) split = q.length()/2;
        String q1 = q.substring(0, split);
        String q2 = q.substring(split + 1);
        writeln_scaled(q1.c_str(), 40, startY, qScale, false, COL_BLACK);
        writeln_scaled(q2.c_str(), 40, startY + 40, qScale, false, COL_BLACK);
        startY += 40;
    } else {
        writeln_scaled(q.c_str(), 40, startY, qScale, false, COL_BLACK);
    }
    
    String auth = "— " + onlineAuthor;
    int aw = get_text_width_scaled(auth.c_str(), 0.35);
    writeln_scaled(auth.c_str(), P_WIDTH - 40 - aw, startY + 50, 0.35, true, COL_DARK);
}

void drawAgendaSection(int y) {
    useSerif = false;
    writeln_scaled("CALENDAR", 35, y, 0.35, true, COL_GRAY);
    
    int curY = y + 40;
    if (events.empty()) {
        writeln_scaled("No events scheduled.", 40, curY, 0.45, false, COL_DARK);
    } else {
        for(int i=0; i < (int)events.size() && i < 3; i++) {
            // Minimal icon (small dot)
            draw_circle_rotated(45, curY - 12, 3, COL_BLACK);
            writeln_scaled(events[i].time.c_str(), 65, curY, 0.45, true, COL_BLACK);
            writeln_scaled(events[i].title.c_str(), 140, curY, 0.45, false, COL_BLACK);
            curY += 45;
        }
    }
}

void drawGlobalNav() {
    int y = P_HEIGHT - 85;
    draw_line_rotated(0, y, P_WIDTH, y, COL_BLACK);
    const char* navLabels[] = {"LIB", "STRE", "SYNC", "WIFI"};
    int btnW = P_WIDTH / 4;
    for (int i = 0; i < 4; i++) {
        int bx = i * btnW;
        if (i > 0) draw_line_rotated(bx, y + 15, bx, P_HEIGHT - 15, COL_GRAY);
        int tw = get_text_width_scaled(navLabels[i], 0.4);
        writeln_scaled(navLabels[i], bx + (btnW - tw)/2, y + 50, 0.4, true, COL_BLACK);
    }
}

void updateDashboard() {
    fetchDashboardData();
    epd_poweron();
    epd_clear();
    memset(framebuffer, COL_WHITE, L_WIDTH * L_HEIGHT / 2);

    drawMinimalHeader();
    drawDateSection(85);
    drawWeatherSection(160);
    drawSystemStats(340);
    drawQuoteSection(480);
    drawAgendaSection(720);
    drawGlobalNav();

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
    } else {
        updateDashboard();
    }
}

void fetchDashboardData() {
    if (WiFi.status() != WL_CONNECTED) {
        WiFi.begin(WIFI_SSID_1, WIFI_PASS_1);
        int retries = 0;
        while (WiFi.status() != WL_CONNECTED && retries < 10) { delay(500); retries++; }
    }

    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        WiFiClientSecure client;
        client.setInsecure();
        
        // Weather
        if (http.begin(client, "https://wttr.in/?format=%25C+%25t")) {
            if (http.GET() == 200) {
                String p = http.getString(); p.trim();
                int s = p.indexOf(' ');
                if (s > 0) { weatherDesc = p.substring(0, s); tempStr = p.substring(s + 1); tempStr.replace("+", ""); }
            }
            http.end();
        }

        // Quote
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

        // Calendar
        if (http.begin(client, "https://outlook.office365.com/owa/calendar/140c083fb557498894d90a9f2264cf6c@mistrysiddh.com/e21ab2dfb37e46aea043c2bcc75b86c85675685166080299637/calendar.ics")) {
            if (http.GET() == 200) {
                events.clear();
                WiFiClient *stream = http.getStreamPtr();
                while (stream->available()) {
                    String l = stream->readStringUntil('\n'); l.trim();
                    if (l.startsWith("BEGIN:VEVENT")) {
                        String sum = "", st = "";
                        while (stream->available()) {
                            l = stream->readStringUntil('\n'); l.trim();
                            if (l.startsWith("SUMMARY:")) { sum = l.substring(8); sum.replace("\\,", ","); }
                            else if (l.startsWith("DTSTART")) {
                                int c = l.indexOf(':');
                                if (c > 0) { String r = l.substring(c + 1); if (r.length() >= 13) st = r.substring(9, 11) + ":" + r.substring(11, 13); }
                            }
                            else if (l.startsWith("END:VEVENT")) break;
                        }
                        if (sum != "" && st != "") events.push_back({st, sum});
                        if (events.size() > 3) break;
                    }
                }
            }
            http.end();
        }
    }
}