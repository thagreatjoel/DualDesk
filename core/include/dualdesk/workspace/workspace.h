#pragma once

#include "../core/types.h"
#include "window_info.h"
#include <windows.h>
#include <vector>
#include <string>
#include <unordered_set>

namespace dualdesk {

class Workspace {
public:
    Workspace(WorkspaceId id, const std::string& name, HMONITOR monitor);
    ~Workspace();

    // ============================================================
    // GETTERS
    // ============================================================
    WorkspaceId GetId() const { return id_; }
    HMONITOR GetMonitor() const { return monitor_; }
    std::string GetName() const { return name_; }
    void SetName(const std::string& name) { name_ = name; }
    char GetLetter() const { return 'A' + (char)id_; }
    
    // ============================================================
    // MONITOR BOUNDS
    // ============================================================
    RECT GetMonitorBounds() const;

    // ============================================================
    // WINDOW MANAGEMENT
    // ============================================================
    void AddWindow(HWND hwnd);
    void RemoveWindow(HWND hwnd);
    bool HasWindow(HWND hwnd) const;
    std::vector<WindowInfo> GetWindows() const;
    size_t GetWindowCount() const { return windows_.size(); }
    void ClearWindows();
    
    // ============================================================
    // DEVICE ASSIGNMENT
    // ============================================================
    void AssignKeyboard(DeviceId keyboardId) { keyboardId_ = keyboardId; }
    void AssignMouse(DeviceId mouseId) { mouseId_ = mouseId; }
    DeviceId GetKeyboardId() const { return keyboardId_; }
    DeviceId GetMouseId() const { return mouseId_; }
    bool HasKeyboard() const { return keyboardId_ != 0; }
    bool HasMouse() const { return mouseId_ != 0; }

    // ============================================================
    // WORKSPACE STATE
    // ============================================================
    bool IsActive() const { return isActive_; }
    void Activate();
    void Deactivate();

    // ============================================================
    // WINDOW OPERATIONS
    // ============================================================
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