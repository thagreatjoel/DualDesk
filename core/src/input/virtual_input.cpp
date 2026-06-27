#include "dualdesk/input/virtual_input.h"
#include "dualdesk/core/logger.h"
#include <windows.h>
#include <thread>
#include <chrono>

namespace dualdesk {

VirtualInput::VirtualInput() {
    LOG_DEBUG("VirtualInput created");
}

VirtualInput::~VirtualInput() {
    LOG_DEBUG("VirtualInput destroyed");
}

bool VirtualInput::SendKeyToWindow(HWND hwnd, const InputEvent& event) {
    if (!hwnd || !IsWindow(hwnd)) {
        LOG_DEBUG("Invalid window handle");
        return false;
    }

    UINT msg = (event.type == InputEventType::KeyDown) ? WM_KEYDOWN : WM_KEYUP;
    WPARAM wParam = event.keyCode;
    LPARAM lParam = (event.scanCode << 16) | 
                    (event.isExtended ? 0x01000000 : 0) |
                    (event.type == InputEventType::KeyDown ? 0 : 0xC0000000);

    return PostMessage(hwnd, msg, wParam, lParam) != 0;
}

bool VirtualInput::SendMouseToWindow(HWND hwnd, const InputEvent& event) {
    if (!hwnd || !IsWindow(hwnd)) {
        LOG_DEBUG("Invalid window handle");
        return false;
    }

    POINT pt = event.mousePosition;
    ScreenToClient(hwnd, &pt);

    UINT msg = 0;
    WPARAM wParam = 0;
    LPARAM lParam = MAKELPARAM(pt.x, pt.y);

    switch(event.type) {
        case InputEventType::MouseMove:
            msg = WM_MOUSEMOVE;
            wParam = event.mouseButtons;
            break;
        case InputEventType::MouseButtonDown:
            msg = WM_LBUTTONDOWN;
            wParam = MK_LBUTTON;
            break;
        case InputEventType::MouseButtonUp:
            msg = WM_LBUTTONUP;
            break;
        case InputEventType::MouseWheel:
            msg = WM_MOUSEWHEEL;
            wParam = MAKEWPARAM(0, event.wheelDelta);
            break;
        default:
            return false;
    }

    return PostMessage(hwnd, msg, wParam, lParam) != 0;
}

bool VirtualInput::SendInputToWindow(HWND hwnd, const InputEvent& event) {
    if (event.type == InputEventType::KeyDown || 
        event.type == InputEventType::KeyUp) {
        return SendKeyToWindow(hwnd, event);
    } else {
        return SendMouseToWindow(hwnd, event);
    }
}

bool VirtualInput::SendGlobalKey(const InputEvent& event) {
    INPUT input = CreateKeyboardInput(event.keyCode, 
                                      event.type == InputEventType::KeyDown);
    return SendInput(1, &input, sizeof(INPUT)) == 1;
}

bool VirtualInput::SendGlobalMouse(const InputEvent& event) {
    uint32_t flags = 0;
    switch(event.type) {
        case InputEventType::MouseMove:
            flags = MOUSEEVENTF_MOVE;
            break;
        case InputEventType::MouseButtonDown:
            flags = MOUSEEVENTF_LEFTDOWN;
            break;
        case InputEventType::MouseButtonUp:
            flags = MOUSEEVENTF_LEFTUP;
            break;
        case InputEventType::MouseWheel:
            flags = MOUSEEVENTF_WHEEL;
            break;
        default:
            return false;
    }

    INPUT input = CreateMouseInput(event.mousePosition.x, 
                                   event.mousePosition.y, 
                                   flags);
    return SendInput(1, &input, sizeof(INPUT)) == 1;
}

bool VirtualInput::PostKeyToWindow(HWND hwnd, const InputEvent& event) {
    return SendKeyToWindow(hwnd, event);
}

bool VirtualInput::PostMouseToWindow(HWND hwnd, const InputEvent& event) {
    return SendMouseToWindow(hwnd, event);
}

bool VirtualInput::SendKeyCombination(HWND hwnd, uint16_t keyCode, 
                                      bool withCtrl, bool withShift, bool withAlt) {
    std::vector<INPUT> inputs;

    // Add modifier keys
    if (withCtrl) {
        inputs.push_back(CreateKeyboardInput(VK_CONTROL, true));
    }
    if (withShift) {
        inputs.push_back(CreateKeyboardInput(VK_SHIFT, true));
    }
    if (withAlt) {
        inputs.push_back(CreateKeyboardInput(VK_MENU, true));
    }

    // Add the key
    inputs.push_back(CreateKeyboardInput(keyCode, true));
    
    // Wait a bit
    if (delayMs_ > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(delayMs_));
    }

    // Release the key
    inputs.push_back(CreateKeyboardInput(keyCode, false));

    // Release modifiers
    if (withAlt) {
        inputs.push_back(CreateKeyboardInput(VK_MENU, false));
    }
    if (withShift) {
        inputs.push_back(CreateKeyboardInput(VK_SHIFT, false));
    }
    if (withCtrl) {
        inputs.push_back(CreateKeyboardInput(VK_CONTROL, false));
    }

    return SendInputArray(inputs);
}

bool VirtualInput::SendMouseClick(HWND hwnd, int x, int y, bool leftButton) {
    // Move mouse to position
    SetCursorPos(x, y);

    // Simulate click
    INPUT inputs[2];
    uint32_t downFlag = leftButton ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_RIGHTDOWN;
    uint32_t upFlag = leftButton ? MOUSEEVENTF_LEFTUP : MOUSEEVENTF_RIGHTUP;

    inputs[0] = CreateMouseInput(x, y, downFlag);
    inputs[1] = CreateMouseInput(x, y, upFlag);

    return SendInput(2, inputs, sizeof(INPUT)) == 2;
}

bool VirtualInput::FocusWindow(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) return false;
    return SetForegroundWindow(hwnd) != 0;
}

bool VirtualInput::BringWindowToFront(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) return false;
    return SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0, 
                        SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW) != 0;
}

INPUT VirtualInput::CreateKeyboardInput(uint16_t keyCode, bool keyDown) {
    INPUT input = {0};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = keyCode;
    input.ki.dwFlags = keyDown ? 0 : KEYEVENTF_KEYUP;
    return input;
}

INPUT VirtualInput::CreateMouseInput(int x, int y, uint32_t flags) {
    INPUT input = {0};
    input.type = INPUT_MOUSE;
    input.mi.dx = x;
    input.mi.dy = y;
    input.mi.dwFlags = flags;
    return input;
}

INPUT VirtualInput::CreateMouseButtonInput(uint32_t flags) {
    INPUT input = {0};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = flags;
    return input;
}

bool VirtualInput::SendInputArray(std::vector<INPUT>& inputs) {
    if (inputs.empty()) return false;
    UINT result = SendInput((UINT)inputs.size(), inputs.data(), sizeof(INPUT));
    return result == inputs.size();
}

} // namespace dualdesk