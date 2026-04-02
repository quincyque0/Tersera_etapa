#ifndef NETWORK_UTILS_H
#define NETWORK_UTILS_H

#include <string>

class NetworkUtils {
public:
    static std::string getIPAddress();
    static std::string getMacAddress();
};

#endif