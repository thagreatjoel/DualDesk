#pragma once

#include "input_event.h"
#include "../workspace/workspace_manager.h"
#include <functional>

namespace dualdesk {

/**
 * @brief Filters input events based on workspace assignments
 * 
 * The InputFilter determines which input events should be processed
 * based on the currently active workspace and device assignments.
 */
class InputFilter {
public:
    InputFilter();
    ~InputFilter();

    // Set workspace manager reference
    void SetWorkspaceManager(WorkspaceManager* manager);

    // Filter input events
    bool ShouldProcessEvent(const InputEvent& event) const;
    bool ShouldProcessKeyEvent(const InputEvent& event) const;
    bool ShouldProcessMouseEvent(const InputEvent& event) const;

    // Filter settings
    void SetFilterMouseMovement(bool filter) { filterMouseMovement_ = filter; }
    void SetFilterSystemKeys(bool filter) { filterSystemKeys_ = filter; }
    bool IsFilteringMouseMovement() const { return filterMouseMovement_; }
    bool IsFilteringSystemKeys() const { return filterSystemKeys_; }

    // Get target workspace for an event
    Workspace* GetTargetWorkspace(const InputEvent& event) const;

private:
    // Check if device is assigned to active workspace
    bool IsDeviceInActiveWorkspace(DeviceId deviceId) const;
    bool IsDeviceInWorkspace(DeviceId deviceId, Workspace* workspace) const;

    // Check if key is a system key
    bool IsSystemKey(uint16_t keyCode) const;

    WorkspaceManager* workspaceManager_ = nullptr;
    bool filterMouseMovement_ = true;
    bool filterSystemKeys_ = true;
};

} // namespace dualdesk