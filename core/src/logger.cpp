#pragma once

#include <string>
#include <source_location>
#include <memory>

namespace dualdesk {

enum class LogLevel {
    Trace,
    Debug,
    Info,
    Warn,
    Error,
    Critical,
    Off
};

class ILogger {
public:
    virtual ~ILogger() = default;
    
    virtual void Log(LogLevel level, 
                     std::string_view message,
                     const std::source_location& location = std::source_location::current()) = 0;
    
    virtual void SetLevel(LogLevel level) = 0;
    virtual LogLevel GetLevel() const = 0;
};

// Convenience macros - simple version without formatting
#define LOG_INFO(msg)   ::dualdesk::GetLogger().Log(::dualdesk::LogLevel::Info, msg)
#define LOG_ERROR(msg)  ::dualdesk::GetLogger().Log(::dualdesk::LogLevel::Error, msg)
#define LOG_WARN(msg)   ::dualdesk::GetLogger().Log(::dualdesk::LogLevel::Warn, msg)
#define LOG_DEBUG(msg)  ::dualdesk::GetLogger().Log(::dualdesk::LogLevel::Debug, msg)

ILogger& GetLogger();

} // namespace dualdesk