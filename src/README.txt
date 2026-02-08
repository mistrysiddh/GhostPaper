# GhostPage OS 📖

GhostPage is a high-performance, minimalist E-Ink operating system
designed for the **LilyGo T5-47 S3** (ESP32-S3). It transforms your
device into a distraction-free, premium digital reader with a focus on
typography, smooth transitions, and a clean "Bookshelf" aesthetic.

![GhostPage
Splash](https://img.shields.io/badge/OS-GhostPage-black?style=for-the-badge)
![Platform](https://img.shields.io/badge/Platform-ESP32--S3-orange?style=for-the-badge)
![Display](https://img.shields.io/badge/Display-EPD--4.7--Inch-green?style=for-the-badge)

## ✨ Features

### 📚 Premium Reading Experience

-   **Bookshelf UI**: A clean, grid-based library with "Enhanced Card"
    styling.
-   **Contextual Menus**: Tap any book for instant options (Read, Reset
    Progress, Delete).
-   **Ultra-Fast Engine**: Optimized `.txt` rendering with customizable
    font scaling.
-   **Global Touch Feedback**: Responsive, circular "physical wash"
    indicators for every interaction.

### 🔒 Privacy & Security

-   **Secure Lock Screen**: A "Master Card" style PIN pad that protects
    your library.
-   **Mandatory Setup**: Forces a custom 4-digit PIN creation on first
    boot.
-   **Anti-Ghosting Keypad**: Innovative localized refresh technology
    for secure, trace-free PIN entry.
-   **Auto-Lock**: Automatically secures the device after 5 minutes of
    inactivity.

### 📊 Analytics Dashboard

-   **Reading Stats**: Tracks your "Day Streak" and "Total Pages Read".
-   **Activity Heatmap**: Visualizes your reading habits over the last 7
    days.
-   **Automatic Logging**: Silently tracks progress as you turn pages.

### 🔋 Extreme Power Efficiency

-   **Deep Sleep Mode**: Automatically enters zero-power deep sleep
    after 10 minutes.
-   **Instant Resume**: Wakes up instantly to your last page with a
    single button press.
-   **Weeks of Battery**: Optimized for long-term usage without
    charging.

## 🛠 Hardware Support

Specifically optimized for the **LilyGo T5-47 S3** (4.7-inch E-Paper
display). - **MCU**: ESP32-S3 - **Storage**: SD Card (FAT32) for
books. - **Touch**: GT911 Capacitive Touch. - **RTC**: PCF8563 for
accurate timekeeping.

## 🚀 Installation

1.  **Prerequisites**: Install VS Code and the [PlatformIO
    extension](https://platformio.org/).
2.  **SD Card**: Format to **FAT32** and drop your `.txt` files in the
    root folder.
3.  **Build & Upload**:
    -   Open this folder in VS Code.
    -   Connect your LilyGo via USB-C.
    -   Click the **PlatformIO Upload** arrow (→).
4.  **First Boot**: Follow the on-screen prompt to set your generic
    4-digit PIN.

## 🎮 Controls

### Global

-   **Tap**: Select / Act.
-   **Hardware Button (IO21)**: Wake from Sleep.

### Library

-   **Tap Header**: Open **Analytics Dashboard**.
-   **Tap Footer**: Change bookshelf pages.

### Reader

-   **Tap Text**: Open **Reader Menu** (Font +/-, Refresh, Reset).
-   **Tap Bottom Buttons**: Navigation (Back, Prev, Next).

------------------------------------------------------------------------

*GhostPage OS v1.1 - Crafted for the love of reading.*
