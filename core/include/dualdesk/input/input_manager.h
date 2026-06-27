#pragma once
#include "input_device.h"
#include "input_event.h"
#include <vector>
#include <functional>
#include <unordered_map>
#include <memory>

namespace dualdesk {

class InputManager {
public:
    InputManager();
    ~InputManager();

    // Initialize raw input
    bool Initialize(HWND hwnd);
    void Shutdown();

    // Enumerate devices
    std::vector<InputDevice> EnumerateDevices();
    std::vector<InputDevice> GetKeyboards() const;
    std::vector<InputDevice> GetMice() const;


    
struct PhysicalDevice {
    std::wstring name;
    InputDeviceType type;
    std::vector<DeviceId> interfaces;
};

std::vector<PhysicalDevice> GetPhysicalDevices() const;

    // Get device by ID
    InputDevice GetDevice(DeviceId id) const;

    // Event handling
    using InputEventCallback = std::function<void(const InputEvent&)>;
    void SetInputEventCallback(InputEventCallback callback);
    
    // Process raw input messages
    void ProcessRawInput(HRAWINPUT handle);

private:
    // Register for raw input
    bool RegisterForRawInput(HWND hwnd);
    void UnregisterRawInput();
    
    // Parse raw input data
    InputEvent ParseKeyboardInput(HRAWINPUT handle);
    InputEvent ParseMouseInput(HRAWINPUT handle);
    
    // Device management
    void EnumerateRawInputDevices();
    std::wstring GetDeviceName(HANDLE handle);
    InputDeviceType GetDeviceType(HANDLE handle);
    DeviceId GenerateDeviceId();

    std::unordered_map<DeviceId, InputDevice> devices_;
    InputEventCallback eventCallback_;
    HWND targetWindow_ = nullptr;
    bool initialized_ = false;
    DeviceId nextDeviceId_ = 1;
};

} // namespace dualdesk