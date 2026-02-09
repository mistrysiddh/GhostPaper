#!/bin/bash

# GhostPage OS Binary Builder
# This script compiles the project and merges the resulting binaries into a single file.

set -e

OUTPUT_NAME="ghostpage_os_v0.1.bin"
BUILD_DIR=".pio/build/lilygo-t5-47-s3"

echo "--- 1. Compiling GhostPage OS ---"
pio run

echo ""
echo "--- 2. Locating esptool ---"
# Try to find esptool in the PlatformIO packages or system path
ESPTOOL_PATH=$(find ~/.platformio/packages/tool-esptoolpy -name "esptool.py" | head -n 1)

if [ -z "$ESPTOOL_PATH" ]; then
    if command -v esptool.py &> /dev/null; then
        ESPTOOL_PATH="esptool.py"
    else
        echo "Error: esptool.py not found. Please ensure PlatformIO is installed."
        exit 1
    fi
fi

echo "Using: $ESPTOOL_PATH"

echo ""
echo "--- 3. Merging Binaries into $OUTPUT_NAME ---"
$ESPTOOL_PATH --chip esp32s3 merge_bin 
    -o "$OUTPUT_NAME" 
    --flash_mode dio 
    --flash_size 16MB 
    0x0 "$BUILD_DIR/bootloader.bin" 
    0x8000 "$BUILD_DIR/partitions.bin" 
    0x10000 "$BUILD_DIR/firmware.bin"

echo ""
echo "--- SUCCESS ---"
echo "Merged binary created: $OUTPUT_NAME"
echo "You can flash this file starting at offset 0x0."
