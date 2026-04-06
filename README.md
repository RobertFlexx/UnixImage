# UnixImage, portable by standard. Break a leg!

> Universal disk image writer for (most) Unix and Unix-like operating systems.

> Some random stupid thing i made because these operating systems have no gui ISO image writing software.

> Youre welcome.

> Disclaimer: Linux, and FreeBSD tested. Others may not work, or may not work reliably.

> Heads up: AI was used in this project (a light amount) as a productivity tool, rather than being the          author itself. Making this known due to the uprising of AI slop, which you dont have to worry here.

## Supported Systems

* Linux (all distributions)
* macOS
* FreeBSD
* OpenBSD
* NetBSD
* DragonFly BSD

# Experimental (untested)

* Solaris
* illumos (OpenIndiana, SmartOS, OmniOS)
* MidnightBSD
* HardenedBSD
* GhostBSD
* AIX
* HP-UX
* IRIX
* QNX
* MINIX
* Haiku
* GNU/Hurd

## Supported Image Formats

ISO, IMG, RAW, DMG, VHD, VHDX, VDI, VMDK, QCOW, QCOW2

Compressed formats: XZ, GZ, BZ2, ZSTD, LZ4, ZIP

## Dependencies

GUI mode requires GTK3. CLI mode has no dependencies beyond libc and pthreads.

## Building

### Linux (Debian/Ubuntu)

```
apt install build-essential libgtk-3-dev
cc -o uniximage uniximage.c $(pkg-config --cflags --libs gtk+-3.0) -lpthread
```

### Linux (Fedora/RHEL)

```
dnf install gcc gtk3-devel
cc -o uniximage uniximage.c $(pkg-config --cflags --libs gtk+-3.0) -lpthread
```

### Linux (Arch)

```
pacman -S base-devel gtk3
cc -o uniximage uniximage.c $(pkg-config --cflags --libs gtk+-3.0) -lpthread
```

### FreeBSD

```
pkg install gtk3 pkgconf
cc -o uniximage uniximage.c $(pkg-config --cflags --libs gtk+-3.0) -lpthread
```

### OpenBSD

```
pkg_add gtk+3
cc -o uniximage uniximage.c $(pkg-config --cflags --libs gtk+-3.0) -lpthread
```

### NetBSD

```
pkgin install gtk3
cc -o uniximage uniximage.c $(pkg-config --cflags --libs gtk+-3.0) -lpthread
```

### DragonFly BSD

```
pkg install gtk3 pkgconf
cc -o uniximage uniximage.c $(pkg-config --cflags --libs gtk+-3.0) -lpthread
```

### macOS

```
brew install gtk+3 pkg-config
cc -o uniximage uniximage.c $(pkg-config --cflags --libs gtk+-3.0) -lpthread
```

### Solaris/illumos

```
pkg install gtk3 developer/build/pkg-config
cc -o uniximage uniximage.c $(pkg-config --cflags --libs gtk+-3.0) -lpthread
```

### CLI-only build (any system)

```
cc -o uniximage uniximage.c -DCLI_MODE -lpthread
```

## Usage

### GUI mode

```
sudo ./uniximage
```

### CLI mode

```
sudo ./uniximage -i image.iso -d /dev/sdX
sudo ./uniximage -i image.iso -d /dev/sdX -V -y
sudo ./uniximage --list
```

### Options

```
-h, --help        Show help
-v, --version     Show version
-l, --list        List available devices
-i, --image FILE  Image file
-d, --device DEV  Target device
-y, --yes         Skip confirmation
-V, --verify      Verify after writing
-b, --blocksize N Block size in bytes
-c, --cli         Force CLI mode
```

## License

GPLv3


## Prologue

I understand, and im fully aware im highly completionist. I aim for posix standard philosophy for Unix!
