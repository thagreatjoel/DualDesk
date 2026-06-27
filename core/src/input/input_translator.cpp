#include "dualdesk/input/input_translator.h"
#include "dualdesk/core/logger.h"
#include <windows.h>

namespace dualdesk {

InputTranslator::InputTranslator() {
    LOG_DEBUG("InputTranslator created");
}

InputTranslator::~InputTranslator() {
    LOG_DEBUG("InputTranslator destroyed");
}

bool InputTranslator::TranslateToWindowsMessage(const InputEvent& event, 
                                                 UINT& msg, 
                                                 WPARAM& wParam, 
                                                 LPARAM& lParam) {
    msg = GetMessageType(event);
    wParam = GetWParam(event);
    lParam = GetLParam(event);
    return msg != 0;
}

UINT InputTranslator::GetMessageType(const InputEvent& event) const {
    switch(event.type) {
        case InputEventType::KeyDown:
            return WM_KEYDOWN;
        case InputEventType::KeyUp:
            return WM_KEYUP;
        case InputEventType::MouseMove:
            return WM_MOUSEMOVE;
        case InputEventType::MouseButtonDown:
            return WM_LBUTTONDOWN;
        case InputEventType::MouseButtonUp:
            return WM_LBUTTONUP;
        case InputEventType::MouseWheel:
            return WM_MOUSEWHEEL;
        default:
            return 0;
    }
}

WPARAM InputTranslator::GetWParam(const InputEvent& event) const {
    switch(event.type) {
        case InputEventType::KeyDown:
        case InputEventType::KeyUp:
            return event.keyCode;
        case InputEventType::MouseButtonDown:
            return MK_LBUTTON;
        case InputEventType::MouseWheel:
            return MAKEWPARAM(0, event.wheelDelta);
        default:
            return 0;
    }
}

LPARAM InputTranslator::GetLParam(const InputEvent& event) const {
    switch(event.type) {
        case InputEventType::KeyDown:
            return (event.scanCode << 16) | 
                   (event.isExtended ? 0x01000000 : 0);
        case InputEventType::KeyUp:
            return (event.scanCode << 16) | 
                   (event.isExtended ? 0x01000000 : 0) |
                   0xC0000000;
        case InputEventType::MouseMove:
        case InputEventType::MouseButtonDown:
        case InputEventType::MouseButtonUp:
            return MAKELPARAM(event.mousePosition.x, event.mousePosition.y);
        default:
            return 0;
    }
}

uint16_t InputTranslator::TranslateVirtualKey(uint16_t keyCode) const {
    // Map our key codes to Windows virtual keys
    // This is a simple pass-through for now
    return keyCode;
}

uint16_t InputTranslator::TranslateScanCode(uint16_t scanCode) const {
    return scanCode;
}

bool InputTranslator::IsKeyDown(uint16_t keyCode) const {
    UpdateKeyboardState();
    return (keyboardState_[keyCode] & 0x80) != 0;
}

bool InputTranslator::IsKeyToggled(uint16_t keyCode) const {
    UpdateKeyboardState();
    return (keyboardState_[keyCode] & 0x01) != 0;
}

uint8_t InputTranslator::GetKeyState(uint16_t keyCode) const {
    UpdateKeyboardState();
    return keyboardState_[keyCode];
}

void InputTranslator::UpdateKeyboardState() const {
    if (!keyboardStateInitialized_) {
        GetKeyboardState(keyboardState_);
        keyboardStateInitialized_ = true;
    }
}

void InputTranslator::UpdateKeyState(uint16_t keyCode, bool isDown) const {
    UpdateKeyboardState();
    if (isDown) {
        keyboardState_[keyCode] |= 0x80;
    } else {
        keyboardState_[keyCode] &= ~0x80;
    }
}

uint32_t InputTranslator::TranslateMouseButtons(uint16_t buttons) const {
    uint32_t result = 0;
    if (buttons & RI_MOUSE_LEFT_BUTTON_DOWN) result |= MK_LBUTTON;
    if (buttons & RI_MOUSE_RIGHT_BUTTON_DOWN) result |= MK_RBUTTON;
    if (buttons & RI_MOUSE_MIDDLE_BUTTON_DOWN) result |= MK_MBUTTON;
    return result;
}

} // namespace dualdesk