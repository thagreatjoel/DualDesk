#pragma once

#include <string>
#include <fstream>
#include <iostream>
#include <chrono>
#include <iomanip>
#include <mutex>
#include <ctime>

namespace dualdesk {

class Logger {
public:
    static void SetLogFile(const std::string& path) {
        std::lock_guard<std::mutex> lock(getMutex());
        getLogFilePath() = path;
    }

    static void Info(const std::string& msg) {
        Log("INFO", msg);
    }

    static void Debug(const std::string& msg) {
        Log("DEBUG", msg);
    }

    static void Error(const std::string& msg) {
        Log("ERROR", msg);
    }

    static void Warn(const std::string& msg) {
        Log("WARN", msg);
    }

private:
    static void Log(const std::string& level, const std::string& msg) {
        std::lock_guard<std::mutex> lock(getMutex());
        
        auto now = std::chrono::system_clock::now();
        std::time_t time_t = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;
        
        std::string output = std::string("[") + level + "] ";
        output += std::to_string(time_t) + "." + std::to_string(ms.count()) + " ";
        output += msg + "\n";
        
        // Console
        std::cout << output;
        
        // File
        if (!getLogFilePath().empty()) {
            std::ofstream file(getLogFilePath(), std::ios::app);
            if (file.is_open()) {
                file << output;
            }
        }
    }

    static std::mutex& getMutex() {
        static std::mutex mutex;
        return mutex;
    }

    static std::string& getLogFilePath() {
        static std::string path = "dualddesk.log";
        return path;
    }
};

// Convenience macros - NO circular includes!
#define LOG_INFO(msg)   ::dualdesk::Logger::Info(msg)
#define LOG_DEBUG(msg)  ::dualdesk::Logger::Debug(msg)
#define LOG_ERROR(msg)  ::dualdesk::Logger::Error(msg)
#define LOG_WARN(msg)   ::dualdesk::Logger::Warn(msg)

} // namespace dualdesk