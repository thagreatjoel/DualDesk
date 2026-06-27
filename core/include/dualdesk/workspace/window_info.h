#pragma once
#include <windows.h>
#include <string>

namespace dualdesk {

struct WindowInfo {
    HWND handle = nullptr;
    std::wstring title;
    std::wstring className;
    RECT position = {0, 0, 0, 0};
    bool isVisible = false;
    bool isMinimized = false;
    bool isMaximized = false;
    
    int Width() const { return position.right - position.left; }
    int Height() const { return position.bottom - position.top; }
    
    // Which monitor this window is on
    HMONITOR GetMonitor() const {
        return MonitorFromWindow(handle, MONITOR_DEFAULTTONEAREST);
    }
};

} // namespace dualdesk