#!/bin/bash

set -e

INSTALL_DIR="${INSTALL_PREFIX:-/usr/local}"
BIN_DIR="$INSTALL_DIR/bin"
DESKTOP_FILE_DIR="$INSTALL_DIR/share/applications"

echo "Installing UnixImage..."

if [ "$(id -u)" -ne 0 ]; then
    echo "Error: Run as root or set INSTALL_PREFIX"
    exit 1
fi

echo "Compiling CLI version..."
cc -o uniximage-cli uniximage.c -DCLI_MODE -lpthread

GUI_BUILT=0
if pkg-config --cflags --libs gtk+-3.0 >/dev/null 2>&1; then
    echo "Compiling GUI version..."
    cc -o uniximage uniximage.c $(pkg-config --cflags --libs gtk+-3.0) -lpthread
    GUI_BUILT=1
else
    echo "Warning: GTK3 not found, GUI version not compiled"
    echo "To compile GUI, install GTK3 development packages"
fi

mkdir -p "$BIN_DIR"
mkdir -p "$DESKTOP_FILE_DIR"

if [ "$GUI_BUILT" -eq 1 ]; then
    install -m755 uniximage "$BIN_DIR/"
else
    install -m755 uniximage-cli "$BIN_DIR/uniximage"
fi
install -m755 uniximage-cli "$BIN_DIR/"

cat > "$DESKTOP_FILE_DIR/uniximage.desktop" <<EOF
[Desktop Entry]
Version=1.0
Type=Application
Name=UnixImage Writer
GenericName=ISO Image Writer
Comment=Write ISO images to USB drives
Exec=$BIN_DIR/uniximage %f
TryExec=$BIN_DIR/uniximage
Terminal=false
Categories=System;DiskUtility;
MimeType=application/x-iso-image;
Keywords=usb;iso;burn;write;disk;image;
StartupNotify=false
EOF
chmod 644 "$DESKTOP_FILE_DIR/uniximage.desktop"

update-desktop-database "$DESKTOP_FILE_DIR" 2>/dev/null || true

echo "UnixImage installed to $BIN_DIR/"
