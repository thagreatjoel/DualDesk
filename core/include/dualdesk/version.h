#pragma once

#include <string>

namespace dualdesk {

/**
 * @brief Version information for the application
 */
struct Version {
    static constexpr int Major = 0;
    static constexpr int Minor = 1;
    static constexpr int Patch = 0;
    static constexpr const char* Build = "dev";

    /**
     * @brief Returns the version string
     */
    static std::string GetString() {
        return std::format("{}.{}.{}-{}", Major, Minor, Patch, Build);
    }
};

} // namespace dualdesk