#pragma once

#include <cstdint>
#include <string>

namespace dualdesk {

using DeviceId = uint64_t;
using WorkspaceId = uint64_t;
using WindowHandle = void*;

constexpr uint32_t INVALID_DEVICE_ID = 0;
constexpr WorkspaceId INVALID_WORKSPACE_ID = 0;

} // namespace dualdesk