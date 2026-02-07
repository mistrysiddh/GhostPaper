# GhostPage OS 📖

GhostPage is a high-performance, minimalist E-Ink operating system designed for the **LilyGo T5-47 S3** (ESP32-S3). It transforms your device into a distraction-free, premium digital reader with a focus on typography, smooth transitions, and a clean "Bookshelf" aesthetic.

![GhostPage Splash](https://img.shields.io/badge/OS-GhostPage-black?style=for-the-badge)
![Platform](https://img.shields.io/badge/Platform-ESP32--S3-orange?style=for-the-badge)
![Display](https://img.shields.io/badge/Display-EPD--4.7--Inch-green?style=for-the-badge)

## ✨ Features

- **Premium Bookshelf UI**: A clean, grid-based library with "Enhanced Card" styling, featuring drop shadows and decorative borders.
- **Contextual Card Menus**: Library-style menus that appear directly on covers for reading, resetting progress, or closing.
- **Ultra-Fast Reader Engine**: Optimized `.txt` rendering with customizable font scaling.
- **Anti-Ghosting Technology**: 
  - **Physical Wash**: A triple-flash clearing technique for the main text area to eliminate remnants of previous pages.
  - **Grayscale Refresh**: High-quality full-screen refreshes when needed.
- **Global Touch Feedback**: Immediate visual confirmation for every tap using localized, high-speed indicators (Circle/Dot style).
- **Minimalist Reader Menu**: Full-body overlay menu in the reader for FONT +/- adjustments, full refreshes, and quick navigation.
- **Smart Power Management**: Battery voltage monitoring and deep-sleep ready architecture.

## 🛠 Hardware Support

Specifically optimized for the **LilyGo T5-47 S3** (4.7-inch E-Paper display).
- **MCU**: ESP32-S3
- **Storage**: SD Card support for book storage (FAT32 recommended).
- **Touch**: GT911 Capacitive Touch support.
- **RTC**: PCF8563 for timekeeping.

## 🚀 Installation & Uploading

Follow these steps to get GhostPage running on your device:

### 1. Prerequisites
- **Visual Studio Code (VS Code)**: [Download and install here](https://code.visualstudio.com/).
- **PlatformIO IDE Extension**: 
  1. Open VS Code.
  2. Click on the **Extensions** icon on the left sidebar (or press `Ctrl+Shift+X`).
  3. Search for "PlatformIO IDE" and click **Install**.
- **USB-C Cable**: A high-quality data cable to connect the board to your computer.

### 2. Prepare the SD Card
- Format your SD card to **FAT32**.
- Create a folder (optional) or simply drop your `.txt` files onto the root of the card.
- Insert the card into the LilyGo T5-47 S3.

### 3. Open the Project
- Download or clone this repository to your computer.
- In VS Code, go to **File > Open Folder...** and select the `Lilygo` project directory.
- Wait for PlatformIO to initialize and download the necessary libraries.

### 4. Configuration (Optional)
- Open `include/config.h` to adjust settings like:
  - `#define TAP_INDICATOR_STYLE`: Set to `1` for the dot indicator.
  - WiFi credentials for future NTP/RSS sync features.

### 5. Build and Upload
1. Connect your LilyGo T5-47 S3 to your computer via USB-C.
2. Look at the bottom status bar in VS Code:
   - Click the **Checkmark icon** (✔) to **Build** the code and check for errors.
   - Click the **Arrow icon** (→) to **Upload** the firmware to your board.
3. If the upload fails, ensure the correct port is selected or try putting the board into "Bootloader Mode" by holding the **Boot** button while plugging in the USB cable.

## 🎮 Controls

### Library Screen
- **Tap Book**: Opens the options menu (READ, RESET, BACK).
- **Footer Navigation**: Tap left/right areas of the footer to change bookshelf pages.

### Reader Screen
- **Tap Main Body**: Opens the Reader Menu (Font Scale, Refresh, etc.).
- **Footer Buttons**:
  - **Back**: Exit to Library.
  - **Prev/Next**: Turn pages.

---
*Created with focus on minimalist design and performance.*