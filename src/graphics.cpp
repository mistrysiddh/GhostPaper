#include "common.h"
#include "firasans.h"
#include "crimson.h"
#include "zlib/zlib.h"

// --- Helper for Circle Section ---
void draw_circle_section(int16_t x0, int16_t y0, int16_t r, uint8_t corners, uint8_t color) {
    int16_t f = 1 - r;
    int16_t ddF_x = 1;
    int16_t ddF_y = -2 * r;
    int16_t x = 0;
    int16_t y = r;

    while (x < y) {
        if (f >= 0) {
            y--;
            ddF_y += 2;
            f += ddF_y;
        }
        x++;
        ddF_x += 2;
        f += ddF_x;
        // 1: Top-Left, 2: Top-Right, 4: Bottom-Right, 8: Bottom-Left
        if (corners & 0x4) { // Bottom-Right
            draw_pixel_rotated(x0 + x, y0 + y, color);
            draw_pixel_rotated(x0 + y, y0 + x, color);
        }
        if (corners & 0x8) { // Bottom-Left
            draw_pixel_rotated(x0 - x, y0 + y, color);
            draw_pixel_rotated(x0 - y, y0 + x, color);
        }
        if (corners & 0x1) { // Top-Left
            draw_pixel_rotated(x0 - x, y0 - y, color);
            draw_pixel_rotated(x0 - y, y0 - x, color);
        }
        if (corners & 0x2) { // Top-Right
            draw_pixel_rotated(x0 + x, y0 - y, color);
            draw_pixel_rotated(x0 + y, y0 - x, color);
        }
    }
}

// --- Public Graphics Functions ---

void draw_pixel_rotated(int16_t x, int16_t y, uint8_t gray) {
    int16_t lx = (L_WIDTH - 1) - y;
    int16_t ly = x;
    if (lx >= 0 && lx < L_WIDTH && ly >= 0 && ly < L_HEIGHT && framebuffer) {
        epd_draw_pixel(lx, ly, gray & 0xF0, framebuffer); 
    }
}

void draw_line_rotated(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t color) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy, e2;
    for (;;) {
        draw_pixel_rotated(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void fill_rect_rotated(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t gray) {
    for (int i = 0; i < w; i++) for (int j = 0; j < h; j++) draw_pixel_rotated(x + i, y + j, gray);
}

void draw_rect_rotated(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t gray) {
    for (int i = 0; i < w; i++) { draw_pixel_rotated(x + i, y, gray); draw_pixel_rotated(x + i, y + h - 1, gray); }
    for (int j = 0; j < h; j++) { draw_pixel_rotated(x, y + j, gray); draw_pixel_rotated(x + w - 1, y + j, gray); }
}

void draw_rounded_rect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint8_t gray) {
    draw_line_rotated(x + r, y, x + w - r - 1, y, gray); // Top
    draw_line_rotated(x + r, y + h - 1, x + w - r - 1, y + h - 1, gray); // Bottom
    draw_line_rotated(x, y + r, x, y + h - r - 1, gray); // Left
    draw_line_rotated(x + w - 1, y + r, x + w - 1, y + h - r - 1, gray); // Right
    
    draw_circle_section(x + r, y + r, r, 1, gray); // Top-left
    draw_circle_section(x + w - r - 1, y + r, r, 2, gray); // Top-right
    draw_circle_section(x + w - r - 1, y + h - r - 1, r, 4, gray); // Bot-right
    draw_circle_section(x + r, y + h - r - 1, r, 8, gray); // Bot-left
}

void draw_circle_rotated(int16_t xm, int16_t ym, int16_t r, uint8_t color) {
    draw_circle_section(xm, ym, r, 0xF, color); // All corners
    draw_pixel_rotated(xm, ym - r, color);
    draw_pixel_rotated(xm, ym + r, color);
    draw_pixel_rotated(xm - r, ym, color);
    draw_pixel_rotated(xm + r, ym, color);
}

uint32_t decode_utf8(const char** s) {
    uint32_t cp = (unsigned char)**s;
    if (cp < 0x80) { (*s)++; return cp; }
    if ((cp & 0xE0) == 0xC0) { cp = ((cp & 0x1F) << 6) | ((*s)[1] & 0x3F); *s += 2; return cp; }
    if ((cp & 0xF0) == 0xE0) { cp = ((cp & 0x0F) << 12) | (((*s)[1] & 0x3F) << 6) | (((*s)[2] & 0x3F)); *s += 3; return cp; }
    (*s)++; return cp;
}

int drawChar(uint32_t cp, int x, int y, float scale, bool bold, uint8_t color) {
    GFXfont *font = useSerif ? (GFXfont *)&Crimson : (GFXfont *)&FiraSans; 
    GFXglyph *glyph; get_glyph(font, cp, &glyph);
    if (!glyph) return (cp == ' ') ? (int)(12 * scale) : 0;
    uint32_t offset = glyph->data_offset; int32_t bw = (glyph->width + 1) / 2;
    unsigned long bsize = bw * glyph->height; uint8_t *bitmap = (uint8_t *)malloc(bsize);
    if (bitmap && uncompress(bitmap, &bsize, &font->bitmap[offset], glyph->compressed_size) == Z_OK) {
        for (int j = 0; j < glyph->height; j++) {
            for (int i = 0; i < glyph->width; i++) {
                uint8_t v = (i % 2 == 0) ? (bitmap[j * bw + i / 2] & 0x0F) : (bitmap[j * bw + i / 2] >> 4);
                if (v > 2) { 
                    int px = x + (int)((glyph->left + i) * scale);
                    int py = y - (int)((glyph->top - j) * scale);
                    draw_pixel_rotated(px, py, color); 
                    if (bold) { draw_pixel_rotated(px + 1, py, color); draw_pixel_rotated(px, py + 1, color); }
                }
            }
        }
        free(bitmap);
    }
    return (int)(glyph->advance_x * scale);
}

int get_text_width_scaled(const char* string, float scale) {
    int totalWidth = 0; const char* p = string;
    GFXfont *font = useSerif ? (GFXfont *)&Crimson : (GFXfont *)&FiraSans;
    while (*p) {
        uint32_t cp = decode_utf8(&p);
        GFXglyph *glyph; get_glyph(font, cp, &glyph);
        if (glyph) totalWidth += (int)(glyph->advance_x * scale); 
        else if (cp == ' ') totalWidth += (int)(12 * scale);
    }
    return totalWidth;
}

void writeln_scaled(const char *string, int x, int y, float scale, bool bold, uint8_t color) {
    const char* p = string; int curX = x;
    while (*p) {
        uint32_t cp = decode_utf8(&p);
        curX += drawChar(cp, curX, y, scale, bold, color);
    }
}