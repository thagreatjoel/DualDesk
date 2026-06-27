#pragma once

#include "logger.h"
#include <exception>
#include <string>

namespace dualdesk {

class DualDeskException : public std::exception {
public:
    explicit DualDeskException(const std::string& message)
        : message_(message) {}

    const char* what() const noexcept override {
        return message_.c_str();
    }

private:
    std::string message_;
};

class ErrorHandler {
public:
    static void HandleException(const std::exception& e) {
        LOG_ERROR("Exception caught: {}", e.what());
    }

    static void ThrowIf(bool condition, const std::string& message) {
        if (condition) {
            throw DualDeskException(message);
        }
    }
};

} // namespace dualdesk