#include "dualdesk/input/input_filter.h"
#include "dualdesk/core/logger.h"
#include <windows.h>

namespace dualdesk {

InputFilter::InputFilter() {
    LOG_DEBUG("InputFilter created");
}

InputFilter::~InputFilter() {
    LOG_DEBUG("InputFilter destroyed");
}

void InputFilter::SetWorkspaceManager(WorkspaceManager* manager) {
    workspaceManager_ = manager;
    LOG_DEBUG("WorkspaceManager set in InputFilter");
}

bool InputFilter::ShouldProcessEvent(const InputEvent& event) const {
    if (!workspaceManager_) {
        return true;
    }

    if (!IsDeviceInActiveWorkspace(event.deviceId)) {
        return false;
    }

    if (filterMouseMovement_ && event.type == InputEventType::MouseMove) {
        return false;
    }

    if (filterSystemKeys_ && IsSystemKey(event.keyCode)) {
        return false;
    }

    return true;
}

bool InputFilter::ShouldProcessKeyEvent(const InputEvent& event) const {
    if (!workspaceManager_) return true;
    if (!IsDeviceInActiveWorkspace(event.deviceId)) return false;
    if (filterSystemKeys_ && IsSystemKey(event.keyCode)) return false;
    return true;
}

bool InputFilter::ShouldProcessMouseEvent(const InputEvent& event) const {
    if (!workspaceManager_) return true;
    if (!IsDeviceInActiveWorkspace(event.deviceId)) return false;
    if (filterMouseMovement_ && event.type == InputEventType::MouseMove) return false;
    return true;
}

Workspace* InputFilter::GetTargetWorkspace(const InputEvent& event) const {
    if (!workspaceManager_) return nullptr;

    Workspace* active = workspaceManager_->GetActiveWorkspace();
    if (!active) return nullptr;

    if (active->GetKeyboardId() == event.deviceId ||
        active->GetMouseId() == event.deviceId) {
        return active;
    }

    for (auto* workspace : workspaceManager_->GetAllWorkspaces()) {
        if (workspace->GetKeyboardId() == event.deviceId ||
            workspace->GetMouseId() == event.deviceId) {
            return workspace;
        }
    }

    return active;
}

bool InputFilter::IsDeviceInActiveWorkspace(DeviceId deviceId) const {
    if (!workspaceManager_) return true;
    Workspace* active = workspaceManager_->GetActiveWorkspace();
    if (!active) return false;
    return active->GetKeyboardId() == deviceId ||
           active->GetMouseId() == deviceId;
}

bool InputFilter::IsDeviceInWorkspace(DeviceId deviceId, Workspace* workspace) const {
    if (!workspace) return false;
    return workspace->GetKeyboardId() == deviceId ||
           workspace->GetMouseId() == deviceId;
}

bool InputFilter::IsSystemKey(uint16_t keyCode) const {
    return keyCode == VK_LWIN || keyCode == VK_RWIN ||
           keyCode == VK_SNAPSHOT || keyCode == VK_CANCEL ||
           keyCode == VK_ATTN || keyCode == VK_CRSEL ||
           keyCode == VK_EXSEL || keyCode == VK_PAUSE ||
           keyCode == VK_PACKET || keyCode == VK_OEM_CLEAR;
}

} // namespace dualdesk