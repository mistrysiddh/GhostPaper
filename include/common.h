#ifndef COMMON_H
#define COMMON_H

#include <Arduino.h>
#include <vector>
#include <Preferences.h>
#include <TouchDrvGT911.hpp>
#include <pcf8563.h>
#include <SD.h>
#include "epd_driver.h"
#include "config.h"

// --- State ---
enum AppState { STATE_SPLASH, STATE_PRIVACY, STATE_SET_PIN, STATE_LIBRARY, STATE_READING, STATE_BOOK_OPTIONS, STATE_READER_OPTIONS, STATE_LIBRARY_OPTIONS, STATE_GHOSTDROP };
enum LibFilter { FILTER_ALL, FILTER_NEW, FILTER_READING, FILTER_FINISHED };

extern RTC_DATA_ATTR AppState appState;
extern RTC_DATA_ATTR LibFilter libraryFilter;
extern RTC_DATA_ATTR int librarySelection; 
extern RTC_DATA_ATTR int currentFileIndex;
extern RTC_DATA_ATTR long textPos;
extern RTC_DATA_ATTR float fontScale;
extern RTC_DATA_ATTR long lastPageByteCount;
extern RTC_DATA_ATTR int focusedBookIndex;

// --- Shared Objects ---
extern uint8_t *framebuffer;
extern std::vector<String> books;
extern std::vector<String> filteredBooks;
extern Preferences prefs;
extern TouchDrvGT911 touch;
extern PCF8563_Class rtc;
extern std::vector<long> pageHistory;
extern RTC_DATA_ATTR bool touchEnabled;

// --- Prototypes ---
void updateLibrary();
void partialUpdateLibraryHeader();
void applyLibraryFilter();
void drawEnhancedBookCover(int x, int y, String title, int progress, int bookIndex);
void updateReader(bool partial_refresh = false);
void partialUpdateHeader();
long renderPage(const char* text, int startX, int startY, int maxWidth, int maxHeight);
void openBook();
void handleNext();
String getPrefKey(String path);
float getBatteryVoltage();
String getTimeString();
void scanFiles(String path);
void showTransitionEffect();
void updateBookCardMenu(int index, bool showMenu);
void updateReaderMenu(bool showMenu);
void updateLibraryMenu(bool showMenu);
void redrawReaderText();
void forceFullRefresh();
void showFinishedScreen();
void drawPinPad(bool settingNew = false);
void handlePinTouch(int x, int y);
void goToDeepSleep();
void startGhostDrop();
void stopGhostDrop();
void handleGhostDropTouch(int x, int y);
void drawGhostDropUI(String status);

// --- Graphics Prototypes ---
void draw_pixel_rotated(int16_t x, int16_t y, uint8_t gray);
void draw_line_rotated(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t color);
void fill_rect_rotated(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t gray);
void draw_rect_rotated(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t gray);
void draw_rounded_rect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint8_t gray);
void draw_circle_rotated(int16_t xm, int16_t ym, int16_t r, uint8_t color);
void writeln_scaled(const char *string, int x, int y, float scale, bool bold, uint8_t color);
int get_text_width_scaled(const char* string, float scale);
uint32_t decode_utf8(const char** s);

#endif
