#pragma once
#include "input_device.h"
#include <windows.h>
#include <string>

namespace dualdesk {

enum class InputEventType {
    Unknown,
    KeyDown,
    KeyUp,
    MouseMove,
    MouseButtonDown,
    MouseButtonUp,
    MouseWheel
};

struct InputEvent {
    InputEventType type = InputEventType::Unknown;
    DeviceId deviceId = 0;
    uint64_t timestamp = 0;
    
    // ============================================================
    // ADD THESE FIELDS - For device routing
    // ============================================================
    HANDLE deviceHandle = NULL;     // ← ADD THIS
    int deltaX = 0;                 // ← ADD THIS
    int deltaY = 0;                 // ← ADD THIS
    
    // Keyboard data
    uint16_t keyCode = 0;
    uint16_t scanCode = 0;
    bool isExtended = false;
    uint8_t keyState = 0;
    
    // Mouse data
    POINT mousePosition = {0, 0};
    POINT mouseDelta = {0, 0};
    uint16_t mouseButtons = 0;
    int wheelDelta = 0;
    
    std::string ToString() const;
};

} // namespace dualdesk