#pragma once

#include "input_event.h"
#include <windows.h>

namespace dualdesk {

/**
 * @brief Translates input events to Windows messages
 * 
 * The InputTranslator converts our internal InputEvent format
 * to Windows messages (WM_KEYDOWN, WM_MOUSEMOVE, etc.)
 */
class InputTranslator {
public:
    InputTranslator();
    ~InputTranslator();

    // Translate event to Windows messages
    bool TranslateToWindowsMessage(const InputEvent& event, 
                                   UINT& msg, 
                                   WPARAM& wParam, 
                                   LPARAM& lParam);

    // Get message parameters
    UINT GetMessageType(const InputEvent& event) const;
    WPARAM GetWParam(const InputEvent& event) const;
    LPARAM GetLParam(const InputEvent& event) const;

    // Virtual key code conversion
    uint16_t TranslateVirtualKey(uint16_t keyCode) const;
    uint16_t TranslateScanCode(uint16_t scanCode) const;

    // Keyboard state
    bool IsKeyDown(uint16_t keyCode) const;
    bool IsKeyToggled(uint16_t keyCode) const;
    uint8_t GetKeyState(uint16_t keyCode) const;

    // Mouse button conversion
    uint32_t TranslateMouseButtons(uint16_t buttons) const;

private:
    // Keyboard state tracking
    mutable uint8_t keyboardState_[256];
    mutable bool keyboardStateInitialized_ = false;

    void UpdateKeyboardState() const;
    void UpdateKeyState(uint16_t keyCode, bool isDown) const;
};

} // namespace dualdesk