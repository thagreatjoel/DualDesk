#include "dualdesk/workspace/workspace.h"
#include "dualdesk/core/logger.h"
#include <sstream>

namespace dualdesk {

Workspace::Workspace(WorkspaceId id, HMONITOR monitor)
    : id_(id), monitor_(monitor) {
    name_ = "Workspace " + std::to_string(id);
    LOG_DEBUG("Workspace created: " + name_);
}

Workspace::~Workspace() {
    LOG_DEBUG("Workspace destroyed: " + name_);
}

void Workspace::AddWindow(HWND hwnd) {
    if (hwnd == nullptr) return;
    windows_.insert(hwnd);
    std::string msg = "Window added to workspace " + name_ + ": " + std::to_string((long long)hwnd);
    LOG_DEBUG(msg);
}

void Workspace::RemoveWindow(HWND hwnd) {
    if (hwnd == nullptr) return;
    windows_.erase(hwnd);
    std::string msg = "Window removed from workspace " + name_ + ": " + std::to_string((long long)hwnd);
    LOG_DEBUG(msg);
}

bool Workspace::HasWindow(HWND hwnd) const {
    return windows_.find(hwnd) != windows_.end();
}

std::vector<WindowInfo> Workspace::GetWindows() const {
    std::vector<WindowInfo> result;
    for (HWND hwnd : windows_) {
        WindowInfo info;
        info.handle = hwnd;
        
        wchar_t title[256];
        if (GetWindowTextW(hwnd, title, 256)) {
            info.title = title;
        }
        
        wchar_t className[256];
        if (GetClassNameW(hwnd, className, 256)) {
            info.className = className;
        }
        
        GetWindowRect(hwnd, &info.position);
        info.isVisible = IsWindowVisible(hwnd);
        info.isMinimized = IsIconic(hwnd) != 0;
        info.isMaximized = IsZoomed(hwnd) != 0;
        
        result.push_back(info);
    }
    return result;
}

void Workspace::ClearWindows() {
    windows_.clear();
    LOG_DEBUG("All windows cleared from workspace " + name_);
}

void Workspace::Activate() {
    if (isActive_) return;
    isActive_ = true;
    LOG_INFO("Workspace activated: " + name_);
}

void Workspace::Deactivate() {
    if (!isActive_) return;
    isActive_ = false;
    LOG_INFO("Workspace deactivated: " + name_);
}

void Workspace::MoveAllWindowsToWorkspace(const Workspace& target) {
    LOG_INFO("Moving all windows from " + name_ + " to " + target.GetName());
}

void Workspace::BringToFront() {
    for (HWND hwnd : windows_) {
        if (IsWindow(hwnd) && IsWindowVisible(hwnd)) {
            SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0, 
                        SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
        }
    }
    LOG_INFO("Workspace brought to front: " + name_);
}

} // namespace dualdesk