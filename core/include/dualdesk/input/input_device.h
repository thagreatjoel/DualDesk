#pragma once
#include "../core/types.h"
#include <windows.h>
#include <string>

namespace dualdesk {

enum class InputDeviceType {
    Unknown,
    Keyboard,
    Mouse,
    Touchpad,
    TouchScreen
};

enum class InputDeviceState {
    Disconnected,
    Connected,
    Active,
    Suspended
};

struct InputDevice {
    DeviceId id = 0;
    InputDeviceType type = InputDeviceType::Unknown;
    InputDeviceState state = InputDeviceState::Disconnected;
    std::wstring deviceName;
    std::wstring deviceId;
    HANDLE deviceHandle = nullptr;
    bool isVirtual = false;
    
    // For keyboard
    bool hasLEDSupport = false;
    
    // For mouse
    int buttonCount = 0;
    bool hasWheel = false;
    
    std::string ToString() const;
};

} // namespace dualdesk