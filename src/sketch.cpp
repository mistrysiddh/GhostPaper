#include "sketch.h"

// Simple RAM buffer for the sketch (reusing the main framebuffer is tricky if we want to redraw UI over it)
// For now, we'll draw directly to the EPD framebuffer and rely on partial refreshes.

void drawSketchUI() {
    int y = P_HEIGHT - 80;
    
    // Bottom Bar Background
    fill_rect_rotated(0, y, P_WIDTH, 80, COL_WHITE);
    draw_line_rotated(0, y, P_WIDTH, y, COL_BLACK);
    
    // EXIT Button
    int btnW = 120;
    int btnH = 50;
    int btnX = 30;
    int btnY = y + 15;
    draw_rounded_rect(btnX, btnY, btnW, btnH, 8, COL_BLACK);
    const char* l1 = "EXIT";
    int tw1 = get_text_width_scaled(l1, 0.45);
    writeln_scaled(l1, btnX + (btnW - tw1)/2, btnY + 35, 0.45, true, COL_BLACK);

    // CLEAR Button
    int btnX2 = P_WIDTH - 30 - btnW;
    draw_rounded_rect(btnX2, btnY, btnW, btnH, 8, COL_BLACK);
    const char* l2 = "CLEAR";
    int tw2 = get_text_width_scaled(l2, 0.45);
    writeln_scaled(l2, btnX2 + (btnW - tw2)/2, btnY + 35, 0.45, true, COL_BLACK);
    
    // Title
    const char* t = "GhostSketch";
    int tw = get_text_width_scaled(t, 0.5);
    writeln_scaled(t, (P_WIDTH - tw)/2, y + 45, 0.5, true, COL_GRAY);
}

void updateSketch() {
    epd_poweron();
    epd_clear();
    memset(framebuffer, COL_WHITE, L_WIDTH * L_HEIGHT / 2);
    
    drawSketchUI();
    
    epd_draw_grayscale_image(epd_full_screen(), framebuffer);
    epd_poweroff();
}

void handleSketchTouch(int x, int y) {
    // Check UI interactions first
    if (y > P_HEIGHT - 80) {
        int btnW = 120;
        int btnH = 50;
        int btnY = P_HEIGHT - 80 + 15;

        // EXIT
        if (x > 30 && x < 30 + btnW) {
            appState = STATE_DASHBOARD;
            updateDashboard();
            return;
        }
        
        // CLEAR
        if (x > P_WIDTH - 30 - btnW && x < P_WIDTH - 30) {
            updateSketch(); // Clears everything
            return;
        }
        return;
    }

    // Drawing Logic
    // We draw a small filled circle at the touch point for a brush effect
    int brushSize = 4;
    
    // We need to write directly to the framebuffer and trigger a partial update
    // But since `framebuffer` is in 4-bit mode (2 pixels per byte), direct manipulation is complex.
    // We'll use the helper `fill_rect_rotated` which writes to the global `framebuffer`.
    
    fill_rect_rotated(x - brushSize/2, y - brushSize/2, brushSize, brushSize, COL_BLACK);
    
    // Define the update area for the driver (slightly larger than the brush)
    Rect_t area = {
        .x = (int32_t)(960 - 1 - (y + brushSize)), // Mapped coordinates for the driver
        .y = (int32_t)(x - brushSize),
        .width = (uint32_t)(brushSize * 2),
        .height = (uint32_t)(brushSize * 2)
    };

    // Bounds checking
    if (area.x < 0) area.x = 0;
    if (area.y < 0) area.y = 0;
    if (area.width > 960) area.width = 960;
    if (area.height > 540) area.height = 540;

    epd_poweron();
    epd_draw_grayscale_image_area(area, framebuffer);
    epd_poweroff();
}
