#include "NetworkUtils.h"
#include <ifaddrs.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <net/if_dl.h>
#include <cstring>

std::string NetworkUtils::getIPAddress() {
    struct ifaddrs *ifaddr, *ifa;
    std::string ipAddress;
    getifaddrs(&ifaddr);
    for (ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == nullptr) continue;
        if (strcmp(ifa->ifa_name, "en0") == 0 && 
            ifa->ifa_addr->sa_family == AF_INET) {
            
            struct sockaddr_in *addr = (struct sockaddr_in *)ifa->ifa_addr;
            char buffer[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(addr->sin_addr), buffer, INET_ADDRSTRLEN);
            ipAddress = std::string(buffer);
            break;
        }
    }
    
    freeifaddrs(ifaddr);
    return ipAddress;
}

std::string NetworkUtils::getMacAddress() {
    struct ifaddrs *ifaddr, *ifa;
    std::string macAddress;
    
    getifaddrs(&ifaddr);
    
    for (ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == nullptr) continue;
        
        if (strcmp(ifa->ifa_name, "en0") == 0 && 
            ifa->ifa_addr->sa_family == AF_LINK) {
            
            struct sockaddr_dl* sdl = (struct sockaddr_dl*)ifa->ifa_addr;
            unsigned char* mac = (unsigned char*)LLADDR(sdl);
            
            char macBuffer[18];
            snprintf(macBuffer, sizeof(macBuffer), "%02X:%02X:%02X:%02X:%02X:%02X",
                     mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
            macAddress = std::string(macBuffer);
            break;
        }
    }
    
    freeifaddrs(ifaddr);
    return macAddress;
}