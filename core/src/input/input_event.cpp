#include "dualdesk/input/input_event.h"
#include <sstream>

namespace dualdesk {

std::string InputEvent::ToString() const {
    std::stringstream ss;
    ss << "Event: ";
    
    switch(type) {
        case InputEventType::KeyDown: ss << "KeyDown"; break;
        case InputEventType::KeyUp: ss << "KeyUp"; break;
        case InputEventType::MouseMove: ss << "MouseMove"; break;
        case InputEventType::MouseButtonDown: ss << "MouseButtonDown"; break;
        case InputEventType::MouseButtonUp: ss << "MouseButtonUp"; break;
        case InputEventType::MouseWheel: ss << "MouseWheel"; break;
        default: ss << "Unknown"; break;
    }
    
    ss << " (Device: " << deviceId << ")";
    
    if (type == InputEventType::KeyDown || type == InputEventType::KeyUp) {
        ss << " Key: 0x" << std::hex << keyCode;
    }
    
    if (type == InputEventType::MouseMove) {
        ss << " Pos: (" << mousePosition.x << ", " << mousePosition.y << ")";
    }
    
    return ss.str();
}

} // namespace dualdesk