# GhostPage: Minimalist E-Reader for LilyGo T5-47 S3

GhostPage is a high-performance, ultra-minimalist E-Ink reader OS designed specifically for the **LilyGo T5-47 S3** (ESP32-S3). It features a clean bookshelf interface, rapid text rendering, and a distraction-free interaction model.

## ✨ Features

-   **Direct Bookshelf UI:** Tap a book card to open it immediately.
-   **Touch-Only Navigation:** Entirely controlled via the touchscreen (no physical buttons required).
-   **Fast Partial Updates:** Rapid page turns using 1-bit black-and-white rendering to minimize flickering.
-   **Auto-Cleaning:** Periodically performs a full grayscale refresh to eliminate ghosting.
-   **Smart Header:** Flicker-free partial updates for the Real-Time Clock and Battery Voltage.
-   **Native Format Support:** Optimized for reading `.txt` files from SD card.

## 🛠️ Hardware Requirements

-   **Device:** LilyGo T5-4.7 inch S3 (EPD47 ESP32-S3)
-   **Storage:** MicroSD card (formatted to FAT32)
-   **Battery:** 3.7V LiPo (Standard JST connector)

## 🚀 Getting Started

### 1. Prerequisites
-   Install [PlatformIO](https://platformio.org/).
-   Prepare a MicroSD card with `.txt` files in the root directory.

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

## 📖 Interaction Guide

-   **Splash Screen:** Automatically transitions to the Library after 5 seconds.
-   **Library Navigation:**
    *   **Tap a Book Card:** Opens the book immediately.
    *   **Footer Taps:** Tap the left/right sides of the footer to change library pages.
-   **Reader Screen:**
    *   **Next Page:** Tap the "Next" button in the footer.
    *   **Previous Page:** Tap the "Prev" button in the footer.
    *   **Exit to Library:** Tap the "Back" button in the footer.

## 📁 Project Structure

-   `src/main.cpp`: Core state machine and touch interaction logic.
-   `src/library.cpp`: Procedural book cover generation and bookshelf UI.
-   `src/reader.cpp`: High-speed text rendering engine with partial update support.
-   `src/graphics.cpp`: Rotation-aware drawing primitives.
-   `include/config.h`: Hardware definitions and UI constants.

## ⚖️ License

MIT License - Feel free to use and modify for personal or commercial projects.