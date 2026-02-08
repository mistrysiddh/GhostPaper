#ifndef CONFIG_H
#define CONFIG_H

// --- Debug ---
#define DEBUG_ON true

// --- WiFi Settings ---
#define WIFI_SSID_1 "Mistry Siddh"
#define WIFI_PASS_1 "hello@635241"

#define WIFI_SSID_2 "OPTIONAL_SSID"
#define WIFI_PASS_2 "OPTIONAL_PASSWORD"

#define WIFI_SSID_3 "OPTIONAL_SSID"
#define WIFI_PASS_3 "OPTIONAL_PASSWORD"

// --- Time Settings ---
#define NTP_SERVER "pool.ntp.org"
#define GMT_OFFSET_SEC 19800 // IST (GMT +5:30)
#define DAYLIGHT_OFFSET_SEC 0

// --- Hardware Pins (LilyGo T5-47 S3) ---
#undef SD_MISO
#undef SD_MOSI
#undef SD_SCLK
#undef SD_CS
#undef TOUCH_SDA
#undef TOUCH_SCL
#undef TOUCH_INT
#undef TOUCH_RST
#undef BUTTON_1
#undef BATT_PIN

#define SD_MISO     16
#define SD_MOSI     15
#define SD_SCLK     11
#define SD_CS       42

#define TOUCH_SDA   18
#define TOUCH_SCL   17
#define TOUCH_INT   47
#define TOUCH_RST   -1

#define BUTTON_1    21 
#define BATT_PIN    14

// --- Display Layout ---
#define L_WIDTH    960
#define L_HEIGHT   540
#define P_WIDTH    540
#define P_HEIGHT   960

#define MARGIN       30

#define HEADER_HEIGHT 80

#define FOOTER_HEIGHT 60

#define LINE_HEIGHT   42 



// --- UI Layout Constants ---
#define BUTTON_W 130
#define BUTTON_H 50
#define BUTTON_GAP 20
#define BTN_Y_POS 890

// --- Colors ---



#define COL_BLACK 0x00



#define COL_DARK  0x44



#define COL_GRAY  0x88



#define COL_LIGHT 0xBB



#define COL_WHITE 0xFF



#define COL_BG    0xFF







// --- Bookshelf Layout Constants ---







#define TAP_INDICATOR_STYLE 1







#define SHELF_COLS       2







#define SHELF_ROWS       2















#define SHELF_BOOKS_PER_PAGE (SHELF_COLS * SHELF_ROWS)







#define BOOK_W           230







#define BOOK_H           320







#define GAP_X            26















#define GAP_Y            70















#define SHELF_START_Y    120















#endif




