#include "dashboard.h"
#include <WiFi.h>

// Mock Data for Prototype
String weatherDesc = "Clear Sky";
String tempStr = "24C";
String goalText = "Focus on the code.";
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
    
    // Icon Placeholder (Sun)
    draw_circle_rotated(x + w/2, y + 60, 20, COL_BLACK);
    draw_circle_rotated(x + w/2, y + 60, 25, COL_BLACK); // Ray ring
    
    // Text
    int tw = get_text_width_scaled(tempStr.c_str(), 0.8);
    writeln_scaled(tempStr.c_str(), x + (w-tw)/2, y + 110, 0.8, true, COL_BLACK);
    
    int dw = get_text_width_scaled(weatherDesc.c_str(), 0.4);
    writeln_scaled(weatherDesc.c_str(), x + (w-dw)/2, y + 135, 0.4, false, COL_GRAY);
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
    int gw = get_text_width_scaled("DAILY FOCUS", 0.5);
    fill_rect_rotated(x + (w - gw)/2 - 10, y - 10, gw + 20, 20, COL_WHITE); // Clear line for title
    writeln_scaled("DAILY FOCUS", x + (w - gw)/2, y + 5, 0.5, true, COL_BLACK);
    
    // The Goal
    int tw = get_text_width_scaled(goalText.c_str(), 0.6);
    writeln_scaled(goalText.c_str(), x + (w - tw)/2, y + h/2 + 10, 0.6, true, COL_BLACK);
}

void updateDashboard() {
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

void fetchDashboardData() {
    // TODO: Connect to WiFi and fetch OpenWeather / Calendar
}
