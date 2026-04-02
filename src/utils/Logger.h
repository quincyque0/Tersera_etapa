#ifndef LOGGER_H
#define LOGGER_H

#include <string>

class Logger {
public:
    static void log(const std::string& level, const std::string& message);
    static void logMessage(const std::string& level, const std::string& message);
};

#endif