#pragma once

#include <cstdint>
#include <string>
#include <windows.h>  // ← ADD THIS - defines ULONG, HANDLE, etc.

namespace dualdesk {

using DeviceId = uint64_t;
using WorkspaceId = uint64_t;
using WindowHandle = void*;

constexpr uint32_t INVALID_DEVICE_ID = 0;
constexpr WorkspaceId INVALID_WORKSPACE_ID = 0;

// ============================================================
// WORKSPACE_UNASSIGNED - Must be ULONG, not a custom type
// ============================================================
constexpr ULONG WORKSPACE_UNASSIGNED = 0xFFFFFFFF;

} // namespace dualdesk