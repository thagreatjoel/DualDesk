#pragma once
#include "input_manager.h"
#include <windows.h>

namespace dualdesk {

class RawInputHandler {
public:
    RawInputHandler();
    ~RawInputHandler();

    bool Initialize(HWND hwnd, InputManager* manager);
    void Shutdown();
    
    // Handle WM_INPUT messages
    bool HandleInputMessage(HRAWINPUT handle);

private:
    InputManager* manager_ = nullptr;
    HWND targetWindow_ = nullptr;
    bool initialized_ = false;
};

} // namespace dualdesk