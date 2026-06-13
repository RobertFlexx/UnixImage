/*
 * UnixImage compilation definitions below you nitwit
 * compile with GTK3: cc -o uniximage uniximage.c $(pkg-config --cflags --libs gtk+-3.0) -lpthread
 * compile CLI mode: cc -o uniximage uniximage.c -DCLI_MODE -lpthread
 * heads up: this is mildly ai assisted, NOT vibe coded. Dont worry about ai slop
 */

#define _GNU_SOURCE
#define _LARGEFILE64_SOURCE
#define _FILE_OFFSET_BITS 64
// holy fucking boiler plate (ik)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <sys/utsname.h>
#include <dirent.h>
#include <time.h>
#include <stdint.h>
#include <stdarg.h>
#include <ctype.h>
#include <limits.h>
#include <locale.h>

#ifndef CLI_MODE
#include <gtk/gtk.h>
#endif

#ifdef __linux__
#include <sys/mount.h>
#include <linux/fs.h>
#include <mntent.h>
#endif

#ifdef __APPLE__
#include <sys/sysctl.h>
#include <sys/disk.h>
#include <sys/mount.h>
#endif

#if defined(__FreeBSD__) || defined(__DragonFly__)
#include <sys/sysctl.h>
#include <sys/disk.h>
#include <sys/mount.h>
#endif

#if defined(__OpenBSD__)
#include <sys/sysctl.h>
#include <sys/disklabel.h>
#include <sys/dkio.h>
#include <sys/mount.h>
#endif

#if defined(__NetBSD__)
#include <sys/sysctl.h>
#include <sys/disklabel.h>
#include <sys/dkio.h>
#include <sys/mount.h>
#endif
// these below MIGHT work, take it with a grain of salt.
#ifdef __sun
#include <sys/dkio.h>
#include <sys/vtoc.h>
#include <sys/mnttab.h>
#endif

#ifdef _AIX
#include <sys/devinfo.h>
#endif

#ifdef __hpux
#include <sys/diskio.h>
#endif

#define VERSION "2.2"
#define PROGRAM_NAME "UnixImage"
#define MAX_DEVICES 64
#define MAX_PATH_LEN 4096
#define DEFAULT_BLOCK_SIZE (4 * 1024 * 1024)
#define MIN_BLOCK_SIZE (512)
#define MAX_BLOCK_SIZE (64 * 1024 * 1024)
#define HASH_BLOCK_SIZE (1024 * 1024)
#define LOG_FILE "/tmp/uniximage.log"

typedef enum {
    OS_UNKNOWN = 0,
    OS_LINUX,
    OS_MACOS,
    OS_FREEBSD,
    OS_OPENBSD,
    OS_NETBSD,
    OS_DRAGONFLY,
    OS_SOLARIS,
    OS_ILLUMOS,
    OS_AIX,
    OS_HPUX,
    OS_IRIX,
    OS_TRU64,
    OS_SCO,
    OS_QNX,
    OS_MINIX,
    OS_HAIKU,
    OS_GNU_HURD,
    OS_MIDNIGHTBSD,
    OS_HARDENEDBSD,
    OS_GHOSTBSD,
    OS_TRUEOS,
    OS_REDOX,
    OS_SERENITY
} os_type_t;

typedef enum {
    IMG_UNKNOWN = 0,
    IMG_ISO,
    IMG_RAW,
    IMG_IMG,
    IMG_DMG,
    IMG_VHD,
    IMG_VHDX,
    IMG_VDI,
    IMG_VMDK,
    IMG_QCOW,
    IMG_QCOW2,
    IMG_XZ,
    IMG_GZ,
    IMG_BZ2,
    IMG_ZSTD,
    IMG_LZ4,
    IMG_ZIP,
    IMG_WIM
} image_type_t;

typedef enum {
    STATE_IDLE = 0,
    STATE_PREPARING,
    STATE_UNMOUNTING,
    STATE_WRITING,
    STATE_SYNCING,
    STATE_VERIFYING,
    STATE_COMPLETE,
    STATE_ERROR,
    STATE_CANCELLED
} write_state_t;

typedef struct {
    char path[MAX_PATH_LEN];
    char name[256];
    char model[256];
    char serial[128];
    char vendor[128];
    char bus[64];
    uint64_t size;
    uint32_t sector_size;
    int removable;
    int readonly;
    int mounted;
    char mount_point[MAX_PATH_LEN];
} device_t;

typedef struct {
    char path[MAX_PATH_LEN];
    char name[256];
    uint64_t size;
    image_type_t type;
    int compressed;
    int is_windows;
} image_info_t;

typedef enum {
    PARTscheme_AUTO,
    PARTscheme_MBR,
    PARTscheme_GPT
} partition_scheme_t;

typedef enum {
    WRITE_MODE_RAW,
    WRITE_MODE_WINDOWS
} write_mode_t;

typedef struct {
    write_state_t state;
    uint64_t bytes_written;
    uint64_t bytes_total;
    double progress;
    double speed;
    time_t start_time;
    time_t eta;
    char message[512];
    int error_code;
    int cancelled;
    int verify;
    int sync_writes;
    size_t block_size;
    partition_scheme_t partition_scheme;
    int make_bootable;
    write_mode_t mode;
    char win_iso_path[MAX_PATH_LEN];
    char win_usb_device[MAX_PATH_LEN];
    char win_efi_mount[MAX_PATH_LEN];
    char win_ntfs_mount[MAX_PATH_LEN];
    char win_loop_dev[MAX_PATH_LEN];
    uint64_t files_copied;
    uint64_t files_total;
} write_context_t;

#ifndef CLI_MODE
typedef struct {
    char image_path[MAX_PATH_LEN];
    char device_path[MAX_PATH_LEN];
    char status_file[MAX_PATH_LEN];
    char last_output[1024];
    int verify;
    size_t block_size;
    int result;
    int started;
    int status_seen;
    time_t start_time;
    guint poll_id;
} privileged_write_job_t;

#endif

typedef struct {
    device_t devices[MAX_DEVICES];
    int device_count;
    image_info_t image;
    device_t *selected_device;
    write_context_t ctx;
    pthread_t write_thread;
    pthread_mutex_t mutex;
    int running;
    os_type_t os;
    char os_name[64];
    char os_version[128];
    int is_root;
    int sudo_mode;
    FILE *log_file;
#ifndef CLI_MODE
    GtkWidget *window;
    GtkWidget *image_entry;
    GtkWidget *device_list;
    GtkWidget *progress_bar;
    GtkWidget *status_label;
    GtkWidget *speed_label;
    GtkWidget *eta_label;
    GtkWidget *write_button;
    GtkWidget *cancel_button;
    GtkWidget *verify_check;
    GtkWidget *sync_check;
    GtkWidget *blocksize_combo;
    GtkWidget *partition_combo;
    GtkWidget *bootable_check;
    GtkListStore *device_store;
    guint timer_id;
#endif
} app_t;

static app_t app;
static char app_exe_path[MAX_PATH_LEN];
static char status_file_path[MAX_PATH_LEN];
static volatile sig_atomic_t signal_received = 0;

void log_message(const char *level, const char *fmt, ...) {
    va_list args;
    time_t now;
    struct tm *tm_info;
    char timestamp[32];
    
    time(&now);
    tm_info = localtime(&now);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);
    
    va_start(args, fmt);
    
    if (app.log_file) {
        fprintf(app.log_file, "[%s] [%s] ", timestamp, level);
        vfprintf(app.log_file, fmt, args);
        fprintf(app.log_file, "\n");
        fflush(app.log_file);
    }
    
    va_end(args);
    
    va_start(args, fmt);
    if (strcmp(level, "ERROR") == 0) {
        fprintf(stderr, "[%s] ", level);
        vfprintf(stderr, fmt, args);
        fprintf(stderr, "\n");
    }
    va_end(args);
}

static void write_status_snapshot(const write_context_t *ctx) {
    if (!status_file_path[0] || !ctx) return;

    char tmp_path[MAX_PATH_LEN + 64];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp.%ld", status_file_path, (long)getpid());

    FILE *f = fopen(tmp_path, "w");
    if (!f) return;

    fprintf(f, "%.3f\n", ctx->progress);
    fprintf(f, "%lu\n", (unsigned long)ctx->bytes_written);
    fprintf(f, "%lu\n", (unsigned long)ctx->bytes_total);
    fprintf(f, "%.3f\n", ctx->speed);
    fprintf(f, "%ld\n", (long)ctx->eta);
    fprintf(f, "%d\n", (int)ctx->state);
    fprintf(f, "%s\n", ctx->message);
    fclose(f);

    rename(tmp_path, status_file_path);
}

const char *get_os_name(os_type_t os) {
    switch (os) {
        case OS_LINUX: return "Linux";
        case OS_MACOS: return "macOS";
        case OS_FREEBSD: return "FreeBSD";
        case OS_OPENBSD: return "OpenBSD";
        case OS_NETBSD: return "NetBSD";
        case OS_DRAGONFLY: return "DragonFly BSD";
        case OS_SOLARIS: return "Solaris";
        case OS_ILLUMOS: return "illumos";
        case OS_AIX: return "AIX";
        case OS_HPUX: return "HP-UX";
        case OS_IRIX: return "IRIX";
        case OS_TRU64: return "Tru64 UNIX";
        case OS_SCO: return "SCO UnixWare";
        case OS_QNX: return "QNX";
        case OS_MINIX: return "MINIX";
        case OS_HAIKU: return "Haiku";
        case OS_GNU_HURD: return "GNU/Hurd";
        case OS_MIDNIGHTBSD: return "MidnightBSD";
        case OS_HARDENEDBSD: return "HardenedBSD";
        case OS_GHOSTBSD: return "GhostBSD";
        case OS_TRUEOS: return "TrueOS";
        case OS_REDOX: return "Redox";
        case OS_SERENITY: return "SerenityOS";
        default: return "Unknown Unix";
    }
}

os_type_t detect_os(void) {
    struct utsname uts;
    
    if (uname(&uts) != 0) {
        return OS_UNKNOWN;
    }

#if defined(__linux__)
    return OS_LINUX;
#elif defined(__APPLE__) && defined(__MACH__)
    return OS_MACOS;
#elif defined(__FreeBSD__)
    if (strstr(uts.version, "MidnightBSD") != NULL) return OS_MIDNIGHTBSD;
    if (strstr(uts.version, "HardenedBSD") != NULL) return OS_HARDENEDBSD;
    if (strstr(uts.version, "GhostBSD") != NULL) return OS_GHOSTBSD;
    if (strstr(uts.version, "TrueOS") != NULL) return OS_TRUEOS;
    return OS_FREEBSD;
#elif defined(__OpenBSD__)
    return OS_OPENBSD;
#elif defined(__NetBSD__)
    return OS_NETBSD;
#elif defined(__DragonFly__)
    return OS_DRAGONFLY;
#elif defined(__sun) || defined(__sun__)
    if (access("/etc/os-release", F_OK) == 0) {
        FILE *f = fopen("/etc/os-release", "r");
        if (f) {
            char line[256];
            while (fgets(line, sizeof(line), f)) {
                if (strstr(line, "illumos") || strstr(line, "OpenIndiana") ||
                    strstr(line, "SmartOS") || strstr(line, "OmniOS")) {
                    fclose(f);
                    return OS_ILLUMOS;
                }
            }
            fclose(f);
        }
    }
    return OS_SOLARIS;
#elif defined(_AIX)
    return OS_AIX;
#elif defined(__hpux)
    return OS_HPUX;
#elif defined(__sgi)
    return OS_IRIX;
#elif defined(__osf__)
    return OS_TRU64;
#elif defined(_SCO_DS)
    return OS_SCO;
#elif defined(__QNX__)
    return OS_QNX;
#elif defined(__minix)
    return OS_MINIX;
#elif defined(__HAIKU__)
    return OS_HAIKU;
#elif defined(__GNU__)
    return OS_GNU_HURD;
#else
    if (strcmp(uts.sysname, "Linux") == 0) return OS_LINUX;
    if (strcmp(uts.sysname, "Darwin") == 0) return OS_MACOS;
    if (strcmp(uts.sysname, "FreeBSD") == 0) return OS_FREEBSD;
    if (strcmp(uts.sysname, "OpenBSD") == 0) return OS_OPENBSD;
    if (strcmp(uts.sysname, "NetBSD") == 0) return OS_NETBSD;
    if (strcmp(uts.sysname, "DragonFly") == 0) return OS_DRAGONFLY;
    if (strcmp(uts.sysname, "SunOS") == 0) return OS_SOLARIS;
    if (strcmp(uts.sysname, "AIX") == 0) return OS_AIX;
    if (strcmp(uts.sysname, "HP-UX") == 0) return OS_HPUX;
    if (strcmp(uts.sysname, "IRIX") == 0) return OS_IRIX;
    if (strcmp(uts.sysname, "QNX") == 0) return OS_QNX;
    if (strcmp(uts.sysname, "Minix") == 0) return OS_MINIX;
    if (strcmp(uts.sysname, "Haiku") == 0) return OS_HAIKU;
    return OS_UNKNOWN;
#endif
}

void get_os_version(char *buf, size_t len) {
    struct utsname uts;
    
    if (uname(&uts) == 0) {
        snprintf(buf, len, "%s %s", uts.sysname, uts.release);
    } else {
        snprintf(buf, len, "Unknown");
    }

#if defined(__linux__)
    FILE *f = fopen("/etc/os-release", "r");
    if (f) {
        char line[256];
        while (fgets(line, sizeof(line), f)) {
            if (strncmp(line, "PRETTY_NAME=", 12) == 0) {
                char *start = strchr(line, '"');
                char *end = strrchr(line, '"');
                if (start && end && end > start) {
                    size_t l = (size_t)(end - start - 1);
                    if (l >= len) l = len - 1;
                    strncpy(buf, start + 1, l);
                    buf[l] = '\0';
                    break;
                }
            }
        }
        fclose(f);
    }
#elif defined(__APPLE__)
    FILE *p = popen("sw_vers -productVersion 2>/dev/null", "r");
    if (p) {
        char ver[64] = {0};
        if (fgets(ver, sizeof(ver), p)) {
            ver[strcspn(ver, "\n")] = '\0';
            snprintf(buf, len, "macOS %s", ver);
        }
        pclose(p);
    }
#elif defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__) || defined(__DragonFly__)
    if (uname(&uts) == 0) {
        snprintf(buf, len, "%s %s", uts.sysname, uts.release);
    }
#endif
}

const char *format_size(uint64_t size, char *buf, size_t len) {
    const char *units[] = {"B", "KB", "MB", "GB", "TB", "PB", "EB"};
    int unit = 0;
    double dsize = (double)size;
    
    while (dsize >= 1024.0 && unit < 6) {
        dsize /= 1024.0;
        unit++;
    }
    
    if (unit == 0) {
        snprintf(buf, len, "%lu B", (unsigned long)size);
    } else {
        snprintf(buf, len, "%.2f %s", dsize, units[unit]);
    }
    
    return buf;
}

const char *format_time(time_t seconds, char *buf, size_t len) {
    if (seconds < 0) seconds = 0;
    
    if (seconds < 60) {
        snprintf(buf, len, "%lds", (long)seconds);
    } else if (seconds < 3600) {
        snprintf(buf, len, "%ldm %lds", (long)(seconds / 60), (long)(seconds % 60));
    } else {
        snprintf(buf, len, "%ldh %ldm %lds", 
                 (long)(seconds / 3600), 
                 (long)((seconds % 3600) / 60),
                 (long)(seconds % 60));
    }
    return buf;
}

const char *format_speed(double bytes_per_sec, char *buf, size_t len) {
    if (bytes_per_sec < 0) bytes_per_sec = 0;
    
    if (bytes_per_sec < 1024.0) {
        snprintf(buf, len, "%.0f B/s", bytes_per_sec);
    } else if (bytes_per_sec < 1024.0 * 1024.0) {
        snprintf(buf, len, "%.2f KB/s", bytes_per_sec / 1024.0);
    } else if (bytes_per_sec < 1024.0 * 1024.0 * 1024.0) {
        snprintf(buf, len, "%.2f MB/s", bytes_per_sec / (1024.0 * 1024.0));
    } else {
        snprintf(buf, len, "%.2f GB/s", bytes_per_sec / (1024.0 * 1024.0 * 1024.0));
    }
    return buf;
}

image_type_t detect_image_type(const char *path) {
    const char *ext = strrchr(path, '.');
    if (!ext) return IMG_RAW;
    
    ext++;
    
    if (strcasecmp(ext, "iso") == 0) return IMG_ISO;
    if (strcasecmp(ext, "img") == 0) return IMG_IMG;
    if (strcasecmp(ext, "raw") == 0) return IMG_RAW;
    if (strcasecmp(ext, "dmg") == 0) return IMG_DMG;
    if (strcasecmp(ext, "vhd") == 0) return IMG_VHD;
    if (strcasecmp(ext, "vhdx") == 0) return IMG_VHDX;
    if (strcasecmp(ext, "vdi") == 0) return IMG_VDI;
    if (strcasecmp(ext, "vmdk") == 0) return IMG_VMDK;
    if (strcasecmp(ext, "qcow") == 0) return IMG_QCOW;
    if (strcasecmp(ext, "qcow2") == 0) return IMG_QCOW2;
    if (strcasecmp(ext, "xz") == 0) return IMG_XZ;
    if (strcasecmp(ext, "gz") == 0) return IMG_GZ;
    if (strcasecmp(ext, "bz2") == 0) return IMG_BZ2;
    if (strcasecmp(ext, "zst") == 0) return IMG_ZSTD;
    if (strcasecmp(ext, "lz4") == 0) return IMG_LZ4;
    if (strcasecmp(ext, "zip") == 0) return IMG_ZIP;
    if (strcasecmp(ext, "wim") == 0) return IMG_WIM;
    if (strcasecmp(ext, "swm") == 0) return IMG_WIM;
    
    unsigned char magic[16];
    int fd = open(path, O_RDONLY);
    if (fd >= 0) {
        ssize_t n = read(fd, magic, sizeof(magic));
        close(fd);
        
        if (n >= 8) {
            if (magic[0] == 0x1F && magic[1] == 0x8B) return IMG_GZ;
            if (magic[0] == 0xFD && magic[1] == '7' && magic[2] == 'z') return IMG_XZ;
            if (magic[0] == 'B' && magic[1] == 'Z' && magic[2] == 'h') return IMG_BZ2;
            if (magic[0] == 0x28 && magic[1] == 0xB5 && magic[2] == 0x2F) return IMG_ZSTD;
            if (magic[0] == 0x04 && magic[1] == 0x22 && magic[2] == 0x4D) return IMG_LZ4;
            if (magic[0] == 'P' && magic[1] == 'K') return IMG_ZIP;
            if (memcmp(magic, "conectix", 8) == 0) return IMG_VHD;
            if (memcmp(magic, "vhdxfile", 8) == 0) return IMG_VHDX;
            if (memcmp(magic, "KDMV", 4) == 0) return IMG_VMDK;
            if (memcmp(magic, "QFI\xFB", 4) == 0) return IMG_QCOW2;
            if (n >= 16 && memcmp(magic, "MSWIM", 5) == 0) return IMG_WIM;
            if (n >= 16 && memcmp(magic, "WIM", 3) == 0 && magic[3] == 0x00) return IMG_WIM;
        }
    }
    
    return IMG_RAW;
}

const char *get_image_type_name(image_type_t type) {
    switch (type) {
        case IMG_ISO: return "ISO 9660";
        case IMG_RAW: return "Raw Image";
        case IMG_IMG: return "Disk Image";
        case IMG_DMG: return "Apple DMG";
        case IMG_VHD: return "VHD";
        case IMG_VHDX: return "VHDX";
        case IMG_VDI: return "VirtualBox VDI";
        case IMG_VMDK: return "VMware VMDK";
        case IMG_QCOW: return "QCOW";
        case IMG_QCOW2: return "QCOW2";
        case IMG_XZ: return "XZ Compressed";
        case IMG_GZ: return "Gzip Compressed";
        case IMG_BZ2: return "Bzip2 Compressed";
        case IMG_ZSTD: return "Zstd Compressed";
        case IMG_LZ4: return "LZ4 Compressed";
        case IMG_ZIP: return "ZIP Archive";
        case IMG_WIM: return "Windows WIM";
        default: return "Unknown";
    }
}

int is_compressed_image(image_type_t type) {
    return type == IMG_XZ || type == IMG_GZ || type == IMG_BZ2 || 
           type == IMG_ZSTD || type == IMG_LZ4 || type == IMG_ZIP;
}

static const char *decompressor_tool_for_type(image_type_t type) {
    switch (type) {
        case IMG_XZ:
            if (access("/usr/bin/xz", X_OK) == 0 || access("/bin/xz", X_OK) == 0) return "xz";
            return "xzcat";
        case IMG_GZ:
            if (access("/usr/bin/gzip", X_OK) == 0 || access("/bin/gzip", X_OK) == 0) return "gzip";
            return "zcat";
        case IMG_BZ2:
            if (access("/usr/bin/bzip2", X_OK) == 0 || access("/bin/bzip2", X_OK) == 0) return "bzip2";
            return "bzcat";
        case IMG_ZSTD: return "zstd";
        case IMG_LZ4: return "lz4";
        case IMG_ZIP: return "unzip";
        default: return NULL;
    }
}

static int start_decompressor(image_type_t type, const char *path, pid_t *pid_out) {
    int pipefd[2];
    if (pipe(pipefd) != 0) {
        log_message("ERROR", "pipe failed: %s", strerror(errno));
        return -1;
    }

    const char *tool = decompressor_tool_for_type(type);
    if (!tool) {
        close(pipefd[0]);
        close(pipefd[1]);
        errno = EINVAL;
        return -1;
    }

    pid_t pid = fork();
    if (pid < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        log_message("ERROR", "fork failed: %s", strerror(errno));
        return -1;
    }

    if (pid == 0) {
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);

        switch (type) {
            case IMG_XZ:
                if (strcmp(tool, "xz") == 0) execlp(tool, tool, "-dc", path, (char *)NULL);
                else execlp(tool, tool, path, (char *)NULL);
                break;
            case IMG_GZ:
                if (strcmp(tool, "gzip") == 0) execlp(tool, tool, "-dc", path, (char *)NULL);
                else execlp(tool, tool, path, (char *)NULL);
                break;
            case IMG_BZ2:
                if (strcmp(tool, "bzip2") == 0) execlp(tool, tool, "-dc", path, (char *)NULL);
                else execlp(tool, tool, path, (char *)NULL);
                break;
            case IMG_ZSTD:
                execlp(tool, tool, "-dc", path, (char *)NULL);
                break;
            case IMG_LZ4:
                execlp(tool, tool, "-dc", path, (char *)NULL);
                break;
            case IMG_ZIP:
                execlp(tool, tool, "-p", path, (char *)NULL);
                break;
            default:
                break;
        }
        _exit(127);
    }

    close(pipefd[1]);
    *pid_out = pid;
    return pipefd[0];
}

static int wait_for_reader(pid_t pid, const char *what) {
    if (pid <= 0) return 0;

    int status = 0;
    while (waitpid(pid, &status, 0) < 0) {
        if (errno == EINTR) continue;
        log_message("ERROR", "waitpid failed for %s: %s", what, strerror(errno));
        return -1;
    }

    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        log_message("ERROR", "%s failed with status %d", what,
                    WIFEXITED(status) ? WEXITSTATUS(status) : -1);
        return -1;
    }

    return 0;
}

static int open_image_reader(const char *path, image_type_t type, pid_t *pid_out, const char **mode_out) {
    *pid_out = -1;
    *mode_out = "raw file";

    if (!is_compressed_image(type)) {
        return open(path, O_RDONLY);
    }

    const char *tool = decompressor_tool_for_type(type);
    if (!tool) {
        errno = ENOTSUP;
        return -1;
    }

    *mode_out = tool;
    return start_decompressor(type, path, pid_out);
}

static int close_image_reader(int fd, pid_t pid, const char *mode) {
    int rc = 0;
    if (fd >= 0 && close(fd) != 0) rc = -1;
    if (wait_for_reader(pid, mode) != 0) rc = -1;
    return rc;
}

static int buffer_contains_ascii_ci(const unsigned char *haystack, size_t haystack_len,
                                    const char *needle) {
    size_t needle_len = strlen(needle);
    if (needle_len == 0 || haystack_len < needle_len) return 0;

    for (size_t i = 0; i <= haystack_len - needle_len; i++) {
        size_t j = 0;
        for (; j < needle_len; j++) {
            unsigned char a = haystack[i + j];
            unsigned char b = (unsigned char)needle[j];
            if (tolower(a) != tolower(b)) break;
        }
        if (j == needle_len) return 1;
    }

    return 0;
}

int is_windows_image(const char *image_path) {
    int fd = open(image_path, O_RDONLY);
    if (fd < 0) return 0;

    /*
     * Windows ISO directory records are not guaranteed to live in the first
     * 64 KiB. Scan a sane prefix with overlap and bounded case-insensitive
     * matching so Windows 10/11 ISOs do not silently fall through to raw-dd mode.
     */
    const size_t chunk_size = 1024 * 1024;
    const size_t overlap = 256;
    const uint64_t max_scan = 256ULL * 1024ULL * 1024ULL;
    unsigned char *buf = malloc(chunk_size + overlap);
    if (!buf) {
        close(fd);
        return 0;
    }

    size_t carry = 0;
    uint64_t scanned = 0;
    int saw_boot_wim = 0;
    int saw_install_image = 0;
    int saw_efi = 0;
    int saw_windows = 0;

    while (scanned < max_scan) {
        ssize_t n = read(fd, buf + carry, chunk_size);
        if (n < 0) {
            if (errno == EINTR) continue;
            break;
        }
        if (n == 0) break;

        size_t total = carry + (size_t)n;

        if (buffer_contains_ascii_ci(buf, total, "sources/boot.wim") ||
            buffer_contains_ascii_ci(buf, total, "boot.wim;1") ||
            buffer_contains_ascii_ci(buf, total, "boot.wim")) {
            saw_boot_wim = 1;
        }

        if (buffer_contains_ascii_ci(buf, total, "sources/install.wim") ||
            buffer_contains_ascii_ci(buf, total, "sources/install.esd") ||
            buffer_contains_ascii_ci(buf, total, "install.wim;1") ||
            buffer_contains_ascii_ci(buf, total, "install.esd;1") ||
            buffer_contains_ascii_ci(buf, total, "install.wim") ||
            buffer_contains_ascii_ci(buf, total, "install.esd")) {
            saw_install_image = 1;
        }

        if (buffer_contains_ascii_ci(buf, total, "efi/microsoft") ||
            buffer_contains_ascii_ci(buf, total, "efi/boot") ||
            buffer_contains_ascii_ci(buf, total, "bootx64.efi") ||
            buffer_contains_ascii_ci(buf, total, "microsoft")) {
            saw_efi = 1;
        }

        if (buffer_contains_ascii_ci(buf, total, "microsoft corporation") ||
            buffer_contains_ascii_ci(buf, total, "bootmgr") ||
            buffer_contains_ascii_ci(buf, total, "windows")) {
            saw_windows = 1;
        }

        if (saw_boot_wim && saw_install_image && (saw_efi || saw_windows)) {
            free(buf);
            close(fd);
            return 3;
        }

        scanned += (uint64_t)n;
        carry = total > overlap ? overlap : total;
        memmove(buf, buf + total - carry, carry);
    }

    free(buf);
    close(fd);

    if (saw_boot_wim && saw_install_image) return 2;
    if ((saw_boot_wim || saw_install_image) && (saw_efi || saw_windows)) return 1;
    return 0;
}




#if defined(__linux__)
static uint64_t get_device_size(const char *path);
static int unmount_device(const char *device);

static void set_write_status(write_context_t *ctx, write_state_t state, double progress,
                             const char *fmt, ...) {
    va_list ap;
    pthread_mutex_lock(&app.mutex);
    ctx->state = state;
    if (progress >= 0.0) ctx->progress = progress;
    va_start(ap, fmt);
    vsnprintf(ctx->message, sizeof(ctx->message), fmt, ap);
    va_end(ap);
    pthread_mutex_unlock(&app.mutex);
    write_status_snapshot(ctx);
}

static int check_tool(const char *tool) {
    char *path_env = getenv("PATH");
    if (!path_env || !*path_env) path_env = "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin";

    char *copy = strdup(path_env);
    if (!copy) return 0;

    int found = 0;
    char *save = NULL;
    for (char *dir = strtok_r(copy, ":", &save); dir; dir = strtok_r(NULL, ":", &save)) {
        char candidate[MAX_PATH_LEN];
        snprintf(candidate, sizeof(candidate), "%s/%s", dir, tool);
        if (access(candidate, X_OK) == 0) {
            found = 1;
            break;
        }
    }

    free(copy);
    return found;
}

static int check_required_tools(void) {
    const char *required[] = {
        "parted", "mkfs.vfat", "mount", "umount", "losetup", NULL
    };

    int missing = 0;
    for (int i = 0; required[i]; i++) {
        if (!check_tool(required[i])) {
            log_message("ERROR", "Missing required tool: %s", required[i]);
            missing++;
        }
    }

    if (!check_tool("wimlib-imagex") && !check_tool("wimsplit")) {
        log_message("WARNING", "wimlib-imagex/wimsplit not found; Windows ISOs with install.wim > 4 GiB will fail");
    }

    return missing == 0 ? 0 : -1;
}

static int run_cmdv(const char *const argv[]) {
    char rendered[2048];
    rendered[0] = '\0';

    for (int i = 0; argv[i]; i++) {
        if (i) strncat(rendered, " ", sizeof(rendered) - strlen(rendered) - 1);
        strncat(rendered, argv[i], sizeof(rendered) - strlen(rendered) - 1);
    }
    log_message("INFO", "RUN: %s", rendered);

    pid_t pid = fork();
    if (pid < 0) {
        log_message("ERROR", "fork failed: %s", strerror(errno));
        return -1;
    }

    if (pid == 0) {
        execvp(argv[0], (char * const *)argv);
        _exit(127);
    }

    int status = 0;
    while (waitpid(pid, &status, 0) < 0) {
        if (errno == EINTR) continue;
        log_message("ERROR", "waitpid failed: %s", strerror(errno));
        return -1;
    }

    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        log_message("ERROR", "Command failed (%d): %s",
                    WIFEXITED(status) ? WEXITSTATUS(status) : -1, rendered);
        return -1;
    }

    return 0;
}

static int run_cmdv_capture(const char *const argv[], char *out, size_t out_len) {
    if (out_len == 0) return -1;
    out[0] = '\0';

    int pipefd[2];
    if (pipe(pipefd) != 0) {
        log_message("ERROR", "pipe failed: %s", strerror(errno));
        return -1;
    }

    pid_t pid = fork();
    if (pid < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        log_message("ERROR", "fork failed: %s", strerror(errno));
        return -1;
    }

    if (pid == 0) {
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);
        execvp(argv[0], (char * const *)argv);
        _exit(127);
    }

    close(pipefd[1]);

    size_t used = 0;
    for (;;) {
        ssize_t n = read(pipefd[0], out + used, out_len - used - 1);
        if (n < 0) {
            if (errno == EINTR) continue;
            break;
        }
        if (n == 0) break;
        used += (size_t)n;
        if (used >= out_len - 1) break;
    }
    out[used] = '\0';

    close(pipefd[0]);

    int status = 0;
    while (waitpid(pid, &status, 0) < 0) {
        if (errno == EINTR) continue;
        return -1;
    }

    out[strcspn(out, "\r\n")] = '\0';

    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) return -1;
    return 0;
}

static int mkdir_p(const char *path, mode_t mode) {
    char tmp[MAX_PATH_LEN];
    snprintf(tmp, sizeof(tmp), "%s", path);

    size_t len = strlen(tmp);
    if (len == 0) return -1;
    if (tmp[len - 1] == '/') tmp[len - 1] = '\0';

    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(tmp, mode) != 0 && errno != EEXIST) return -1;
            *p = '/';
        }
    }

    if (mkdir(tmp, mode) != 0 && errno != EEXIST) return -1;
    return 0;
}

static void partition_path_linux(const char *device, int partno, char *out, size_t out_len) {
    size_t len = strlen(device);
    int needs_p = len > 0 && isdigit((unsigned char)device[len - 1]);
    snprintf(out, out_len, "%s%s%d", device, needs_p ? "p" : "", partno);
}

static int is_child_partition_path(const char *device, const char *candidate) {
    size_t n = strlen(device);
    if (strncmp(device, candidate, n) != 0) return 0;
    char c = candidate[n];
    return isdigit((unsigned char)c) || c == 'p';
}


static int mount_source_matches_device_linux(const char *device, const char *source) {
    if (!source || !*source) return 0;
    if (strcmp(source, device) == 0) return 1;
    if (strncmp(source, "/dev/", 5) == 0 && is_child_partition_path(device, source)) return 1;
    return 0;
}

static int target_has_critical_mount_linux(const char *device) {
    const char *critical[] = {"/", "/home", "/boot", "/boot/efi", NULL};

    FILE *f = setmntent("/proc/self/mounts", "r");
    if (!f) f = setmntent("/proc/mounts", "r");

    if (f) {
        struct mntent *mnt;
        while ((mnt = getmntent(f)) != NULL) {
            for (int i = 0; critical[i]; i++) {
                if (strcmp(mnt->mnt_dir, critical[i]) == 0 &&
                    mount_source_matches_device_linux(device, mnt->mnt_fsname)) {
                    log_message("ERROR", "Target device contains active critical mount %s (%s)",
                                mnt->mnt_dir, mnt->mnt_fsname);
                    endmntent(f);
                    return 1;
                }
            }
        }
        endmntent(f);
    }

    if (check_tool("findmnt")) {
        for (int i = 0; critical[i]; i++) {
            char source[MAX_PATH_LEN];
            const char *argv[] = {"findmnt", "-n", "-o", "SOURCE", "--target", critical[i], NULL};
            if (run_cmdv_capture(argv, source, sizeof(source)) == 0 &&
                mount_source_matches_device_linux(device, source)) {
                log_message("ERROR", "Target device contains active critical mount %s (%s)",
                            critical[i], source);
                return 1;
            }
        }
    }

    return 0;
}

static int unmount_device_linux_deep(const char *device) {
    FILE *f = setmntent("/proc/self/mounts", "r");
    if (!f) f = setmntent("/proc/mounts", "r");
    if (!f) {
        log_message("WARNING", "Cannot read mount table, falling back to generic unmount");
        return unmount_device(device);
    }

    typedef struct {
        char spec[MAX_PATH_LEN];
        char mountpoint[MAX_PATH_LEN];
    } mount_entry_t;

    mount_entry_t entries[128];
    int count = 0;
    struct mntent *mnt;

    while ((mnt = getmntent(f)) != NULL && count < 128) {
        if (strcmp(mnt->mnt_fsname, device) == 0 ||
            is_child_partition_path(device, mnt->mnt_fsname)) {
            snprintf(entries[count].spec, sizeof(entries[count].spec), "%s", mnt->mnt_fsname);
            snprintf(entries[count].mountpoint, sizeof(entries[count].mountpoint), "%s", mnt->mnt_dir);
            count++;
        }
    }
    endmntent(f);

    for (int pass = 0; pass < count; pass++) {
        int best = -1;
        size_t best_len = 0;
        for (int i = 0; i < count; i++) {
            if (!entries[i].mountpoint[0]) continue;
            size_t l = strlen(entries[i].mountpoint);
            if (l > best_len) {
                best = i;
                best_len = l;
            }
        }
        if (best < 0) break;

        const char *argv[] = {"umount", entries[best].mountpoint, NULL};
        run_cmdv(argv);
        entries[best].mountpoint[0] = '\0';
    }

    return 0;
}

static int reread_partition_table_linux(const char *device) {
    const char *partprobe[] = {"partprobe", device, NULL};
    const char *blockdev[] = {"blockdev", "--rereadpt", device, NULL};
    const char *udevadm[] = {"udevadm", "settle", NULL};

    if (check_tool("partprobe")) run_cmdv(partprobe);
    if (check_tool("blockdev")) run_cmdv(blockdev);
    if (check_tool("udevadm")) run_cmdv(udevadm);

    usleep(800000);
    return 0;
}

static int wait_for_partition_linux(const char *partition) {
    for (int i = 0; i < 50; i++) {
        if (access(partition, F_OK) == 0) return 0;
        usleep(100000);
    }
    log_message("ERROR", "Partition did not appear: %s", partition);
    return -1;
}

static int wipe_device_linux(const char *device) {
    log_message("INFO", "Wiping device signatures: %s", device);

    if (check_tool("wipefs")) {
        const char *wipefs[] = {"wipefs", "-a", device, NULL};
        run_cmdv(wipefs);
    }

    int fd = open(device, O_WRONLY | O_CLOEXEC);
    if (fd < 0) {
        log_message("ERROR", "Cannot open device for wiping: %s", strerror(errno));
        return -1;
    }

    unsigned char zeros[1024 * 1024];
    memset(zeros, 0, sizeof(zeros));

    if (lseek(fd, 0, SEEK_SET) < 0 || write(fd, zeros, sizeof(zeros)) < 0) {
        log_message("ERROR", "Failed to wipe start of device: %s", strerror(errno));
        close(fd);
        return -1;
    }

    uint64_t dev_size = get_device_size(device);
    if (dev_size > sizeof(zeros)) {
        off_t off = (off_t)(dev_size - sizeof(zeros));
        if (lseek(fd, off, SEEK_SET) >= 0) {
            (void)write(fd, zeros, sizeof(zeros));
        }
    }

    fsync(fd);
    close(fd);
    sync();
    return 0;
}

static int create_windows_fat32_partition(const char *device, partition_scheme_t scheme,
                                          char *partition_out, size_t partition_out_len) {
    partition_path_linux(device, 1, partition_out, partition_out_len);

    if (scheme == PARTscheme_GPT) {
        const char *mklabel[] = {"parted", "-s", device, "mklabel", "gpt", NULL};
        const char *mkpart[] = {"parted", "-s", device, "mkpart", "ESP", "fat32", "1MiB", "100%", NULL};
        const char *esp[] = {"parted", "-s", device, "set", "1", "esp", "on", NULL};
        const char *boot[] = {"parted", "-s", device, "set", "1", "boot", "on", NULL};

        if (run_cmdv(mklabel) != 0) return -1;
        if (run_cmdv(mkpart) != 0) return -1;
        run_cmdv(esp);
        run_cmdv(boot);
    } else {
        const char *mklabel[] = {"parted", "-s", device, "mklabel", "msdos", NULL};
        const char *mkpart[] = {"parted", "-s", device, "mkpart", "primary", "fat32", "1MiB", "100%", NULL};
        const char *boot[] = {"parted", "-s", device, "set", "1", "boot", "on", NULL};
        const char *lba[] = {"parted", "-s", device, "set", "1", "lba", "on", NULL};

        if (run_cmdv(mklabel) != 0) return -1;
        if (run_cmdv(mkpart) != 0) return -1;
        run_cmdv(boot);
        run_cmdv(lba);
    }

    sync();
    reread_partition_table_linux(device);
    return wait_for_partition_linux(partition_out);
}

static int format_partition(const char *partition, const char *fs_type, const char *label) {
    log_message("INFO", "Formatting %s as %s (label: %s)", partition, fs_type, label);

    if (strcmp(fs_type, "fat32") == 0 || strcmp(fs_type, "vfat") == 0) {
        const char *argv[] = {"mkfs.vfat", "-F", "32", "-n", label, partition, NULL};
        return run_cmdv(argv);
    }

    if (strcmp(fs_type, "ntfs") == 0) {
        if (!check_tool("mkfs.ntfs")) {
            log_message("ERROR", "mkfs.ntfs is not installed");
            return -1;
        }
        const char *argv[] = {"mkfs.ntfs", "-f", "-L", label, partition, NULL};
        return run_cmdv(argv);
    }

    log_message("ERROR", "Unsupported filesystem: %s", fs_type);
    return -1;
}

static int mount_iso_linux(const char *iso_path, char *loop_dev, char *mount_point) {
    log_message("INFO", "Mounting ISO: %s", iso_path);

    if (mkdir_p(mount_point, 0755) != 0) {
        log_message("ERROR", "Failed to create mount point %s: %s", mount_point, strerror(errno));
        return -1;
    }

    const char *losetup[] = {"losetup", "-f", "--show", "-r", iso_path, NULL};
    if (run_cmdv_capture(losetup, loop_dev, MAX_PATH_LEN) != 0 || strncmp(loop_dev, "/dev/", 5) != 0) {
        log_message("ERROR", "Failed to allocate loop device for ISO");
        return -1;
    }

    const char *mount_args[] = {"mount", "-o", "ro", loop_dev, mount_point, NULL};
    if (run_cmdv(mount_args) != 0) {
        const char *detach[] = {"losetup", "-d", loop_dev, NULL};
        run_cmdv(detach);
        loop_dev[0] = '\0';
        return -1;
    }

    log_message("INFO", "ISO mounted at %s using %s", mount_point, loop_dev);
    return 0;
}

static int unmount_iso_linux(const char *loop_dev, const char *mount_point) {
    log_message("INFO", "Unmounting ISO from %s", mount_point);

    if (mount_point && *mount_point) {
        const char *umount_args[] = {"umount", mount_point, NULL};
        run_cmdv(umount_args);
        rmdir(mount_point);
    }

    if (loop_dev && *loop_dev) {
        const char *detach[] = {"losetup", "-d", loop_dev, NULL};
        run_cmdv(detach);
    }

    return 0;
}

static int mount_partition_linux_rw(const char *partition, const char *mount_point, int readonly) {
    if (mkdir_p(mount_point, 0755) != 0) {
        log_message("ERROR", "Failed to create mount point %s: %s", mount_point, strerror(errno));
        return -1;
    }

    const char *opts = readonly ? "ro" : "rw,sync";
    const char *argv[] = {"mount", "-o", opts, partition, mount_point, NULL};
    return run_cmdv(argv);
}

/* Compatibility wrapper for older code paths. */
static int __attribute__((unused)) mount_partition_linux(const char *partition, const char *mount_point) {
    return mount_partition_linux_rw(partition, mount_point, 0);
}

static uint64_t count_bytes_in_dir_excluding(const char *dir, const char *skip_path) {
    uint64_t total = 0;
    DIR *d = opendir(dir);
    if (!d) return 0;

    struct dirent *entry;
    while ((entry = readdir(d)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;

        char path[MAX_PATH_LEN];
        snprintf(path, sizeof(path), "%s/%s", dir, entry->d_name);

        if (skip_path && strcmp(path, skip_path) == 0) continue;

        struct stat st;
        if (lstat(path, &st) != 0) continue;

        if (S_ISDIR(st.st_mode)) {
            total += count_bytes_in_dir_excluding(path, skip_path);
        } else if (S_ISREG(st.st_mode)) {
            total += (uint64_t)st.st_size;
        }
    }

    closedir(d);
    return total;
}

static int copy_file_stream(const char *src, const char *dst, write_context_t *ctx) {
    int in = open(src, O_RDONLY | O_CLOEXEC);
    if (in < 0) {
        log_message("ERROR", "Cannot open source file %s: %s", src, strerror(errno));
        return -1;
    }

    int out = open(dst, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
    if (out < 0) {
        log_message("ERROR", "Cannot create destination file %s: %s", dst, strerror(errno));
        close(in);
        return -1;
    }

    unsigned char *buf = malloc(1024 * 1024);
    if (!buf) {
        close(in);
        close(out);
        return -1;
    }

    int rc = 0;
    for (;;) {
        if (ctx->cancelled || signal_received) {
            rc = -2;
            break;
        }

        ssize_t n = read(in, buf, 1024 * 1024);
        if (n < 0) {
            if (errno == EINTR) continue;
            log_message("ERROR", "Read failed for %s: %s", src, strerror(errno));
            rc = -1;
            break;
        }
        if (n == 0) break;

        ssize_t off = 0;
        while (off < n) {
            ssize_t w = write(out, buf + off, (size_t)(n - off));
            if (w < 0) {
                if (errno == EINTR) continue;
                log_message("ERROR", "Write failed for %s: %s", dst, strerror(errno));
                rc = -1;
                break;
            }
            off += w;
            ctx->bytes_written += (uint64_t)w;
            if (ctx->bytes_total > 0) {
                ctx->progress = (double)ctx->bytes_written / (double)ctx->bytes_total * 100.0;
                if (ctx->progress > 99.0) ctx->progress = 99.0;
            }
            char copied_buf[32], total_buf[32];
            format_size(ctx->bytes_written, copied_buf, sizeof(copied_buf));
            format_size(ctx->bytes_total, total_buf, sizeof(total_buf));
            snprintf(ctx->message, sizeof(ctx->message), "Copying Windows files: %s / %s", copied_buf, total_buf);
            write_status_snapshot(ctx);
        }

        if (rc != 0) break;
    }

    fsync(out);
    free(buf);
    close(in);
    close(out);

    if (rc == -2) {
        log_message("INFO", "Copy cancelled");
        return -2;
    }
    return rc;
}

static int copy_tree_excluding(const char *src, const char *dst,
                               const char *skip_path, write_context_t *ctx) {
    if (skip_path && strcmp(src, skip_path) == 0) {
        log_message("INFO", "Skipping %s; it will be split directly to FAT32", src);
        return 0;
    }

    struct stat st;
    if (lstat(src, &st) != 0) {
        log_message("ERROR", "Cannot stat %s: %s", src, strerror(errno));
        return -1;
    }

    if (S_ISDIR(st.st_mode)) {
        if (mkdir_p(dst, 0755) != 0) {
            log_message("ERROR", "Cannot create directory %s: %s", dst, strerror(errno));
            return -1;
        }

        DIR *dir = opendir(src);
        if (!dir) {
            log_message("ERROR", "Cannot open directory %s: %s", src, strerror(errno));
            return -1;
        }

        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;

            char src_path[MAX_PATH_LEN];
            char dst_path[MAX_PATH_LEN];
            snprintf(src_path, sizeof(src_path), "%s/%s", src, entry->d_name);
            snprintf(dst_path, sizeof(dst_path), "%s/%s", dst, entry->d_name);

            int rc = copy_tree_excluding(src_path, dst_path, skip_path, ctx);
            if (rc != 0) {
                closedir(dir);
                return rc;
            }
        }

        closedir(dir);
        return 0;
    }

    if (S_ISREG(st.st_mode)) {
        return copy_file_stream(src, dst, ctx);
    }

    return 0;
}

/* Compatibility wrapper for older code paths. */
static int __attribute__((unused)) copy_file_recursive(const char *src, const char *dst, void *ctx) {
    return copy_tree_excluding(src, dst, NULL, (write_context_t *)ctx);
}

static uint64_t count_files_in_dir(const char *dir) {
    uint64_t count = 0;
    DIR *d = opendir(dir);
    if (!d) return 0;

    struct dirent *entry;
    while ((entry = readdir(d)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;

        char path[MAX_PATH_LEN];
        snprintf(path, sizeof(path), "%s/%s", dir, entry->d_name);

        struct stat st;
        if (lstat(path, &st) == 0) {
            if (S_ISDIR(st.st_mode)) count += count_files_in_dir(path);
            else if (S_ISREG(st.st_mode)) count++;
        }
    }

    closedir(d);
    return count;
}

static int find_first_existing(char out[MAX_PATH_LEN], const char *base, const char *const names[]) {
    for (int i = 0; names[i]; i++) {
        snprintf(out, MAX_PATH_LEN, "%s/%s", base, names[i]);
        if (access(out, F_OK) == 0) return 0;
    }
    out[0] = '\0';
    return -1;
}

static int get_install_image(const char *iso_mount, char out[MAX_PATH_LEN], int *is_wim,
                             uint64_t *size_out) {
    const char *const install_wim_names[] = {
        "sources/install.wim", "sources/INSTALL.WIM", "SOURCES/INSTALL.WIM", NULL
    };
    const char *const install_esd_names[] = {
        "sources/install.esd", "sources/INSTALL.ESD", "SOURCES/INSTALL.ESD", NULL
    };

    struct stat st;
    if (find_first_existing(out, iso_mount, install_wim_names) == 0 && stat(out, &st) == 0) {
        *is_wim = 1;
        *size_out = (uint64_t)st.st_size;
        return 0;
    }

    if (find_first_existing(out, iso_mount, install_esd_names) == 0 && stat(out, &st) == 0) {
        *is_wim = 0;
        *size_out = (uint64_t)st.st_size;
        return 0;
    }

    return -1;
}

static int validate_windows_iso_mount(const char *mount_point) {
    char path[MAX_PATH_LEN];

    const char *const boot_wim_names[] = {
        "sources/boot.wim", "sources/BOOT.WIM", "SOURCES/BOOT.WIM", NULL
    };
    const char *const bootmgr_names[] = {
        "bootmgr", "BOOTMGR", NULL
    };
    const char *const efi_boot_names[] = {
        "efi/boot/bootx64.efi", "efi/Boot/bootx64.efi", "EFI/BOOT/BOOTX64.EFI", NULL
    };

    if (find_first_existing(path, mount_point, boot_wim_names) != 0) {
        log_message("ERROR", "Invalid Windows ISO: sources/boot.wim is missing");
        return -1;
    }

    int is_wim = 0;
    uint64_t install_size = 0;
    if (get_install_image(mount_point, path, &is_wim, &install_size) != 0) {
        log_message("ERROR", "Invalid Windows ISO: sources/install.wim or install.esd is missing");
        return -1;
    }

    if (find_first_existing(path, mount_point, bootmgr_names) != 0) {
        log_message("WARNING", "bootmgr not found at ISO root");
    }

    if (find_first_existing(path, mount_point, efi_boot_names) != 0) {
        log_message("WARNING", "EFI bootloader not found; USB may not boot in UEFI mode");
    }

    return 0;
}

static int validate_usb_contents(const char *mount_point) {
    char path[MAX_PATH_LEN];
    int errors = 0;

    const char *const boot_wim_names[] = {
        "sources/boot.wim", "sources/BOOT.WIM", "SOURCES/BOOT.WIM", NULL
    };
    const char *const install_names[] = {
        "sources/install.wim", "sources/INSTALL.WIM", "SOURCES/INSTALL.WIM",
        "sources/install.esd", "sources/INSTALL.ESD", "SOURCES/INSTALL.ESD",
        "sources/install.swm", "sources/INSTALL.SWM", "SOURCES/INSTALL.SWM", NULL
    };
    const char *const efi_names[] = {
        "efi/boot/bootx64.efi", "efi/Boot/bootx64.efi", "EFI/BOOT/BOOTX64.EFI", NULL
    };
    const char *const setup_names[] = {
        "setup.exe", "SETUP.EXE", "sources/setup.exe", "SOURCES/SETUP.EXE", NULL
    };

    log_message("INFO", "=== USB Validation ===");

    if (find_first_existing(path, mount_point, efi_names) == 0) {
        log_message("INFO", "[OK] UEFI bootloader present");
    } else {
        log_message("ERROR", "[FAIL] UEFI bootloader missing");
        errors++;
    }

    if (find_first_existing(path, mount_point, boot_wim_names) == 0) {
        log_message("INFO", "[OK] sources/boot.wim present");
    } else {
        log_message("ERROR", "[FAIL] sources/boot.wim missing");
        errors++;
    }

    if (find_first_existing(path, mount_point, install_names) == 0) {
        log_message("INFO", "[OK] Installation image present");
    } else {
        log_message("ERROR", "[FAIL] install.wim/install.esd/install.swm missing");
        errors++;
    }

    if (find_first_existing(path, mount_point, setup_names) == 0) {
        log_message("INFO", "[OK] Windows setup present");
    } else {
        log_message("WARNING", "setup.exe was not found where expected");
    }

    log_message("INFO", "=== Validation %s ===", errors == 0 ? "PASSED" : "FAILED");
    return errors;
}

static int split_wim_to_usb(const char *wim_source, const char *usb_mount) {
    char swm_dest[MAX_PATH_LEN];
    snprintf(swm_dest, sizeof(swm_dest), "%s/sources/install.swm", usb_mount);

    unlink(swm_dest);

    if (check_tool("wimlib-imagex")) {
        const char *argv[] = {"wimlib-imagex", "split", wim_source, swm_dest, "3800", NULL};
        return run_cmdv(argv);
    }

    if (check_tool("wimsplit")) {
        const char *argv[] = {"wimsplit", wim_source, swm_dest, "3800", NULL};
        return run_cmdv(argv);
    }

    log_message("ERROR", "No WIM splitting tool available");
    return -1;
}

static int windows_usb_write(const char *iso_path, const char *device_path, write_context_t *ctx) {
    log_message("INFO", "Starting Windows USB creation: %s -> %s", iso_path, device_path);

    char iso_mount_template[] = "/tmp/uniximage-iso-XXXXXX";
    char usb_mount_template[] = "/tmp/uniximage-usb-XXXXXX";
    char *iso_mount = mkdtemp(iso_mount_template);
    char *usb_mount = mkdtemp(usb_mount_template);

    char loop_dev[MAX_PATH_LEN] = "";
    char data_partition[MAX_PATH_LEN] = "";
    char install_image[MAX_PATH_LEN] = "";
    int install_is_wim = 0;
    uint64_t install_size = 0;
    int usb_mounted = 0;
    int iso_mounted = 0;
    int rc = -1;

    if (!iso_mount || !usb_mount) {
        set_write_status(ctx, STATE_ERROR, -1.0, "Failed to create temporary mount directories");
        if (iso_mount) rmdir(iso_mount);
        if (usb_mount) rmdir(usb_mount);
        return -1;
    }

    snprintf(ctx->win_efi_mount, sizeof(ctx->win_efi_mount), "%s", iso_mount);
    snprintf(ctx->win_ntfs_mount, sizeof(ctx->win_ntfs_mount), "%s", usb_mount);

    if (check_required_tools() != 0) {
        set_write_status(ctx, STATE_ERROR, -1.0, "Missing required system tools");
        goto cleanup;
    }

    set_write_status(ctx, STATE_PREPARING, 0.0, "Mounting and checking Windows ISO...");

    if (mount_iso_linux(iso_path, loop_dev, iso_mount) != 0) {
        set_write_status(ctx, STATE_ERROR, -1.0, "Failed to mount Windows ISO");
        goto cleanup;
    }
    iso_mounted = 1;
    snprintf(ctx->win_loop_dev, sizeof(ctx->win_loop_dev), "%s", loop_dev);

    if (validate_windows_iso_mount(iso_mount) != 0) {
        set_write_status(ctx, STATE_ERROR, -1.0, "Not a valid Windows installer ISO");
        goto cleanup;
    }

    if (get_install_image(iso_mount, install_image, &install_is_wim, &install_size) != 0) {
        set_write_status(ctx, STATE_ERROR, -1.0, "Cannot find Windows install image");
        goto cleanup;
    }

    int needs_split = install_is_wim && install_size > (4ULL * 1024ULL * 1024ULL * 1024ULL - 1ULL);
    if (!install_is_wim && install_size > (4ULL * 1024ULL * 1024ULL * 1024ULL - 1ULL)) {
        set_write_status(ctx, STATE_ERROR, -1.0,
                         "install.esd is larger than FAT32 allows; use an ISO with WIM or a smaller ESD");
        goto cleanup;
    }

    if (needs_split && !check_tool("wimlib-imagex") && !check_tool("wimsplit")) {
        set_write_status(ctx, STATE_ERROR, -1.0,
                         "install.wim > 4 GiB; install wimlib-imagex/wimtools for splitting");
        goto cleanup;
    }

    const char *skip_path = needs_split ? install_image : NULL;
    uint64_t bytes_to_copy = count_bytes_in_dir_excluding(iso_mount, skip_path);
    ctx->bytes_total = bytes_to_copy + (needs_split ? install_size : 0);
    ctx->bytes_written = 0;
    ctx->files_total = count_files_in_dir(iso_mount);
    ctx->files_copied = 0;

    uint64_t dev_size = get_device_size(device_path);
    if (dev_size == 0) {
        set_write_status(ctx, STATE_ERROR, -1.0, "Cannot read target device size");
        goto cleanup;
    }

    uint64_t safety_slack = 128ULL * 1024ULL * 1024ULL;
    if (ctx->bytes_total + safety_slack > dev_size) {
        set_write_status(ctx, STATE_ERROR, -1.0, "Target USB is too small for this Windows ISO");
        goto cleanup;
    }

    set_write_status(ctx, STATE_UNMOUNTING, 2.0, "Unmounting target USB...");
    unmount_device_linux_deep(device_path);

    set_write_status(ctx, STATE_UNMOUNTING, 4.0, "Wiping target USB...");
    if (wipe_device_linux(device_path) != 0) {
        set_write_status(ctx, STATE_ERROR, -1.0, "Failed to wipe target USB");
        goto cleanup;
    }

    set_write_status(ctx, STATE_PREPARING, 8.0, "Creating Windows USB partition...");
    partition_scheme_t scheme = ctx->partition_scheme == PARTscheme_GPT ? PARTscheme_GPT : PARTscheme_MBR;
    if (create_windows_fat32_partition(device_path, scheme, data_partition, sizeof(data_partition)) != 0) {
        set_write_status(ctx, STATE_ERROR, -1.0, "Failed to create FAT32 partition");
        goto cleanup;
    }
    snprintf(ctx->win_usb_device, sizeof(ctx->win_usb_device), "%s", data_partition);

    set_write_status(ctx, STATE_PREPARING, 12.0, "Formatting as FAT32...");
    if (format_partition(data_partition, "fat32", "WINUSB") != 0) {
        set_write_status(ctx, STATE_ERROR, -1.0, "Failed to format FAT32 partition");
        goto cleanup;
    }

    reread_partition_table_linux(device_path);

    set_write_status(ctx, STATE_WRITING, 15.0, "Mounting USB partition...");
    if (mount_partition_linux_rw(data_partition, usb_mount, 0) != 0) {
        set_write_status(ctx, STATE_ERROR, -1.0, "Failed to mount USB partition");
        goto cleanup;
    }
    usb_mounted = 1;

    set_write_status(ctx, STATE_WRITING, 18.0,
                     needs_split ? "Copying files, skipping large install.wim..." : "Copying Windows files...");

    int copy_rc = copy_tree_excluding(iso_mount, usb_mount, skip_path, ctx);
    if (copy_rc == -2) {
        set_write_status(ctx, STATE_CANCELLED, -1.0, "Cancelled");
        goto cleanup;
    }
    if (copy_rc != 0) {
        set_write_status(ctx, STATE_ERROR, -1.0, "File copy failed");
        goto cleanup;
    }

    if (needs_split) {
        set_write_status(ctx, STATE_WRITING, ctx->progress, "Splitting install.wim for FAT32...");
        if (split_wim_to_usb(install_image, usb_mount) != 0) {
            set_write_status(ctx, STATE_ERROR, -1.0, "WIM split failed");
            goto cleanup;
        }

        char swm_check[MAX_PATH_LEN];
        snprintf(swm_check, sizeof(swm_check), "%s/sources/install.swm", usb_mount);
        if (access(swm_check, F_OK) != 0) {
            set_write_status(ctx, STATE_ERROR, -1.0, "WIM split did not create install.swm");
            goto cleanup;
        }
    }

    set_write_status(ctx, STATE_SYNCING, 98.0, "Syncing USB...");
#ifdef SYS_syncfs
    int mount_fd = open(usb_mount, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (mount_fd >= 0) {
        syncfs(mount_fd);
        close(mount_fd);
    }
#endif
    sync();

    set_write_status(ctx, STATE_VERIFYING, 99.0, "Validating USB contents...");
    if (validate_usb_contents(usb_mount) != 0) {
        set_write_status(ctx, STATE_ERROR, -1.0, "USB validation failed");
        goto cleanup;
    }

    set_write_status(ctx, STATE_COMPLETE, 100.0, "Complete! Windows USB is ready.");
    log_message("INFO", "Windows USB creation completed successfully");
    rc = 0;

cleanup:
    if (usb_mounted) {
        const char *umount_usb[] = {"umount", usb_mount, NULL};
        run_cmdv(umount_usb);
    }
    if (iso_mounted) {
        unmount_iso_linux(loop_dev, iso_mount);
    } else {
        if (loop_dev[0]) {
            const char *detach[] = {"losetup", "-d", loop_dev, NULL};
            run_cmdv(detach);
        }
        if (iso_mount) rmdir(iso_mount);
    }

    if (usb_mount) rmdir(usb_mount);

    if (rc != 0 && ctx->state != STATE_ERROR && ctx->state != STATE_CANCELLED) {
        set_write_status(ctx, STATE_ERROR, -1.0, "Windows USB creation failed");
    }

    return rc;
}
#endif


static int setup_windows_boot(const char *device_path, partition_scheme_t scheme) {
#if defined(__linux__)
    int fd = open(device_path, O_RDONLY);
    if (fd < 0) return -1;
    
    unsigned char mbr[512];
    if (read(fd, mbr, 512) != 512) {
        close(fd);
        return -1;
    }
    close(fd);
    
    if (scheme == PARTscheme_GPT || scheme == PARTscheme_AUTO) {
        uint8_t gpt_header[92];
        fd = open(device_path, O_RDONLY);
        if (fd >= 0) {
            lseek(fd, 512, SEEK_SET);
            if (read(fd, gpt_header, 92) == 92) {
                if (gpt_header[0] == 'E' && gpt_header[1] == 'F' && 
                    gpt_header[2] == 'I' && gpt_header[3] == ' ') {
                    close(fd);
                    log_message("INFO", "GPT/UEFI signature detected - already bootable");
                    return 0;
                }
            }
            close(fd);
        }
    }
    
    fd = open(device_path, O_WRONLY);
    if (fd < 0) return -1;
    
    if (scheme == PARTscheme_MBR || scheme == PARTscheme_AUTO) {
        unsigned char bootcode[512];
        memset(bootcode, 0, 512);
        bootcode[440] = 0x80;
        bootcode[441] = 0x00;
        bootcode[442] = 0x00;
        bootcode[443] = 0x00;
        
        bootcode[444] = 0x00;
        bootcode[445] = 0xFE;
        bootcode[446] = 0xFF;
        bootcode[447] = 0x00;
        
        bootcode[448] = 0x00;
        bootcode[449] = 0x00;
        bootcode[450] = 0x00;
        bootcode[451] = 0x00;
        
        bootcode[452] = 0x00;
        bootcode[453] = 0x00;
        bootcode[454] = 0x00;
        bootcode[455] = 0x00;
        
        bootcode[510] = 0x55;
        bootcode[511] = 0xAA;
        
        lseek(fd, 0, SEEK_SET);
        write(fd, bootcode, 512);
        
        log_message("INFO", "MBR boot signature set (0x55AA)");
    }
    
    close(fd);
    return 0;
#else
    (void)device_path;
    (void)scheme;
    return 0;
#endif
}

#if defined(__linux__)
static int linux_block_name_from_path(const char *path, char *out, size_t out_len) {
    const char *name = strrchr(path, '/');
    name = name ? name + 1 : path;
    if (!name || !*name || out_len == 0) return -1;

    snprintf(out, out_len, "%s", name);

    size_t len = strlen(out);
    char sysfs_path[512];
    snprintf(sysfs_path, sizeof(sysfs_path), "/sys/block/%s", out);
    if (access(sysfs_path, F_OK) == 0) return 0;

    char *p = strrchr(out, 'p');
    if (p && p > out && isdigit((unsigned char)p[1])) {
        *p = '\0';
        snprintf(sysfs_path, sizeof(sysfs_path), "/sys/block/%s", out);
        if (access(sysfs_path, F_OK) == 0) return 0;
    }

    snprintf(out, out_len, "%s", name);
    len = strlen(out);
    while (len > 0 && isdigit((unsigned char)out[len - 1])) {
        out[--len] = '\0';
    }

    if (len == 0) return -1;
    snprintf(sysfs_path, sizeof(sysfs_path), "/sys/block/%s", out);
    return access(sysfs_path, F_OK) == 0 ? 0 : -1;
}

static uint64_t read_u64_file(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    unsigned long long value = 0;
    int ok = fscanf(f, "%llu", &value) == 1;
    fclose(f);
    return ok ? (uint64_t)value : 0;
}
#endif

uint64_t get_device_size(const char *path) {
    uint64_t size = 0;

#if defined(__linux__)
    char block_name[128];
    if (linux_block_name_from_path(path, block_name, sizeof(block_name)) == 0) {
        char sysfs_path[512];
        snprintf(sysfs_path, sizeof(sysfs_path), "/sys/block/%s/size", block_name);
        uint64_t sectors = read_u64_file(sysfs_path);
        if (sectors > 0) return sectors * 512ULL;
    }
#endif

    int fd = open(path, O_RDONLY);
    
    if (fd < 0) return 0;
    
#if defined(__linux__)
    if (ioctl(fd, BLKGETSIZE64, &size) != 0) {
        unsigned long sectors = 0;
        if (ioctl(fd, BLKGETSIZE, &sectors) == 0) {
            size = sectors * 512ULL;
        }
    }
#elif defined(__APPLE__)
    uint32_t block_size = 0;
    uint64_t block_count = 0;
    if (ioctl(fd, DKIOCGETBLOCKSIZE, &block_size) == 0 &&
        ioctl(fd, DKIOCGETBLOCKCOUNT, &block_count) == 0) {
        size = (uint64_t)block_size * block_count;
    }
#elif defined(__FreeBSD__) || defined(__DragonFly__)
    off_t media_size = 0;
    if (ioctl(fd, DIOCGMEDIASIZE, &media_size) == 0) {
        size = (uint64_t)media_size;
    }
#elif defined(__OpenBSD__) || defined(__NetBSD__)
    struct disklabel dl;
    memset(&dl, 0, sizeof(dl));
    if (ioctl(fd, DIOCGDINFO, &dl) == 0) {
        size = (uint64_t)dl.d_secsize * dl.d_secperunit;
    }
#elif defined(__sun)
    struct dk_minfo minfo;
    memset(&minfo, 0, sizeof(minfo));
    if (ioctl(fd, DKIOCGMEDIAINFO, &minfo) == 0) {
        size = minfo.dki_capacity * minfo.dki_lbsize;
    }
#else
    off_t end = lseek(fd, 0, SEEK_END);
    if (end > 0) size = (uint64_t)end;
    lseek(fd, 0, SEEK_SET);
#endif
    
    close(fd);
    return size;
}

uint32_t get_device_sector_size(const char *path) {
    uint32_t sector_size = 512;

#if defined(__linux__)
    char block_name[128];
    if (linux_block_name_from_path(path, block_name, sizeof(block_name)) == 0) {
        char sysfs_path[512];
        snprintf(sysfs_path, sizeof(sysfs_path), "/sys/block/%s/queue/logical_block_size", block_name);
        uint64_t ss = read_u64_file(sysfs_path);
        if (ss > 0 && ss <= UINT32_MAX) return (uint32_t)ss;
    }
#endif

    int fd = open(path, O_RDONLY);
    
    if (fd < 0) return sector_size;
    
#if defined(__linux__)
    int ss = 0;
    if (ioctl(fd, BLKSSZGET, &ss) == 0 && ss > 0) {
        sector_size = (uint32_t)ss;
    }
#elif defined(__APPLE__)
    ioctl(fd, DKIOCGETBLOCKSIZE, &sector_size);
#elif defined(__FreeBSD__) || defined(__DragonFly__)
    u_int ss = 0;
    if (ioctl(fd, DIOCGSECTORSIZE, &ss) == 0 && ss > 0) {
        sector_size = ss;
    }
#endif
    
    close(fd);
    return sector_size;
}

int is_device_removable(const char *path) {
#if defined(__linux__)
    char sysfs_path[512];
    char base_name[128];
    if (linux_block_name_from_path(path, base_name, sizeof(base_name)) != 0) return 0;
    
    snprintf(sysfs_path, sizeof(sysfs_path), 
             "/sys/block/%s/removable", base_name);
    
    FILE *f = fopen(sysfs_path, "r");
    if (f) {
        int removable = 0;
        if (fscanf(f, "%d", &removable) == 1) {
            fclose(f);
            return removable;
        }
        fclose(f);
    }
    return 0;
#elif defined(__FreeBSD__) || defined(__DragonFly__)
    const char *name = strrchr(path, '/');
    if (name) name++;
    else name = path;
    return (strncmp(name, "da", 2) == 0);
#else
    (void)path;
    return 1;
#endif
}

int is_device_readonly(const char *path) {
    int fd = open(path, O_WRONLY);
    if (fd < 0) {
        if (errno == EROFS) return 1;
        if (errno == EACCES && geteuid() == 0) return 1;
        return 0;
    }
    close(fd);
    return 0;
}

#if defined(__linux__)
void get_device_model_linux(const char *dev_name, char *model, size_t len) {
    char sysfs_path[512];
    char dev_path[160];
    char base_name[128];

    snprintf(dev_path, sizeof(dev_path), "/dev/%s", dev_name);
    if (linux_block_name_from_path(dev_path, base_name, sizeof(base_name)) != 0) return;
    
    snprintf(sysfs_path, sizeof(sysfs_path), 
             "/sys/block/%s/device/model", base_name);
    
    FILE *f = fopen(sysfs_path, "r");
    if (f) {
        if (fgets(model, (int)len, f)) {
            model[strcspn(model, "\n")] = '\0';
            size_t mlen = strlen(model);
            while (mlen > 0 && model[mlen - 1] == ' ') {
                model[--mlen] = '\0';
            }
        }
        fclose(f);
    }
}

void get_device_vendor_linux(const char *dev_name, char *vendor, size_t len) {
    char sysfs_path[512];
    char dev_path[160];
    char base_name[128];

    snprintf(dev_path, sizeof(dev_path), "/dev/%s", dev_name);
    if (linux_block_name_from_path(dev_path, base_name, sizeof(base_name)) != 0) return;
    
    snprintf(sysfs_path, sizeof(sysfs_path), 
             "/sys/block/%s/device/vendor", base_name);
    
    FILE *f = fopen(sysfs_path, "r");
    if (f) {
        if (fgets(vendor, (int)len, f)) {
            vendor[strcspn(vendor, "\n")] = '\0';
            size_t vlen = strlen(vendor);
            while (vlen > 0 && vendor[vlen - 1] == ' ') {
                vendor[--vlen] = '\0';
            }
        }
        fclose(f);
    }
}
#endif

int enumerate_devices_linux(device_t *devices, int max_devices) {
#if defined(__linux__)
    int count = 0;
    DIR *dir = opendir("/sys/block");
    
    if (!dir) return 0;
    
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && count < max_devices) {
        if (entry->d_name[0] == '.') continue;
        if (strncmp(entry->d_name, "loop", 4) == 0) continue;
        if (strncmp(entry->d_name, "ram", 3) == 0) continue;
        if (strncmp(entry->d_name, "dm-", 3) == 0) continue;
        if (strncmp(entry->d_name, "sr", 2) == 0) continue;
        if (strncmp(entry->d_name, "fd", 2) == 0) continue;
        if (strncmp(entry->d_name, "md", 2) == 0) continue;
        if (strncmp(entry->d_name, "nbd", 3) == 0) continue;
        if (strncmp(entry->d_name, "zram", 4) == 0) continue;
        
        device_t *dev = &devices[count];
        memset(dev, 0, sizeof(device_t));
        
        snprintf(dev->path, sizeof(dev->path), "/dev/%s", entry->d_name);
        snprintf(dev->name, sizeof(dev->name), "%s", entry->d_name);
        
        dev->size = get_device_size(dev->path);
        if (dev->size == 0) continue;
        
        dev->sector_size = get_device_sector_size(dev->path);
        dev->removable = is_device_removable(dev->path);
        dev->readonly = is_device_readonly(dev->path);
        
        get_device_model_linux(entry->d_name, dev->model, sizeof(dev->model));
        get_device_vendor_linux(entry->d_name, dev->vendor, sizeof(dev->vendor));
        
        count++;
    }
    
    closedir(dir);
    return count;
#else
    (void)devices;
    (void)max_devices;
    return 0;
#endif
}

int enumerate_devices_macos(device_t *devices, int max_devices) {
#if defined(__APPLE__)
    int count = 0;
    FILE *p = popen("diskutil list 2>/dev/null | grep '^/dev/disk' | awk '{print $1}'", "r");
    
    if (!p) return 0;
    
    char line[256];
    while (fgets(line, sizeof(line), p) && count < max_devices) {
        line[strcspn(line, "\n")] = '\0';
        
        if (strcmp(line, "/dev/disk0") == 0) continue;
        
        device_t *dev = &devices[count];
        memset(dev, 0, sizeof(device_t));
        
        strncpy(dev->path, line, sizeof(dev->path) - 1);
        char *name = strrchr(line, '/');
        snprintf(dev->name, sizeof(dev->name), "%s", name ? name + 1 : line);
        
        dev->size = get_device_size(dev->path);
        if (dev->size == 0) continue;
        
        dev->sector_size = get_device_sector_size(dev->path);
        dev->removable = 1;
        
        char cmd[512];
        snprintf(cmd, sizeof(cmd), 
                 "diskutil info %s 2>/dev/null | grep 'Device / Media Name' | cut -d: -f2", 
                 line);
        FILE *p2 = popen(cmd, "r");
        if (p2) {
            if (fgets(dev->model, sizeof(dev->model), p2)) {
                dev->model[strcspn(dev->model, "\n")] = '\0';
                char *m = dev->model;
                while (*m == ' ') memmove(m, m + 1, strlen(m));
            }
            pclose(p2);
        }
        
        count++;
    }
    
    pclose(p);
    return count;
#else
    (void)devices;
    (void)max_devices;
    return 0;
#endif
}

int enumerate_devices_freebsd(device_t *devices, int max_devices) {
#if defined(__FreeBSD__) || defined(__DragonFly__)
    int count = 0;
    FILE *p = popen("geom disk list 2>/dev/null", "r");
    
    if (!p) {
        p = popen("ls /dev/da[0-9]* /dev/ada[0-9]* 2>/dev/null | grep -v '[sp][0-9]'", "r");
        if (!p) return 0;
        
        char line[256];
        while (fgets(line, sizeof(line), p) && count < max_devices) {
            line[strcspn(line, "\n")] = '\0';
            
            device_t *dev = &devices[count];
            memset(dev, 0, sizeof(device_t));
            
            strncpy(dev->path, line, sizeof(dev->path) - 1);
            char *name = strrchr(line, '/');
            snprintf(dev->name, sizeof(dev->name), "%s", name ? name + 1 : line);
            
            dev->size = get_device_size(dev->path);
            if (dev->size == 0) continue;
            
            dev->sector_size = get_device_sector_size(dev->path);
            dev->removable = (strncmp(dev->name, "da", 2) == 0);
            
            count++;
        }
        pclose(p);
        return count;
    }
    
    char line[256];
    device_t *current_dev = NULL;
    // this shit is kinda scuffed
    while (fgets(line, sizeof(line), p) && count < max_devices) {
        char *geom_name = strstr(line, "Geom name:");
        if (geom_name) {
            char name[64];
            if (sscanf(geom_name, "Geom name: %63s", name) == 1) {
                current_dev = &devices[count];
                memset(current_dev, 0, sizeof(device_t));
                
                snprintf(current_dev->path, sizeof(current_dev->path), "/dev/%s", name);
                strncpy(current_dev->name, name, sizeof(current_dev->name) - 1);
                current_dev->removable = (strncmp(name, "da", 2) == 0);
            }
        } else if (strstr(line, "Mediasize:") && current_dev) {
            uint64_t size = 0;
            if (sscanf(line, " Mediasize: %lu", (unsigned long *)&size) == 1) {
                current_dev->size = size;
                current_dev->sector_size = get_device_sector_size(current_dev->path);
                count++;
                current_dev = NULL;
            }
        } else if (strstr(line, "descr:") && current_dev) {
            char *descr = strstr(line, "descr:");
            if (descr) {
                descr += 6;
                while (*descr == ' ') descr++;
                strncpy(current_dev->model, descr, sizeof(current_dev->model) - 1);
                current_dev->model[strcspn(current_dev->model, "\n")] = '\0';
            }
        }
    }
    
    pclose(p);
    return count;
#else
    (void)devices;
    (void)max_devices;
    return 0;
#endif
}

int enumerate_devices_openbsd(device_t *devices, int max_devices) {
#if defined(__OpenBSD__)
    int count = 0;
    FILE *p = popen("sysctl -n hw.disknames 2>/dev/null", "r");
    
    if (!p) return 0;
    
    char line[1024];
    if (fgets(line, sizeof(line), p)) {
        char *token = strtok(line, ",");
        while (token && count < max_devices) {
            while (*token == ' ') token++;
            char *colon = strchr(token, ':');
            if (colon) *colon = '\0';
            
            token[strcspn(token, "\n")] = '\0';
            
            if (strlen(token) > 0 && strncmp(token, "cd", 2) != 0 && 
                strncmp(token, "fd", 2) != 0) {
                
                device_t *dev = &devices[count];
                memset(dev, 0, sizeof(device_t));
                
                snprintf(dev->path, sizeof(dev->path), "/dev/r%sc", token);
                strncpy(dev->name, token, sizeof(dev->name) - 1);
                
                dev->size = get_device_size(dev->path);
                if (dev->size > 0) {
                    dev->sector_size = 512;
                    dev->removable = (strncmp(token, "sd", 2) == 0);
                    count++;
                }
            }
            
            token = strtok(NULL, ",");
        }
    }
    
    pclose(p);
    return count;
#else
    (void)devices;
    (void)max_devices;
    return 0;
#endif
}

int enumerate_devices_netbsd(device_t *devices, int max_devices) {
#if defined(__NetBSD__)
    int count = 0;
    FILE *p = popen("sysctl -n hw.disknames 2>/dev/null", "r");
    
    if (!p) return 0;
    
    char line[1024];
    if (fgets(line, sizeof(line), p)) {
        char *token = strtok(line, " ");
        while (token && count < max_devices) {
            token[strcspn(token, "\n")] = '\0';
            
            if (strncmp(token, "cd", 2) != 0 && strncmp(token, "fd", 2) != 0) {
                device_t *dev = &devices[count];
                memset(dev, 0, sizeof(device_t));
                
                snprintf(dev->path, sizeof(dev->path), "/dev/r%sd", token);
                strncpy(dev->name, token, sizeof(dev->name) - 1);
                
                dev->size = get_device_size(dev->path);
                if (dev->size > 0) {
                    dev->sector_size = 512;
                    dev->removable = (strncmp(token, "sd", 2) == 0);
                    count++;
                }
            }
            
            token = strtok(NULL, " ");
        }
    }
    
    pclose(p);
    return count;
#else
    (void)devices;
    (void)max_devices;
    return 0;
#endif
}

int enumerate_devices_solaris(device_t *devices, int max_devices) {
#if defined(__sun)
    int count = 0;
    DIR *dir = opendir("/dev/rdsk");
    
    if (!dir) return 0;
    
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && count < max_devices) {
        if (!strstr(entry->d_name, "s2")) continue;
        
        device_t *dev = &devices[count];
        memset(dev, 0, sizeof(device_t));
        
        snprintf(dev->path, sizeof(dev->path), "/dev/rdsk/%s", entry->d_name);
        snprintf(dev->name, sizeof(dev->name), "%s", entry->d_name);
        
        dev->size = get_device_size(dev->path);
        if (dev->size > 0) {
            dev->sector_size = 512;
            count++;
        }
    }
    
    closedir(dir);
    return count;
#else
    (void)devices;
    (void)max_devices;
    return 0;
#endif
}

int enumerate_devices_generic(device_t *devices, int max_devices) {
    int count = 0;
    const char *dev_patterns[] = {
        "/dev/sd?",
        "/dev/hd?",
        "/dev/da?",
        "/dev/ada?",
        "/dev/wd?",
        "/dev/disk?",
        NULL
    };
    
    for (int i = 0; dev_patterns[i] && count < max_devices; i++) {
        char cmd[256];
        snprintf(cmd, sizeof(cmd), "ls %s 2>/dev/null", dev_patterns[i]);
        
        FILE *p = popen(cmd, "r");
        if (!p) continue;
        
        char line[256];
        while (fgets(line, sizeof(line), p) && count < max_devices) {
            line[strcspn(line, "\n")] = '\0';
            
            device_t *dev = &devices[count];
            memset(dev, 0, sizeof(device_t));
            
            strncpy(dev->path, line, sizeof(dev->path) - 1);
            char *name = strrchr(line, '/');
            snprintf(dev->name, sizeof(dev->name), "%s", name ? name + 1 : line);
            
            dev->size = get_device_size(dev->path);
            if (dev->size > 0) {
                dev->sector_size = 512;
                dev->removable = 1;
                count++;
            }
        }
        
        pclose(p);
    }
    
    return count;
}

int enumerate_devices(device_t *devices, int max_devices) {
    switch (app.os) {
        case OS_LINUX:
            return enumerate_devices_linux(devices, max_devices);
        case OS_MACOS:
            return enumerate_devices_macos(devices, max_devices);
        case OS_FREEBSD:
        case OS_DRAGONFLY:
        case OS_MIDNIGHTBSD:
        case OS_HARDENEDBSD:
        case OS_GHOSTBSD:
        case OS_TRUEOS:
            return enumerate_devices_freebsd(devices, max_devices);
        case OS_OPENBSD:
            return enumerate_devices_openbsd(devices, max_devices);
        case OS_NETBSD:
            return enumerate_devices_netbsd(devices, max_devices);
        case OS_SOLARIS:
        case OS_ILLUMOS:
            return enumerate_devices_solaris(devices, max_devices);
        default:
            return enumerate_devices_generic(devices, max_devices);
    }
}

int unmount_device(const char *device) {
    char cmd[MAX_PATH_LEN + 128];
    int ret = 0;
    
    log_message("INFO", "Unmounting device: %s", device);
    
    switch (app.os) {
        case OS_LINUX:
            snprintf(cmd, sizeof(cmd), "umount %s* 2>/dev/null", device);
            ret = system(cmd);
            snprintf(cmd, sizeof(cmd), "udisksctl unmount -b %s 2>/dev/null", device);
            system(cmd);
            break;
            
        case OS_MACOS:
            snprintf(cmd, sizeof(cmd), "diskutil unmountDisk %s 2>/dev/null", device);
            ret = system(cmd);
            break;
            
        case OS_FREEBSD:
        case OS_DRAGONFLY:
        case OS_MIDNIGHTBSD:
        case OS_HARDENEDBSD:
        case OS_GHOSTBSD:
        case OS_TRUEOS:
        case OS_OPENBSD:
        case OS_NETBSD:
            snprintf(cmd, sizeof(cmd), "umount %s 2>/dev/null", device);
            ret = system(cmd);
            snprintf(cmd, sizeof(cmd), "umount %s* 2>/dev/null", device);
            system(cmd);
            break;
            
        case OS_SOLARIS:
        case OS_ILLUMOS:
            snprintf(cmd, sizeof(cmd), "umount %s 2>/dev/null", device);
            ret = system(cmd);
            break;
            
        default:
            snprintf(cmd, sizeof(cmd), "umount %s 2>/dev/null", device);
            ret = system(cmd);
            break;
    }
    
    usleep(500000);
    return ret;
}

void *write_thread(void *arg) {
    (void)arg;

    write_context_t *ctx = &app.ctx;
    const char *image_path = app.image.path;
    const char *device_path = app.selected_device->path;

    int src_fd = -1, dst_fd = -1;
    pid_t src_pid = -1;
    const char *src_mode = "raw file";
    unsigned char *buffer = NULL;
    image_type_t image_type = app.image.type;
    int compressed = is_compressed_image(image_type);

    int is_windows = is_windows_image(image_path);
    if (is_windows > 0) {
        log_message("INFO", "Windows ISO detected (confidence: %d)", is_windows);
        ctx->mode = WRITE_MODE_WINDOWS;
    } else {
        ctx->mode = WRITE_MODE_RAW;
    }

#if defined(__linux__)
    if (target_has_critical_mount_linux(device_path)) {
        pthread_mutex_lock(&app.mutex);
        ctx->state = STATE_ERROR;
        ctx->error_code = EACCES;
        snprintf(ctx->message, sizeof(ctx->message),
                 "Refusing to write to a disk that appears to contain /, /home, or /boot");
        pthread_mutex_unlock(&app.mutex);
        log_message("ERROR", "SAFETY: Refusing to write to likely system disk %s", device_path);
        goto cleanup;
    }
#endif

    if (ctx->mode == WRITE_MODE_WINDOWS) {
#if defined(__linux__)
        log_message("INFO", "Using Windows file-based installation mode");

        if (geteuid() != 0) {
            pthread_mutex_lock(&app.mutex);
            ctx->state = STATE_ERROR;
            ctx->error_code = EACCES;
            snprintf(ctx->message, sizeof(ctx->message), "Root privileges required for Windows mode");
            pthread_mutex_unlock(&app.mutex);
            log_message("ERROR", "Root privileges required for Windows mode");
            goto cleanup;
        }

        int ret = windows_usb_write(image_path, device_path, ctx);
        if (ret != 0) {
            pthread_mutex_lock(&app.mutex);
            ctx->state = STATE_ERROR;
            ctx->error_code = EIO;
            if (ctx->message[0] == '\0') {
                snprintf(ctx->message, sizeof(ctx->message), "Windows USB creation failed");
            }
            pthread_mutex_unlock(&app.mutex);
            log_message("ERROR", "Windows USB creation failed");
        }

        goto cleanup;
#else
        pthread_mutex_lock(&app.mutex);
        ctx->state = STATE_ERROR;
        ctx->error_code = ENOTSUP;
        snprintf(ctx->message, sizeof(ctx->message), "Windows mode not supported on this platform");
        pthread_mutex_unlock(&app.mutex);
        goto cleanup;
#endif
    }

    pthread_mutex_lock(&app.mutex);
    ctx->state = STATE_PREPARING;
    snprintf(ctx->message, sizeof(ctx->message), "Preparing...");
    pthread_mutex_unlock(&app.mutex);

    log_message("INFO", "Starting write: %s -> %s", image_path, device_path);
    
    buffer = malloc(ctx->block_size);
    if (!buffer) {
        pthread_mutex_lock(&app.mutex);
        ctx->state = STATE_ERROR;
        ctx->error_code = ENOMEM;
        snprintf(ctx->message, sizeof(ctx->message), "Memory allocation failed");
        pthread_mutex_unlock(&app.mutex);
        goto cleanup;
    }
    
    pthread_mutex_lock(&app.mutex);
    ctx->state = STATE_UNMOUNTING;
    snprintf(ctx->message, sizeof(ctx->message), "Unmounting device...");
    pthread_mutex_unlock(&app.mutex);
    
#if defined(__linux__)
    unmount_device_linux_deep(device_path);
#else
    unmount_device(device_path);
#endif
    
    src_fd = open_image_reader(image_path, image_type, &src_pid, &src_mode);
    if (src_fd < 0) {
        pthread_mutex_lock(&app.mutex);
        ctx->state = STATE_ERROR;
        ctx->error_code = errno;
        snprintf(ctx->message, sizeof(ctx->message), "Failed to open image reader: %s", strerror(errno));
        pthread_mutex_unlock(&app.mutex);
        log_message("ERROR", "Failed to open image reader: %s", strerror(errno));
        goto cleanup;
    }
    log_message("INFO", "Image read mode: %s", src_mode);
    
    int flags = O_WRONLY;
#ifdef O_SYNC
    if (ctx->sync_writes) {
        flags |= O_SYNC;
    }
#endif
    
    dst_fd = open(device_path, flags);
    if (dst_fd < 0) {
        dst_fd = open(device_path, O_WRONLY);
    }
    
    if (dst_fd < 0) {
        pthread_mutex_lock(&app.mutex);
        ctx->state = STATE_ERROR;
        ctx->error_code = errno;
        snprintf(ctx->message, sizeof(ctx->message), "Failed to open device: %s", strerror(errno));
        pthread_mutex_unlock(&app.mutex);
        write_status_snapshot(ctx);
        log_message("ERROR", "Failed to open device: %s", strerror(errno));
        goto cleanup;
    }
    
    pthread_mutex_lock(&app.mutex);
    ctx->state = STATE_WRITING;
    ctx->start_time = time(NULL);
    ctx->bytes_written = 0;
    pthread_mutex_unlock(&app.mutex);
    write_status_snapshot(ctx);
    
    log_message("INFO", "Writing %lu bytes", (unsigned long)ctx->bytes_total);
    
    while (!ctx->cancelled) {
        ssize_t bytes_read = read(src_fd, buffer, ctx->block_size);
        
        if (bytes_read < 0) {
            if (errno == EINTR) continue;
            pthread_mutex_lock(&app.mutex);
            ctx->state = STATE_ERROR;
            ctx->error_code = errno;
            snprintf(ctx->message, sizeof(ctx->message), "Read error: %s", strerror(errno));
            pthread_mutex_unlock(&app.mutex);
            write_status_snapshot(ctx);
            log_message("ERROR", "Read error: %s", strerror(errno));
            goto cleanup;
        }
        
        if (bytes_read == 0) break;
        
        ssize_t total_written = 0;
        while (total_written < bytes_read && !ctx->cancelled) {
            ssize_t written = write(dst_fd, buffer + total_written, 
                                    (size_t)(bytes_read - total_written));
            
            if (written < 0) {
                if (errno == EINTR) continue;
                pthread_mutex_lock(&app.mutex);
                ctx->state = STATE_ERROR;
                ctx->error_code = errno;
                snprintf(ctx->message, sizeof(ctx->message), "Write error: %s", strerror(errno));
                pthread_mutex_unlock(&app.mutex);
                write_status_snapshot(ctx);
                log_message("ERROR", "Write error: %s", strerror(errno));
                goto cleanup;
            }
            
            total_written += written;
        }
        
        pthread_mutex_lock(&app.mutex);
        ctx->bytes_written += (uint64_t)bytes_read;
        if (!compressed && ctx->bytes_total > 0) {
            ctx->progress = (double)ctx->bytes_written / (double)ctx->bytes_total * 100.0;
        }
        
        time_t elapsed = time(NULL) - ctx->start_time;
        if (elapsed > 0) {
            ctx->speed = (double)ctx->bytes_written / (double)elapsed;
            if (!compressed && ctx->bytes_total >= ctx->bytes_written && ctx->speed > 0.0) {
                uint64_t remaining = ctx->bytes_total - ctx->bytes_written;
                ctx->eta = (time_t)((double)remaining / ctx->speed);
            } else {
                ctx->eta = 0;
            }
        }
        
        char size_buf[32], total_buf[32];
        format_size(ctx->bytes_written, size_buf, sizeof(size_buf));
        format_size(ctx->bytes_total, total_buf, sizeof(total_buf));
        if (compressed) {
            snprintf(ctx->message, sizeof(ctx->message), "Writing decompressed: %s", size_buf);
        } else {
            snprintf(ctx->message, sizeof(ctx->message), "Writing: %s / %s", size_buf, total_buf);
        }
        pthread_mutex_unlock(&app.mutex);
        write_status_snapshot(ctx);
    }

    if (ctx->cancelled) {
        if (src_fd >= 0) close(src_fd);
        if (src_pid > 0) {
            int status = 0;
            while (waitpid(src_pid, &status, 0) < 0 && errno == EINTR) {}
        }
        src_fd = -1;
        src_pid = -1;
        pthread_mutex_lock(&app.mutex);
        ctx->state = STATE_CANCELLED;
        snprintf(ctx->message, sizeof(ctx->message), "Cancelled by user");
        pthread_mutex_unlock(&app.mutex);
        write_status_snapshot(ctx);
        log_message("INFO", "Write cancelled by user");
        goto cleanup;
    }

    if (close_image_reader(src_fd, src_pid, src_mode) != 0) {
        src_fd = -1;
        src_pid = -1;
        pthread_mutex_lock(&app.mutex);
        ctx->state = STATE_ERROR;
        ctx->error_code = EIO;
        snprintf(ctx->message, sizeof(ctx->message), "Image decompression/read failed");
        pthread_mutex_unlock(&app.mutex);
        write_status_snapshot(ctx);
        goto cleanup;
    }
    src_fd = -1;
    src_pid = -1;
    
    pthread_mutex_lock(&app.mutex);
    ctx->state = STATE_SYNCING;
    snprintf(ctx->message, sizeof(ctx->message), "Syncing to disk...");
    pthread_mutex_unlock(&app.mutex);
    write_status_snapshot(ctx);
    
    log_message("INFO", "Syncing data to disk");
    
    fsync(dst_fd);
    
#if defined(__linux__)
    ioctl(dst_fd, BLKFLSBUF, 0);
#endif
    
    sync();
    
    if (ctx->verify && !ctx->cancelled) {
        pthread_mutex_lock(&app.mutex);
        ctx->state = STATE_VERIFYING;
        snprintf(ctx->message, sizeof(ctx->message), "Verifying...");
        pthread_mutex_unlock(&app.mutex);
        write_status_snapshot(ctx);
        
        log_message("INFO", "Verifying written data");
        
        close(dst_fd);

        src_fd = open_image_reader(image_path, image_type, &src_pid, &src_mode);
        if (src_fd < 0) {
            pthread_mutex_lock(&app.mutex);
            ctx->state = STATE_ERROR;
            ctx->error_code = errno;
            snprintf(ctx->message, sizeof(ctx->message), "Failed to reopen image for verification");
            pthread_mutex_unlock(&app.mutex);
            write_status_snapshot(ctx);
            goto cleanup;
        }
        
        dst_fd = open(device_path, O_RDONLY);
        if (dst_fd < 0) {
            pthread_mutex_lock(&app.mutex);
            ctx->state = STATE_ERROR;
            ctx->error_code = errno;
            snprintf(ctx->message, sizeof(ctx->message), "Failed to reopen device for verification");
            pthread_mutex_unlock(&app.mutex);
            write_status_snapshot(ctx);
            goto cleanup;
        }
        
        unsigned char *verify_buffer = malloc(ctx->block_size);
        if (!verify_buffer) {
            pthread_mutex_lock(&app.mutex);
            ctx->state = STATE_ERROR;
            ctx->error_code = ENOMEM;
            snprintf(ctx->message, sizeof(ctx->message), "Memory allocation failed");
            pthread_mutex_unlock(&app.mutex);
            write_status_snapshot(ctx);
            goto cleanup;
        }
        
        uint64_t verified = 0;
        int verify_failed = 0;
        
        while (!ctx->cancelled && !verify_failed) {
            ssize_t src_read = read(src_fd, buffer, ctx->block_size);
            if (src_read < 0) {
                if (errno == EINTR) continue;
                verify_failed = 1;
                break;
            }
            if (src_read == 0) break;
            
            ssize_t dst_read = read(dst_fd, verify_buffer, (size_t)src_read);
            if (dst_read != src_read) {
                verify_failed = 1;
                break;
            }
            
            if (memcmp(buffer, verify_buffer, (size_t)src_read) != 0) {
                verify_failed = 1;
                break;
            }
            
            verified += (uint64_t)src_read;
            
            pthread_mutex_lock(&app.mutex);
            if (compressed) {
                ctx->progress = 0.0;
                char verified_buf[32];
                format_size(verified, verified_buf, sizeof(verified_buf));
                snprintf(ctx->message, sizeof(ctx->message), "Verifying decompressed: %s", verified_buf);
            } else if (ctx->bytes_total > 0) {
                ctx->progress = (double)verified / (double)ctx->bytes_total * 100.0;
                snprintf(ctx->message, sizeof(ctx->message), "Verifying: %.1f%%", ctx->progress);
            } else {
                snprintf(ctx->message, sizeof(ctx->message), "Verifying...");
            }
            pthread_mutex_unlock(&app.mutex);
            write_status_snapshot(ctx);
        }

        if (ctx->cancelled) {
            if (src_fd >= 0) close(src_fd);
            if (src_pid > 0) {
                int status = 0;
                while (waitpid(src_pid, &status, 0) < 0 && errno == EINTR) {}
            }
            src_fd = -1;
            src_pid = -1;
            free(verify_buffer);
            pthread_mutex_lock(&app.mutex);
            ctx->state = STATE_CANCELLED;
            snprintf(ctx->message, sizeof(ctx->message), "Cancelled by user");
            pthread_mutex_unlock(&app.mutex);
            write_status_snapshot(ctx);
            goto cleanup;
        }

        if (close_image_reader(src_fd, src_pid, src_mode) != 0) {
            src_fd = -1;
            src_pid = -1;
            verify_failed = 1;
        }
        src_fd = -1;
        src_pid = -1;
        
        free(verify_buffer);
        
        if (verify_failed) {
            pthread_mutex_lock(&app.mutex);
            ctx->state = STATE_ERROR;
            ctx->error_code = EIO;
            snprintf(ctx->message, sizeof(ctx->message), "Verification failed!");
            pthread_mutex_unlock(&app.mutex);
            write_status_snapshot(ctx);
            log_message("ERROR", "Verification failed");
            goto cleanup;
        }
        
        log_message("INFO", "Verification successful");
    }
    
    pthread_mutex_lock(&app.mutex);
    ctx->state = STATE_COMPLETE;
    ctx->progress = 100.0;
    snprintf(ctx->message, sizeof(ctx->message), "Complete!");
    pthread_mutex_unlock(&app.mutex);
    write_status_snapshot(ctx);
    
    if (ctx->make_bootable && ctx->mode != WRITE_MODE_WINDOWS) {
        pthread_mutex_lock(&app.mutex);
        ctx->state = STATE_PREPARING;
        snprintf(ctx->message, sizeof(ctx->message), "Setting up bootable...");
        pthread_mutex_unlock(&app.mutex);
        write_status_snapshot(ctx);

        setup_windows_boot(device_path, ctx->partition_scheme);
    }

    log_message("INFO", "Write completed successfully");
    
cleanup:
    if (buffer) free(buffer);
    if (src_fd >= 0) close_image_reader(src_fd, src_pid, src_mode);
    if (dst_fd >= 0) close(dst_fd);
    
    sync();
    
    pthread_mutex_lock(&app.mutex);
    app.running = 0;
    pthread_mutex_unlock(&app.mutex);
    
    return NULL;
}

void signal_handler(int sig) {
    signal_received = sig;
    app.ctx.cancelled = 1;
}

#ifndef CLI_MODE

void update_device_list(void) {
    gtk_list_store_clear(app.device_store);
    
    app.device_count = enumerate_devices(app.devices, MAX_DEVICES);
    
    for (int i = 0; i < app.device_count; i++) {
        device_t *dev = &app.devices[i];
        char size_str[32];
        char display[MAX_PATH_LEN + 512];
        
        format_size(dev->size, size_str, sizeof(size_str));
        
        if (strlen(dev->model) > 0) {
            snprintf(display, sizeof(display), "%s - %s - %s%s%s",
                     dev->path, size_str, dev->model,
                     dev->removable ? " [Removable]" : "",
                     dev->readonly ? " [Read-Only]" : "");
        } else {
            snprintf(display, sizeof(display), "%s - %s%s%s",
                     dev->path, size_str,
                     dev->removable ? " [Removable]" : "",
                     dev->readonly ? " [Read-Only]" : "");
        }
        
        GtkTreeIter iter;
        gtk_list_store_append(app.device_store, &iter);
        gtk_list_store_set(app.device_store, &iter,
                           0, display,
                           1, i,
                           -1);
    }
}

void on_browse_clicked(GtkButton *button, gpointer user_data) {
    (void)button;
    (void)user_data;
    
    GtkWidget *dialog = gtk_file_chooser_dialog_new(
        "Select Disk Image",
        GTK_WINDOW(app.window),
        GTK_FILE_CHOOSER_ACTION_OPEN,
        "_Cancel", GTK_RESPONSE_CANCEL,
        "_Open", GTK_RESPONSE_ACCEPT,
        NULL);
    
    GtkFileFilter *filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "Disk Images");
    gtk_file_filter_add_pattern(filter, "*.iso");
    gtk_file_filter_add_pattern(filter, "*.img");
    gtk_file_filter_add_pattern(filter, "*.raw");
    gtk_file_filter_add_pattern(filter, "*.dmg");
    gtk_file_filter_add_pattern(filter, "*.vhd");
    gtk_file_filter_add_pattern(filter, "*.vdi");
    gtk_file_filter_add_pattern(filter, "*.vmdk");
    gtk_file_filter_add_pattern(filter, "*.qcow2");
    gtk_file_filter_add_pattern(filter, "*.xz");
    gtk_file_filter_add_pattern(filter, "*.gz");
    gtk_file_filter_add_pattern(filter, "*.bz2");
    gtk_file_filter_add_pattern(filter, "*.wim");
    gtk_file_filter_add_pattern(filter, "*.swm");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);
    
    GtkFileFilter *all_filter = gtk_file_filter_new();
    gtk_file_filter_set_name(all_filter, "All Files");
    gtk_file_filter_add_pattern(all_filter, "*");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), all_filter);
    
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        
        strncpy(app.image.path, filename, sizeof(app.image.path) - 1);
        
        char *basename_ptr = strrchr(filename, '/');
        strncpy(app.image.name, basename_ptr ? basename_ptr + 1 : filename, 
                sizeof(app.image.name) - 1);
        
        struct stat st;
        if (stat(filename, &st) == 0) {
            app.image.size = (uint64_t)st.st_size;
        }
        
        app.image.type = detect_image_type(filename);
        app.image.compressed = is_compressed_image(app.image.type);
        app.image.is_windows = 0;
        
        char display[MAX_PATH_LEN + 128];
        char size_str[32];
        format_size(app.image.size, size_str, sizeof(size_str));
        snprintf(display, sizeof(display), "%s (%s, %s)", 
                 app.image.name, size_str, get_image_type_name(app.image.type));
        
        gtk_entry_set_text(GTK_ENTRY(app.image_entry), display);
        
        g_free(filename);
        
        log_message("INFO", "Selected image: %s (%lu bytes, type: %s)",
                    app.image.path, (unsigned long)app.image.size,
                    get_image_type_name(app.image.type));
    }
    
    gtk_widget_destroy(dialog);
}

void on_refresh_clicked(GtkButton *button, gpointer user_data) {
    (void)button;
    (void)user_data;
    update_device_list();
}

void on_device_selected(GtkTreeSelection *selection, gpointer user_data) {
    (void)user_data;
    
    GtkTreeModel *model;
    GtkTreeIter iter;
    
    if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
        int index;
        gtk_tree_model_get(model, &iter, 1, &index, -1);
        
        if (index >= 0 && index < app.device_count) {
            app.selected_device = &app.devices[index];
            log_message("INFO", "Selected device: %s", app.selected_device->path);
        }
    }
}

gboolean update_progress(gpointer user_data) {
    (void)user_data;
    
    pthread_mutex_lock(&app.mutex);
    
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(app.progress_bar), 
                                   app.ctx.progress / 100.0);
    
    char progress_text[64];
    snprintf(progress_text, sizeof(progress_text), "%.1f%%", app.ctx.progress);
    gtk_progress_bar_set_text(GTK_PROGRESS_BAR(app.progress_bar), progress_text);
    
    gtk_label_set_text(GTK_LABEL(app.status_label), app.ctx.message);
    
    if (app.ctx.speed > 0) {
        char speed_str[64];
        format_speed(app.ctx.speed, speed_str, sizeof(speed_str));
        gtk_label_set_text(GTK_LABEL(app.speed_label), speed_str);
    }
    
    if (app.ctx.eta > 0) {
        char eta_str[64];
        format_time(app.ctx.eta, eta_str, sizeof(eta_str));
        gtk_label_set_text(GTK_LABEL(app.eta_label), eta_str);
    }
    
    int running = app.running;
    write_state_t state = app.ctx.state;
    
    pthread_mutex_unlock(&app.mutex);
    
    if (!running) {
        gtk_widget_set_sensitive(app.write_button, TRUE);
        gtk_widget_set_sensitive(app.cancel_button, FALSE);
        
        if (state == STATE_COMPLETE) {
            GtkWidget *dialog = gtk_message_dialog_new(
                GTK_WINDOW(app.window),
                GTK_DIALOG_MODAL,
                GTK_MESSAGE_INFO,
                GTK_BUTTONS_OK,
                "Image written successfully!");
            gtk_dialog_run(GTK_DIALOG(dialog));
            gtk_widget_destroy(dialog);
        } else if (state == STATE_ERROR) {
            GtkWidget *dialog = gtk_message_dialog_new(
                GTK_WINDOW(app.window),
                GTK_DIALOG_MODAL,
                GTK_MESSAGE_ERROR,
                GTK_BUTTONS_OK,
                "Error: %s", app.ctx.message);
            gtk_dialog_run(GTK_DIALOG(dialog));
            gtk_widget_destroy(dialog);
        }
        
        app.timer_id = 0;
        return G_SOURCE_REMOVE;
    }
    
    return G_SOURCE_CONTINUE;
}

static int get_current_executable_path(char *out, size_t out_len) {
    ssize_t len = readlink("/proc/self/exe", out, out_len - 1);
    if (len > 0) {
        out[len] = '\0';
        return 0;
    }

    if (app_exe_path[0]) {
        snprintf(out, out_len, "%s", app_exe_path);
        return 0;
    }

    return -1;
}

static int find_program_path(const char *program, char *out, size_t out_len) {
    const char *path_env = getenv("PATH");
    if (path_env && *path_env) {
        char *copy = strdup(path_env);
        if (copy) {
            char *save = NULL;
            for (char *dir = strtok_r(copy, ":", &save); dir; dir = strtok_r(NULL, ":", &save)) {
                snprintf(out, out_len, "%s/%s", dir, program);
                if (access(out, X_OK) == 0) {
                    free(copy);
                    return 0;
                }
            }
            free(copy);
        }
    }

    const char *dirs[] = {"/usr/bin", "/bin", "/usr/local/bin", "/usr/host/bin", "/opt/bin", NULL};
    for (int i = 0; dirs[i]; i++) {
        snprintf(out, out_len, "%s/%s", dirs[i], program);
        if (access(out, X_OK) == 0) return 0;
    }

    out[0] = '\0';
    return -1;
}

static gboolean privileged_status_poll(gpointer user_data) {
    privileged_write_job_t *job = (privileged_write_job_t *)user_data;
    FILE *f = fopen(job->status_file, "r");
    if (!f) {
        if (time(NULL) - job->start_time > 15) {
            gtk_label_set_text(GTK_LABEL(app.status_label), "Waiting for administrator authentication or privileged writer startup...");
            gtk_progress_bar_pulse(GTK_PROGRESS_BAR(app.progress_bar));
        }
        return G_SOURCE_CONTINUE;
    }

    double progress = 0.0;
    unsigned long bytes_written = 0;
    unsigned long bytes_total = 0;
    double speed = 0.0;
    long eta = 0;
    int state = 0;
    char message[512] = "";

    int ok = fscanf(f, "%lf\n%lu\n%lu\n%lf\n%ld\n%d\n",
                    &progress, &bytes_written, &bytes_total, &speed, &eta, &state) == 6;
    if (ok && fgets(message, sizeof(message), f)) {
        message[strcspn(message, "\r\n")] = '\0';
    }
    fclose(f);

    if (!ok) return G_SOURCE_CONTINUE;
    job->status_seen = 1;

    if (message[0]) {
        gtk_label_set_text(GTK_LABEL(app.status_label), message);
        gtk_progress_bar_set_text(GTK_PROGRESS_BAR(app.progress_bar), message);
    }

    if (bytes_total > 0 || progress > 0.0) {
        double fraction = progress / 100.0;
        if (fraction < 0.0) fraction = 0.0;
        if (fraction > 1.0) fraction = 1.0;
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(app.progress_bar), fraction);
    } else {
        gtk_progress_bar_pulse(GTK_PROGRESS_BAR(app.progress_bar));
    }

    if (speed > 0.0) {
        char speed_buf[32];
        format_speed(speed, speed_buf, sizeof(speed_buf));
        gtk_label_set_text(GTK_LABEL(app.speed_label), speed_buf);
    }

    if (eta > 0) {
        char eta_buf[32];
        format_time((time_t)eta, eta_buf, sizeof(eta_buf));
        gtk_label_set_text(GTK_LABEL(app.eta_label), eta_buf);
    }

    return G_SOURCE_CONTINUE;
}

static int run_privileged_cli_write(privileged_write_job_t *job) {
    char exe_path[MAX_PATH_LEN];
    char pkexec_path[MAX_PATH_LEN];
    char block_size_arg[32];
    if (get_current_executable_path(exe_path, sizeof(exe_path)) != 0) return -1;
    if (find_program_path("pkexec", pkexec_path, sizeof(pkexec_path)) != 0) {
        snprintf(job->last_output, sizeof(job->last_output), "pkexec was not found in PATH or common system locations");
        return 127;
    }
    snprintf(block_size_arg, sizeof(block_size_arg), "%lu", (unsigned long)job->block_size);

    int pipefd[2];
    if (pipe(pipefd) != 0) return -1;

    pid_t pid = fork();
    if (pid < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        return -1;
    }

    if (pid == 0) {
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[1]);

        if (job->verify) {
            char *const argv[] = {
                pkexec_path, exe_path, "--sudo-mode", "--cli",
                "-i", job->image_path,
                "-d", job->device_path,
                "-b", block_size_arg,
                "--status-file", job->status_file,
                "-V", "-y", NULL
            };
            execv(pkexec_path, argv);
        } else {
            char *const argv[] = {
                pkexec_path, exe_path, "--sudo-mode", "--cli",
                "-i", job->image_path,
                "-d", job->device_path,
                "-b", block_size_arg,
                "--status-file", job->status_file,
                "-y", NULL
            };
            execv(pkexec_path, argv);
        }
        _exit(127);
    }

    close(pipefd[1]);

        for (;;) {
            char buf[512];
            ssize_t n = read(pipefd[0], buf, sizeof(buf));
        if (n < 0) {
            if (errno == EINTR) continue;
            break;
        }
        if (n == 0) break;

        if (!job->started) job->started = 1;
        size_t copy_len = (size_t)n;
        size_t cur_len = strlen(job->last_output);
        if (cur_len + copy_len >= sizeof(job->last_output)) {
            size_t drop = cur_len + copy_len - sizeof(job->last_output) + 1;
            memmove(job->last_output, job->last_output + drop, cur_len - drop + 1);
            cur_len = strlen(job->last_output);
        }
        memcpy(job->last_output + cur_len, buf, copy_len);
        job->last_output[cur_len + copy_len] = '\0';
    }

    close(pipefd[0]);

    int status = 0;
    while (waitpid(pid, &status, 0) < 0) {
        if (errno == EINTR) continue;
        return -1;
    }

    if (WIFEXITED(status)) return WEXITSTATUS(status);
    return -1;
}

static gboolean privileged_write_finished(gpointer user_data) {
    privileged_write_job_t *job = (privileged_write_job_t *)user_data;

    if (job->poll_id) {
        g_source_remove(job->poll_id);
        job->poll_id = 0;
    }

    gtk_widget_set_sensitive(app.write_button, TRUE);
    gtk_widget_set_sensitive(app.cancel_button, FALSE);
    gtk_label_set_text(GTK_LABEL(app.status_label), job->result == 0 ? "Privileged write complete" : "Privileged write failed");
    gtk_progress_bar_set_text(GTK_PROGRESS_BAR(app.progress_bar), job->result == 0 ? "Complete" : "Failed");

    GtkWidget *dialog = gtk_message_dialog_new(
        GTK_WINDOW(app.window),
        GTK_DIALOG_MODAL,
        job->result == 0 ? GTK_MESSAGE_INFO : GTK_MESSAGE_ERROR,
        GTK_BUTTONS_OK,
        job->result == 0 ? "Image write completed successfully."
                         : "Authentication or privileged write failed. pkexec is required for GUI writes. Last output: %s",
        job->last_output[0] ? job->last_output : "no output");
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);

    if (job->status_file[0]) unlink(job->status_file);
    free(job);
    return G_SOURCE_REMOVE;
}

static void *privileged_write_thread(void *arg) {
    privileged_write_job_t *job = (privileged_write_job_t *)arg;
    job->result = run_privileged_cli_write(job);
    g_idle_add(privileged_write_finished, job);
    return NULL;
}

void on_write_clicked(GtkButton *button, gpointer user_data) {
    (void)button;
    (void)user_data;
    
    if (strlen(app.image.path) == 0) {
        GtkWidget *dialog = gtk_message_dialog_new(
            GTK_WINDOW(app.window),
            GTK_DIALOG_MODAL,
            GTK_MESSAGE_ERROR,
            GTK_BUTTONS_OK,
            "Please select an image file.");
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        return;
    }
    
    if (app.selected_device == NULL) {
        GtkWidget *dialog = gtk_message_dialog_new(
            GTK_WINDOW(app.window),
            GTK_DIALOG_MODAL,
            GTK_MESSAGE_ERROR,
            GTK_BUTTONS_OK,
            "Please select a target device.");
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        return;
    }
    
    if (app.selected_device->readonly) {
        GtkWidget *dialog = gtk_message_dialog_new(
            GTK_WINDOW(app.window),
            GTK_DIALOG_MODAL,
            GTK_MESSAGE_ERROR,
            GTK_BUTTONS_OK,
            "The selected device is read-only.");
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        return;
    }
    
    char confirm_msg[MAX_PATH_LEN * 2 + 512];
    char size_str[32];
    format_size(app.selected_device->size, size_str, sizeof(size_str));
    
    snprintf(confirm_msg, sizeof(confirm_msg),
             "WARNING: All data on %s will be permanently destroyed!\n\n"
             "Device: %s (%s)\n"
             "Image: %s\n\n"
             "Are you absolutely sure you want to continue?",
             app.selected_device->path,
             app.selected_device->path, size_str,
             app.image.name);
    
    GtkWidget *dialog = gtk_message_dialog_new(
        GTK_WINDOW(app.window),
        GTK_DIALOG_MODAL,
        GTK_MESSAGE_WARNING,
        GTK_BUTTONS_YES_NO,
        "%s", confirm_msg);
    
    int response = gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
    
    if (response != GTK_RESPONSE_YES) {
        return;
    }
    
    memset(&app.ctx, 0, sizeof(write_context_t));
    app.ctx.bytes_total = app.image.compressed ? 0 : app.image.size;
    app.ctx.verify = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(app.verify_check));
    app.ctx.sync_writes = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(app.sync_check));
    
    int part_idx = gtk_combo_box_get_active(GTK_COMBO_BOX(app.partition_combo));
    app.ctx.partition_scheme = (partition_scheme_t)part_idx;
    app.ctx.make_bootable = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(app.bootable_check));
    
    if (app.image.is_windows && !app.ctx.make_bootable) {
        app.ctx.make_bootable = 1;
    }
    
    gchar *block_size_str = gtk_combo_box_text_get_active_text(
        GTK_COMBO_BOX_TEXT(app.blocksize_combo));
    app.ctx.block_size = DEFAULT_BLOCK_SIZE;
    if (block_size_str) {
        if (strstr(block_size_str, "512")) app.ctx.block_size = 512;
        else if (strstr(block_size_str, "4 KB")) app.ctx.block_size = 4 * 1024;
        else if (strstr(block_size_str, "64 KB")) app.ctx.block_size = 64 * 1024;
        else if (strstr(block_size_str, "1 MB")) app.ctx.block_size = 1024 * 1024;
        else if (strstr(block_size_str, "4 MB")) app.ctx.block_size = 4 * 1024 * 1024;
        else if (strstr(block_size_str, "16 MB")) app.ctx.block_size = 16 * 1024 * 1024;
        g_free(block_size_str);
    }

    if (geteuid() != 0) {
        GtkWidget *auth_dialog = gtk_message_dialog_new(
            GTK_WINDOW(app.window),
            GTK_DIALOG_MODAL,
            GTK_MESSAGE_INFO,
            GTK_BUTTONS_OK,
            "Administrator privileges are required to write the image. The system authentication prompt will open next.");
        gtk_dialog_run(GTK_DIALOG(auth_dialog));
        gtk_widget_destroy(auth_dialog);

        privileged_write_job_t *job = calloc(1, sizeof(privileged_write_job_t));
        if (!job) {
            GtkWidget *err_dialog = gtk_message_dialog_new(
                GTK_WINDOW(app.window), GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR,
                GTK_BUTTONS_OK, "Memory allocation failed.");
            gtk_dialog_run(GTK_DIALOG(err_dialog));
            gtk_widget_destroy(err_dialog);
            return;
        }

        snprintf(job->image_path, sizeof(job->image_path), "%s", app.image.path);
        snprintf(job->device_path, sizeof(job->device_path), "%s", app.selected_device->path);
        job->verify = app.ctx.verify;
        job->block_size = app.ctx.block_size;

        char status_template[] = "/tmp/uniximage-status-XXXXXX";
        int status_fd = mkstemp(status_template);
        if (status_fd < 0) {
            free(job);
            GtkWidget *err_dialog = gtk_message_dialog_new(
                GTK_WINDOW(app.window), GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR,
                GTK_BUTTONS_OK, "Failed to create privileged writer status file.");
            gtk_dialog_run(GTK_DIALOG(err_dialog));
            gtk_widget_destroy(err_dialog);
            return;
        }
        close(status_fd);
        snprintf(job->status_file, sizeof(job->status_file), "%s", status_template);
        unlink(job->status_file);
        job->start_time = time(NULL);

        gtk_widget_set_sensitive(app.write_button, FALSE);
        gtk_widget_set_sensitive(app.cancel_button, FALSE);
        gtk_label_set_text(GTK_LABEL(app.status_label), "Waiting for administrator authentication...");
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(app.progress_bar), 0.0);
        gtk_progress_bar_pulse(GTK_PROGRESS_BAR(app.progress_bar));
        gtk_progress_bar_set_text(GTK_PROGRESS_BAR(app.progress_bar), "Waiting for administrator authentication");
        job->poll_id = g_timeout_add(250, privileged_status_poll, job);

        pthread_t thread;
        if (pthread_create(&thread, NULL, privileged_write_thread, job) != 0) {
            if (job->poll_id) g_source_remove(job->poll_id);
            unlink(job->status_file);
            free(job);
            GtkWidget *err_dialog = gtk_message_dialog_new(
                GTK_WINDOW(app.window), GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR,
                GTK_BUTTONS_OK, "Failed to start privileged writer.");
            gtk_dialog_run(GTK_DIALOG(err_dialog));
            gtk_widget_destroy(err_dialog);
            return;
        }
        pthread_detach(thread);
        return;
    }
    
    gtk_widget_set_sensitive(app.write_button, FALSE);
    gtk_widget_set_sensitive(app.cancel_button, TRUE);
    
    app.running = 1;
    
    pthread_create(&app.write_thread, NULL, write_thread, NULL);
    
    app.timer_id = g_timeout_add(100, update_progress, NULL);
}

void on_cancel_clicked(GtkButton *button, gpointer user_data) {
    (void)button;
    (void)user_data;
    
    GtkWidget *dialog = gtk_message_dialog_new(
        GTK_WINDOW(app.window),
        GTK_DIALOG_MODAL,
        GTK_MESSAGE_QUESTION,
        GTK_BUTTONS_YES_NO,
        "Are you sure you want to cancel the write operation?");
    
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_YES) {
        app.ctx.cancelled = 1;
    }
    
    gtk_widget_destroy(dialog);
}

void create_gui(void) {
    app.window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(app.window), "UnixImage - Universal Disk Image Writer");
    gtk_window_set_default_size(GTK_WINDOW(app.window), 800, 650);
    gtk_window_set_position(GTK_WINDOW(app.window), GTK_WIN_POS_CENTER);
    gtk_container_set_border_width(GTK_CONTAINER(app.window), 15);
    
    g_signal_connect(app.window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    
    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_container_add(GTK_CONTAINER(app.window), main_box);
    
    GtkWidget *title_label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(title_label), 
                         "<span size='xx-large' weight='bold'>UnixImage Writer</span>");
    gtk_box_pack_start(GTK_BOX(main_box), title_label, FALSE, FALSE, 5);
    
    char subtitle[256];
    snprintf(subtitle, sizeof(subtitle), "Running on %s (%s) - Version %s",
             get_os_name(app.os), app.os_version, VERSION);
    GtkWidget *subtitle_label = gtk_label_new(subtitle);
    gtk_box_pack_start(GTK_BOX(main_box), subtitle_label, FALSE, FALSE, 0);
    
    GtkWidget *image_frame = gtk_frame_new("Image File");
    gtk_box_pack_start(GTK_BOX(main_box), image_frame, FALSE, FALSE, 5);
    
    GtkWidget *image_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(image_box), 10);
    gtk_container_add(GTK_CONTAINER(image_frame), image_box);
    // i love gtk boiler
    app.image_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(app.image_entry), "Select an image file...");
    gtk_editable_set_editable(GTK_EDITABLE(app.image_entry), FALSE);
    gtk_widget_set_hexpand(app.image_entry, TRUE);
    gtk_box_pack_start(GTK_BOX(image_box), app.image_entry, TRUE, TRUE, 0);
    
    GtkWidget *browse_button = gtk_button_new_with_label("Browse...");
    g_signal_connect(browse_button, "clicked", G_CALLBACK(on_browse_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(image_box), browse_button, FALSE, FALSE, 0);
    
    GtkWidget *device_frame = gtk_frame_new("Target Device");
    gtk_box_pack_start(GTK_BOX(main_box), device_frame, TRUE, TRUE, 5);
    
    GtkWidget *device_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(device_box), 10);
    gtk_container_add(GTK_CONTAINER(device_frame), device_box);
    
    GtkWidget *scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                    GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request(scrolled, -1, 180);
    gtk_box_pack_start(GTK_BOX(device_box), scrolled, TRUE, TRUE, 0);
    
    app.device_store = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_INT);
    app.device_list = gtk_tree_view_new_with_model(GTK_TREE_MODEL(app.device_store));
    
    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
    GtkTreeViewColumn *column = gtk_tree_view_column_new_with_attributes(
        "Available Devices", renderer, "text", 0, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(app.device_list), column);
    
    GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(app.device_list));
    g_signal_connect(selection, "changed", G_CALLBACK(on_device_selected), NULL);
    
    gtk_container_add(GTK_CONTAINER(scrolled), app.device_list);
    
    GtkWidget *refresh_button = gtk_button_new_with_label("Refresh Devices");
    g_signal_connect(refresh_button, "clicked", G_CALLBACK(on_refresh_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(device_box), refresh_button, FALSE, FALSE, 0);
    
    GtkWidget *options_frame = gtk_frame_new("Options");
    gtk_box_pack_start(GTK_BOX(main_box), options_frame, FALSE, FALSE, 5);
    
    GtkWidget *options_grid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(options_grid), 20);
    gtk_grid_set_row_spacing(GTK_GRID(options_grid), 8);
    gtk_container_set_border_width(GTK_CONTAINER(options_grid), 10);
    gtk_container_add(GTK_CONTAINER(options_frame), options_grid);
    
    app.verify_check = gtk_check_button_new_with_label("Verify after writing");
    gtk_grid_attach(GTK_GRID(options_grid), app.verify_check, 0, 0, 1, 1);
    
    app.sync_check = gtk_check_button_new_with_label("Synchronous writes");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(app.sync_check), TRUE);
    gtk_grid_attach(GTK_GRID(options_grid), app.sync_check, 1, 0, 1, 1);
    
    GtkWidget *blocksize_label = gtk_label_new("Block Size:");
    gtk_grid_attach(GTK_GRID(options_grid), blocksize_label, 2, 0, 1, 1);
    
    app.blocksize_combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(app.blocksize_combo), "512 B");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(app.blocksize_combo), "4 KB");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(app.blocksize_combo), "64 KB");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(app.blocksize_combo), "1 MB");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(app.blocksize_combo), "4 MB (Default)");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(app.blocksize_combo), "16 MB");
    gtk_combo_box_set_active(GTK_COMBO_BOX(app.blocksize_combo), 4);
    gtk_grid_attach(GTK_GRID(options_grid), app.blocksize_combo, 3, 0, 1, 1);
    
    GtkWidget *partition_label = gtk_label_new("Partition:");
    gtk_grid_attach(GTK_GRID(options_grid), partition_label, 0, 1, 1, 1);
    
    app.partition_combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(app.partition_combo), "Auto (Recommended)");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(app.partition_combo), "MBR");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(app.partition_combo), "GPT");
    gtk_combo_box_set_active(GTK_COMBO_BOX(app.partition_combo), 0);
    gtk_grid_attach(GTK_GRID(options_grid), app.partition_combo, 1, 1, 1, 1);
    
    app.bootable_check = gtk_check_button_new_with_label("Make bootable (Windows)");
    gtk_grid_attach(GTK_GRID(options_grid), app.bootable_check, 0, 2, 2, 1);
    
    GtkWidget *progress_frame = gtk_frame_new("Progress");
    gtk_box_pack_start(GTK_BOX(main_box), progress_frame, FALSE, FALSE, 5);
    
    GtkWidget *progress_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(progress_box), 10);
    gtk_container_add(GTK_CONTAINER(progress_frame), progress_box);
    
    app.progress_bar = gtk_progress_bar_new();
    gtk_progress_bar_set_show_text(GTK_PROGRESS_BAR(app.progress_bar), TRUE);
    gtk_progress_bar_set_text(GTK_PROGRESS_BAR(app.progress_bar), "Ready");
    gtk_box_pack_start(GTK_BOX(progress_box), app.progress_bar, FALSE, FALSE, 0);
    
    GtkWidget *status_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 20);
    gtk_box_pack_start(GTK_BOX(progress_box), status_box, FALSE, FALSE, 0);
    
    app.status_label = gtk_label_new("Ready");
    gtk_box_pack_start(GTK_BOX(status_box), app.status_label, FALSE, FALSE, 0);
    
    app.speed_label = gtk_label_new("");
    gtk_box_pack_start(GTK_BOX(status_box), app.speed_label, FALSE, FALSE, 0);
    
    GtkWidget *eta_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_end(GTK_BOX(status_box), eta_box, FALSE, FALSE, 0);
    
    gtk_box_pack_start(GTK_BOX(eta_box), gtk_label_new("ETA:"), FALSE, FALSE, 0);
    app.eta_label = gtk_label_new("--");
    gtk_box_pack_start(GTK_BOX(eta_box), app.eta_label, FALSE, FALSE, 0);
    
    GtkWidget *button_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_pack_start(GTK_BOX(main_box), button_box, FALSE, FALSE, 10);
    
    app.write_button = gtk_button_new_with_label("Write Image");
    gtk_widget_set_size_request(app.write_button, 150, 40);
    g_signal_connect(app.write_button, "clicked", G_CALLBACK(on_write_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(button_box), app.write_button, TRUE, TRUE, 0);
    
    app.cancel_button = gtk_button_new_with_label("Cancel");
    gtk_widget_set_size_request(app.cancel_button, 150, 40);
    gtk_widget_set_sensitive(app.cancel_button, FALSE);
    g_signal_connect(app.cancel_button, "clicked", G_CALLBACK(on_cancel_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(button_box), app.cancel_button, TRUE, TRUE, 0);
    
    GtkWidget *exit_button = gtk_button_new_with_label("Exit");
    gtk_widget_set_size_request(exit_button, 150, 40);
    g_signal_connect(exit_button, "clicked", G_CALLBACK(gtk_main_quit), NULL);
    gtk_box_pack_start(GTK_BOX(button_box), exit_button, TRUE, TRUE, 0);
    
    update_device_list();
    
    gtk_widget_show_all(app.window);
}

#endif

static int reexec_with_privilege(int argc, char *argv[]) {
    const char *tools[] = {"sudo", "doas", "pkexec", NULL};

    for (int t = 0; tools[t]; t++) {
        char **new_argv = calloc((size_t)argc + 3, sizeof(char *));
        if (!new_argv) return -1;

        int n = 0;
        new_argv[n++] = (char *)tools[t];
        for (int i = 0; i < argc; i++) {
            new_argv[n++] = argv[i];
        }
        new_argv[n++] = "--sudo-mode";
        new_argv[n] = NULL;

        fprintf(stderr, "Administrator privileges are required to write disk images. Trying %s...\n", tools[t]);
        execvp(tools[t], new_argv);
        free(new_argv);
    }

    fprintf(stderr, "Error: writing requires root privileges. Install sudo/doas/pkexec or run as root.\n");
    return -1;
}

void print_usage(const char *prog) {
    printf("UnixImage - Universal Disk Image Writer v%s\n", VERSION);
    printf("Usage: %s [options] [image] [device]\n\n", prog);
    printf("Options:\n");
    printf("  -h, --help        Show this help message\n");
    printf("  -v, --version     Show version information\n");
    printf("  -l, --list        List available devices\n");
    printf("  -i, --image FILE  Specify image file\n");
    printf("  -d, --device DEV  Specify target device\n");
    printf("  -y, --yes         Skip confirmation prompt\n");
    printf("  -V, --verify      Verify after writing\n");
    printf("  -b, --blocksize N Set block size in bytes\n");
    printf("  -q, --quiet       Quiet mode\n");
    printf("  --status-file F   Write machine-readable progress to file\n");
#ifndef CLI_MODE
    printf("  -c, --cli         Force CLI mode\n");
#endif
    printf("\nSupported image formats:\n");
    printf("  ISO, IMG, RAW, DMG, VHD, VHDX, VDI, VMDK, QCOW, QCOW2\n");
    printf("  Compressed: XZ, GZ, BZ2, ZSTD, LZ4, ZIP\n");
    printf("  Windows installer ISOs: FAT32 USB with automatic install.wim splitting\n");
    printf("\nSupported operating systems:\n");
    printf("  Linux, macOS, FreeBSD, OpenBSD, NetBSD, DragonFly BSD,\n");
    printf("  Solaris, illumos, AIX, HP-UX, IRIX, QNX, MINIX, Haiku,\n");
    printf("  GNU/Hurd, MidnightBSD, HardenedBSD, GhostBSD, and more\n");
}

void print_version(void) {
    printf("UnixImage v%s\n", VERSION);
    printf("Universal Disk Image Writer for Unix-like Systems\n");
    printf("Running on: %s (%s)\n", get_os_name(app.os), app.os_version);
}

void list_devices_cli(void) {
    printf("Available devices:\n");
    printf("%-20s %-12s %-8s %s\n", "Device", "Size", "Type", "Model");
    printf("--------------------------------------------------------------------------------\n");
    
    app.device_count = enumerate_devices(app.devices, MAX_DEVICES);
    
    if (app.device_count == 0) {
        printf("No devices found. Make sure you have proper permissions.\n");
        return;
    }
    
    for (int i = 0; i < app.device_count; i++) {
        device_t *dev = &app.devices[i];
        char size_str[32];
        format_size(dev->size, size_str, sizeof(size_str));
        
        printf("%-20s %-12s %-8s %s\n",
               dev->path, size_str,
               dev->removable ? "USB" : "Fixed",
               dev->model);
    }
}

int cli_write(const char *image_path, const char *device_path, 
              int verify, int skip_confirm, size_t block_size) {
    struct stat st;
    if (stat(image_path, &st) != 0) {
        fprintf(stderr, "Error: Cannot access image file: %s\n", image_path);
        return 1;
    }
    
    strncpy(app.image.path, image_path, sizeof(app.image.path) - 1);
    app.image.size = (uint64_t)st.st_size;
    app.image.type = detect_image_type(image_path);
    app.image.compressed = is_compressed_image(app.image.type);
    app.image.is_windows = is_windows_image(image_path);
    
    int found = 0;
    app.device_count = enumerate_devices(app.devices, MAX_DEVICES);
    for (int i = 0; i < app.device_count; i++) {
        if (strcmp(app.devices[i].path, device_path) == 0) {
            app.selected_device = &app.devices[i];
            found = 1;
            break;
        }
    }
    
    if (!found) {
        if (app.device_count >= MAX_DEVICES) {
            fprintf(stderr, "Error: Device list is full; cannot add manual target: %s\n", device_path);
            return 1;
        }
        device_t *manual_dev = &app.devices[app.device_count];
        memset(manual_dev, 0, sizeof(device_t));
        strncpy(manual_dev->path, device_path, sizeof(manual_dev->path) - 1);
        manual_dev->size = get_device_size(device_path);
        if (manual_dev->size == 0) {
            fprintf(stderr, "Error: Cannot access device: %s\n", device_path);
            return 1;
        }
        app.selected_device = manual_dev;
    }
    
    char img_size[32], dev_size[32];
    format_size(app.image.size, img_size, sizeof(img_size));
    format_size(app.selected_device->size, dev_size, sizeof(dev_size));
    
    printf("Image:  %s (%s, %s)\n", image_path, img_size, 
           get_image_type_name(app.image.type));
    printf("Device: %s (%s)\n", device_path, dev_size);
    if (app.image.is_windows) {
        printf("Mode:   Windows installer USB (partition, FAT32 format, file copy)\n");
    } else if (app.image.compressed) {
        printf("Mode:   Decompress while writing (%s)\n", get_image_type_name(app.image.type));
    } else {
        printf("Mode:   Raw block image write\n");
    }
    printf("\n");

    if (!app.image.compressed && !app.image.is_windows && app.image.size > app.selected_device->size) {
        fprintf(stderr, "Error: Image is larger than target device.\n");
        return 1;
    }
    
    if (!skip_confirm) {
        printf("WARNING: All data on %s will be destroyed!\n", device_path);
        printf("Type 'yes' to continue: ");
        fflush(stdout);
        
        char response[16];
        if (fgets(response, sizeof(response), stdin) == NULL || 
            strncmp(response, "yes", 3) != 0) {
            printf("Aborted.\n");
            return 1;
        }
    }
    
    memset(&app.ctx, 0, sizeof(write_context_t));
    app.ctx.bytes_total = app.image.compressed ? 0 : app.image.size;
    app.ctx.verify = verify;
    app.ctx.block_size = block_size;
    app.ctx.sync_writes = 1;
    app.running = 1;
    snprintf(app.ctx.message, sizeof(app.ctx.message), "Starting privileged writer...");
    write_status_snapshot(&app.ctx);
    
    pthread_t thread;
    if (pthread_create(&thread, NULL, write_thread, NULL) != 0) {
        fprintf(stderr, "Error: Failed to start write thread.\n");
        app.running = 0;
        return 1;
    }
    
    time_t last_update = 0;
    while (app.running) {
        time_t now = time(NULL);
        if (now != last_update) {
            pthread_mutex_lock(&app.mutex);
            
            char size_written[32], size_total[32], speed[32], eta[32];
            format_size(app.ctx.bytes_written, size_written, sizeof(size_written));
            format_size(app.ctx.bytes_total, size_total, sizeof(size_total));
            format_speed(app.ctx.speed, speed, sizeof(speed));
            format_time(app.ctx.eta, eta, sizeof(eta));
            
            if (app.image.compressed) {
                printf("\r[%s] %s written @ %s    ",
                       app.ctx.message, size_written, speed);
            } else {
                printf("\r[%5.1f%%] %s / %s @ %s ETA: %s    ",
                       app.ctx.progress, size_written, size_total, speed, eta);
            }
            fflush(stdout);
            
            pthread_mutex_unlock(&app.mutex);
            last_update = now;
        }
        usleep(100000);
    }
    
    pthread_join(thread, NULL);
    
    printf("\n");
    
    if (app.ctx.state == STATE_COMPLETE) {
        printf("Success! Image written successfully.\n");
        return 0;
    } else if (app.ctx.state == STATE_CANCELLED) {
        printf("Cancelled.\n");
        return 1;
    } else {
        printf("Error: %s\n", app.ctx.message);
        return 1;
    }
}

int main(int argc, char *argv[]) {
    setlocale(LC_ALL, "");

    memset(&app, 0, sizeof(app_t));
    if (argc > 0 && argv[0]) {
        snprintf(app_exe_path, sizeof(app_exe_path), "%s", argv[0]);
    }
    pthread_mutex_init(&app.mutex, NULL);

    app.os = detect_os();
    get_os_version(app.os_version, sizeof(app.os_version));
    strncpy(app.os_name, get_os_name(app.os), sizeof(app.os_name) - 1);
    app.is_root = (geteuid() == 0);

    app.log_file = fopen(LOG_FILE, "a");

    log_message("INFO", "UnixImage v%s starting on %s", VERSION, app.os_name);

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    int cli_mode = 0;
    int list_only = 0;
    int skip_confirm = 0;
    int verify = 0;
    size_t block_size = DEFAULT_BLOCK_SIZE;
    char *image_file = NULL;
    char *device_file = NULL;
    char *status_file = NULL;
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0) {
            print_version();
            return 0;
        } else if (strcmp(argv[i], "-l") == 0 || strcmp(argv[i], "--list") == 0) {
            list_only = 1;
        } else if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--cli") == 0) {
            cli_mode = 1;
        } else if (strcmp(argv[i], "-y") == 0 || strcmp(argv[i], "--yes") == 0) {
            skip_confirm = 1;
        } else if (strcmp(argv[i], "--sudo-mode") == 0) {
            app.sudo_mode = 1;
        } else if (strcmp(argv[i], "--status-file") == 0 && i + 1 < argc) {
            status_file = argv[++i];
        } else if (strcmp(argv[i], "-V") == 0 || strcmp(argv[i], "--verify") == 0) {
            verify = 1;
        } else if ((strcmp(argv[i], "-b") == 0 || strcmp(argv[i], "--blocksize") == 0) 
                   && i + 1 < argc) {
            block_size = (size_t)atol(argv[++i]);
            if (block_size < MIN_BLOCK_SIZE || block_size > MAX_BLOCK_SIZE) {
                fprintf(stderr, "Invalid block size. Using default.\n");
                block_size = DEFAULT_BLOCK_SIZE;
            }
        } else if ((strcmp(argv[i], "-i") == 0 || strcmp(argv[i], "--image") == 0) 
                   && i + 1 < argc) {
            image_file = argv[++i];
        } else if ((strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--device") == 0) 
                   && i + 1 < argc) {
            device_file = argv[++i];
        } else if (argv[i][0] != '-') {
            if (!image_file) {
                image_file = argv[i];
            } else if (!device_file) {
                device_file = argv[i];
            }
        }
    }
    
    if (list_only) {
        list_devices_cli();
        if (app.log_file) fclose(app.log_file);
        return 0;
    }
    
#ifdef CLI_MODE
    cli_mode = 1;
#endif
    
    if (image_file && device_file) {
        cli_mode = 1;
}

    if (status_file) {
        snprintf(status_file_path, sizeof(status_file_path), "%s", status_file);
    }

    if (cli_mode) {
        if (!image_file || !device_file) {
            fprintf(stderr, "Error: Image and device must be specified in CLI mode.\n");
            fprintf(stderr, "Use -h for help.\n");
            if (app.log_file) fclose(app.log_file);
            return 1;
        }
        
        if (!app.is_root && !app.sudo_mode) {
            if (app.log_file) fclose(app.log_file);
            pthread_mutex_destroy(&app.mutex);
            return reexec_with_privilege(argc, argv) == 0 ? 0 : 1;
        }
        
        int ret = cli_write(image_file, device_file, verify, skip_confirm, block_size);
        if (app.log_file) fclose(app.log_file);
        pthread_mutex_destroy(&app.mutex);
        return ret;
    }
    
#ifndef CLI_MODE
    if (!app.is_root) {
        fprintf(stderr, 
            "Warning: Running without root privileges. Some devices may not be accessible.\n");
    }
    
    gtk_init(&argc, &argv);
    create_gui();
    gtk_main();
#else
    print_usage(argv[0]);
#endif
    
    if (app.log_file) {
        fclose(app.log_file);
    }
    
    pthread_mutex_destroy(&app.mutex);
    
    return 0;
}
