#include "dualdesk/input/input_manager.h"
#include "dualdesk/core/logger.h"
#include <windows.h>
#include <hidsdi.h>
#include <setupapi.h>
#include <devguid.h>
#include <string>

#pragma comment(lib, "hid.lib")
#pragma comment(lib, "setupapi.lib")

namespace dualdesk {

InputManager::InputManager() {
    LOG_DEBUG("InputManager created");
}

InputManager::~InputManager() {
    Shutdown();
}

bool InputManager::Initialize(HWND hwnd) {
    if (initialized_) {
        LOG_WARN("InputManager already initialized");
        return true;
    }
    
    targetWindow_ = hwnd;
    
    if (!RegisterForRawInput(hwnd)) {
        LOG_ERROR("Failed to register for raw input");
        return false;
    }
    
    EnumerateRawInputDevices();
    initialized_ = true;
    
    LOG_INFO("InputManager initialized with {} devices", devices_.size());
    return true;
}

void InputManager::Shutdown() {
    if (!initialized_) return;
    
    UnregisterRawInput();
    devices_.clear();
    initialized_ = false;
    LOG_INFO("InputManager shutdown");
}

bool InputManager::RegisterForRawInput(HWND hwnd) {
    // Register for keyboard input
    RAWINPUTDEVICE ridKeyboard;
    ridKeyboard.usUsagePage = 0x01;  // Generic Desktop
    ridKeyboard.usUsage = 0x06;      // Keyboard
    ridKeyboard.dwFlags = RIDEV_INPUTSINK;
    ridKeyboard.hwndTarget = hwnd;
    
    // Register for mouse input
    RAWINPUTDEVICE ridMouse;
    ridMouse.usUsagePage = 0x01;     // Generic Desktop
    ridMouse.usUsage = 0x02;         // Mouse
    ridMouse.dwFlags = RIDEV_INPUTSINK;
    ridMouse.hwndTarget = hwnd;
    
    RAWINPUTDEVICE devices[] = {ridKeyboard, ridMouse};
    
    if (!RegisterRawInputDevices(devices, 2, sizeof(RAWINPUTDEVICE))) {
        LOG_ERROR("Failed to register raw input devices. Error: {}", GetLastError());
        return false;
    }
    
    LOG_INFO("Registered for raw input");
    return true;
}

void InputManager::UnregisterRawInput() {
    RAWINPUTDEVICE rid = {0};
    rid.dwFlags = RIDEV_REMOVE;
    RegisterRawInputDevices(&rid, 1, sizeof(RAWINPUTDEVICE));
    LOG_DEBUG("Unregistered raw input");
}

void InputManager::EnumerateRawInputDevices() {
    // Get number of devices
    UINT deviceCount = 0;
    if (GetRawInputDeviceList(nullptr, &deviceCount, sizeof(RAWINPUTDEVICELIST)) != 0) {
        LOG_ERROR("Failed to get raw input device count");
        return;
    }
    
    if (deviceCount == 0) {
        LOG_INFO("No raw input devices found");
        return;
    }
    
    // Get device list
    std::vector<RAWINPUTDEVICELIST> deviceList(deviceCount);
    if (GetRawInputDeviceList(deviceList.data(), &deviceCount, sizeof(RAWINPUTDEVICELIST)) == (UINT)-1) {
        LOG_ERROR("Failed to get raw input device list");
        return;
    }
    
    LOG_INFO("Found {} raw input devices", deviceCount);
    
    // Group devices by parent
    std::unordered_map<std::wstring, std::vector<RAWINPUTDEVICELIST>> deviceGroups;
    
    for (const auto& device : deviceList) {
        InputDevice info;
        info.id = GenerateDeviceId();
        info.deviceHandle = device.hDevice;
        info.deviceName = GetDeviceName(device.hDevice);
        info.type = GetDeviceType(device.hDevice);
        info.state = InputDeviceState::Connected;
        
        // Try to get a friendly name
        std::wstring friendlyName = info.deviceName;
        
        // Extract device type from path
        if (info.deviceName.find(L"ELAN07FB") != std::wstring::npos) {
            friendlyName = L"Touchpad";
        } else if (info.deviceName.find(L"HPQ8001") != std::wstring::npos) {
            friendlyName = L"Keyboard";
        } else if (info.deviceName.find(L"VID_3151") != std::wstring::npos) {
            friendlyName = L"External Mouse";
        } else if (info.deviceName.find(L"USB") != std::wstring::npos) {
            friendlyName = L"USB Device";
        }
        
        // Add friendly name to device
        info.deviceName = friendlyName;
        
        devices_[info.id] = info;
        
        std::string name(info.deviceName.begin(), info.deviceName.end());
        std::string typeStr;
        switch(info.type) {
            case InputDeviceType::Keyboard: typeStr = "Keyboard"; break;
            case InputDeviceType::Mouse: typeStr = "Mouse"; break;
            default: typeStr = "Unknown"; break;
        }
        LOG_INFO("  - {} ({})", name, typeStr);
    }
}

std::wstring InputManager::GetDeviceName(HANDLE handle) {
    UINT size = 0;
    if (GetRawInputDeviceInfo(handle, RIDI_DEVICENAME, nullptr, &size) != 0) {
        return L"Unknown";
    }
    
    std::vector<wchar_t> name(size + 1);
    if (GetRawInputDeviceInfo(handle, RIDI_DEVICENAME, name.data(), &size) == (UINT)-1) {
        return L"Unknown";
    }
    
    return std::wstring(name.data());
}

InputDeviceType InputManager::GetDeviceType(HANDLE handle) {
    RID_DEVICE_INFO deviceInfo;
    UINT size = sizeof(deviceInfo);
    
    if (GetRawInputDeviceInfo(handle, RIDI_DEVICEINFO, &deviceInfo, &size) == (UINT)-1) {
        return InputDeviceType::Unknown;
    }
    
    switch(deviceInfo.dwType) {
        case RIM_TYPEKEYBOARD:
            return InputDeviceType::Keyboard;
        case RIM_TYPEMOUSE:
            return InputDeviceType::Mouse;
        default:
            return InputDeviceType::Unknown;
    }
}

DeviceId InputManager::GenerateDeviceId() {
    return nextDeviceId_++;
}

std::vector<InputDevice> InputManager::EnumerateDevices() {
    std::vector<InputDevice> result;
    for (const auto& pair : devices_) {
        result.push_back(pair.second);
    }
    return result;
}

std::vector<InputDevice> InputManager::GetKeyboards() const {
    std::vector<InputDevice> result;
    for (const auto& pair : devices_) {
        if (pair.second.type == InputDeviceType::Keyboard) {
            result.push_back(pair.second);
        }
    }
    return result;
}

std::vector<InputDevice> InputManager::GetMice() const {
    std::vector<InputDevice> result;
    for (const auto& pair : devices_) {
        if (pair.second.type == InputDeviceType::Mouse) {
            result.push_back(pair.second);
        }
    }
    return result;
}

InputDevice InputManager::GetDevice(DeviceId id) const {
    auto it = devices_.find(id);
    if (it != devices_.end()) {
        return it->second;
    }
    return InputDevice{};
}

std::vector<InputManager::PhysicalDevice> InputManager::GetPhysicalDevices() const {
    std::unordered_map<std::wstring, PhysicalDevice> physicalDevices;
    
    for (const auto& pair : devices_) {
        const auto& device = pair.second;
        std::wstring baseName = device.deviceName;
        
        // Remove interface suffixes (Col01, Col02, etc.)
        size_t colPos = baseName.find(L"Col");
        if (colPos != std::wstring::npos) {
            baseName = baseName.substr(0, colPos - 1);
        }
        
        // Remove trailing numbers
        while (!baseName.empty() && iswdigit(baseName.back())) {
            baseName.pop_back();
        }
        
        if (physicalDevices.find(baseName) == physicalDevices.end()) {
            PhysicalDevice phys;
            phys.name = baseName;
            phys.type = device.type;
            physicalDevices[baseName] = phys;
        }
        
        physicalDevices[baseName].interfaces.push_back(device.id);
    }
    
    std::vector<PhysicalDevice> result;
    for (auto& pair : physicalDevices) {
        result.push_back(pair.second);
    }
    
    return result;
}

void InputManager::SetInputEventCallback(InputEventCallback callback) {
    eventCallback_ = callback;
}

void InputManager::ProcessRawInput(HRAWINPUT handle) {
    UINT size = 0;
    if (GetRawInputData(handle, RID_INPUT, nullptr, &size, sizeof(RAWINPUTHEADER)) != 0) {
        return;
    }
    
    std::vector<BYTE> buffer(size);
    if (GetRawInputData(handle, RID_INPUT, buffer.data(), &size, sizeof(RAWINPUTHEADER)) != size) {
        return;
    }
    
    RAWINPUT* rawInput = reinterpret_cast<RAWINPUT*>(buffer.data());
    
    InputEvent event;
    
    switch(rawInput->header.dwType) {
        case RIM_TYPEKEYBOARD:
            event = ParseKeyboardInput(handle);
            break;
        case RIM_TYPEMOUSE:
            event = ParseMouseInput(handle);
            break;
        default:
            return;
    }
    
    if (eventCallback_) {
        eventCallback_(event);
    }
}

InputEvent InputManager::ParseKeyboardInput(HRAWINPUT handle) {
    UINT size = 0;
    GetRawInputData(handle, RID_INPUT, nullptr, &size, sizeof(RAWINPUTHEADER));
    std::vector<BYTE> buffer(size);
    GetRawInputData(handle, RID_INPUT, buffer.data(), &size, sizeof(RAWINPUTHEADER));
    
    RAWINPUT* rawInput = reinterpret_cast<RAWINPUT*>(buffer.data());
    RAWKEYBOARD keyboard = rawInput->data.keyboard;
    
    InputEvent event;
    event.type = (keyboard.Flags & RI_KEY_BREAK) ? InputEventType::KeyUp : InputEventType::KeyDown;
    event.keyCode = keyboard.VKey;
    event.scanCode = keyboard.MakeCode;
    event.isExtended = (keyboard.Flags & RI_KEY_E0) != 0;
    
    // Try to find the device
    for (const auto& pair : devices_) {
        if (pair.second.deviceHandle == rawInput->header.hDevice) {
            event.deviceId = pair.first;
            break;
        }
    }
    
    return event;
}

InputEvent InputManager::ParseMouseInput(HRAWINPUT handle) {
    UINT size = 0;
    GetRawInputData(handle, RID_INPUT, nullptr, &size, sizeof(RAWINPUTHEADER));
    std::vector<BYTE> buffer(size);
    GetRawInputData(handle, RID_INPUT, buffer.data(), &size, sizeof(RAWINPUTHEADER));
    
    RAWINPUT* rawInput = reinterpret_cast<RAWINPUT*>(buffer.data());
    RAWMOUSE mouse = rawInput->data.mouse;
    
    InputEvent event;
    event.mousePosition.x = 0;
    event.mousePosition.y = 0;
    event.mouseDelta.x = mouse.lLastX;
    event.mouseDelta.y = mouse.lLastY;
    event.mouseButtons = mouse.ulButtons;
    
    // Try to find the device
    for (const auto& pair : devices_) {
        if (pair.second.deviceHandle == rawInput->header.hDevice) {
            event.deviceId = pair.first;
            break;
        }
    }
    
    if (mouse.usFlags & MOUSE_MOVE_ABSOLUTE) {
        event.type = InputEventType::MouseMove;
    }
    
    if (mouse.ulButtons & RI_MOUSE_LEFT_BUTTON_DOWN) {
        event.type = InputEventType::MouseButtonDown;
    } else if (mouse.ulButtons & RI_MOUSE_LEFT_BUTTON_UP) {
        event.type = InputEventType::MouseButtonUp;
    }
    
    if (mouse.ulButtons & RI_MOUSE_WHEEL) {
        event.type = InputEventType::MouseWheel;
        event.wheelDelta = (short)mouse.ulButtons;
    }
    
    return event;
}

} // namespace dualdesk
