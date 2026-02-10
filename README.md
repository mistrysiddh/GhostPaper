# GhostPaper OS 📖

GhostPaper is a high-performance, minimalist E-Ink operating system designed for the **LilyGo T5-47 S3** (ESP32-S3). It transforms your device into a distraction-free productivity powerhouse and digital reader, featuring a sophisticated "Newspaper Modern" aesthetic and high-performance E-Ink optimizations.

![GhostPaper Version](https://img.shields.io/badge/Version-1.0--Stable-black?style=for-the-badge)
![Platform](https://img.shields.io/badge/Platform-ESP32--S3-orange?style=for-the-badge)
![Display](https://img.shields.io/badge/Display-EPD--4.7--Inch-green?style=for-the-badge)

## ✨ New in v1.0

### 📊 GhostBoard (System Dashboard)
A minimalist, Swiss-grid "Productivity Poster" that turns your device into a real-time desk monitor.
- **Hero Clock**: Large, high-contrast digital time and elegant date.
- **Keyless Weather**: Automatic location-based weather via `wttr.in` (No API keys required).
- **Outlook Sync**: Live integration with Outlook ICS feeds to display your upcoming agenda.
- **Online Quotes**: Rotating literary wisdom fetched in real-time from `api.quotable.io`.
- **Hacker Stats**: Labeled bar indicators for real-time CPU, RAM, and Battery health.
- **Global Nav**: A persistent one-tap navigation bar to jump between all OS modules.

### 🏪 GhostStore (Cloud Library)
Browse and download thousands of classics directly from the device.
- **Project Gutenberg**: Native OPDS client integration.
- **Deep Pagination**: Optimized fetching logic that gathers up to 500 books in a single pass.
- **Smart Sync**: Track downloaded books with high-contrast "READ" status indicators.

### 🖼️ Intelligent Screensaver
- **Soft Lock**: Automatically secures the device after 5 minutes of inactivity.
- **Full-Screen Art**: Displays a crisp, centered monochrome illustration (`dashboard.bmp`) with a full hardware refresh.
- **Scaling Engine**: Custom Nearest-Neighbor algorithm to stretch 1-bit art to the full 540x960 resolution.

## 📚 Core Features

### 📖 Premium Reading
- **Ultra-Fast Engine**: Highly optimized `.txt` rendering with custom font scaling and zero-latency page turns.
- **Hybrid Typography**: Seamlessly switches between Sans-Serif (FiraSans) and Serif (Crimson) for an authentic book feel.
- **GhostDrop**: Wireless book uploads via local WebServer and QR code.
- **Anti-Ghosting**: Innovative "Physical Wash" and "Area Refresh" technology for secure, trace-free transitions.

### 🔒 Privacy & Security
- **Auto-Unlock**: 6-digit PIN system that unlocks instantly upon completing the code.
- **Hardware Lock**: Long-press IO21 to toggle touch input with a visible `[LOCKED]` status indicator.

## 🛠 Technical Stack

- **Framework**: Arduino / PlatformIO (ESP32-S3)
- **Graphics Core**: Custom rotated drawing engine with `epd_draw_grayscale_image_area` for distortion-free partial updates.
- **Storage**: SD Card (FAT32) for books/images and NVS Preferences for system state.
- **Memory**: Aggressive PSRAM utilization via `MALLOC_CAP_SPIRAM` for high-resolution bitmap scaling.

## 🏗 Codebase Architecture

- **`dashboard.cpp`**: Manages the Newspaper-style UI, API fetching, and calendar parsing.
- **`opds_client.cpp`**: Handles XML/OPDS parsing, sequential pagination, and file downloads.
- **`graphics.cpp`**: The rendering engine, including rotated primitives and the BMP scaling engine.
- **`main.cpp`**: System kernel handling the state machine, power management, and touch mapping.

## 📋 Key Constraints & Patterns

1. **Rotation**: Hardware is landscape (960x540); UI is portrait (540x960). All drawing MUST use `_rotated` helpers.
2. **Buffer Management**: Use `epd_draw_grayscale_image_area` for partial updates to prevent buffer misalignment.
3. **E-Ink Efficiency**: Use "Physical Washes" (repeatedly pushing pixels) to clear ghosting before drawing complex menus.

## 🚀 Installation

1. **Prerequisites**: Install VS Code + PlatformIO.
2. **SD Card**: Create an `images/` folder and place `dashboard.bmp` inside.
3. **Build**: Use the PlatformIO "Upload" task to flash the firmware.
4. **Setup**: Follow the on-screen prompt to initialize your security PIN.

---
*GhostPaper OS - Optimized for the love of reading and focus.*