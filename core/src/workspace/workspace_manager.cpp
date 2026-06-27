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
    
    std::string msg = "WorkspaceManager initialized with " + std::to_string(workspaces_.size()) + " workspaces";
    LOG_INFO(msg);
    return true;
}

void WorkspaceManager::Shutdown() {
    if (!initialized_) return;
    DestroyWorkspaces();
    initialized_ = false;
    LOG_INFO("WorkspaceManager shutdown");
}

void WorkspaceManager::CreateWorkspaces() {
    workspaces_.clear();
    currentWorkspaceId_ = 0;

    auto monitors = displayManager_->EnumerateDisplays();
    
    if (monitors.empty()) {
        LOG_WARN("No monitors found, creating default workspace");
        WorkspaceId id = GenerateWorkspaceId();
        auto workspace = std::make_unique<Workspace>(id, nullptr);
        workspace->SetName("Default Workspace");
        workspaces_[id] = std::move(workspace);
        currentWorkspaceId_ = id;
        return;
    }

    for (const auto& display : monitors) {
        WorkspaceId id = GenerateWorkspaceId();
        auto workspace = std::make_unique<Workspace>(id, display.monitor);
        std::string name = "Monitor " + std::to_string(display.Width()) + "x" + std::to_string(display.Height());
        workspace->SetName(name);
        workspaces_[id] = std::move(workspace);
    }

    if (!workspaces_.empty()) {
        currentWorkspaceId_ = workspaces_.begin()->first;
        workspaces_[currentWorkspaceId_]->Activate();
        std::string msg = "Active workspace: " + workspaces_[currentWorkspaceId_]->GetName();
        LOG_INFO(msg);
    }

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
        HMONITOR monitor = MonitorFromWindow(window.handle, MONITOR_DEFAULTTONEAREST);
        Workspace* workspace = GetWorkspaceByMonitor(monitor);
        if (workspace) {
            workspace->AddWindow(window.handle);
        }
    }
    
    std::string msg = "Assigned " + std::to_string(windows.size()) + " windows to workspaces";
    LOG_INFO(msg);
}

void WorkspaceManager::AssignWindowToWorkspace(HWND hwnd, WorkspaceId workspaceId) {
    Workspace* workspace = GetWorkspace(workspaceId);
    if (!workspace) {
        std::string msg = "Workspace " + std::to_string(workspaceId) + " not found";
        LOG_WARN(msg);
        return;
    }

    for (auto& pair : workspaces_) {
        if (pair.second->HasWindow(hwnd)) {
            pair.second->RemoveWindow(hwnd);
        }
    }

    workspace->AddWindow(hwnd);
    std::string msg = "Window assigned to workspace " + std::to_string(workspaceId);
    LOG_DEBUG(msg);
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
    if (!IsWindow(hwnd)) {
        RemoveWindowFromWorkspace(hwnd);
        return;
    }

    HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    Workspace* workspace = GetWorkspaceByMonitor(monitor);
    
    if (!workspace) {
        LOG_WARN("No workspace for window");
        return;
    }

    for (auto& pair : workspaces_) {
        if (pair.second->HasWindow(hwnd)) {
            if (pair.second.get() == workspace) {
                return;
            }
            pair.second->RemoveWindow(hwnd);
        }
    }

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

bool WorkspaceManager::SwitchToWorkspace(WorkspaceId id) {
    Workspace* workspace = GetWorkspace(id);
    if (!workspace) {
        std::string msg = "Workspace " + std::to_string(id) + " not found";
        LOG_WARN(msg);
        return false;
    }

    WorkspaceId oldId = currentWorkspaceId_;
    
    Workspace* oldWorkspace = GetActiveWorkspace();
    if (oldWorkspace) {
        oldWorkspace->Deactivate();
    }

    workspace->Activate();
    currentWorkspaceId_ = id;
    workspace->BringToFront();

    std::string msg = "Switched to workspace: " + workspace->GetName();
    LOG_INFO(msg);

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
        std::string msg = "Workspace " + std::to_string(workspaceId) + " not found";
        LOG_WARN(msg);
        return;
    }

    for (auto& pair : workspaces_) {
        if (pair.second->GetKeyboardId() == keyboardId) {
            pair.second->AssignKeyboard(0);
        }
    }

    workspace->AssignKeyboard(keyboardId);
    std::string msg = "Keyboard " + std::to_string(keyboardId) + " assigned to workspace " + std::to_string(workspaceId);
    LOG_INFO(msg);
}

void WorkspaceManager::AssignMouseToWorkspace(DeviceId mouseId, WorkspaceId workspaceId) {
    Workspace* workspace = GetWorkspace(workspaceId);
    if (!workspace) {
        std::string msg = "Workspace " + std::to_string(workspaceId) + " not found";
        LOG_WARN(msg);
        return;
    }

    for (auto& pair : workspaces_) {
        if (pair.second->GetMouseId() == mouseId) {
            pair.second->AssignMouse(0);
        }
    }

    workspace->AssignMouse(mouseId);
    std::string msg = "Mouse " + std::to_string(mouseId) + " assigned to workspace " + std::to_string(workspaceId);
    LOG_INFO(msg);
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
    auto windows = windowTracker_.GetAllWindows();
    for (const auto& window : windows) {
        UpdateWindowAssignment(window.handle);
    }
    LOG_DEBUG("WorkspaceManager updated");
}

void WorkspaceManager::TrackWindows() {
    // Placeholder for future window tracking
}

Workspace* WorkspaceManager::FindWorkspaceForWindow(HWND hwnd) {
    for (auto& pair : workspaces_) {
        if (pair.second->HasWindow(hwnd)) {
            return pair.second.get();
        }
    }
    return nullptr;
}

} // namespace dualdesk