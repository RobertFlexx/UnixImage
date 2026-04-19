#!/bin/bash

set -e

INSTALL_DIR="${INSTALL_PREFIX:-/usr/local}"
BIN_DIR="$INSTALL_DIR/bin"
SHARE_DIR="$INSTALL_DIR/share/applications"
ICON_DIR="$INSTALL_DIR/share/icons/hicolor"
ICON_DIR_48="$INSTALL_DIR/share/icons/hicolor/48x48/apps"
ICON_DIR_128="$INSTALL_DIR/share/icons/hicolor/128x128/apps"
ICON_DIR_256="$INSTALL_DIR/share/icons/hicolor/256x256/apps"
DESKTOP_FILE_DIR="$INSTALL_DIR/share/applications"

echo "Installing UnixImage..."

if [ "$(id -u)" -ne 0 ]; then
    echo "Error: Run as root or set INSTALL_PREFIX"
    exit 1
fi

echo "Compiling CLI version..."
cc -o uniximage-cli uniximage.c -DCLI_MODE -lpthread

if pkg-config --cflags --libs gtk+-3.0 >/dev/null 2>&1; then
    echo "Compiling GUI version..."
    cc -o uniximage uniximage.c $(pkg-config --cflags --libs gtk+-3.0) -lpthread
else
    echo "Warning: GTK3 not found, GUI version not compiled"
    echo "To compile GUI, install GTK3 development packages"
fi

mkdir -p "$BIN_DIR"
mkdir -p "$DESKTOP_FILE_DIR"
mkdir -p "$ICON_DIR_48"
mkdir -p "$ICON_DIR_128"
mkdir -p "$ICON_DIR_256"
mkdir -p "$ICON_DIR/scalable/apps"

install -m755 uniximage "$BIN_DIR/"
install -m755 uniximage-cli "$BIN_DIR/"

install -m644 uniximage.desktop "$DESKTOP_FILE_DIR/"
install -m644 uniximage-48.png "$ICON_DIR_48/"
install -m644 uniximage-128.png "$ICON_DIR_128/"
install -m644 uniximage.png "$ICON_DIR_256/"
install -m644 uniximage.svg "$ICON_DIR/scalable/apps/"

update-desktop-database "$DESKTOP_FILE_DIR" 2>/dev/null || true

echo "UnixImage installed to $BIN_DIR/"