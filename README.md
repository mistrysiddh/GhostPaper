# GhostPage OS 📖

GhostPage is a high-performance, minimalist E-Ink operating system designed for the **LilyGo T5-47 S3** (ESP32-S3). It transforms your device into a distraction-free, premium digital reader with a focus on typography, smooth transitions, and a clean "Bookshelf" aesthetic.

![GhostPage Splash](https://img.shields.io/badge/OS-GhostPage-black?style=for-the-badge)
![Platform](https://img.shields.io/badge/Platform-ESP32--S3-orange?style=for-the-badge)
![Display](https://img.shields.io/badge/Display-EPD--4.7--Inch-green?style=for-the-badge)

## ✨ Features

### 📚 Premium Reading Experience
- **Bookshelf UI**: A clean, grid-based library with "Enhanced Card" styling.
- **Smart Organization**: Automatic folders for **NEW**, **READING**, and **FINISHED** books.
- **System Books**: Pre-loaded, undeletable core library content (e.g., *Echoes of the Code*).
- **GhostDrop (Wireless Sync)**: WiFi-powered book uploads via a simple web interface and QR code.
- **Ultra-Fast Engine**: Optimized `.txt` rendering with customizable font scaling.
- **Anti-Ghosting**: Innovative "Physical Wash" technology for secure, trace-free page turns and menu transitions.

### 🔒 Privacy & Security
- **Borderless Master Lock**: A sleek, minimalist PIN pad that protects your library.
- **Mandatory Setup**: Guided custom 6-digit PIN creation on first boot.
- **Privacy Screen**: Automatically secures the device after 5 minutes of inactivity.

### 🔋 Extreme Power Efficiency
- **Deep Sleep Mode**: Automatically enters zero-power deep sleep after 10 minutes of inactivity.
- **Instant Resume**: Wakes up instantly to your last page with a single button press (**IO21**).
- **Zero-Power Display**: Holds the last screen indefinitely without consuming battery.

## 🛠 Hardware Support

Specifically optimized for the **LilyGo T5-47 S3** (4.7-inch E-Paper display).
- **MCU**: ESP32-S3
- **Storage**: SD Card (FAT32) for books.
- **Touch**: GT911 Capacitive Touch.
- **RTC**: PCF8563 for accurate timekeeping.

## 🚀 Installation

1. **Prerequisites**: Install VS Code and the [PlatformIO extension](https://platformio.org/).
2. **SD Card**: Format to **FAT32** and drop your `.txt` files in the root folder.
3. **Build & Upload**:
   - Open this folder in VS Code.
   - Connect your LilyGo via USB-C.
   - Click the **PlatformIO Upload** arrow (→).
4. **First Boot**: Follow the on-screen prompt to set your generic 6-digit PIN.

## 🎮 Controls

### Global
- **Tap**: Select / Act.
- **Hardware Button (IO21)**: Wake from Deep Sleep.

### Library
- **Tap Header ("MY LIBRARY")**: Open **Library Menu** (Next Page, Prev Page, Refresh, Sync).
- **Tab Bar**: Filter by **ALL**, **NEW**, **READING**, or **FINISHED**.
- **Tap Book**: Open book options (READ, RESET, DELETE, BACK).
- **Footer Arrows**: Tap "< PREV" or "NEXT >" to scroll through your books.

### Reader
- **Header ("MENU")**: Open **Reader Menu** (Font +/-, Refresh, Sync).
- **Tap Text**: Also opens the **Reader Menu**.
- **Tap Bottom Buttons**: Navigation (Back, Prev, Next).

---
*GhostPage OS v1.2 - Crafted for the love of reading.*
