#pragma once

#include "input_event.h"
#include <windows.h>
#include <vector>

namespace dualdesk {

/**
 * @brief Injects virtual input events into windows
 * 
 * VirtualInput simulates keyboard and mouse input using
 * SendInput, PostMessage, and SendMessage APIs.
 */
class VirtualInput {
public:
    VirtualInput();
    ~VirtualInput();

    // Send input to a specific window
    bool SendKeyToWindow(HWND hwnd, const InputEvent& event);
    bool SendMouseToWindow(HWND hwnd, const InputEvent& event);
    bool SendInputToWindow(HWND hwnd, const InputEvent& event);

    // Send input globally
    bool SendGlobalKey(const InputEvent& event);
    bool SendGlobalMouse(const InputEvent& event);

    // Post messages (non-blocking)
    bool PostKeyToWindow(HWND hwnd, const InputEvent& event);
    bool PostMouseToWindow(HWND hwnd, const InputEvent& event);

    // Send key combination
    bool SendKeyCombination(HWND hwnd, uint16_t keyCode, bool withCtrl, 
                           bool withShift, bool withAlt);

    // Send mouse click
    bool SendMouseClick(HWND hwnd, int x, int y, bool leftButton);

    // Focus window
    bool FocusWindow(HWND hwnd);
    bool BringWindowToFront(HWND hwnd);

    // Input injection settings
    void SetDelayMilliseconds(DWORD delay) { delayMs_ = delay; }
    DWORD GetDelayMilliseconds() const { return delayMs_; }

private:
    // Helper functions
    INPUT CreateKeyboardInput(uint16_t keyCode, bool keyDown);
    INPUT CreateMouseInput(int x, int y, uint32_t flags);
    INPUT CreateMouseButtonInput(uint32_t flags);

    // Send input array
    bool SendInputArray(std::vector<INPUT>& inputs);

    DWORD delayMs_ = 0;
};

} // namespace dualdesk