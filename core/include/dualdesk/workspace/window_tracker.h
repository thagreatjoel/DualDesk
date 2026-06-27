#pragma once
#include "window_info.h"
#include <vector>
#include <functional>

namespace dualdesk {

class WindowTracker {
public:
    WindowTracker();
    ~WindowTracker();

    // Get all windows
    std::vector<WindowInfo> GetAllWindows();
    
    // Get windows on a specific monitor
    std::vector<WindowInfo> GetWindowsOnMonitor(HMONITOR monitor);
    
    // Get windows by workspace
    std::vector<WindowInfo> GetWindowsOnWorkspace(int workspaceId);
    
    // Callback for window changes
    using WindowChangeCallback = std::function<void(const WindowInfo&, const std::string& action)>;
    void SetWindowChangeCallback(WindowChangeCallback callback);

private:
    // EnumWindows callback
    static BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam);
    
    // Check if window should be tracked
    static bool IsWindowTrackable(HWND hwnd);
    
    std::vector<WindowInfo> windows_;
    WindowChangeCallback changeCallback_;
    HWND hookWindow_ = nullptr;
};

} // namespace dualdesk