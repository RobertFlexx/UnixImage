#!/bin/sh
# Build as the invoking user; elevate only the installation step if necessary.
set -eu
set -f

INSTALL_PREFIX=${INSTALL_PREFIX:-/usr/local}
DESTDIR=${DESTDIR:-}
CC=${CC:-cc}
CPPFLAGS=${CPPFLAGS:-}
CFLAGS=${CFLAGS:-}
LDFLAGS=${LDFLAGS:-}
LDLIBS=${LDLIBS:-}
GUI=${GUI:-auto}
case "$GUI" in auto|yes|no) ;; *) echo 'GUI must be auto, yes, or no' >&2; exit 1 ;; esac
case "$INSTALL_PREFIX" in /*) ;; *) echo 'INSTALL_PREFIX must be absolute' >&2; exit 1 ;; esac
case "$DESTDIR" in ''|/*) ;; *) echo 'DESTDIR must be absolute' >&2; exit 1 ;; esac
SOURCE_DIR=$(CDPATH= cd -P "$(dirname "$0")" && pwd)

# mkdir is POSIX; mktemp and its options differ across Unix implementations.
build_dir=
cleanup() { [ -z "$build_dir" ] || rm -rf "$build_dir"; }
trap cleanup 0
trap 'exit 1' HUP INT TERM
n=0
while [ "$n" -lt 100 ]; do
    candidate=${TMPDIR:-/tmp}/uniximage-build-$$-$n
    if (umask 077 && mkdir "$candidate") 2>/dev/null; then
        build_dir=$(CDPATH= cd -P "$candidate" && pwd)
        break
    fi
    n=$((n + 1))
done
[ -n "$build_dir" ] || { echo 'Cannot create build directory' >&2; exit 1; }

# Use feature probes rather than assuming that every cc accepts GCC options.
cat > "$build_dir/standard.c" <<'PROBE'
#include <sys/types.h>
typedef char large_off_t[(sizeof(off_t) >= 8) ? 1 : -1];
int main(void) { for (int i = 0; i < 1; ++i) { } return 0; }
PROBE
standard_found=no
for standard in '' '-std=c99' '-qlanglvl=extc99' '-xc99=all' '-AC99'; do
    if $CC $CPPFLAGS $CFLAGS $standard -D_FILE_OFFSET_BITS=64 "$build_dir/standard.c" \
        $LDFLAGS $LDLIBS -o "$build_dir/probe" > "$build_dir/compiler.log" 2>&1; then
        standard_found=yes
        break
    fi
done
if [ "$standard_found" = no ]; then
    cat "$build_dir/compiler.log" >&2
    echo 'A C99 compiler with 64-bit file offsets is required. Set CC/CFLAGS as needed.' >&2
    exit 1
fi
cat > "$build_dir/threads.c" <<'PROBE'
#include <pthread.h>
#include <time.h>
static void *worker(void *p) { return p; }
int main(void) {
    pthread_t thread;
#ifdef CLOCK_MONOTONIC
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now)) return 1;
#endif
    if (pthread_create(&thread, 0, worker, 0)) return 1;
    return pthread_join(thread, 0);
}
PROBE
threads_found=no
for threads in '-pthread' '-pthreads' '-mt' '-D_REENTRANT -lpthread' '-D_THREAD_SAFE -lpthreads' ''; do
    for clock_lib in '' '-lrt'; do
        if $CC $CPPFLAGS $CFLAGS $standard $threads "$build_dir/threads.c" \
            $LDFLAGS $threads $clock_lib $LDLIBS -o "$build_dir/probe" \
            > "$build_dir/compiler.log" 2>&1; then
            threads_found=yes
            break
        fi
    done
    [ "$threads_found" = no ] || break
done
if [ "$threads_found" = no ]; then
    cat "$build_dir/compiler.log" >&2
    echo 'Cannot link pthreads and the system clock. Set CC/CFLAGS/LDFLAGS/LDLIBS as needed.' >&2
    exit 1
fi

printf 'Compiling CLI with %s (%s %s %s)...\n' "$CC" "$standard" "$threads" "$clock_lib"
$CC $CPPFLAGS $CFLAGS $standard $threads -DCLI_MODE "$SOURCE_DIR/uniximage.c" \
    $LDFLAGS $threads $clock_lib $LDLIBS -o "$build_dir/uniximage-cli"

GUI_BUILT=0
if [ "$GUI" != no ]; then
    if [ -z "${PKG_CONFIG:-}" ]; then
        if command -v pkg-config >/dev/null 2>&1; then PKG_CONFIG=pkg-config
        else PKG_CONFIG=pkgconf
        fi
    fi
    if "$PKG_CONFIG" --exists gtk+-3.0 2>/dev/null; then
        echo 'Compiling GTK3 GUI...'
        $CC $CPPFLAGS $CFLAGS $standard $threads $("$PKG_CONFIG" --cflags gtk+-3.0) \
            "$SOURCE_DIR/uniximage.c" $LDFLAGS $("$PKG_CONFIG" --libs gtk+-3.0) \
            $threads $clock_lib $LDLIBS -o "$build_dir/uniximage"
        GUI_BUILT=1
    elif [ "$GUI" = yes ]; then
        echo 'GTK3 development files and pkg-config/pkgconf are required for GUI=yes.' >&2
        exit 1
    else
        echo 'GTK3 not found; installing the CLI only (use GUI=yes to require the GUI).'
    fi
fi

# Escape the absolute executable path for the Desktop Entry Exec grammar.
# Backslashes are escaped twice, for the entry value and for its quoted argument.
desktop_exec=$(printf '%s' "$INSTALL_PREFIX/bin/uniximage" | sed 's/\\/\\\\\\\\/g; s/"/\\\\"/g; s/`/\\\\`/g; s/\$/\\\\$/g; s/%/%%/g')
terminal=true
[ "$GUI_BUILT" -eq 0 ] || terminal=false
cat > "$build_dir/uniximage.desktop" <<EOF_DESKTOP
[Desktop Entry]
Version=1.0
Type=Application
Name=UnixImage Writer
GenericName=ISO Image Writer
Comment=Write ISO images to USB drives
Exec="$desktop_exec" %f
Terminal=$terminal
Categories=System;
MimeType=application/x-iso-image;
Keywords=usb;iso;burn;write;disk;image;
StartupNotify=false
EOF_DESKTOP

# A small, fixed helper keeps compilation out of sudo and works without GNU install.
cat > "$build_dir/install-files.sh" <<'INSTALL'
#!/bin/sh
set -eu
umask 022
build_dir=$1
prefix=$2
has_gui=$3
mkdir -p "$prefix/bin" "$prefix/share/applications"
# Unlink first so reinstalling a running executable works on Unix (ETXTBSY).
rm -f "$prefix/bin/uniximage" "$prefix/bin/uniximage-cli"
cp "$build_dir/uniximage-cli" "$prefix/bin/uniximage-cli"
if [ "$has_gui" -eq 1 ]; then
    cp "$build_dir/uniximage" "$prefix/bin/uniximage"
else
    cp "$build_dir/uniximage-cli" "$prefix/bin/uniximage"
fi
chmod 755 "$prefix/bin/uniximage" "$prefix/bin/uniximage-cli"
cp "$build_dir/uniximage.desktop" "$prefix/share/applications/uniximage.desktop"
chmod 644 "$prefix/share/applications/uniximage.desktop"
if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database "$prefix/share/applications" 2>/dev/null || :
fi
INSTALL

can_create_in() {
    dir=$1
    while [ ! -d "$dir" ]; do dir=$(dirname "$dir"); done
    [ -w "$dir" ] && [ -x "$dir" ]
}
prefix=$DESTDIR$INSTALL_PREFIX
if can_create_in "$prefix/bin" && can_create_in "$prefix/share/applications"; then
    /bin/sh "$build_dir/install-files.sh" "$build_dir" "$prefix" "$GUI_BUILT"
elif command -v sudo >/dev/null 2>&1; then
    echo "Administrator privileges are needed to install in $prefix."
    sudo /bin/sh "$build_dir/install-files.sh" "$build_dir" "$prefix" "$GUI_BUILT"
elif command -v doas >/dev/null 2>&1; then
    echo "Administrator privileges are needed to install in $prefix."
    doas /bin/sh "$build_dir/install-files.sh" "$build_dir" "$prefix" "$GUI_BUILT"
else
    echo 'Run as root, install sudo/doas, or choose a writable INSTALL_PREFIX/DESTDIR.' >&2
    exit 1
fi
printf 'UnixImage installed to %s/bin/\n' "$prefix"
