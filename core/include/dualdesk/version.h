#pragma once

#include <string>
#include <sstream>

namespace dualdesk {

#define DUALDESK_VERSION_MAJOR 1
#define DUALDESK_VERSION_MINOR 1
#define DUALDESK_VERSION_PATCH 0

inline std::string GetVersionString() {
    std::stringstream ss;
    ss << DUALDESK_VERSION_MAJOR << "."
       << DUALDESK_VERSION_MINOR << "."
       << DUALDESK_VERSION_PATCH;
    return ss.str();
}

inline int GetVersionMajor() { return DUALDESK_VERSION_MAJOR; }
inline int GetVersionMinor() { return DUALDESK_VERSION_MINOR; }
inline int GetVersionPatch() { return DUALDESK_VERSION_PATCH; }

} // namespace dualdesk