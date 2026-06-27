#include "dualdesk/workspace/workspace_manager.h"
#include "dualdesk/core/logger.h"
#include <algorithm>

namespace dualdesk {

WorkspaceManager::WorkspaceManager() {
    LOG_DEBUG("WorkspaceManager created");
}

WorkspaceManager::~WorkspaceManager() {
    Shutdown();
}

bool WorkspaceManager::Initialize(DisplayManager* displayManager, InputManager* inputManager) {
    if (initialized_) {
        LOG_WARN("WorkspaceManager already initialized");
        return true;
    }

    displayManager_ = displayManager;
    inputManager_ = inputManager;

    if (!displayManager_) {
        LOG_ERROR("DisplayManager is null");
        return false;
    }

    CreateWorkspaces();
    initialized_ = true;
    LOG_INFO("WorkspaceManager initialized with {} workspaces", workspaces_.size());
    return true;
}

void WorkspaceManager::Shutdown() {
    if (!initialized_) return;
    DestroyWorkspaces();
    initialized_ = false;
    LOG_INFO("WorkspaceManager shutdown");
}

void WorkspaceManager::CreateWorkspaces() {
    // Clear existing workspaces
    workspaces_.clear();
    currentWorkspaceId_ = 0;

    // Get all monitors
    auto monitors = displayManager_->EnumerateDisplays();
    
    if (monitors.empty()) {
        LOG_WARN("No monitors found, creating default workspace");
        WorkspaceId id = GenerateWorkspaceId();
        auto workspace = std::make_unique<Workspace>(id, nullptr);
        workspaces_[id] = std::move(workspace);
        currentWorkspaceId_ = id;
        return;
    }

    // Create one workspace per monitor
    for (const auto& display : monitors) {
        WorkspaceId id = GenerateWorkspaceId();
        auto workspace = std::make_unique<Workspace>(id, display.monitor);
        workspace->SetName("Monitor " + std::to_string(display.Width()) + "x" + 
                          std::to_string(display.Height()));
        workspaces_[id] = std::move(workspace);
    }

    // Set first workspace as active
    if (!workspaces_.empty()) {
        currentWorkspaceId_ = workspaces_.begin()->first;
        workspaces_[currentWorkspaceId_]->Activate();
        LOG_INFO("Active workspace: {}", workspaces_[currentWorkspaceId_]->GetName());
    }

    // Assign windows to workspaces
    AssignWindowsToWorkspaces();
}

void WorkspaceManager::DestroyWorkspaces() {
    for (auto& pair : workspaces_) {
        pair.second->Deactivate();
        pair.second->ClearWindows();
    }
    workspaces_.clear();
    currentWorkspaceId_ = 0;
    LOG_DEBUG("All workspaces destroyed");
}

Workspace* WorkspaceManager::GetWorkspace(WorkspaceId id) {
    auto it = workspaces_.find(id);
    return (it != workspaces_.end()) ? it->second.get() : nullptr;
}

Workspace* WorkspaceManager::GetWorkspaceByMonitor(HMONITOR monitor) {
    for (auto& pair : workspaces_) {
        if (pair.second->GetMonitor() == monitor) {
            return pair.second.get();
        }
    }
    return nullptr;
}

Workspace* WorkspaceManager::GetActiveWorkspace() {
    return GetWorkspace(currentWorkspaceId_);
}

std::vector<Workspace*> WorkspaceManager::GetAllWorkspaces() {
    std::vector<Workspace*> result;
    for (auto& pair : workspaces_) {
        result.push_back(pair.second.get());
    }
    return result;
}

WorkspaceId WorkspaceManager::GenerateWorkspaceId() {
    return nextWorkspaceId_++;
}

void WorkspaceManager::AssignWindowsToWorkspaces() {
    auto windows = windowTracker_.GetAllWindows();
    
    for (const auto& window : windows) {
        // Determine which monitor this window is on
        HMONITOR monitor = MonitorFromWindow(window.handle, MONITOR_DEFAULTTONEAREST);
        
        // Find workspace for this monitor
        Workspace* workspace = GetWorkspaceByMonitor(monitor);
        if (workspace) {
            workspace->AddWindow(window.handle);
        }
    }
    
    LOG_INFO("Assigned {} windows to workspaces", windows.size());
}

void WorkspaceManager::AssignWindowToWorkspace(HWND hwnd, WorkspaceId workspaceId) {
    Workspace* workspace = GetWorkspace(workspaceId);
    if (!workspace) {
        LOG_WARN("Workspace {} not found", workspaceId);
        return;
    }

    // Remove from current workspace
    for (auto& pair : workspaces_) {
        if (pair.second->HasWindow(hwnd)) {
            pair.second->RemoveWindow(hwnd);
        }
    }

    // Add to target workspace
    workspace->AddWindow(hwnd);
    LOG_DEBUG("Window assigned to workspace {}", workspaceId);
}

void WorkspaceManager::AssignWindowToMonitorWorkspace(HWND hwnd, HMONITOR monitor) {
    Workspace* workspace = GetWorkspaceByMonitor(monitor);
    if (!workspace) {
        LOG_WARN("No workspace found for monitor");
        return;
    }
    AssignWindowToWorkspace(hwnd, workspace->GetId());
}

void WorkspaceManager::UpdateWindowAssignment(HWND hwnd) {
    // Check if window still exists
    if (!IsWindow(hwnd)) {
        RemoveWindowFromWorkspace(hwnd);
        return;
    }

    // Get current monitor
    HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    Workspace* workspace = GetWorkspaceByMonitor(monitor);
    
    if (!workspace) {
        LOG_WARN("No workspace for window");
        return;
    }

    // Check if window is already in a workspace
    for (auto& pair : workspaces_) {
        if (pair.second->HasWindow(hwnd)) {
            // If it's the same workspace, do nothing
            if (pair.second.get() == workspace) {
                return;
            }
            // Otherwise, move it
            pair.second->RemoveWindow(hwnd);
        }
    }

    // Add to new workspace
    workspace->AddWindow(hwnd);
}

void WorkspaceManager::RemoveWindowFromWorkspace(HWND hwnd) {
    for (auto& pair : workspaces_) {
        if (pair.second->HasWindow(hwnd)) {
            pair.second->RemoveWindow(hwnd);
            LOG_DEBUG("Window removed from workspace");
            return;
        }
    }
}

void WorkspaceManager::TrackWindows() {
    // Track window creation/destruction
    // This will be expanded in Phase 8
}

bool WorkspaceManager::SwitchToWorkspace(WorkspaceId id) {
    Workspace* workspace = GetWorkspace(id);
    if (!workspace) {
        LOG_WARN("Workspace {} not found", id);
        return false;
    }

    WorkspaceId oldId = currentWorkspaceId_;
    
    // Deactivate current workspace
    Workspace* oldWorkspace = GetActiveWorkspace();
    if (oldWorkspace) {
        oldWorkspace->Deactivate();
    }

    // Activate new workspace
    workspace->Activate();
    currentWorkspaceId_ = id;

    // Bring workspace windows to front
    workspace->BringToFront();

    LOG_INFO("Switched to workspace: {}", workspace->GetName());

    // Notify callback
    if (workspaceChangeCallback_) {
        workspaceChangeCallback_(oldId, id);
    }

    return true;
}

bool WorkspaceManager::SwitchToWorkspace(HMONITOR monitor) {
    Workspace* workspace = GetWorkspaceByMonitor(monitor);
    if (!workspace) {
        LOG_WARN("No workspace found for monitor");
        return false;
    }
    return SwitchToWorkspace(workspace->GetId());
}

bool WorkspaceManager::SwitchToNextWorkspace() {
    if (workspaces_.empty()) return false;

    auto it = workspaces_.find(currentWorkspaceId_);
    if (it == workspaces_.end()) {
        it = workspaces_.begin();
    } else {
        ++it;
        if (it == workspaces_.end()) {
            it = workspaces_.begin();
        }
    }

    return SwitchToWorkspace(it->first);
}

bool WorkspaceManager::SwitchToPreviousWorkspace() {
    if (workspaces_.empty()) return false;

    auto it = workspaces_.find(currentWorkspaceId_);
    if (it == workspaces_.end() || it == workspaces_.begin()) {
        it = workspaces_.end();
        --it;
    } else {
        --it;
    }

    return SwitchToWorkspace(it->first);
}

void WorkspaceManager::AssignKeyboardToWorkspace(DeviceId keyboardId, WorkspaceId workspaceId) {
    Workspace* workspace = GetWorkspace(workspaceId);
    if (!workspace) {
        LOG_WARN("Workspace {} not found", workspaceId);
        return;
    }

    // Remove keyboard from current workspace
    for (auto& pair : workspaces_) {
        if (pair.second->GetKeyboardId() == keyboardId) {
            pair.second->AssignKeyboard(0);
        }
    }

    workspace->AssignKeyboard(keyboardId);
    LOG_INFO("Keyboard {} assigned to workspace {}", keyboardId, workspaceId);
}

void WorkspaceManager::AssignMouseToWorkspace(DeviceId mouseId, WorkspaceId workspaceId) {
    Workspace* workspace = GetWorkspace(workspaceId);
    if (!workspace) {
        LOG_WARN("Workspace {} not found", workspaceId);
        return;
    }

    // Remove mouse from current workspace
    for (auto& pair : workspaces_) {
        if (pair.second->GetMouseId() == mouseId) {
            pair.second->AssignMouse(0);
        }
    }

    workspace->AssignMouse(mouseId);
    LOG_INFO("Mouse {} assigned to workspace {}", mouseId, workspaceId);
}

Workspace* WorkspaceManager::GetWorkspaceForDevice(DeviceId deviceId) {
    for (auto& pair : workspaces_) {
        if (pair.second->GetKeyboardId() == deviceId ||
            pair.second->GetMouseId() == deviceId) {
            return pair.second.get();
        }
    }
    return nullptr;
}

void WorkspaceManager::SetWorkspaceChangeCallback(WorkspaceChangeCallback callback) {
    workspaceChangeCallback_ = callback;
}

void WorkspaceManager::Update() {
    // Update window assignments
    auto windows = windowTracker_.GetAllWindows();
    for (const auto& window : windows) {
        UpdateWindowAssignment(window.handle);
    }
    LOG_DEBUG("WorkspaceManager updated");
}

} // namespace dualdesk