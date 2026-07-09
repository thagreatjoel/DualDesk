#include "dualdesk/workspace/workspace.h"
#include "dualdesk/core/logger.h"
#include <algorithm>

namespace dualdesk {

Workspace::Workspace(WorkspaceId id, const std::string& name, HMONITOR monitor)
    : id_(id), monitor_(monitor), name_(name) {
    LOG_DEBUG("Workspace created: %s (ID: %d)", name_.c_str(), id_);
}

Workspace::~Workspace() {
    LOG_DEBUG("Workspace destroyed: %s (ID: %d)", name_.c_str(), id_);
}

void Workspace::AddWindow(HWND hwnd) {
    if (!hwnd) return;
    if (windows_.find(hwnd) != windows_.end()) return;
    windows_.insert(hwnd);
    LOG_DEBUG("Window added to workspace %s", name_.c_str());
}

void Workspace::RemoveWindow(HWND hwnd) {
    if (!hwnd) return;
    auto it = windows_.find(hwnd);
    if (it != windows_.end()) {
        windows_.erase(it);
        LOG_DEBUG("Window removed from workspace %s", name_.c_str());
    }
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
        result.push_back(info);
    }
    return result;
}

void Workspace::ClearWindows() {
    windows_.clear();
    LOG_DEBUG("All windows cleared from workspace %s", name_.c_str());
}

void Workspace::Activate() {
    isActive_ = true;
    LOG_DEBUG("Workspace activated: %s", name_.c_str());
}

void Workspace::Deactivate() {
    isActive_ = false;
    LOG_DEBUG("Workspace deactivated: %s", name_.c_str());
}

void Workspace::MoveAllWindowsToWorkspace(const Workspace& target) {
    LOG_WARN("MoveAllWindowsToWorkspace not implemented");
}

void Workspace::BringToFront() {
    for (HWND hwnd : windows_) {
        SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0, 
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }
    LOG_DEBUG("Workspace brought to front: %s", name_.c_str());
}

// ============================================================
// GetMonitorBounds implementation
// ============================================================
RECT Workspace::GetMonitorBounds() const {
    RECT bounds = {0, 0, 0, 0};
    if (monitor_) {
        MONITORINFOEXW info;
        info.cbSize = sizeof(MONITORINFOEXW);
        if (GetMonitorInfoW(monitor_, &info)) {
            bounds = info.rcMonitor;
        }
    }
    return bounds;
}

} // namespace dualdesk