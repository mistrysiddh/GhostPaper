#include "dashboard.h"
#include <WiFi.h>

// Mock Data for Prototype
String weatherDesc = "Clear Sky";
String tempStr = "24C";
String onlineQuote = "Books are a uniquely portable magic.";
String onlineAuthor = "Stephen King";
struct CalendarEvent { String time; String title; };
std::vector<CalendarEvent> events = {
    {"09:00", "Daily Standup"},
    {"13:00", "Deep Work"},
    {"17:00", "Gym"}
};

void drawWeatherWidget(int x, int y, int w, int h) {
    draw_rect_rotated(x, y, w, h, COL_BLACK);
    
    // Header
    fill_rect_rotated(x, y, w, 30, COL_BLACK);
    writeln_scaled("WEATHER", x + 10, y + 22, 0.4, true, COL_WHITE);
    
    // Dynamic Icon Logic
    int icX = x + w/2;
    int icY = y + 60;
    String desc = weatherDesc;
    desc.toLowerCase();

    if (desc.indexOf("clear") != -1 || desc.indexOf("sun") != -1) {
        draw_circle_rotated(icX, icY, 15, COL_BLACK);
        for(int a=0; a<360; a+=45) {
            float rad = a * 0.01745;
            draw_line_rotated(icX + cos(rad)*20, icY + sin(rad)*20, icX + cos(rad)*28, icY + sin(rad)*28, COL_BLACK);
        }
    } else if (desc.indexOf("cloud") != -1 || desc.indexOf("overcast") != -1) {
        draw_circle_rotated(icX - 10, icY + 5, 12, COL_BLACK);
        draw_circle_rotated(icX + 10, icY + 5, 12, COL_BLACK);
        draw_circle_rotated(icX, icY - 5, 12, COL_BLACK);
    } else if (desc.indexOf("rain") != -1 || desc.indexOf("drizzle") != -1) {
        draw_circle_rotated(icX, icY - 10, 12, COL_BLACK);
        for(int i=-1; i<=1; i++) draw_line_rotated(icX + i*15, icY + 5, icX + i*15 - 5, icY + 25, COL_BLACK);
    } else {
        // Default generic star/snowflake
        for(int i=0; i<4; i++) {
            float rad = i * 0.785 * 2;
            draw_line_rotated(icX - cos(rad)*20, icY - sin(rad)*20, icX + cos(rad)*20, icY + sin(rad)*20, COL_BLACK);
        }
    }
    
    // Text
    int tw = get_text_width_scaled(tempStr.c_str(), 0.8);
    writeln_scaled(tempStr.c_str(), x + (w-tw)/2, y + 110, 0.8, true, COL_BLACK);
    
    int dw = get_text_width_scaled(weatherDesc.c_str(), 0.4);
    writeln_scaled(weatherDesc.c_str(), x + (w-dw)/2, y + 135, 0.4, false, COL_GRAY);

    // Battery Info
    float batt = getBatteryVoltage();
    int pct = map((int)(batt * 100), 330, 420, 0, 100);
    if (pct > 100) pct = 100; if (pct < 0) pct = 0;
    char bStr[32]; sprintf(bStr, "BATTERY: %d%%", pct);
    writeln_scaled(bStr, x + 10, y + h - 12, 0.3, true, COL_BLACK);
}

void drawCalendarWidget(int x, int y, int w, int h) {
    draw_rect_rotated(x, y, w, h, COL_BLACK);
    
    // Header
    fill_rect_rotated(x, y, w, 30, COL_BLACK);
    writeln_scaled("CALENDAR", x + 10, y + 22, 0.4, true, COL_WHITE);
    
    int startY = y + 45;
    for(auto& e : events) {
        writeln_scaled(e.time.c_str(), x + 10, startY, 0.45, true, COL_BLACK);
        writeln_scaled(e.title.c_str(), x + 60, startY, 0.45, false, COL_BLACK);
        startY += 30;
        draw_line_rotated(x + 10, startY - 20, x + w - 10, startY - 20, COL_GRAY);
    }
}

void drawGoalWidget(int x, int y, int w, int h) {
    // A nice double-border box for the Goal
    draw_rect_rotated(x, y, w, h, COL_BLACK);
    draw_rect_rotated(x + 4, y + 4, w - 8, h - 8, COL_BLACK);
    
    // Title
    int gw = get_text_width_scaled("QUOTE OF THE DAY", 0.5);
    fill_rect_rotated(x + (w - gw)/2 - 10, y - 10, gw + 20, 20, COL_WHITE); 
    writeln_scaled("QUOTE OF THE DAY", x + (w - gw)/2, y + 5, 0.5, true, COL_BLACK);
    
    // The Quote
    String q = onlineQuote;
    float qScale = 0.45;
    if (q.length() > 60) qScale = 0.35;
    if (q.length() > 100) qScale = 0.3;

    // Simple multi-line logic
    if (get_text_width_scaled(q.c_str(), qScale) > w - 40) {
        int split = q.indexOf(' ', q.length()/2);
        if (split == -1) split = q.length()/2;
        String q1 = q.substring(0, split);
        String q2 = q.substring(split + 1);
        int tw1 = get_text_width_scaled(q1.c_str(), qScale);
        int tw2 = get_text_width_scaled(q2.c_str(), qScale);
        writeln_scaled(q1.c_str(), x + (w - tw1)/2, y + h/2 - 5, qScale, true, COL_BLACK);
        writeln_scaled(q2.c_str(), x + (w - tw2)/2, y + h/2 + 25, qScale, true, COL_BLACK);
    } else {
        int tw = get_text_width_scaled(q.c_str(), qScale);
        writeln_scaled(q.c_str(), x + (w - tw)/2, y + h/2 + 10, qScale, true, COL_BLACK);
    }

    // Author
    String auth = "- " + onlineAuthor;
    int aw = get_text_width_scaled(auth.c_str(), 0.35);
    writeln_scaled(auth.c_str(), x + w - aw - 20, y + h - 20, 0.35, false, COL_GRAY);
}

void updateDashboard() {
    fetchDashboardData();
    epd_poweron();
    epd_clear();
    memset(framebuffer, COL_WHITE, L_WIDTH * L_HEIGHT / 2);

    // 1. Master Header (Time & Date)
    fill_rect_rotated(0, 0, P_WIDTH, 120, COL_BLACK);
    
    String timeS = getTimeString(); // "HH:MM"
    int timeW = get_text_width_scaled(timeS.c_str(), 2.0);
    writeln_scaled(timeS.c_str(), (P_WIDTH - timeW)/2, 80, 2.0, true, COL_WHITE);
    
    const char* dateS = "Tuesday, Feb 10"; // Mock for now
    int dateW = get_text_width_scaled(dateS, 0.5);
    writeln_scaled(dateS, (P_WIDTH - dateW)/2, 110, 0.5, false, COL_GRAY);

    // 2. The Focus Goal (Center Stage)
    drawGoalWidget(40, 160, P_WIDTH - 80, 150);

    // 3. Grid for Widgets
    int midY = 340;
    int widgetW = (P_WIDTH - 60) / 2;
    int widgetH = 200;
    
    drawWeatherWidget(20, midY, widgetW, widgetH);
    drawCalendarWidget(20 + widgetW + 20, midY, widgetW, widgetH);

    // 4. "Now Reading" Footer
    int footerY = P_HEIGHT - 120;
    fill_rect_rotated(20, footerY, P_WIDTH - 40, 100, COL_LIGHT);
    draw_rect_rotated(20, footerY, P_WIDTH - 40, 100, COL_BLACK);
    
    writeln_scaled("CONTINUE READING:", 35, footerY + 30, 0.4, true, COL_BLACK);
    // Grab the last book from prefs if possible, otherwise placeholder
    writeln_scaled("Echoes of the Code", 35, footerY + 65, 0.6, true, COL_BLACK);
    writeln_scaled("42%", P_WIDTH - 80, footerY + 65, 0.6, false, COL_BLACK);
    
    // Bottom Branding
    const char* brand = "GHOSTBOARD OS";
    int bw = get_text_width_scaled(brand, 0.4);
    writeln_scaled(brand, (P_WIDTH - bw)/2, P_HEIGHT - 20, 0.4, true, COL_GRAY);

    epd_draw_grayscale_image(epd_full_screen(), framebuffer);
    epd_poweroff();
}

void handleDashboardTouch(int x, int y) {
    // Bottom area -> Jump to Library/Reading
    if (y > P_HEIGHT - 120) {
         // visual feedback
         Rect_t area = { .x = (int32_t)(960 - P_HEIGHT), .y = 20, .width = 120, .height = (uint32_t)(P_WIDTH - 40) }; 
         // Note: Mapping might need tuning based on partialUpdateRegion logic
         // For now, simpler:
         appState = STATE_LIBRARY;
         showTransitionEffect();
         updateLibrary();
    } else {
        // Tap anywhere else to refresh
        updateDashboard();
    }
}

#include <HTTPClient.h>
#include <WiFiClientSecure.h>

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
        
        // 1. Fetch Keyless Weather (wttr.in)
        if (http.begin(client, "https://wttr.in/?format=%25C+%25t")) { // "Condition + Temperature"
            int code = http.GET();
            if (code == 200) {
                String payload = http.getString();
                payload.trim();
                int spaceIdx = payload.indexOf(' ');
                if (spaceIdx > 0) {
                    weatherDesc = payload.substring(0, spaceIdx);
                    tempStr = payload.substring(spaceIdx + 1);
                    tempStr.replace("+", ""); // Clean up formatting
                }
            }
            http.end();
        }

        // 2. Fetch Quote
        if (http.begin(client, "https://api.quotable.io/random?tags=literature|wisdom")) {
            int code = http.GET();
            if (code == 200) {
                String payload = http.getString();
                // Simple manual parsing to avoid heavy JSON library
                int quoteStart = payload.indexOf("\"content\":\"") + 11;
                int quoteEnd = payload.indexOf("\"", quoteStart);
                if (quoteStart > 10) onlineQuote = payload.substring(quoteStart, quoteEnd);
                
                int authStart = payload.indexOf("\"author\":\"") + 10;
                int authEnd = payload.indexOf("\"", authStart);
                if (authStart > 9) onlineAuthor = payload.substring(authStart, authEnd);
            }
            http.end();
        }
    }
}
