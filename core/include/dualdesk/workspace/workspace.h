#pragma once

#include "../core/types.h"
#include "window_info.h"
#include <windows.h>
#include <vector>
#include <string>
#include <unordered_set>

namespace dualdesk {

/**
 * @brief Represents a single workspace on a monitor
 * 
 * A workspace contains a collection of windows that belong to a specific monitor.
 * Each workspace has its own input devices (keyboard + mouse) assigned.
 */
class Workspace {
public:
    Workspace(WorkspaceId id, HMONITOR monitor);
    ~Workspace();

    // Basic properties
    WorkspaceId GetId() const { return id_; }
    HMONITOR GetMonitor() const { return monitor_; }
    std::string GetName() const { return name_; }
    void SetName(const std::string& name) { name_ = name; }

    // Window management
    void AddWindow(HWND hwnd);
    void RemoveWindow(HWND hwnd);
    bool HasWindow(HWND hwnd) const;
    std::vector<WindowInfo> GetWindows() const;
    size_t GetWindowCount() const { return windows_.size(); }
    void ClearWindows();

    // Input device assignment
    void AssignKeyboard(DeviceId keyboardId) { keyboardId_ = keyboardId; }
    void AssignMouse(DeviceId mouseId) { mouseId_ = mouseId; }
    DeviceId GetKeyboardId() const { return keyboardId_; }
    DeviceId GetMouseId() const { return mouseId_; }
    bool HasKeyboard() const { return keyboardId_ != 0; }
    bool HasMouse() const { return mouseId_ != 0; }

    // Workspace state
    bool IsActive() const { return isActive_; }
    void Activate();
    void Deactivate();

    // Window management helpers
    void MoveAllWindowsToWorkspace(const Workspace& target);
    void BringToFront();

private:
    WorkspaceId id_;
    HMONITOR monitor_;
    std::string name_;
    std::unordered_set<HWND> windows_;
    DeviceId keyboardId_ = 0;
    DeviceId mouseId_ = 0;
    bool isActive_ = false;
};

} // namespace dualdesk