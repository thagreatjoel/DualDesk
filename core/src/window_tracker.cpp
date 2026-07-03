#include "dualdesk/workspace/window_tracker.h"
#include "dualdesk/core/logger.h"
#include <windows.h>
#include <string>

namespace dualdesk {

// Helper to convert wstring to string
std::string WStringToString(const std::wstring& wstr) {
    if (wstr.empty()) return std::string();
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), 
                                          NULL, 0, NULL, NULL);
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), 
                        &strTo[0], size_needed, NULL, NULL);
    return strTo;
}

WindowTracker::WindowTracker() {
    LOG_DEBUG("WindowTracker created");
}

WindowTracker::~WindowTracker() {
    LOG_DEBUG("WindowTracker destroyed");
}

std::vector<WindowInfo> WindowTracker::GetAllWindows() {
    windows_.clear();
    EnumWindows(EnumWindowsProc, (LPARAM)this);
    
    std::string msg = "Found " + std::to_string(windows_.size()) + " trackable windows";
    LOG_INFO(msg);
    
    return windows_;
}

BOOL CALLBACK WindowTracker::EnumWindowsProc(HWND hwnd, LPARAM lParam) {
    auto* tracker = reinterpret_cast<WindowTracker*>(lParam);
    if (!tracker) return TRUE;
    
    if (!IsWindowTrackable(hwnd)) return TRUE;
    
    WindowInfo info;
    info.handle = hwnd;
    
    // Get window title
    wchar_t title[256];
    if (GetWindowTextW(hwnd, title, 256)) {
        info.title = title;
    }
    
    // Get window class name
    wchar_t className[256];
    if (GetClassNameW(hwnd, className, 256)) {
        info.className = className;
    }
    
    // Get window position
    GetWindowRect(hwnd, &info.position);
    
    // Get window state
    info.isVisible = IsWindowVisible(hwnd);
    info.isMinimized = IsIconic(hwnd) != 0;
    info.isMaximized = IsZoomed(hwnd) != 0;
    
    // Only track visible windows with titles
    if (info.isVisible && !info.title.empty()) {
        tracker->windows_.push_back(info);
    }
    
    return TRUE;
}

bool WindowTracker::IsWindowTrackable(HWND hwnd) {
    if (!IsWindowVisible(hwnd)) return false;
    
    wchar_t title[256];
    if (GetWindowTextW(hwnd, title, 256) == 0) return false;
    if (wcslen(title) == 0) return false;
    
    wchar_t className[256];
    GetClassNameW(hwnd, className, 256);
    std::wstring cls = className;
    
    if (cls == L"Progman" || cls == L"WorkerW" || 
        cls == L"Shell_TrayWnd" || cls == L"Button") {
        return false;
    }
    
    return true;
}

std::vector<WindowInfo> WindowTracker::GetWindowsOnMonitor(HMONITOR monitor) {
    std::vector<WindowInfo> result;
    auto allWindows = GetAllWindows();
    
    for (const auto& window : allWindows) {
        HMONITOR windowMonitor = MonitorFromWindow(window.handle, MONITOR_DEFAULTTONEAREST);
        if (windowMonitor == monitor) {
            result.push_back(window);
        }
    }
    
    std::string msg = "Found " + std::to_string(result.size()) + " windows on this monitor";
    LOG_INFO(msg);
    
    for (const auto& window : result) {
        std::string titleStr = WStringToString(window.title);
        LOG_INFO("  - " + titleStr);
    }
    
    return result;
}

} // namespace dualdesk