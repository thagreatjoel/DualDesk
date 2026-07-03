#include "dualdesk/core/logger.h"
#include <cstdio>
#include <ctime>
#include <windows.h>

namespace dualdesk {

Logger& Logger::GetInstance() {
    static Logger instance;
    return instance;
}

Logger::Logger() : m_logLevel(LogLevel::Info) {}

Logger::~Logger() {}

void Logger::SetLogFile(const std::string& filename) {
    m_logFile = filename;
}

void Logger::SetLogLevel(LogLevel level) {
    m_logLevel = level;
}

// ===== VARIADIC LOG IMPLEMENTATION =====
void Logger::Log(LogLevel level, const char* format, ...) {
    if (level < m_logLevel) return;
    
    char buffer[2048];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    // Add timestamp
    SYSTEMTIME st;
    GetLocalTime(&st);
    
    char timestamp[64];
    snprintf(timestamp, sizeof(timestamp), "[%02d:%02d:%02d.%03d] ", 
        st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
    
    // Add level
    const char* levelStr = "";
    switch (level) {
        case LogLevel::Trace: levelStr = "TRACE"; break;
        case LogLevel::Debug: levelStr = "DEBUG"; break;
        case LogLevel::Info:  levelStr = "INFO";  break;
        case LogLevel::Warn:  levelStr = "WARN";  break;
        case LogLevel::Error: levelStr = "ERROR"; break;
        case LogLevel::Fatal: levelStr = "FATAL"; break;
    }
    
    std::string output = timestamp;
    output += "[";
    output += levelStr;
    output += "] ";
    output += buffer;
    output += "\n";
    
    OutputDebugStringA(output.c_str());
}

// ===== STRING-ONLY LOG =====
void Logger::Log(LogLevel level, const std::string& message) {
    if (level < m_logLevel) return;
    
    SYSTEMTIME st;
    GetLocalTime(&st);
    
    char timestamp[64];
    snprintf(timestamp, sizeof(timestamp), "[%02d:%02d:%02d.%03d] ", 
        st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
    
    const char* levelStr = "";
    switch (level) {
        case LogLevel::Trace: levelStr = "TRACE"; break;
        case LogLevel::Debug: levelStr = "DEBUG"; break;
        case LogLevel::Info:  levelStr = "INFO";  break;
        case LogLevel::Warn:  levelStr = "WARN";  break;
        case LogLevel::Error: levelStr = "ERROR"; break;
        case LogLevel::Fatal: levelStr = "FATAL"; break;
    }
    
    std::string output = timestamp;
    output += "[";
    output += levelStr;
    output += "] ";
    output += message;
    output += "\n";
    
    OutputDebugStringA(output.c_str());
}

} // namespace dualdesk