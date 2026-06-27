#pragma once
#include "input_device.h"
#include <windows.h>

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