@echo off
setlocal enabledelayedexpansion

:: GhostPage OS Binary Builder
:: This script compiles the project and merges the resulting binaries into a single file.

set "OUTPUT_NAME=ghostpage_os_v0.1.bin"
set "BUILD_DIR=.pio\build\lilygo-t5-47-s3"

echo --- 1. Compiling GhostPage OS ---
call pio run
if %ERRORLEVEL% neq 0 (
    echo Compilation failed.
    exit /b %ERRORLEVEL%
)

echo.
echo --- 2. Locating esptool ---

:: Try to find esptool in the PlatformIO packages
set "ESPTOOL_PATH="
for /d %%i in ("%USERPROFILE%\.platformio\packages\tool-esptoolpy*") do (
    if exist "%%i\esptool.py" (
        set "ESPTOOL_PATH=%%i\esptool.py"
        goto :found_esptool
    )
)

:: Try system path
where esptool.py >nul 2>&1
if %ERRORLEVEL% equ 0 (
    for /f "delims=" %%i in ('where esptool.py') do (
        set "ESPTOOL_PATH=%%i"
        goto :found_esptool
    )
)

echo Error: esptool.py not found. Please ensure PlatformIO is installed.
exit /b 1

:found_esptool
echo Using: !ESPTOOL_PATH!

echo.
echo --- 3. Merging Binaries into %OUTPUT_NAME% ---
:: Use python to run esptool.py
python "!ESPTOOL_PATH!" --chip esp32s3 merge_bin ^
    -o "%OUTPUT_NAME%" ^
    --flash_mode dio ^
    --flash_size 16MB ^
    0x0 "%BUILD_DIR%\bootloader.bin" ^
    0x8000 "%BUILD_DIR%\partitions.bin" ^
    0x10000 "%BUILD_DIR%\firmware.bin"

if %ERRORLEVEL% neq 0 (
    echo Merging failed.
    exit /b %ERRORLEVEL%
)

echo.
echo --- SUCCESS ---
echo Merged binary created: %OUTPUT_NAME%
echo You can flash this file starting at offset 0x0.