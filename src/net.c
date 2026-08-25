#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <locale.h>
#include <ifaddrs.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <arpa/inet.h>
#include <SystemConfiguration/SCNetworkConfiguration.h>
#include <IOKit/IOKitLib.h>
#include <curses.h>

/* Version comes from the VERSION file via the build system
   (-DNET_VERSION='"x.y.z"'); this is the standalone fallback. */
#ifndef NET_VERSION
#define NET_VERSION  "1.0.0"
#endif
#define MAX_ROWS 128

// Column content widths (excluding borders / padding)
#define W1 14   // Interface
#define W2 28   // Hardware Port
#define W3 18   // IPv4 Address
#define W4 18   // MAC Address
#define W7 10   // Max BW
#define W5 8    // Status
#define W6 20   // Serial Number

// Active ANSI colors
#define RESET       "\033[0m"
#define BOLD_CYAN   "\033[1;36m"
#define BOLD_BLUE   "\033[1;34m"
#define WHITE       "\033[1;37m"
#define GREEN       "\033[32m"
#define YELLOW      "\033[33m"
#define MAGENTA     "\033[35m"
#define CYAN        "\033[36m"
#define BOLD_WHITE  "\033[1;37m"

// Inactive ANSI colors (dim equivalents)
#define DIM_CYAN    "\033[2;36m"
#define DIM_BLUE    "\033[2;34m"
#define DIM_WHITE   "\033[2;37m"
#define DIM_GREEN   "\033[2;32m"
#define DIM_YELLOW  "\033[2;33m"
#define DIM_MAGENTA "\033[2;35m"

// ncurses color pair IDs
#define CP_HEADER  1
#define CP_IFACE   2
#define CP_PORT    3
#define CP_IP      4
#define CP_MAC     5
#define CP_STATUS  6
#define CP_SERIAL  7
#define CP_BW      8

typedef struct {
    char ifname[IFNAMSIZ];
    char hw_port[128];
    char ip[INET_ADDRSTRLEN];
    char mac[32];
    char bandwidth[16];
    char serial[64];
    int  active;
} Row;

// ─── System helpers ───────────────────────────────────────────────────────────

static int use_colors(void) {
    return isatty(STDOUT_FILENO) ? 1 : 0;
}

static void get_hardware_port_name(const char *ifname, char *buf, size_t len) {
    CFArrayRef interfaces = SCNetworkInterfaceCopyAll();
    if (!interfaces) { snprintf(buf, len, "%s", ifname); return; }

    CFIndex count = CFArrayGetCount(interfaces);
    for (CFIndex i = 0; i < count; i++) {
        SCNetworkInterfaceRef iface = (SCNetworkInterfaceRef)CFArrayGetValueAtIndex(interfaces, i);
        CFStringRef bsd = SCNetworkInterfaceGetBSDName(iface);
        if (!bsd) continue;

        char bsd_name[IFNAMSIZ];
        if (!CFStringGetCString(bsd, bsd_name, sizeof(bsd_name), kCFStringEncodingASCII)) continue;
        if (strcmp(bsd_name, ifname) != 0) continue;

        CFStringRef display = SCNetworkInterfaceGetLocalizedDisplayName(iface);
        if (display)
            CFStringGetCString(display, buf, len, kCFStringEncodingUTF8);
        else
            snprintf(buf, len, "%s", ifname);

        CFRelease(interfaces);
        return;
    }
    CFRelease(interfaces);
    snprintf(buf, len, "%s", ifname);
}

static void get_mac_address(const char *ifname, char *mac_str, size_t len) {
    struct ifaddrs *ifaddr, *ifa;
    if (getifaddrs(&ifaddr) == -1) { snprintf(mac_str, len, "N/A"); return; }

    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr) continue;
        if (ifa->ifa_addr->sa_family != AF_LINK) continue;
        if (strcmp(ifa->ifa_name, ifname) != 0) continue;

        struct sockaddr_dl *sdl = (struct sockaddr_dl *)ifa->ifa_addr;
        if (sdl->sdl_alen > 0) {
            char *p = mac_str;
            size_t remaining = len;
            const unsigned char *addr = (const unsigned char *)LLADDR(sdl);
            for (int i = 0; i < sdl->sdl_alen; i++) {
                int n = snprintf(p, remaining, "%s%02X", i > 0 ? ":" : "", addr[i]);
                if (n < 0 || (size_t)n >= remaining) break;
                p += n; remaining -= n;
            }
            freeifaddrs(ifaddr);
            return;
        }
    }
    snprintf(mac_str, len, "N/A");
    freeifaddrs(ifaddr);
}

// Returns speed in Mbps for a single media word, 0 if not decodable.
static uint64_t media_to_mbps(int media) {
    if (IFM_TYPE(media) != IFM_ETHER) return 0;

    switch (IFM_SUBTYPE(media)) {
    // 10 Mbps
    case IFM_10_T: case IFM_10_2: case IFM_10_5:
    case IFM_10_STP: case IFM_10_FL:
        return 10;
    // 100 Mbps
    case IFM_100_TX: case IFM_100_FX: case IFM_100_T4:
    case IFM_100_VG: case IFM_100_T2:
#ifdef IFM_100_T
    case IFM_100_T:
#endif
#ifdef IFM_100_SGMII
    case IFM_100_SGMII:
#endif
        return 100;
    // 1 Gbps
    case IFM_1000_SX: case IFM_1000_LX: case IFM_1000_CX:
    case IFM_1000_T: case IFM_1000_KX:
#ifdef IFM_1000_CX_SGMII
    case IFM_1000_CX_SGMII:
#endif
#ifdef IFM_1000_SGMII
    case IFM_1000_SGMII:
#endif
        return 1000;
    // 2.5 Gbps
    case IFM_2500_T: case IFM_2500_SX: case IFM_2500_KX:
#ifdef IFM_2500_X
    case IFM_2500_X:
#endif
        return 2500;
    // 5 Gbps
    case IFM_5000_T:
#ifdef IFM_5000_KR
    case IFM_5000_KR:
#endif
#ifdef IFM_5000_KR_S
    case IFM_5000_KR_S:
#endif
#ifdef IFM_5000_KR1
    case IFM_5000_KR1:
#endif
        return 5000;
    // 10 Gbps
    case IFM_10G_SR: case IFM_10G_LR: case IFM_10G_CX4:
    case IFM_10G_T:  case IFM_10G_KX4: case IFM_10G_KR:
    case IFM_10G_CR1: case IFM_10G_ER:
#ifdef IFM_10G_TWINAX
    case IFM_10G_TWINAX:
#endif
#ifdef IFM_10G_TWINAX_LONG
    case IFM_10G_TWINAX_LONG:
#endif
#ifdef IFM_10G_LRM
    case IFM_10G_LRM:
#endif
#ifdef IFM_10G_AOC
    case IFM_10G_AOC:
#endif
#ifdef IFM_10G_SFI
    case IFM_10G_SFI:
#endif
        return 10000;
    // 20 Gbps
    case IFM_20G_KR2:
        return 20000;
    // 25 Gbps
#ifdef IFM_25G_CR
    case IFM_25G_CR:
#endif
#ifdef IFM_25G_KR
    case IFM_25G_KR:
#endif
#ifdef IFM_25G_SR
    case IFM_25G_SR:
#endif
#ifdef IFM_25G_LR
    case IFM_25G_LR:
#endif
#ifdef IFM_25G_T
    case IFM_25G_T:
#endif
#ifdef IFM_25G_ACC
    case IFM_25G_ACC:
#endif
#ifdef IFM_25G_AOC
    case IFM_25G_AOC:
#endif
#ifdef IFM_25G_PCIE
    case IFM_25G_PCIE:
#endif
        return 25000;
    // 40 Gbps
#ifdef IFM_40G_CR4
    case IFM_40G_CR4:
#endif
#ifdef IFM_40G_SR4
    case IFM_40G_SR4:
#endif
#ifdef IFM_40G_LR4
    case IFM_40G_LR4:
#endif
#ifdef IFM_40G_KR4
    case IFM_40G_KR4:
#endif
#ifdef IFM_40G_XLPPI
    case IFM_40G_XLPPI:
#endif
#ifdef IFM_40G_XLAUI
    case IFM_40G_XLAUI:
#endif
#ifdef IFM_40G_ER4
    case IFM_40G_ER4:
#endif
        return 40000;
    // 50 Gbps
#ifdef IFM_50G_PCIE
    case IFM_50G_PCIE:
#endif
#ifdef IFM_50G_CR2
    case IFM_50G_CR2:
#endif
#ifdef IFM_50G_KR2
    case IFM_50G_KR2:
#endif
#ifdef IFM_50G_SR2
    case IFM_50G_SR2:
#endif
#ifdef IFM_50G_LR2
    case IFM_50G_LR2:
#endif
        return 50000;
    // 56 Gbps
#ifdef IFM_56G_R4
    case IFM_56G_R4:
#endif
        return 56000;
    // 100 Gbps
#ifdef IFM_100G_CR4
    case IFM_100G_CR4:
#endif
#ifdef IFM_100G_SR4
    case IFM_100G_SR4:
#endif
#ifdef IFM_100G_KR4
    case IFM_100G_KR4:
#endif
#ifdef IFM_100G_LR4
    case IFM_100G_LR4:
#endif
        return 100000;
    default:
        return 0;
    }
}

static void format_bandwidth(uint64_t mbps, char *buf, size_t len) {
    if (mbps == 0) {
        snprintf(buf, len, "N/A");
    } else if (mbps < 1000) {
        snprintf(buf, len, "%llu Mbps", (unsigned long long)mbps);
    } else if (mbps % 1000 == 0) {
        snprintf(buf, len, "%llu Gbps", (unsigned long long)(mbps / 1000));
    } else {
        // e.g. 2500 → "2.5 Gbps"
        snprintf(buf, len, "%.4g Gbps", (double)mbps / 1000.0);
    }
}

static void get_max_bandwidth(const char *ifname, char *buf, size_t len) {
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) { snprintf(buf, len, "N/A"); return; }

    struct ifmediareq ifmr;
    memset(&ifmr, 0, sizeof(ifmr));
    strlcpy(ifmr.ifm_name, ifname, IFNAMSIZ);

    // First call: query the number of supported media types.
    if (ioctl(fd, SIOCGIFMEDIA, &ifmr) < 0 || ifmr.ifm_count == 0) {
        close(fd);
        snprintf(buf, len, "N/A");
        return;
    }

    int count = ifmr.ifm_count;
    int *mlist = calloc(count, sizeof(int));
    if (!mlist) { close(fd); snprintf(buf, len, "N/A"); return; }

    // Second call: fill the media list.
    ifmr.ifm_ulist = mlist;
    if (ioctl(fd, SIOCGIFMEDIA, &ifmr) < 0) {
        free(mlist); close(fd);
        snprintf(buf, len, "N/A");
        return;
    }
    close(fd);

    uint64_t max_mbps = 0;
    for (int i = 0; i < count; i++) {
        uint64_t s = media_to_mbps(mlist[i]);
        if (s > max_mbps) max_mbps = s;
    }
    free(mlist);

    format_bandwidth(max_mbps, buf, len);
}

static void get_serial_number(const char *ifname, char *buf, size_t len) {
    CFMutableDictionaryRef match = IOBSDNameMatching(kIOMainPortDefault, 0, ifname);
    if (!match) { snprintf(buf, len, "N/A"); return; }

    io_service_t service = IOServiceGetMatchingService(kIOMainPortDefault, match);
    if (!service) { snprintf(buf, len, "N/A"); return; }

    static const CFStringRef keys[] = {
        CFSTR("USB Serial Number"),
        CFSTR("serial-number"),
    };

    CFTypeRef prop = NULL;
    for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]) && !prop; i++) {
        prop = IORegistryEntrySearchCFProperty(
            service, kIOServicePlane, keys[i], kCFAllocatorDefault,
            kIORegistryIterateRecursively | kIORegistryIterateParents);
    }
    IOObjectRelease(service);

    if (!prop) { snprintf(buf, len, "N/A"); return; }

    if (CFGetTypeID(prop) == CFStringGetTypeID()) {
        CFStringGetCString((CFStringRef)prop, buf, len, kCFStringEncodingASCII);
    } else if (CFGetTypeID(prop) == CFDataGetTypeID()) {
        const UInt8 *bytes = CFDataGetBytePtr((CFDataRef)prop);
        CFIndex dlen = CFDataGetLength((CFDataRef)prop);
        size_t copy = ((size_t)dlen < len - 1) ? (size_t)dlen : len - 1;
        memcpy(buf, bytes, copy);
        buf[copy] = '\0';
    } else {
        snprintf(buf, len, "N/A");
    }
    CFRelease(prop);
}

// ─── Data collection ──────────────────────────────────────────────────────────

static int collect_rows(Row *rows, int max, int show_all, int long_mode) {
    struct ifaddrs *ifaddr, *ifa;
    if (getifaddrs(&ifaddr) == -1) return -1;

    int count = 0;
    char active_names[64][IFNAMSIZ];
    int  active_count = 0;

    for (ifa = ifaddr; ifa && count < max; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr) continue;
        if (ifa->ifa_addr->sa_family != AF_INET) continue;
        if (!(ifa->ifa_flags & IFF_UP)) continue;

        Row *r = &rows[count++];
        strlcpy(r->ifname, ifa->ifa_name, IFNAMSIZ);
        get_hardware_port_name(ifa->ifa_name, r->hw_port, sizeof(r->hw_port));
        inet_ntop(AF_INET, &((struct sockaddr_in *)ifa->ifa_addr)->sin_addr,
                  r->ip, sizeof(r->ip));
        get_mac_address(ifa->ifa_name, r->mac, sizeof(r->mac));
        if (long_mode) {
            get_max_bandwidth(ifa->ifa_name, r->bandwidth, sizeof(r->bandwidth));
            get_serial_number(ifa->ifa_name, r->serial, sizeof(r->serial));
        }
        r->active = 1;

        if (active_count < 64)
            strlcpy(active_names[active_count++], ifa->ifa_name, IFNAMSIZ);
    }

    if (show_all) {
        char seen[64][IFNAMSIZ];
        int  seen_count = 0;

        for (ifa = ifaddr; ifa && count < max; ifa = ifa->ifa_next) {
            if (!ifa->ifa_addr) continue;
            if (ifa->ifa_addr->sa_family != AF_LINK) continue;

            int skip = 0;
            for (int i = 0; i < active_count; i++)
                if (strcmp(active_names[i], ifa->ifa_name) == 0) { skip = 1; break; }
            if (skip) continue;

            for (int i = 0; i < seen_count; i++)
                if (strcmp(seen[i], ifa->ifa_name) == 0) { skip = 1; break; }
            if (skip) continue;

            if (seen_count < 64)
                strlcpy(seen[seen_count++], ifa->ifa_name, IFNAMSIZ);

            Row *r = &rows[count++];
            strlcpy(r->ifname, ifa->ifa_name, IFNAMSIZ);
            get_hardware_port_name(ifa->ifa_name, r->hw_port, sizeof(r->hw_port));
            strlcpy(r->ip, "N/A", sizeof(r->ip));
            get_mac_address(ifa->ifa_name, r->mac, sizeof(r->mac));
            if (long_mode) {
                get_max_bandwidth(ifa->ifa_name, r->bandwidth, sizeof(r->bandwidth));
                get_serial_number(ifa->ifa_name, r->serial, sizeof(r->serial));
            }
            r->active = 0;
        }
    }

    freeifaddrs(ifaddr);
    return count;
}

// ─── Help / version ───────────────────────────────────────────────────────────

static void print_help(const char *prog) {
    printf(
        "Usage: %s [OPTIONS]\n"
        "\n"
        "Show network interfaces and their IPv4 addresses.\n"
        "\n"
        "Options:\n"
        "  -h, --help     Show this help message and exit\n"
        "  -v, --version  Print version and exit\n"
        "  -a, --all      Show all interfaces, including inactive ones\n"
        "  -l, --long     Long view: all columns plus max bandwidth and serial number\n"
        "                 (default shows Hardware Port and IPv4 Address only)\n"
        "  -t, --table    Render a rich bordered table using ncurses\n",
        prog);
}

static void print_version(void) {
    printf("net %s\n", NET_VERSION);
}

// ─── Text mode ────────────────────────────────────────────────────────────────

static void print_text(const Row *rows, int count, int colors, int long_mode) {
    // Separator strings: exact visual width, UTF-8 multibyte — not snprintf.
    static const char SEP1[] = "──────────────";               // W1=14
    static const char SEP2[] = "────────────────────────────"; // W2=28
    static const char SEP3[] = "──────────────────";           // W3=18
    static const char SEP4[] = "──────────────────";           // W4=18
    static const char SEP7[] = "──────────";                   // W7=10
    static const char SEP5[] = "────────";                     // W5=8
    static const char SEP6[] = "────────────────────";         // W6=20

    if (long_mode) {
        char h1[W1+1], h2[W2+1], h3[W3+1], h4[W4+1], h7[W7+1], h6[W6+1];
        snprintf(h1, sizeof(h1), "%-*s", W1, "Interface");
        snprintf(h2, sizeof(h2), "%-*s", W2, "Hardware Port");
        snprintf(h3, sizeof(h3), "%-*s", W3, "IPv4 Address");
        snprintf(h4, sizeof(h4), "%-*s", W4, "MAC Address");
        snprintf(h7, sizeof(h7), "%-*s", W7, "Max BW");
        snprintf(h6, sizeof(h6), "%-*s", W6, "Serial Number");

        if (colors) {
            printf("%s%s%s %s%s%s %s%s%s %s%s%s %s%s%s %s%-*s%s %s%s%s\n",
                   BOLD_CYAN, h1, RESET, BOLD_CYAN, h2, RESET,
                   BOLD_CYAN, h3, RESET, BOLD_CYAN, h4, RESET,
                   BOLD_CYAN, h7, RESET,
                   BOLD_CYAN, W5, "Status", RESET,
                   BOLD_CYAN, h6, RESET);
            printf("%s%s%s %s%s%s %s%s%s %s%s%s %s%s%s %s%s%s %s%s%s\n",
                   BOLD_CYAN, SEP1, RESET, BOLD_CYAN, SEP2, RESET,
                   BOLD_CYAN, SEP3, RESET, BOLD_CYAN, SEP4, RESET,
                   BOLD_CYAN, SEP7, RESET, BOLD_CYAN, SEP5, RESET,
                   BOLD_CYAN, SEP6, RESET);
        } else {
            printf("%s %s %s %s %s %-*s %s\n", h1, h2, h3, h4, h7, W5, "Status", h6);
            printf("%s %s %s %s %s %s %s\n",   SEP1, SEP2, SEP3, SEP4, SEP7, SEP5, SEP6);
        }
    } else {
        char h2[W2+1], h3[W3+1];
        snprintf(h2, sizeof(h2), "%-*s", W2, "Hardware Port");
        snprintf(h3, sizeof(h3), "%-*s", W3, "IPv4 Address");

        if (colors) {
            printf("%s%s%s %s%s%s\n", BOLD_CYAN, h2, RESET, BOLD_CYAN, h3, RESET);
            printf("%s%s%s %s%s%s\n", BOLD_CYAN, SEP2, RESET, BOLD_CYAN, SEP3, RESET);
        } else {
            printf("%s %s\n", h2, h3);
            printf("%s %s\n", SEP2, SEP3);
        }
    }

    int prev_active = 1;
    for (int i = 0; i < count; i++) {
        const Row *r = &rows[i];
        int a = r->active;

        if (prev_active && !a) { printf("\n"); prev_active = 0; }

        if (long_mode) {
            char c1[W1+1], c2[W2+1], c3[W3+1], c4[W4+1], c7[W7+1], c6[W6+1];
            snprintf(c1, sizeof(c1), "%-*s", W1, r->ifname);
            snprintf(c2, sizeof(c2), "%-*s", W2, r->hw_port);
            snprintf(c3, sizeof(c3), "%-*s", W3, r->ip);
            snprintf(c4, sizeof(c4), "%-*s", W4, r->mac);
            snprintf(c7, sizeof(c7), "%-*s", W7, r->bandwidth);
            snprintf(c6, sizeof(c6), "%-*s", W6, r->serial);

            if (colors) {
                printf("%s%s%s %s%s%s %s%s%s %s%s%s %s%s%s %s%-*s%s %s%s%s\n",
                       a ? BOLD_BLUE : DIM_BLUE,    c1, RESET,
                       a ? WHITE     : DIM_WHITE,   c2, RESET,
                       a ? GREEN     : DIM_GREEN,   c3, RESET,
                       a ? YELLOW    : DIM_YELLOW,  c4, RESET,
                       a ? BOLD_WHITE: DIM_WHITE,   c7, RESET,
                       a ? MAGENTA   : DIM_MAGENTA, W5, a ? "Active" : "Inactive", RESET,
                       a ? CYAN      : DIM_CYAN,    c6, RESET);
            } else {
                printf("%s %s %s %s %s %-*s %s\n",
                       c1, c2, c3, c4, c7, W5, a ? "Active" : "Inactive", c6);
            }
        } else {
            char c2[W2+1], c3[W3+1];
            snprintf(c2, sizeof(c2), "%-*s", W2, r->hw_port);
            snprintf(c3, sizeof(c3), "%-*s", W3, r->ip);

            if (colors) {
                printf("%s%s%s %s%s%s\n",
                       a ? WHITE   : DIM_WHITE, c2, RESET,
                       a ? GREEN   : DIM_GREEN, c3, RESET);
            } else {
                printf("%s %s\n", c2, c3);
            }
        }
    }
}

// ─── ncurses mode ─────────────────────────────────────────────────────────────

static void nc_hline(int y, chtype left, chtype mid, chtype right, int long_mode) {
    move(y, 0);
    if (long_mode) {
        addch(left);
        for (int i = 0; i < W1+2; i++) addch(ACS_HLINE); addch(mid);
        for (int i = 0; i < W2+2; i++) addch(ACS_HLINE); addch(mid);
        for (int i = 0; i < W3+2; i++) addch(ACS_HLINE); addch(mid);
        for (int i = 0; i < W4+2; i++) addch(ACS_HLINE); addch(mid);
        for (int i = 0; i < W7+2; i++) addch(ACS_HLINE); addch(mid);
        for (int i = 0; i < W5+2; i++) addch(ACS_HLINE); addch(mid);
        for (int i = 0; i < W6+2; i++) addch(ACS_HLINE);
        addch(right);
    } else {
        addch(left);
        for (int i = 0; i < W2+2; i++) addch(ACS_HLINE); addch(mid);
        for (int i = 0; i < W3+2; i++) addch(ACS_HLINE);
        addch(right);
    }
}

static void nc_header(int y, int long_mode) {
    move(y, 0);
    if (long_mode) {
        attrset(A_NORMAL); addch(ACS_VLINE);
        attrset(COLOR_PAIR(CP_HEADER) | A_BOLD); printw(" %-*s ", W1, "Interface");
        attrset(A_NORMAL); addch(ACS_VLINE);
        attrset(COLOR_PAIR(CP_HEADER) | A_BOLD); printw(" %-*s ", W2, "Hardware Port");
        attrset(A_NORMAL); addch(ACS_VLINE);
        attrset(COLOR_PAIR(CP_HEADER) | A_BOLD); printw(" %-*s ", W3, "IPv4 Address");
        attrset(A_NORMAL); addch(ACS_VLINE);
        attrset(COLOR_PAIR(CP_HEADER) | A_BOLD); printw(" %-*s ", W4, "MAC Address");
        attrset(A_NORMAL); addch(ACS_VLINE);
        attrset(COLOR_PAIR(CP_HEADER) | A_BOLD); printw(" %-*s ", W7, "Max BW");
        attrset(A_NORMAL); addch(ACS_VLINE);
        attrset(COLOR_PAIR(CP_HEADER) | A_BOLD); printw(" %-*s ", W5, "Status");
        attrset(A_NORMAL); addch(ACS_VLINE);
        attrset(COLOR_PAIR(CP_HEADER) | A_BOLD); printw(" %-*s ", W6, "Serial Number");
        attrset(A_NORMAL); addch(ACS_VLINE);
    } else {
        attrset(A_NORMAL); addch(ACS_VLINE);
        attrset(COLOR_PAIR(CP_HEADER) | A_BOLD); printw(" %-*s ", W2, "Hardware Port");
        attrset(A_NORMAL); addch(ACS_VLINE);
        attrset(COLOR_PAIR(CP_HEADER) | A_BOLD); printw(" %-*s ", W3, "IPv4 Address");
        attrset(A_NORMAL); addch(ACS_VLINE);
    }
}

static void nc_row(int y, const Row *row, int long_mode) {
    int a = row->active;
    chtype mod = a ? A_BOLD : A_DIM;

    move(y, 0);
    if (long_mode) {
        attrset(A_NORMAL);                                  addch(ACS_VLINE);
        attrset(COLOR_PAIR(CP_IFACE)  | mod);               printw(" %-*s ", W1, row->ifname);
        attrset(A_NORMAL);                                  addch(ACS_VLINE);
        attrset(COLOR_PAIR(CP_PORT)   | mod);               printw(" %-*s ", W2, row->hw_port);
        attrset(A_NORMAL);                                  addch(ACS_VLINE);
        attrset(COLOR_PAIR(CP_IP)     | (a ? 0 : A_DIM));  printw(" %-*s ", W3, row->ip);
        attrset(A_NORMAL);                                  addch(ACS_VLINE);
        attrset(COLOR_PAIR(CP_MAC)    | (a ? 0 : A_DIM));  printw(" %-*s ", W4, row->mac);
        attrset(A_NORMAL);                                  addch(ACS_VLINE);
        attrset(COLOR_PAIR(CP_BW)     | (a ? A_BOLD : A_DIM)); printw(" %-*s ", W7, row->bandwidth);
        attrset(A_NORMAL);                                  addch(ACS_VLINE);
        attrset(COLOR_PAIR(CP_STATUS) | (a ? 0 : A_DIM));  printw(" %-*s ", W5, a ? "Active" : "Inactive");
        attrset(A_NORMAL);                                  addch(ACS_VLINE);
        attrset(COLOR_PAIR(CP_SERIAL) | (a ? 0 : A_DIM));  printw(" %-*s ", W6, row->serial);
        attrset(A_NORMAL);                                  addch(ACS_VLINE);
    } else {
        attrset(A_NORMAL);                                  addch(ACS_VLINE);
        attrset(COLOR_PAIR(CP_PORT)   | mod);               printw(" %-*s ", W2, row->hw_port);
        attrset(A_NORMAL);                                  addch(ACS_VLINE);
        attrset(COLOR_PAIR(CP_IP)     | (a ? 0 : A_DIM));  printw(" %-*s ", W3, row->ip);
        attrset(A_NORMAL);                                  addch(ACS_VLINE);
    }
}

static void print_ncurses(const Row *rows, int count, int long_mode) {
    setlocale(LC_ALL, "");
    initscr();

    if (!has_colors()) {
        endwin();
        fprintf(stderr, "Terminal has no color support; falling back to text output.\n");
        print_text(rows, count, 0, long_mode);
        return;
    }

    start_color();
    use_default_colors();
    init_pair(CP_HEADER, COLOR_CYAN,    -1);
    init_pair(CP_IFACE,  COLOR_BLUE,    -1);
    init_pair(CP_PORT,   COLOR_WHITE,   -1);
    init_pair(CP_IP,     COLOR_GREEN,   -1);
    init_pair(CP_MAC,    COLOR_YELLOW,  -1);
    init_pair(CP_STATUS, COLOR_MAGENTA, -1);
    init_pair(CP_SERIAL, COLOR_CYAN,    -1);
    init_pair(CP_BW,     COLOR_WHITE,   -1);

    noecho();
    cbreak();
    curs_set(0);

    int split = count;
    for (int i = 0; i < count; i++)
        if (!rows[i].active) { split = i; break; }

    int y = 0;
    nc_hline(y++, ACS_ULCORNER, ACS_TTEE,  ACS_URCORNER, long_mode);
    nc_header(y++, long_mode);
    nc_hline(y++, ACS_LTEE,     ACS_PLUS,  ACS_RTEE,     long_mode);

    for (int i = 0; i < split; i++)
        nc_row(y++, &rows[i], long_mode);

    if (split < count) {
        nc_hline(y++, ACS_LTEE, ACS_PLUS, ACS_RTEE, long_mode);
        for (int i = split; i < count; i++)
            nc_row(y++, &rows[i], long_mode);
    }

    nc_hline(y++, ACS_LLCORNER, ACS_BTEE, ACS_LRCORNER, long_mode);

    attrset(A_DIM);
    mvprintw(y + 1, 0, "Press any key to exit");
    attrset(A_NORMAL);

    refresh();
    getch();
    endwin();
}

// ─── Entry point ──────────────────────────────────────────────────────────────

int main(int argc, char *argv[]) {
    int show_all  = 0;
    int use_table = 0;
    int long_mode = 0;

    static const struct option long_opts[] = {
        { "help",    no_argument, NULL, 'h' },
        { "version", no_argument, NULL, 'v' },
        { "all",     no_argument, NULL, 'a' },
        { "long",    no_argument, NULL, 'l' },
        { "table",   no_argument, NULL, 't' },
        { NULL, 0, NULL, 0 }
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "hvalt", long_opts, NULL)) != -1) {
        switch (opt) {
        case 'h': print_help(argv[0]); return 0;
        case 'v': print_version();     return 0;
        case 'a': show_all  = 1; break;
        case 'l': long_mode = 1; break;
        case 't': use_table = 1; break;
        default:
            fprintf(stderr, "Try '%s --help' for usage.\n", argv[0]);
            return 1;
        }
    }

    Row rows[MAX_ROWS];
    int count = collect_rows(rows, MAX_ROWS, show_all, long_mode);
    if (count < 0) { perror("getifaddrs"); return 1; }

    if (use_table)
        print_ncurses(rows, count, long_mode);
    else
        print_text(rows, count, use_colors(), long_mode);

    return 0;
}
