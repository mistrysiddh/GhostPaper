# GhostPage: Minimalist E-Reader for LilyGo T5-47 S3

GhostPage is a high-performance, minimalist E-Ink reader OS designed specifically for the **LilyGo T5-47 S3** (ESP32-S3). It features a clean bookshelf interface, rapid text rendering, and low-power consumption.

## ✨ Features

-   **Modern Bookshelf UI:** Card-based library view with automatic cover formatting.
-   **Smart Navigation:** Single-tap to open an inline menu (Open, Reset, Back).
-   **High-Speed Rendering:** Custom text engine optimized for the EPD47 display.
-   **Touch Debounce Logic:** Robust protection against touch jitter and accidental double-turns.
-   **Battery Efficiency:** Automatic deep sleep mode with one-button wake-up.
-   **Format Support:** Native support for `.txt` files.
-   **RTC Integration:** Displays real-time clock synced via NTP.
-   **Format Tags:** Visual badges identifying file types (TXT/EPUB).

## 🛠️ Hardware Requirements

-   **Device:** LilyGo T5-47 S3 (EPD47 ESP32-S3)
-   **Storage:** MicroSD card (formatted to FAT32)
-   **Battery:** 3.7V LiPo (Standard JST connector)

## 🚀 Getting Started

### 1. Prerequisites
-   Install [PlatformIO](https://platformio.org/) (VS Code Extension recommended).
-   A MicroSD card with some `.txt` files in the root directory.

### 2. Configuration
Open `include/config.h` and update your Wi-Fi credentials for time synchronization:
```cpp
#define WIFI_SSID_1 "Your_SSID"
#define WIFI_PASS_1 "Your_Password"
```

### 3. Installation
1.  Connect your LilyGo device via USB.
2.  Open the project in PlatformIO.
3.  Run the **Upload** task.

## 📖 How to Use

-   **Splash Screen:** Automatically transitions to the Library after 5 seconds.
-   **Library Navigation:**
    *   **Tap a Book Card:** Opens an inline menu.
    *   **OPEN:** Start reading from your last saved position.
    *   **RESET:** Clear reading progress for that specific book.
    *   **BACK:** Return to the covers view.
    *   **Footer Taps:** Tap the left/right sides of the footer to change library pages.
-   **Reader Screen:**
    *   Only the bottom buttons (**Back**, **Prev**, **Next**) respond to touch to prevent accidental turns while holding the device.
-   **Sleep Mode:** The device enters deep sleep after 5 minutes of inactivity. Press the main button to wake up.

## 📁 File Structure

-   `src/main.cpp`: Core state machine and touch handling.
-   `src/library.cpp`: Bookshelf UI and Menu logic.
-   `src/reader.cpp`: Text rendering and file parsing.
-   `src/graphics.cpp`: Custom rotation-aware drawing primitives.
-   `include/config.h`: Hardware pins and UI constants.

## ⚖️ License

MIT License - Feel free to use and modify for personal or commercial projects.
