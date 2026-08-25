#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ifaddrs.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <mach/mach.h>
#include <IOKit/IOKitLib.h>
#include <CoreFoundation/CoreFoundation.h>

const char* get_interface_type(const char *ifname) {
    // ✅ Fixed: Replaced deprecated kIOMasterPortDefault with kIOMainPortDefault
    io_service_t service = IOServiceGetMatchingService(kIOMainPortDefault, IOServiceNameMatching(ifname));
    if (service == MACH_PORT_NULL) {
        return "Unknown";
    }

    CFStringRef cfClass = IORegistryEntryCreateCFProperty((io_registry_entry_t)service, CFSTR("IOClass"), kCFAllocatorDefault, 0);

    char className[64] = {0};
    if (cfClass) {
        CFStringGetCString(cfClass, className, sizeof(className), kCFStringEncodingASCII);
        CFRelease(cfClass);
    }
    IOObjectRelease(service);

    if (strcmp(className, "IO80211Interface") == 0) return "Wi-Fi";
    if (strcmp(className, "IOEthernetInterface") == 0) return "Ethernet";
    if (strcmp(className, "UTPInterface") == 0) return "Thunderbolt/USB Ethernet";
    if (strcmp(className, "ppp") == 0) return "PPP";
    if (strcmp(className, "lo") == 0) return "Loopback";
    if (strcmp(className, "bridge") == 0) return "Bridge";
    if (strcmp(className, "utun") == 0) return "VPN/Tunnel";
    if (strcmp(className, "gif") == 0) return "Tunnel";
    if (strcmp(className, "stf") == 0) return "6to4";
    if (strcmp(className, "awdl") == 0) return "AWDL";
    if (strcmp(className, "llw") == 0) return "LLW";
    if (strcmp(className, "ipsec") == 0) return "IPsec";
    if (strcmp(className, "p2p") == 0) return "P2P";

    if (strncmp(className, "en", 2) == 0) return "Ethernet";
    if (strncmp(className, "wl", 2) == 0) return "Wi-Fi";
    if (strncmp(className, "awdl", 4) == 0) return "AWDL";
    if (strncmp(className, "llw", 3) == 0) return "LLW";
    if (strncmp(className, "utun", 4) == 0) return "VPN/Tunnel";
    if (strncmp(className, "gif", 3) == 0) return "Tunnel";
    if (strncmp(className, "stf", 3) == 0) return "6to4";
    if (strncmp(className, "lo", 2) == 0) return "Loopback";
    if (strncmp(className, "bridge", 6) == 0) return "Bridge";
    if (strncmp(className, "ppp", 3) == 0) return "PPP";

    return "Unknown";
}

int main(void) {
    struct ifaddrs *ifaddr, *ifa;

    if (getifaddrs(&ifaddr) == -1) {
        perror("getifaddrs");
        return 1;
    }

    printf("%-15s %-25s %-18s %-10s\n", "Interface", "Type", "IPv4 Address", "Status");
    printf("%-15s %-25s %-18s %-10s\n", "---------", "----", "----------", "------");

    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL) continue;

        if (ifa->ifa_addr->sa_family == AF_INET) {
            if ((ifa->ifa_flags & IFF_UP)) {
                char ip_str[INET_ADDRSTRLEN];
                struct sockaddr_in *sock = (struct sockaddr_in *)ifa->ifa_addr;
                inet_ntop(AF_INET, &sock->sin_addr, ip_str, sizeof(ip_str));

                const char *type = get_interface_type(ifa->ifa_name);
                printf("%-15s %-25s %-18s %-10s\n",
                       ifa->ifa_name,
                       type,
                       ip_str,
                       "Active");
            }
        }
    }

    freeifaddrs(ifaddr);
    return 0;
}

