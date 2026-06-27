#pragma once
#include "input_device.h"
#include "input_event.h"
#include <vector>
#include <functional>
#include <unordered_map>
#include <memory>
#include <atomic>

namespace dualdesk {

class InputManager {
public:
    InputManager();
    ~InputManager();

    bool Initialize(HWND hwnd);
    void Shutdown();

    std::vector<InputDevice> EnumerateDevices();
    std::vector<InputDevice> GetKeyboards() const;
    std::vector<InputDevice> GetMice() const;
    InputDevice GetDevice(DeviceId id) const;

    using InputEventCallback = std::function<void(const InputEvent&)>;
    void SetInputEventCallback(InputEventCallback callback);
    
    using DeviceChangeCallback = std::function<void(const InputDevice&, bool added)>;
    void SetDeviceChangeCallback(DeviceChangeCallback callback);
    
    void ProcessRawInput(HRAWINPUT handle);
    void RefreshDevices();
    void CheckForDeviceChanges();

    struct PhysicalDevice {
        std::wstring name;
        InputDeviceType type;
        std::vector<DeviceId> interfaces;
    };
    std::vector<PhysicalDevice> GetPhysicalDevices() const;

private:
    bool RegisterForRawInput(HWND hwnd);
    void UnregisterRawInput();
    
    InputEvent ParseKeyboardInput(HRAWINPUT handle);
    InputEvent ParseMouseInput(HRAWINPUT handle);
    
    void EnumerateRawInputDevices();
    void EnumerateRawInputDevicesWithComparison();
    std::wstring GetDeviceName(HANDLE handle);
    InputDeviceType GetDeviceType(HANDLE handle);
    DeviceId GenerateDeviceId();

    bool CompareDeviceLists(const std::vector<InputDevice>& oldList, 
                           const std::vector<InputDevice>& newList,
                           std::vector<InputDevice>& added,
                           std::vector<InputDevice>& removed);

    std::unordered_map<DeviceId, InputDevice> devices_;
    InputEventCallback eventCallback_;
    DeviceChangeCallback deviceChangeCallback_;
    HWND targetWindow_ = nullptr;
    bool initialized_ = false;
    DeviceId nextDeviceId_ = 1;
    
    std::atomic<bool> deviceChangePending_{false};
    
    // Use Windows tick count instead of chrono
    DWORD lastDeviceCheckTime_ = 0;
    static constexpr DWORD DEVICE_CHECK_INTERVAL_MS = 1000;
};

} // namespace dualdesk