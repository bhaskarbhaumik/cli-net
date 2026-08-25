#define _DARWIN_C_SOURCE // Required to expose macOS-specific ioctls like SIOCGIFLLADDR
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ifaddrs.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <mach/mach.h>
#include <IOKit/IOKitLib.h>
#include <CoreFoundation/CoreFoundation.h>

// ANSI terminal color codes (disabled automatically if not a TTY)
#define RESET     "\033[0m"
#define BOLD_CYAN "\033[1;36m"
#define BOLD_BLUE "\033[1;34m"
#define WHITE     "\033[1;37m"
#define GREEN     "\033[32m"
#define YELLOW    "\033[33m"
#define MAGENTA   "\033[35m"

// Detect terminal to avoid breaking pipe output
static int use_colors(void) {
    return isatty(STDOUT_FILENO) ? 1 : 0;
}

// Traverse IOKit registry up the device tree to find the user-facing hardware product name
void get_hardware_port_name(const char *ifname, char *buf, size_t len) {
    io_service_t service = IOServiceGetMatchingService(kIOMainPortDefault, IOServiceNameMatching(ifname));
    if (service == MACH_PORT_NULL) {
        snprintf(buf, len, "%s", ifname);
        return;
    }

    io_service_t parent = 0;
    // ✅ Fixed: Added missing 3rd argument (pointer to parent)
    IORegistryEntryGetParentEntry(service, kIOServicePlane, &parent);
    int found = 0;

    while (parent != MACH_PORT_NULL) {
        CFTypeRef prop = IORegistryEntryCreateCFProperty(parent, CFSTR("product-name"), kCFAllocatorDefault, 0);
        if (prop && CFGetTypeID(prop) == CFStringGetTypeID()) {
            CFStringGetCString(prop, buf, len, kCFStringEncodingASCII);
            CFRelease(prop);
            found = 1;
            break;
        }
        if (prop) CFRelease(prop);

        io_service_t next_parent = 0;
        // ✅ Fixed: Added missing 3rd argument
        IORegistryEntryGetParentEntry(parent, kIOServicePlane, &next_parent);
        IOObjectRelease(parent);
        parent = next_parent;
    }

    if (!found) {
        snprintf(buf, len, "%s", ifname);
    }

    IOObjectRelease(service);
}

// Extract MAC address via SIOCGIFLLADDR ioctl
int get_mac_address(const char *ifname, char *mac_str, size_t len) {
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        snprintf(mac_str, len, "N/A");
        return -1;
    }

    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strlcpy(ifr.ifr_name, ifname, IFNAMSIZ);
    struct sockaddr_storage ss;
    memset(&ss, 0, sizeof(ss));

    // ✅ Fixed: ifr.ifr_addr is a struct, not a pointer. Dereference to match type.
    ifr.ifr_addr = *(const struct sockaddr *)&ss;

    if (ioctl(fd, SIOCGIFLLADDR, &ifr) == 0) {
        struct sockaddr_dl *sdl = (struct sockaddr_dl *)&ss;
        if (sdl->sdl_family == AF_LINK && sdl->sdl_alen > 0) {
            char *p = mac_str;
            size_t remaining = len;
            // ✅ Fixed: Cast to match pointer type, eliminating sign warning
            const unsigned char *addr = (const unsigned char *)LLADDR(sdl);
            for (int i = 0; i < sdl->sdl_alen; i++) {
                int written = snprintf(p, remaining, "%s%02X", i > 0 ? ":" : "", addr[i]);
                if (written < 0 || (size_t)written >= remaining) break;
                p += written;
                remaining -= written;
            }
        } else {
            snprintf(mac_str, len, "N/A");
        }
    } else {
        snprintf(mac_str, len, "N/A");
    }
    close(fd);
    return 0;
}

int main(void) {
    struct ifaddrs *ifaddr, *ifa;
    char hw_port[128] = {0};
    char mac[32] = {0};
    int colors = use_colors();

    if (getifaddrs(&ifaddr) == -1) {
        perror("getifaddrs");
        return 1;
    }

    if (colors) {
        // ✅ Fixed: Format string now exactly matches the 15 arguments passed
        printf("%s%-14s%s %s%-28s%s %s%-18s%s %s%-18s%s %s%-8s%s\n",
               BOLD_BLUE, ifa->ifa_name, RESET,
               WHITE, hw_port, RESET,
               GREEN, ip_str, RESET,
               YELLOW, mac, RESET,
               MAGENTA, "Active", RESET);
        printf("%s%-14s%s %s%-28s%s %s%-18s%s %s%-18s%s %s%-8s%s\n",
               BOLD_CYAN, "Interface", RESET,
               BOLD_CYAN, "Hardware Port", RESET,
               BOLD_CYAN, "IPv4 Address", RESET,
               BOLD_CYAN, "MAC Address", RESET,
               BOLD_CYAN, "Status", RESET);
    } else {
        printf("%-14s %-28s %-18s %-18s %-8s\n", "Interface", "Hardware Port", "IPv4 Address", "MAC Address", "Status");
        printf("%-14s %-28s %-18s %-18s %-8s\n", "---------", "-------------", "----------", "---------", "------");
    }

    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL) continue;
        if (ifa->ifa_addr->sa_family != AF_INET) continue;
        if (!(ifa->ifa_flags & IFF_UP)) continue;

        char ip_str[INET_ADDRSTRLEN];
        struct sockaddr_in *sin = (struct sockaddr_in *)ifa->ifa_addr;
        inet_ntop(AF_INET, &sin->sin_addr, ip_str, sizeof(ip_str));

        get_hardware_port_name(ifa->ifa_name, hw_port, sizeof(hw_port));
        get_mac_address(ifa->ifa_name, mac, sizeof(mac));

        if (colors) {
            printf("%s%-14s%s %s%-28s%s %s%-18s%s %s%-18s%s %s%-8s%s\n",
                   BOLD_BLUE, ifa->ifa_name, RESET,
                   WHITE, hw_port, RESET,
                   GREEN, ip_str, RESET,
                   YELLOW, mac, RESET,
                   MAGENTA, "Active", RESET);
        } else {
            printf("%-14s %-28s %-18s %-18s %-8s\n",
                   ifa->ifa_name, hw_port, ip_str, mac, "Active");
        }
    }

    freeifaddrs(ifaddr);
    return 0;
}
