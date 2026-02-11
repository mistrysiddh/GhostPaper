# GhostPaper OS 📖

GhostPaper is a high-performance, minimalist E-Ink operating system designed for the **LilyGo T5-47 S3** (ESP32-S3). It transforms your device into a distraction-free productivity powerhouse and digital reader, featuring a sophisticated "Newspaper Modern" aesthetic and high-performance E-Ink optimizations.

![GhostPaper Version](https://img.shields.io/badge/Version-1.1--Stable-black?style=for-the-badge)
![Platform](https://img.shields.io/badge/Platform-ESP32--S3-orange?style=for-the-badge)
![Display](https://img.shields.io/badge/Display-EPD--4.7--Inch-green?style=for-the-badge)

## ✨ New in v1.1

### 📊 GhostBoard (System Dashboard)
A minimalist, Swiss-grid "Productivity Poster" that turns your device into a real-time desk monitor.
- **Hero Clock & Date**: Large, high-contrast digital time with a detailed date section (Day | Date | Month).
- **Dynamic Weather**: Automatic weather via `wttr.in` with custom-drawn dynamic icons (Sun, Clouds, Rain).
- **Online Quotes**: Literary wisdom fetched in real-time from `api.quotable.io`.
- **System Health**: Real-time bar indicators for Battery (Voltage) and Storage health.
- **Currently Reading**: A dedicated widget showing your current book's progress with a "Tap to Resume" shortcut.
- **Global Nav**: A persistent navigation bar for instant access to Library, Store, Sync, and WiFi.

### 🏪 GhostStore (Cloud Library)
Browse and download thousands of classics directly from the device.
- **Project Gutenberg**: Native OPDS client integration with sequential pagination.
- **Offline Caching**: Browse the previously fetched catalog even without a WiFi connection.
- **Download Progress**: Real-time progress bars for active book downloads.
- **Status Indicators**: High-contrast "READ" (already on device) and "GET" (available) status.

### 📶 GhostDrop (WiFi Transfer)
Wireless book management via a local WebServer.
- **QR Entry**: Instant access via a custom-generated QR code on the device screen.
- **Wireless Uploads**: Send `.txt` files directly to the SD card from any browser on the same network.
- **Seamless Sync**: Automatically refreshes your library once the transfer is complete.

### 🔒 Privacy & Security
- **Security PIN**: A 6-digit PIN system that secures the device upon boot and wake.
- **Auto-Unlock**: Instantly transitions to the dashboard upon entering the correct code.
- **Hardware Lock**: Long-press **IO21** to toggle touch input with a visible `[LOCKED]` status indicator.

## 📚 Core Features

### 📖 Premium Reading
- **Ultra-Fast Engine**: Highly optimized `.txt` rendering with custom font scaling (**0.4x to 2.5x**).
- **Hybrid Typography**: Seamlessly switches between Sans-Serif (FiraSans) and Serif (Crimson) for an authentic book feel.
- **Smart TOC**: Automatically generates a Table of Contents for easy navigation between chapters.
- **Reading Progress**: Persistent progress tracking with visual bars and percentage completion.
- **Anti-Ghosting**: Innovative "Physical Wash" technology that clears artifacts before complex transitions.

### 🗃️ Library Management
- **Enhanced Covers**: Premium "Floating" cover design with soft shadows, decorative borders, and unique patterns.
- **Smart Filters**: Organize your library by **ALL**, **NEW**, **READING**, or **FINISHED**.
- **Action Menus**: Long-tap any book to READ, RESET progress, or DELETE.

### 🖼️ Intelligent Screensaver
- **Auto-Lock**: Secures the device after 5 minutes of inactivity.
- **Custom Art**: Displays a centered or stretched monochrome illustration (`dashboard.bmp`) with a full hardware refresh.

## 🛠 Technical Stack

- **Framework**: Arduino / PlatformIO (ESP32-S3)
- **Graphics Core**: Custom rotated drawing engine with `MALLOC_CAP_SPIRAM` for high-resolution 4-bit grayscale buffers.
- **Touch Driver**: GT911 with manual portrait mapping.
- **Timekeeping**: PCF8563 RTC for persistent time and date.
- **Libraries**: Button2 (Hardware buttons), QRCode (GhostDrop), TinyXML2 (OPDS Parsing).

## 🏗 Codebase Architecture

- **`main.cpp`**: System kernel, state machine, and power management.
- **`dashboard.cpp`**: Newspaper-style UI and background API fetching (Weather/Quotes).
- **`reader.cpp` / `toc.cpp`**: Reading engine, text wrapping, and chapter indexing.
- **`library.cpp`**: Enhanced bookshelf rendering and library state management.
- **`store.cpp` / `opds_client.cpp`**: Cloud store integration and file downloading.
- **`graphics.cpp`**: Low-level rotated primitives and BMP scaling engine.

## 🚀 Installation

1. **Prerequisites**: Install VS Code + PlatformIO.
2. **SD Card**: 
   - Format to FAT32.
   - Create an `images/` folder and place `dashboard.bmp` (540x960 or similar) inside.
3. **Configuration**: Set your default WiFi credentials in `include/config.h` (optional, can be set via UI).
4. **Build**: Use the PlatformIO "Upload" task to flash the firmware.
5. **Setup**: Follow the on-screen prompt to initialize your 6-digit security PIN.

---
*GhostPaper OS - Optimized for the love of reading and focus.*
