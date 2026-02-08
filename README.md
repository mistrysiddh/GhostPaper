# GhostPage OS 📖

GhostPage is a high-performance, minimalist E-Ink operating system designed for the **LilyGo T5-47 S3** (ESP32-S3). It transforms your device into a distraction-free, premium digital reader with a focus on typography, smooth transitions, and a clean "Bookshelf" aesthetic.

![GhostPage Splash](https://img.shields.io/badge/OS-GhostPage-black?style=for-the-badge)
![Platform](https://img.shields.io/badge/Platform-ESP32--S3-orange?style=for-the-badge)
![Display](https://img.shields.io/badge/Display-EPD--4.7--Inch-green?style=for-the-badge)

## ✨ Features

### 📚 Premium Reading Experience
- **Bookshelf UI**: A clean, grid-based library with "Enhanced Card" styling.
- **Smart Organization**: Automatic folders for **NEW**, **READING**, and **FINISHED** books.
- **GhostDrop (Wireless Sync)**: WiFi-powered book uploads via a simple web interface and QR code.
- **Ultra-Fast Engine**: Highly optimized `.txt` rendering with custom font scaling and zero-latency page turns.
- **Performance-First Core**: Scrubbed codebase with minimized memory footprint and efficient power-state management.
- **Anti-Ghosting**: Innovative "Physical Wash" technology for secure, trace-free transitions.

### 🔒 Privacy & Security
- **Borderless Master Lock**: A sleek, minimalist PIN pad protecting your digital library.
- **Mandatory Setup**: Secure 6-digit PIN initialization on first boot.
- **Auto-Secure**: Privacy screen activates after 5 minutes of inactivity.

### 🔋 Extreme Power Efficiency
- **Deep Sleep Mode**: Zero-power state after 10 minutes of inactivity.
- **Instant Resume**: Wakes up to your exact reading position via hardware button (**IO21**).
- **E-Ink Persistence**: Content remains on screen indefinitely without battery drain.

## 🛠 Hardware Support

Engineered exclusively for the **LilyGo T5-47 S3** (4.7-inch E-Paper display).
- **MCU**: ESP32-S3 (240MHz, PSRAM-enabled).
- **Storage**: SD Card (FAT32) support.
- **Touch**: GT911 Capacitive Touch.
- **RTC**: PCF8563 for synchronized system time.

## 🚀 Installation

1. **Prerequisites**: Install [VS Code](https://code.visualstudio.com/) + [PlatformIO](https://platformio.org/).
2. **SD Card**: Format to **FAT32** and place your `.txt` files in the root.
3. **Build & Upload**:
   - Connect your LilyGo via USB-C.
   - Run the upload command: `.venv\Scripts\pio.exe run --target upload`
4. **First Boot**: Follow the on-screen prompt to set your 6-digit PIN.

## 🎮 Controls

### Global
- **Tap**: Select / Action.
- **Hardware Button (IO21)**: System Wake / Resume.

### Library
- **Tap Header ("MY LIBRARY")**: Access **Library Menu** (Navigation, Refresh, Sync).
- **Tab Bar**: Filter books by status (**ALL**, **NEW**, **READING**, **FINISHED**).
- **Tap Book Card**: Open options (**READ**, **RESET**, **DELETE**, **BACK**).
- **Footer Arrows**: Seamlessly scroll through multiple bookshelf pages.

### Reader
- **Header ("MENU")**: Access **Reader Menu** (Font Scaling, Refresh, Sync).
- **Tap Text Area**: Quick-toggle the Reader Menu.
- **Navigation Bar**: Dedicated **Back**, **Prev**, and **Next** buttons.

## 🤝 Contributing

We welcome contributions to GhostPage OS! When submitting changes, please follow this workflow:

1. **Branch**: Create a feature branch for your changes.
2. **Commit**: Keep commits atomic and use descriptive messages.
   ```bash
   git add .
   git commit -m "feat: add [feature name]"
   ```
3. **Standards**: Ensure code follows existing patterns and includes necessary tests.

---
*GhostPage OS v0.1 - Optimized for the love of reading.*
