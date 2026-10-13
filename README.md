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

CLI mode requires a C99 compiler, libc, and pthreads. The installer probes compiler,
thread, and clock linker flags; older systems may also need librt. GUI builds need
GTK3 development files and pkg-config (or pkgconf).

Graphical disk writes use sudo with a built-in GTK password dialog on non-Linux
systems. Linux prefers pkexec when available and otherwise uses sudo. CLI writes
use sudo or doas (with pkexec as a Linux fallback). The user must be authorized by
the selected privilege tool; no privilege tool is needed when already root.

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

The POSIX `sh` installer builds the CLI and, when GTK3 is available, the GUI.
Run it from any directory; Bash and GNU build utilities are not required. It
builds in a private temporary directory, preserving existing binaries in the
source tree, then installs under `/usr/local`. If the destination needs root
access, only the installation step runs through sudo (or doas), which can prompt
in the terminal.

```sh
./install.sh
```

A writable prefix needs no root access:

```sh
INSTALL_PREFIX="$HOME/.local" ./install.sh
```

Build controls and package staging:

```sh
GUI=no ./install.sh                          # CLI only
GUI=yes ./install.sh                         # require GTK3; fail if unavailable
CC=clang CFLAGS="-O2" ./install.sh
INSTALL_PREFIX=/usr DESTDIR="$PWD/stage" ./install.sh
```

`GUI=auto` is the default. `CC`, `CPPFLAGS`, `CFLAGS`, `LDFLAGS`, `LDLIBS`,
`PKG_CONFIG`, and `TMPDIR` are honored. Use a compiler wrapper if a compiler path
or individual flag contains spaces; compiler flag variables are split into shell
words. `INSTALL_PREFIX` and `DESTDIR` must be absolute paths. On older Unix systems,
select a compiler with C99 and POSIX interfaces (for example `CC=xlc_r` on AIX).
The manual build commands above are examples for common GCC/Clang toolchains;
use the installer to probe flags for other compilers.

The desktop entry runs the CLI in a terminal when GTK3 is unavailable. macOS can
run the installed CLI or GTK executable directly; this installer does not create
a native `.app` bundle. Portability of the installer does not change the platform
status above: experimental device backends still need testing on their native OS.

Regression checks (no root access or disk writes):

```sh
python3 -m unittest discover -s tests -v
```

## Troubleshooting

### The app closes when clicking "Browse"

The GUI's file chooser is GTK3's built-in `GtkFileChooser`, which needs a working
gdk-pixbuf image-loader setup on the system. If gdk-pixbuf's loaders are broken or
incomplete (for example the SVG loader fails with `Unsupported image format`), GTK3
aborts with an assertion from `gtkiconhelper.c` and the whole app closes.

Fix the system gdk-pixbuf stack instead. Reinstall the gdk-pixbuf base package and
the SVG loader (librsvg) of your distribution with real package names:

* Debian / Ubuntu: `sudo apt install --reinstall librsvg2-common libgdk-pixbuf-2.0-0`
* Fedora / RHEL: `sudo dnf reinstall gdk-pixbuf2 librsvg2` (on Fedora 42+ the SVG
  loader is a separate `rsvg-pixbuf-loader` package)
* openSUSE: `sudo zypper install --force gdk-pixbuf gdk-pixbuf-loader-rsvg librsvg`
* Arch Linux: `sudo pacman -S gdk-pixbuf2 librsvg` (if your mirror tracks the newer
  loader split, also/use `librsvg-pixbuf-loader`)
* Gentoo: `sudo emerge -1 x11-libs/gdk-pixbuf gnome-base/librsvg`
* Alpine: `sudo apk fix gdk-pixbuf librsvg gdk-pixbuf-loaders`
* Void Linux: `sudo xbps-reconfigure -f gdk-pixbuf librsvg`
* Slackware: `sudo slackpkg reinstall gdk-pixbuf2 librsvg`
* CRUX: `sudo prt-get reinstall gdk-pixbuf librsvg`
* FreeBSD: `sudo pkg install -f gdk-pixbuf2 librsvg2`
* NetBSD: `sudo pkgin reinstall gdk-pixbuf librsvg`
* OpenBSD: `doas pkg_add gdk-pixbuf librsvg`

Most package managers run `gdk-pixbuf-query-loaders --update-cache` automatically as
a package trigger. On systems that do not (e.g. CRUX, or a manual build), run it by
hand after reinstalling:

```bash
gdk-pixbuf-query-loaders --update-cache
```

If SVG still fails to load after reinstalling, rebuild the cache by hand and make
sure `libpixbufloader-svg.so` exists under your
`/usr/lib*/gdk-pixbuf-2.0/*/loaders/` directory.

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
./uniximage
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
