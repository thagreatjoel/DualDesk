#pragma once

#include "workspace.h"
#include "window_tracker.h"
#include "../display/display_manager.h"
#include "../input/input_manager.h"
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>

namespace dualdesk {

/**
 * @brief Manages all workspaces and their windows
 * 
 * The WorkspaceManager creates one workspace per monitor and
 * handles window assignment, switching, and input routing.
 */
class WorkspaceManager {
public:
    WorkspaceManager();
    ~WorkspaceManager();

    // Initialize with display and input managers
    bool Initialize(DisplayManager* displayManager, InputManager* inputManager);
    void Shutdown();

    // Workspace management
    void CreateWorkspaces();
    void DestroyWorkspaces();
    Workspace* GetWorkspace(WorkspaceId id);
    Workspace* GetWorkspaceByMonitor(HMONITOR monitor);
    Workspace* GetActiveWorkspace();
    std::vector<Workspace*> GetAllWorkspaces();

    // Window management
    void AssignWindowToWorkspace(HWND hwnd, WorkspaceId workspaceId);
    void AssignWindowToMonitorWorkspace(HWND hwnd, HMONITOR monitor);
    void UpdateWindowAssignment(HWND hwnd);
    void RemoveWindowFromWorkspace(HWND hwnd);

    // Workspace switching
    bool SwitchToWorkspace(WorkspaceId id);
    bool SwitchToWorkspace(HMONITOR monitor);
    bool SwitchToNextWorkspace();
    bool SwitchToPreviousWorkspace();

    // Input device assignment
    void AssignKeyboardToWorkspace(DeviceId keyboardId, WorkspaceId workspaceId);
    void AssignMouseToWorkspace(DeviceId mouseId, WorkspaceId workspaceId);
    Workspace* GetWorkspaceForDevice(DeviceId deviceId);

    // Event callbacks
    using WorkspaceChangeCallback = std::function<void(WorkspaceId oldId, WorkspaceId newId)>;
    void SetWorkspaceChangeCallback(WorkspaceChangeCallback callback);

    // Update workspaces (call on monitor changes or window changes)
    void Update();

private:
    // Helper functions
    WorkspaceId GenerateWorkspaceId();
    void TrackWindows();
    void AssignWindowsToWorkspaces();
    Workspace* FindWorkspaceForWindow(HWND hwnd);

    // Members
    std::unordered_map<WorkspaceId, std::unique_ptr<Workspace>> workspaces_;
    DisplayManager* displayManager_ = nullptr;
    InputManager* inputManager_ = nullptr;
    WindowTracker windowTracker_;
    WorkspaceId currentWorkspaceId_ = 0;
    WorkspaceId nextWorkspaceId_ = 1;
    WorkspaceChangeCallback workspaceChangeCallback_;
    bool initialized_ = false;
};

} // namespace dualdesk