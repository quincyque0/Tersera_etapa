#include "Logger.h"
#include <iostream>
#include <chrono>
#include <iomanip>
#include <ctime>

void Logger::log(const std::string& level, const std::string& message) {
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::tm now_tm = *std::localtime(&now_time_t);
    
    std::cout << "[" << std::put_time(&now_tm, "%Y-%m-%d %H:%M:%S") 
              << "." << std::setfill('0') << std::setw(3) << now_ms.count() 
              << "] [" << level << "] " << message << std::endl;
}

void Logger::logMessage(const std::string& level, const std::string& message) {
    log(level, message);
}