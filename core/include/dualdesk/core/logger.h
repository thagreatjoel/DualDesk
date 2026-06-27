#pragma once

#include <string>
#include <iostream>
#include <windows.h>
#include <sstream>

namespace dualdesk {

enum class LogLevel {
    Trace,
    Debug, 
    Info,
    Warn,
    Error,
    Critical
};

class Logger {
public:
    static void Log(LogLevel level, const std::string& message) {
        std::string prefix;
        switch(level) {
            case LogLevel::Trace: prefix = "[TRACE]"; break;
            case LogLevel::Debug: prefix = "[DEBUG]"; break;
            case LogLevel::Info:  prefix = "[INFO]"; break;
            case LogLevel::Warn:  prefix = "[WARN]"; break;
            case LogLevel::Error: prefix = "[ERROR]"; break;
            case LogLevel::Critical: prefix = "[CRITICAL]"; break;
        }
        
        std::string fullMessage = prefix + " " + message + "\n";
        std::cout << fullMessage;
        OutputDebugStringA(fullMessage.c_str());
    }
    
    // Template version for formatted messages
    template<typename... Args>
    static void LogFormatted(LogLevel level, const std::string& format, Args&&... args) {
        // Simple string concatenation for now
        std::string message = format;
        // We'll expand this later
        Log(level, message);
    }
    
    static void Trace(const std::string& msg) { Log(LogLevel::Trace, msg); }
    static void Debug(const std::string& msg) { Log(LogLevel::Debug, msg); }
    static void Info(const std::string& msg) { Log(LogLevel::Info, msg); }
    static void Warn(const std::string& msg) { Log(LogLevel::Warn, msg); }
    static void Error(const std::string& msg) { Log(LogLevel::Error, msg); }
    static void Critical(const std::string& msg) { Log(LogLevel::Critical, msg); }
};

#define LOG_TRACE(msg) ::dualdesk::Logger::Trace(msg)
#define LOG_DEBUG(msg) ::dualdesk::Logger::Debug(msg)
#define LOG_INFO(msg)  ::dualdesk::Logger::Info(msg)
#define LOG_WARN(msg)  ::dualdesk::Logger::Warn(msg)
#define LOG_ERROR(msg) ::dualdesk::Logger::Error(msg)
#define LOG_CRITICAL(msg) ::dualdesk::Logger::Critical(msg)

} // namespace dualdesk