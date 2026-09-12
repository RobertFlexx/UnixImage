<h1 align="center">UnixImage</h1>
<p align="center"><strong>Portable by standard. Break a leg!</strong></p>

<p align="center">
  A cross-platform disk image writer for Unix and Unix-like systems, with both GTK GUI and CLI modes.
</p>

UnixImage is a straightforward image writer built in C for people who want a native-feeling tool instead of a giant kitchen-sink app.

It is designed to write disk images to removable media with a GUI when GTK3 is available, while still supporting a CLI-only build for lean systems and minimal environments.

## What it does

* Writes disk images to target devices
* Supports both GUI and CLI builds
* Detects many common disk image formats
* Handles several compressed image formats
* Includes optional verification after writing
* Includes sync-write support
* Exposes block size, partition, and bootable options in the GUI
* Ships with desktop integration assets and an install script

## Project layout (just so ya know)

* `uniximage.c` - main source file
* `install.sh` - install helper that builds and installs the app
* `uniximage.desktop` - desktop entry
* `uniximage.svg` - scalable app icon
* `uniximage.png` - 256x256 icon
* `uniximage-128.png` - 128x128 icon
* `uniximage-48.png` - 48x48 icon
* `LICENSE` - GPLv3
* `README.md` - this file

## Platform status (tested vs untested)

### Tested

* Linux
* FreeBSD

### Implemented in source

* Linux
* macOS
* FreeBSD
* DragonFly BSD

### Experimental or not fully verified

* OpenBSD
* NetBSD
* Solaris / illumos
* AIX
* HP-UX
* Other Unix-like targets may compile or partially work, but are not guaranteed

## Supported image formats

### Disk images

* ISO
* IMG
* RAW
* DMG
* VHD
* VHDX
* VDI
* VMDK
* QCOW
* QCOW2
* WIM
* SWM

### Compressed images

* XZ
* GZ
* BZ2
* ZSTD
* LZ4
* ZIP

## Dependencies

GUI mode requires GTK3.

CLI mode only requires a C compiler, libc, and pthreads.

## Building

### GUI build

```bash
cc -o uniximage uniximage.c $(pkg-config --cflags --libs gtk+-3.0) -lpthread
```

### CLI-only build

```bash
cc -o uniximage uniximage.c -DCLI_MODE -lpthread
```

## Build examples

### Debian / Ubuntu

```bash
apt install build-essential libgtk-3-dev pkg-config
cc -o uniximage uniximage.c $(pkg-config --cflags --libs gtk+-3.0) -lpthread
```

### Fedora / RHEL

```bash
dnf install gcc gtk3-devel pkgconf-pkg-config
cc -o uniximage uniximage.c $(pkg-config --cflags --libs gtk+-3.0) -lpthread
```

### Arch Linux

```bash
pacman -S base-devel gtk3 pkgconf
cc -o uniximage uniximage.c $(pkg-config --cflags --libs gtk+-3.0) -lpthread
```

### FreeBSD

```bash
pkg install gtk3 pkgconf
cc -o uniximage uniximage.c $(pkg-config --cflags --libs gtk+-3.0) -lpthread
```

### OpenBSD

```bash
pkg_add gtk+3 pkgconf
cc -o uniximage uniximage.c $(pkg-config --cflags --libs gtk+-3.0) -lpthread
```

### NetBSD

```bash
pkgin install gtk3 pkgconf
cc -o uniximage uniximage.c $(pkg-config --cflags --libs gtk+-3.0) -lpthread
```

### macOS

```bash
brew install gtk+3 pkg-config
cc -o uniximage uniximage.c $(pkg-config --cflags --libs gtk+-3.0) -lpthread
```

## Install

The repository includes an install script that builds the CLI version, attempts the GTK GUI build, and installs the binary and desktop file under `/usr/local` by default.

```bash
sudo ./install.sh
```

To install somewhere else:

```bash
sudo INSTALL_PREFIX=/usr ./install.sh
```

## Troubleshooting

### The app closes when clicking "Browse"

The GUI's file chooser is GTK3's built-in `GtkFileChooser`, which needs a working
gdk-pixbuf image-loader setup on the system. If gdk-pixbuf's loaders are broken or
incomplete (for example the SVG loader fails with `Unsupported image format`), GTK3
aborts with an assertion from `gtkiconhelper.c` and the whole app closes.

Fix the system gdk-pixbuf stack instead:

```bash
prt-get reinstall gdk-pixbuf librsvg           # CRUX
gdk-pixbuf-query-loaders --update-cache
```

Notes:

* Zenity is **not** a dependency of this app and does not fix this crash. It only
  *appears* to work because modern zenity is a GTK4/libadwaita app that renders
  icons via librsvg directly, bypassing gdk-pixbuf.
* If your distribution ships a GTK4 version of zenity, it can be used as a plain
  file-selection tool (`zenity --file-selection`), but the GUI's Browse button
  still relies on GTK3 and thus on a repaired gdk-pixbuf.

## Usage

### GUI

```bash
sudo ./uniximage
```

### CLI

```bash
sudo ./uniximage -i image.iso -d /dev/sdX
sudo ./uniximage -i image.iso -d /dev/sdX -V -y
sudo ./uniximage --list
```

## Notes

* Root privileges are generally required for writing directly to block devices
* GTK3 is optional, not mandatory
* Linux and FreeBSD are the currently tested targets
* Other targets should be treated as experimental until verified

## License

GPLv3

## Prologue

I understand, and im fully aware im highly completionist. I aim for posix standard philosophy for Unix!
