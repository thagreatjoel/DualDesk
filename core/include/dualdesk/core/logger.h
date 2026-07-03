#pragma once

#include <string>
#include <fstream>
#include <iostream>
#include <chrono>
#include <ctime>
#include <mutex>

namespace dualdesk {

enum class LogLevel {
    Trace, Debug, Info, Warn, Error, Critical
};

class Logger {
public:
    static void SetLogFile(const std::string& path) {
        std::lock_guard<std::mutex> lock(getMutex());
        getLogFilePath() = path;
    }

    static void Log(LogLevel level, const std::string& msg) {
        std::lock_guard<std::mutex> lock(getMutex());
        
        std::string levelStr;
        switch (level) {
            case LogLevel::Info: levelStr = "INFO"; break;
            case LogLevel::Debug: levelStr = "DEBUG"; break;
            case LogLevel::Warn: levelStr = "WARN"; break;
            case LogLevel::Error: levelStr = "ERROR"; break;
            case LogLevel::Critical: levelStr = "CRITICAL"; break;
            default: levelStr = "UNKNOWN"; break;
        }
        
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::string timeStr = std::ctime(&time_t);
        timeStr.pop_back();
        
        std::string output = "[" + levelStr + "] " + timeStr + " " + msg + "\n";
        
        std::cout << output;
        
        if (!getLogFilePath().empty()) {
            std::ofstream file(getLogFilePath(), std::ios::app);
            if (file.is_open()) {
                file << output;
            }
        }
    }

private:
    static std::mutex& getMutex() {
        static std::mutex mutex;
        return mutex;
    }

    static std::string& getLogFilePath() {
        static std::string path = "dualddesk.log";
        return path;
    }
};

} // namespace dualdesk

// SIMPLE MACROS - NO FORMATTING
#define LOG_INFO(msg)   ::dualdesk::Logger::Log(::dualdesk::LogLevel::Info, msg)
#define LOG_DEBUG(msg)  ::dualdesk::Logger::Log(::dualdesk::LogLevel::Debug, msg)
#define LOG_WARN(msg)   ::dualdesk::Logger::Log(::dualdesk::LogLevel::Warn, msg)
#define LOG_ERROR(msg)  ::dualdesk::Logger::Log(::dualdesk::LogLevel::Error, msg)