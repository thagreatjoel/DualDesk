#include "dualdesk/workspace/window_snapper.h"
#include "dualdesk/core/logger.h"
#include <windows.h>

namespace dualdesk {

WindowSnapper::WindowSnapper() {
    LOG_DEBUG("WindowSnapper created");
}

WindowSnapper::~WindowSnapper() {
    LOG_DEBUG("WindowSnapper destroyed");
}

bool WindowSnapper::SnapToLeft(HWND hwnd) {
    RECT monitor = GetMonitorWorkArea(hwnd);
    RECT rect = GetWindowRect(hwnd);
    rect.left = monitor.left;
    rect.right = monitor.left + (rect.right - rect.left);
    return SetWindowPos(hwnd, NULL, rect.left, rect.top, 
                        rect.right - rect.left, rect.bottom - rect.top,
                        SWP_NOZORDER) != 0;
}

bool WindowSnapper::SnapToRight(HWND hwnd) {
    RECT monitor = GetMonitorWorkArea(hwnd);
    RECT rect = GetWindowRect(hwnd);
    rect.right = monitor.right;
    rect.left = monitor.right - (rect.right - rect.left);
    return SetWindowPos(hwnd, NULL, rect.left, rect.top,
                        rect.right - rect.left, rect.bottom - rect.top,
                        SWP_NOZORDER) != 0;
}

bool WindowSnapper::SnapToTop(HWND hwnd) {
    RECT monitor = GetMonitorWorkArea(hwnd);
    RECT rect = GetWindowRect(hwnd);
    rect.top = monitor.top;
    rect.bottom = monitor.top + (rect.bottom - rect.top);
    return SetWindowPos(hwnd, NULL, rect.left, rect.top,
                        rect.right - rect.left, rect.bottom - rect.top,
                        SWP_NOZORDER) != 0;
}

bool WindowSnapper::SnapToBottom(HWND hwnd) {
    RECT monitor = GetMonitorWorkArea(hwnd);
    RECT rect = GetWindowRect(hwnd);
    rect.bottom = monitor.bottom;
    rect.top = monitor.bottom - (rect.bottom - rect.top);
    return SetWindowPos(hwnd, NULL, rect.left, rect.top,
                        rect.right - rect.left, rect.bottom - rect.top,
                        SWP_NOZORDER) != 0;
}

bool WindowSnapper::SnapToCenter(HWND hwnd) {
    RECT monitor = GetMonitorWorkArea(hwnd);
    RECT rect = GetWindowRect(hwnd);
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;
    rect.left = monitor.left + (monitor.right - monitor.left - width) / 2;
    rect.top = monitor.top + (monitor.bottom - monitor.top - height) / 2;
    rect.right = rect.left + width;
    rect.bottom = rect.top + height;
    return SetWindowPos(hwnd, NULL, rect.left, rect.top,
                        rect.right - rect.left, rect.bottom - rect.top,
                        SWP_NOZORDER) != 0;
}

bool WindowSnapper::SnapToTopLeft(HWND hwnd) {
    RECT monitor = GetMonitorWorkArea(hwnd);
    RECT rect = GetWindowRect(hwnd);
    rect.left = monitor.left;
    rect.top = monitor.top;
    return SetWindowPos(hwnd, NULL, rect.left, rect.top,
                        rect.right - rect.left, rect.bottom - rect.top,
                        SWP_NOZORDER) != 0;
}

bool WindowSnapper::SnapToTopRight(HWND hwnd) {
    RECT monitor = GetMonitorWorkArea(hwnd);
    RECT rect = GetWindowRect(hwnd);
    rect.right = monitor.right;
    rect.top = monitor.top;
    rect.left = monitor.right - (rect.right - rect.left);
    return SetWindowPos(hwnd, NULL, rect.left, rect.top,
                        rect.right - rect.left, rect.bottom - rect.top,
                        SWP_NOZORDER) != 0;
}

bool WindowSnapper::SnapToBottomLeft(HWND hwnd) {
    RECT monitor = GetMonitorWorkArea(hwnd);
    RECT rect = GetWindowRect(hwnd);
    rect.left = monitor.left;
    rect.bottom = monitor.bottom;
    rect.top = monitor.bottom - (rect.bottom - rect.top);
    return SetWindowPos(hwnd, NULL, rect.left, rect.top,
                        rect.right - rect.left, rect.bottom - rect.top,
                        SWP_NOZORDER) != 0;
}

bool WindowSnapper::SnapToBottomRight(HWND hwnd) {
    RECT monitor = GetMonitorWorkArea(hwnd);
    RECT rect = GetWindowRect(hwnd);
    rect.right = monitor.right;
    rect.bottom = monitor.bottom;
    rect.left = monitor.right - (rect.right - rect.left);
    rect.top = monitor.bottom - (rect.bottom - rect.top);
    return SetWindowPos(hwnd, NULL, rect.left, rect.top,
                        rect.right - rect.left, rect.bottom - rect.top,
                        SWP_NOZORDER) != 0;
}

bool WindowSnapper::SnapToLeftHalf(HWND hwnd) {
    RECT monitor = GetMonitorWorkArea(hwnd);
    int halfWidth = (monitor.right - monitor.left) / 2;
    return SetWindowPos(hwnd, NULL, monitor.left, monitor.top,
                        halfWidth, monitor.bottom - monitor.top,
                        SWP_NOZORDER) != 0;
}

bool WindowSnapper::SnapToRightHalf(HWND hwnd) {
    RECT monitor = GetMonitorWorkArea(hwnd);
    int halfWidth = (monitor.right - monitor.left) / 2;
    return SetWindowPos(hwnd, NULL, monitor.left + halfWidth, monitor.top,
                        halfWidth, monitor.bottom - monitor.top,
                        SWP_NOZORDER) != 0;
}

bool WindowSnapper::SnapToGrid(HWND hwnd, int gridSize) {
    RECT rect = GetWindowRect(hwnd);
    int snapSize = (gridSize > 0) ? gridSize : gridSize_;
    
    rect.left = ((rect.left + snapSize / 2) / snapSize) * snapSize;
    rect.top = ((rect.top + snapSize / 2) / snapSize) * snapSize;
    rect.right = ((rect.right + snapSize / 2) / snapSize) * snapSize;
    rect.bottom = ((rect.bottom + snapSize / 2) / snapSize) * snapSize;
    
    return SetWindowPos(hwnd, NULL, rect.left, rect.top,
                        rect.right - rect.left, rect.bottom - rect.top,
                        SWP_NOZORDER) != 0;
}

bool WindowSnapper::SnapToWindow(HWND hwnd, HWND targetWindow) {
    RECT targetRect = GetWindowRect(targetWindow);
    RECT rect = GetWindowRect(hwnd);
    
    // Align left edges
    rect.left = targetRect.left;
    rect.right = rect.left + (rect.right - rect.left);
    
    return SetWindowPos(hwnd, NULL, rect.left, rect.top,
                        rect.right - rect.left, rect.bottom - rect.top,
                        SWP_NOZORDER) != 0;
}

RECT WindowSnapper::GetSnapPosition(HWND hwnd, int snapType) const {
    RECT monitor = GetMonitorWorkArea(hwnd);
    RECT rect = GetWindowRect(hwnd);
    
    switch(snapType) {
        case 0: // Left
            rect.left = monitor.left;
            break;
        case 1: // Right
            rect.right = monitor.right;
            break;
        case 2: // Top
            rect.top = monitor.top;
            break;
        case 3: // Bottom
            rect.bottom = monitor.bottom;
            break;
        default:
            break;
    }
    
    return rect;
}

RECT WindowSnapper::GetWindowRect(HWND hwnd) const {
    RECT rect = {0, 0, 0, 0};
    if (hwnd && IsWindow(hwnd)) {
        ::GetWindowRect(hwnd, &rect);
    }
    return rect;
}

RECT WindowSnapper::GetMonitorWorkArea(HWND hwnd) const {
    HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = {sizeof(MONITORINFO)};
    RECT rect = {0, 0, 0, 0};
    if (monitor && GetMonitorInfo(monitor, &mi)) {
        rect = mi.rcWork;
    }
    return rect;
}

} // namespace dualdesk