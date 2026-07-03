#pragma once

#include <string>
#include <cstdarg>
#include <windows.h>

namespace dualdesk {

enum class LogLevel {
    Trace = 0,
    Debug = 1,
    Info = 2,
    Warn = 3,
    Error = 4,
    Fatal = 5
};

class Logger {
public:
    static Logger& GetInstance();

    void SetLogFile(const std::string& filename);
    void SetLogLevel(LogLevel level);

    // ===== VARIADIC LOG FUNCTION =====
    void Log(LogLevel level, const char* format, ...);

    // ===== STRING-ONLY LOG FUNCTION =====
    void Log(LogLevel level, const std::string& message);

private:
    Logger();
    ~Logger();

    std::string m_logFile;
    LogLevel m_logLevel;
};

} // namespace dualdesk

// ============================================================
// LOG MACROS - Support variadic arguments
// ============================================================
#define LOG_INFO(...)    dualdesk::Logger::GetInstance().Log(dualdesk::LogLevel::Info, __VA_ARGS__)
#define LOG_WARN(...)    dualdesk::Logger::GetInstance().Log(dualdesk::LogLevel::Warn, __VA_ARGS__)
#define LOG_WARNING(...) dualdesk::Logger::GetInstance().Log(dualdesk::LogLevel::Warn, __VA_ARGS__)
#define LOG_ERROR(...)   dualdesk::Logger::GetInstance().Log(dualdesk::LogLevel::Error, __VA_ARGS__)
#define LOG_DEBUG(...)   dualdesk::Logger::GetInstance().Log(dualdesk::LogLevel::Debug, __VA_ARGS__)
#define LOG_TRACE(...)   dualdesk::Logger::GetInstance().Log(dualdesk::LogLevel::Trace, __VA_ARGS__)