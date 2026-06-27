#include "dualdesk/input/input_device.h"
#include <sstream>

namespace dualdesk {

std::string InputDevice::ToString() const {
    std::stringstream ss;
    ss << "Device " << id << ": ";
    
    switch(type) {
        case InputDeviceType::Keyboard: ss << "Keyboard"; break;
        case InputDeviceType::Mouse: ss << "Mouse"; break;
        case InputDeviceType::Touchpad: ss << "Touchpad"; break;
        case InputDeviceType::TouchScreen: ss << "TouchScreen"; break;
        default: ss << "Unknown"; break;
    }
    
    ss << " - ";
    
    // Convert wide string to narrow for display
    std::string name(deviceName.begin(), deviceName.end());
    ss << name;
    
    return ss.str();
}

} // namespace dualdesk