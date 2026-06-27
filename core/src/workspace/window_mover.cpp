#include "dualdesk/workspace/window_mover.h"
#include "dualdesk/workspace/workspace_manager.h"
#include "dualdesk/core/logger.h"
#include <windows.h>

namespace dualdesk {

WindowMover::WindowMover() {
    LOG_DEBUG("WindowMover created");
}

WindowMover::~WindowMover() {
    Shutdown();
}

void WindowMover::Initialize(WorkspaceManager* manager) {
    if (initialized_) {
        LOG_WARN("WindowMover already initialized");
        return;
    }

    workspaceManager_ = manager;
    initialized_ = true;
    LOG_INFO("WindowMover initialized");
}

void WindowMover::Shutdown() {
    if (!initialized_) return;
    initialized_ = false;
    LOG_INFO("WindowMover shutdown");
}

bool WindowMover::MoveWindowToMonitor(HWND hwnd, HMONITOR targetMonitor) {
    if (!initialized_ || !workspaceManager_) {
        LOG_ERROR("WindowMover not initialized");
        return false;
    }

    if (!hwnd || !IsWindow(hwnd)) {
        LOG_ERROR("Invalid window handle");
        return false;
    }

    if (!CanMoveWindow(hwnd)) {
        LOG_WARN("Window cannot be moved");
        return false;
    }

    HMONITOR fromMonitor = GetWindowMonitor(hwnd);
    if (fromMonitor == targetMonitor) {
        LOG_DEBUG("Window already on target monitor");
        return true;
    }

    // Get current window position
    RECT windowRect = GetWindowRect(hwnd);

    // Calculate new position
    RECT newRect = CalculateNewPosition(windowRect, fromMonitor, targetMonitor);

    // Move the window
    bool result = SetWindowPos(hwnd, NULL,
                               newRect.left, newRect.top,
                               newRect.right - newRect.left,
                               newRect.bottom - newRect.top,
                               SWP_NOZORDER | SWP_SHOWWINDOW) != 0;

    if (result) {
        // Update workspace assignment
        Workspace* workspace = workspaceManager_->GetWorkspaceByMonitor(targetMonitor);
        if (workspace) {
            // Remove from current workspace
            workspaceManager_->RemoveWindowFromWorkspace(hwnd);
            // Add to new workspace
            workspace->AddWindow(hwnd);
        }

        LOG_INFO("Window moved to new monitor");

        // Notify callback
        if (moveCallback_) {
            moveCallback_(hwnd, fromMonitor, targetMonitor);
        }
    }

    return result;
}

bool WindowMover::MoveWindowToWorkspace(HWND hwnd, WorkspaceId targetWorkspace) {
    Workspace* workspace = workspaceManager_->GetWorkspace(targetWorkspace);
    if (!workspace) {
        LOG_ERROR("Target workspace not found");
        return false;
    }

    return MoveWindowToMonitor(hwnd, workspace->GetMonitor());
}

bool WindowMover::MoveWindowToNextMonitor(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) return false;

    // Get all workspaces
    auto workspaces = workspaceManager_->GetAllWorkspaces();
    if (workspaces.empty()) return false;

    HMONITOR currentMonitor = GetWindowMonitor(hwnd);
    
    // Find current workspace index
    int currentIndex = -1;
    for (size_t i = 0; i < workspaces.size(); i++) {
        if (workspaces[i]->GetMonitor() == currentMonitor) {
            currentIndex = (int)i;
            break;
        }
    }

    if (currentIndex == -1) return false;

    // Move to next workspace (wrap around)
    int nextIndex = (currentIndex + 1) % workspaces.size();
    return MoveWindowToMonitor(hwnd, workspaces[nextIndex]->GetMonitor());
}

bool WindowMover::MoveWindowToPreviousMonitor(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) return false;

    auto workspaces = workspaceManager_->GetAllWorkspaces();
    if (workspaces.empty()) return false;

    HMONITOR currentMonitor = GetWindowMonitor(hwnd);
    
    int currentIndex = -1;
    for (size_t i = 0; i < workspaces.size(); i++) {
        if (workspaces[i]->GetMonitor() == currentMonitor) {
            currentIndex = (int)i;
            break;
        }
    }

    if (currentIndex == -1) return false;

    int prevIndex = (currentIndex - 1 + workspaces.size()) % workspaces.size();
    return MoveWindowToMonitor(hwnd, workspaces[prevIndex]->GetMonitor());
}

bool WindowMover::MoveAllWindows(WorkspaceId fromWorkspace, WorkspaceId toWorkspace) {
    Workspace* from = workspaceManager_->GetWorkspace(fromWorkspace);
    Workspace* to = workspaceManager_->GetWorkspace(toWorkspace);

    if (!from || !to) {
        LOG_ERROR("Invalid workspace IDs");
        return false;
    }

    auto windows = from->GetWindows();
    bool allSuccess = true;

    for (const auto& window : windows) {
        if (!MoveWindowToMonitor(window.handle, to->GetMonitor())) {
            allSuccess = false;
        }
    }

    return allSuccess;
}

HMONITOR WindowMover::GetWindowMonitor(HWND hwnd) const {
    if (!hwnd || !IsWindow(hwnd)) return nullptr;
    return MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
}

WorkspaceId WindowMover::GetWindowWorkspace(HWND hwnd) const {
    HMONITOR monitor = GetWindowMonitor(hwnd);
    if (!workspaceManager_) return 0;
    Workspace* workspace = workspaceManager_->GetWorkspaceByMonitor(monitor);
    return workspace ? workspace->GetId() : 0;
}

bool WindowMover::CanMoveWindow(HWND hwnd) const {
    if (!hwnd || !IsWindow(hwnd)) return false;
    
    // Check if window is visible
    if (!IsWindowVisible(hwnd)) return false;

    // Check if window is top-level
    if (GetParent(hwnd) != NULL) return false;

    // Check if window is minimized
    if (IsIconic(hwnd)) return false;

    return true;
}

bool WindowMover::MoveWindowWithPosition(HWND hwnd, HMONITOR targetMonitor, 
                                          int offsetX, int offsetY) {
    if (!MoveWindowToMonitor(hwnd, targetMonitor)) {
        return false;
    }

    // Move by offset
    RECT rect = GetWindowRect(hwnd);
    return SetWindowPos(hwnd, NULL,
                        rect.left + offsetX,
                        rect.top + offsetY,
                        0, 0,
                        SWP_NOSIZE | SWP_NOZORDER) != 0;
}

RECT WindowMover::GetWindowRect(HWND hwnd) const {
    RECT rect = {0, 0, 0, 0};
    if (hwnd && IsWindow(hwnd)) {
        ::GetWindowRect(hwnd, &rect);
    }
    return rect;
}

RECT WindowMover::GetMonitorWorkArea(HMONITOR monitor) const {
    MONITORINFO mi = {sizeof(MONITORINFO)};
    RECT rect = {0, 0, 0, 0};
    if (monitor && GetMonitorInfo(monitor, &mi)) {
        rect = mi.rcWork;
    }
    return rect;
}

POINT WindowMover::GetWindowCenter(HWND hwnd) const {
    RECT rect = GetWindowRect(hwnd);
    POINT center = {
        (rect.left + rect.right) / 2,
        (rect.top + rect.bottom) / 2
    };
    return center;
}

bool WindowMover::IsWindowOnMonitor(HWND hwnd, HMONITOR monitor) const {
    return GetWindowMonitor(hwnd) == monitor;
}

bool WindowMover::IsWindowVisible(HWND hwnd) const {
    return hwnd && IsWindow(hwnd) && ::IsWindowVisible(hwnd) != 0;
}

RECT WindowMover::CalculateNewPosition(const RECT& windowRect,
                                       HMONITOR fromMonitor,
                                       HMONITOR toMonitor) {
    RECT fromRect = GetMonitorWorkArea(fromMonitor);
    RECT toRect = GetMonitorWorkArea(toMonitor);

    // Calculate relative position
    int relX = windowRect.left - fromRect.left;
    int relY = windowRect.top - fromRect.top;

    // Calculate width/height
    int width = windowRect.right - windowRect.left;
    int height = windowRect.bottom - windowRect.top;

    // Calculate new position
    RECT newRect;
    newRect.left = toRect.left + relX;
    newRect.top = toRect.top + relY;
    newRect.right = newRect.left + width;
    newRect.bottom = newRect.top + height;

    // Ensure window is within monitor bounds
    if (newRect.right > toRect.right) {
        newRect.left = toRect.right - width;
        newRect.right = toRect.right;
    }
    if (newRect.bottom > toRect.bottom) {
        newRect.top = toRect.bottom - height;
        newRect.bottom = toRect.bottom;
    }
    if (newRect.left < toRect.left) {
        newRect.left = toRect.left;
        newRect.right = toRect.left + width;
    }
    if (newRect.top < toRect.top) {
        newRect.top = toRect.top;
        newRect.bottom = toRect.top + height;
    }

    return newRect;
}

void WindowMover::SetMoveCallback(MoveCallback callback) {
    moveCallback_ = callback;
}

} // namespace dualdesk