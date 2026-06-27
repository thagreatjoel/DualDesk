#include "dualdesk/input/input_manager.h"
#include "dualdesk/core/logger.h"
#include <windows.h>
#include <hidsdi.h>
#include <setupapi.h>
#include <devguid.h>
#include <string>
#include <unordered_map>
#include <set>

#pragma comment(lib, "hid.lib")
#pragma comment(lib, "setupapi.lib")

namespace dualdesk {

InputManager::InputManager() {
    lastDeviceCheckTime_ = GetTickCount();
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
    
    std::string msg = "InputManager initialized with " + std::to_string(devices_.size()) + " devices";
    LOG_INFO(msg);
    
    // Log keyboard and mouse counts
    int kbCount = 0, mouseCount = 0;
    for (const auto& pair : devices_) {
        if (pair.second.type == InputDeviceType::Keyboard) kbCount++;
        else if (pair.second.type == InputDeviceType::Mouse) mouseCount++;
    }
    LOG_INFO("Keyboards: " + std::to_string(kbCount));
    LOG_INFO("Mice: " + std::to_string(mouseCount));
    
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
    LOG_INFO("RegisterForRawInput called");
    
    RAWINPUTDEVICE ridKeyboard = {0};
    ridKeyboard.usUsagePage = 0x01;
    ridKeyboard.usUsage = 0x06;
    ridKeyboard.dwFlags = RIDEV_INPUTSINK;
    ridKeyboard.hwndTarget = hwnd;
    
    RAWINPUTDEVICE ridMouse = {0};
    ridMouse.usUsagePage = 0x01;
    ridMouse.usUsage = 0x02;
    ridMouse.dwFlags = RIDEV_INPUTSINK;
    ridMouse.hwndTarget = hwnd;
    
    RAWINPUTDEVICE devices[] = {ridKeyboard, ridMouse};
    
    if (!RegisterRawInputDevices(devices, 2, sizeof(RAWINPUTDEVICE))) {
        LOG_ERROR("Failed to register raw input devices");
        return false;
    }
    
    LOG_INFO("RAW INPUT REGISTERED SUCCESSFULLY!");
    return true;
}

void InputManager::UnregisterRawInput() {
    RAWINPUTDEVICE rid = {0};
    rid.dwFlags = RIDEV_REMOVE;
    RegisterRawInputDevices(&rid, 1, sizeof(RAWINPUTDEVICE));
}

void InputManager::EnumerateRawInputDevices() {
    UINT deviceCount = 0;
    if (GetRawInputDeviceList(nullptr, &deviceCount, sizeof(RAWINPUTDEVICELIST)) != 0) {
        LOG_ERROR("Failed to get raw input device count");
        return;
    }
    
    if (deviceCount == 0) {
        LOG_INFO("No raw input devices found");
        return;
    }
    
    std::vector<RAWINPUTDEVICELIST> deviceList(deviceCount);
    if (GetRawInputDeviceList(deviceList.data(), &deviceCount, sizeof(RAWINPUTDEVICELIST)) == (UINT)-1) {
        LOG_ERROR("Failed to get raw input device list");
        return;
    }
    
    LOG_INFO("Found " + std::to_string(deviceCount) + " raw input devices");
    
    // Clear and rebuild device map
    devices_.clear();
    
    for (const auto& device : deviceList) {
        InputDevice info;
        info.id = GenerateDeviceId();
        info.deviceHandle = device.hDevice;
        info.type = GetDeviceType(device.hDevice);
        info.state = InputDeviceState::Connected;
        
        // Get device name
        std::wstring devicePath = GetDeviceName(device.hDevice);
        info.deviceName = devicePath;
        
        // Skip unknown devices
        if (info.type == InputDeviceType::Unknown) {
            continue;
        }
        
        // Try to get a friendly name based on device path
        std::wstring friendlyName;
        if (devicePath.find(L"ELAN07FB") != std::wstring::npos) {
            friendlyName = L"Touchpad";
        } else if (devicePath.find(L"HPQ8001") != std::wstring::npos) {
            friendlyName = L"Keyboard";
        } else if (devicePath.find(L"VID_3151") != std::wstring::npos) {
            friendlyName = L"External Mouse";
        } else if (devicePath.find(L"VID_03F0") != std::wstring::npos) {
            if (devicePath.find(L"MI_00") != std::wstring::npos) {
                friendlyName = L"Keyboard";
            } else {
                friendlyName = L"Mouse";
            }
        } else if (devicePath.find(L"USB") != std::wstring::npos) {
            friendlyName = L"USB Device";
        } else {
            // Keep the original name but shorten it
            size_t lastSlash = devicePath.find_last_of(L'#');
            if (lastSlash != std::wstring::npos) {
                friendlyName = devicePath.substr(lastSlash + 1);
                if (friendlyName.length() > 20) {
                    friendlyName = friendlyName.substr(0, 20) + L"...";
                }
            } else {
                friendlyName = L"HID Device";
            }
        }
        
        info.deviceName = friendlyName;
        
        // Store the device
        devices_[info.id] = info;
        
        std::string name(friendlyName.begin(), friendlyName.end());
        std::string typeStr;
        switch(info.type) {
            case InputDeviceType::Keyboard: typeStr = "Keyboard"; break;
            case InputDeviceType::Mouse: typeStr = "Mouse"; break;
            default: typeStr = "Unknown"; break;
        }
        LOG_INFO("  - " + name + " (" + typeStr + ")");
    }
    
    LOG_INFO("Total devices stored: " + std::to_string(devices_.size()));
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

void InputManager::SetInputEventCallback(InputEventCallback callback) {
    eventCallback_ = callback;
}

void InputManager::SetDeviceChangeCallback(DeviceChangeCallback callback) {
    deviceChangeCallback_ = callback;
}

void InputManager::RefreshDevices() {
    LOG_INFO("Refreshing input devices...");
    EnumerateRawInputDevices();
}

void InputManager::ProcessRawInput(HRAWINPUT handle) {
    UINT size = 0;
    if (GetRawInputData(handle, RID_INPUT, nullptr, &size, sizeof(RAWINPUTHEADER)) != 0) {
        return;
    }
    
    if (size == 0) return;
    
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
    event.type = InputEventType::Unknown;
    
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
        event.mouseButtons = MK_LBUTTON;
    } else if (mouse.ulButtons & RI_MOUSE_LEFT_BUTTON_UP) {
        event.type = InputEventType::MouseButtonUp;
        event.mouseButtons = MK_LBUTTON;
    } else if (mouse.ulButtons & RI_MOUSE_RIGHT_BUTTON_DOWN) {
        event.type = InputEventType::MouseButtonDown;
        event.mouseButtons = MK_RBUTTON;
    } else if (mouse.ulButtons & RI_MOUSE_RIGHT_BUTTON_UP) {
        event.type = InputEventType::MouseButtonUp;
        event.mouseButtons = MK_RBUTTON;
    } else if (mouse.ulButtons & RI_MOUSE_MIDDLE_BUTTON_DOWN) {
        event.type = InputEventType::MouseButtonDown;
        event.mouseButtons = MK_MBUTTON;
    } else if (mouse.ulButtons & RI_MOUSE_MIDDLE_BUTTON_UP) {
        event.type = InputEventType::MouseButtonUp;
        event.mouseButtons = MK_MBUTTON;
    }
    
    if (mouse.ulButtons & RI_MOUSE_WHEEL) {
        event.type = InputEventType::MouseWheel;
        event.wheelDelta = (short)mouse.ulButtons;
    }
    
    return event;
}

void InputManager::CheckForDeviceChanges() {
    DWORD now = GetTickCount();
    DWORD elapsed = now - lastDeviceCheckTime_;
    
    if (elapsed < DEVICE_CHECK_INTERVAL_MS) {
        return;
    }
    
    lastDeviceCheckTime_ = now;
    EnumerateRawInputDevices();
}

} // namespace dualdesk