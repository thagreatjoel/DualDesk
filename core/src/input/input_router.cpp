#include "dualdesk/input/input_router.h"
#include "dualdesk/core/logger.h"
#include "dualdesk/core/types.h"
#include <windows.h>

// ============================================================
// ADD THIS - WORKSPACE_UNASSIGNED is defined in types.h
// ============================================================

namespace dualdesk {

InputRouter::InputRouter() {
    LOG_DEBUG("InputRouter created");
}

InputRouter::~InputRouter() {
    Shutdown();
}

bool InputRouter::Initialize(WorkspaceManager* workspaceManager) {
    if (initialized_) {
        LOG_WARN("InputRouter already initialized");
        return true;
    }

    workspaceManager_ = workspaceManager;
    filter_.SetWorkspaceManager(workspaceManager);

    if (!workspaceManager_) {
        LOG_ERROR("WorkspaceManager is null");
        return false;
    }

    initialized_ = true;
    LOG_INFO("InputRouter initialized");
    return true;
}

void InputRouter::Shutdown() {
    if (!initialized_) return;
    initialized_ = false;
    LOG_INFO("InputRouter shutdown");
}

void InputRouter::SetCursorEmulator(CursorEmulator* cursorEmulator) {
    cursorEmulator_ = cursorEmulator;
    LOG_INFO("CursorEmulator set in InputRouter");
}

void InputRouter::SetDeviceWorkspaceMap(const std::map<HANDLE, ULONG>* deviceMap) {
    deviceWorkspaceMap_ = deviceMap;
    LOG_INFO("Device workspace map set in InputRouter");
}

// ============================================================
// FIXED: GetWorkspaceForDevice - uses WORKSPACE_UNASSIGNED from types.h
// ============================================================
ULONG InputRouter::GetWorkspaceForDevice(HANDLE deviceHandle) const {
    if (!deviceWorkspaceMap_) return WORKSPACE_UNASSIGNED;
    
    auto it = deviceWorkspaceMap_->find(deviceHandle);
    if (it != deviceWorkspaceMap_->end()) {
        return it->second;
    }
    return WORKSPACE_UNASSIGNED;
}

// ============================================================
// FIXED: RouteInput - uses event.deviceHandle (added to input_event.h)
// ============================================================
bool InputRouter::RouteInput(const InputEvent& event) {
    processedEventCount_++;

    if (!filter_.ShouldProcessEvent(event)) {
        filteredEventCount_++;
        LOG_DEBUG("Event filtered");
        return false;
    }

    ULONG workspaceId = GetWorkspaceForDevice(event.deviceHandle);
    
    if (workspaceId == WORKSPACE_UNASSIGNED) {
        Workspace* targetWorkspace = filter_.GetTargetWorkspace(event);
        if (targetWorkspace) {
            workspaceId = targetWorkspace->GetId();
        } else {
            LOG_WARN("No target workspace found");
            return false;
        }
    }

    bool result = false;
    
    if (event.type == InputEventType::KeyDown || 
        event.type == InputEventType::KeyUp) {
        result = RouteKeyboardEvent(event);
    } else if (event.type == InputEventType::MouseMove ||
               event.type == InputEventType::MouseButtonDown ||
               event.type == InputEventType::MouseButtonUp ||
               event.type == InputEventType::MouseWheel) {
        result = RouteMouseEvent(event);
    }

    if (result) {
        routedEventCount_++;
    }

    if (routeCallback_) {
        Workspace* workspace = workspaceManager_->GetWorkspace(workspaceId);
        routeCallback_(event, workspace);
    }

    return result;
}

bool InputRouter::RouteKeyboardEvent(const InputEvent& event) {
    ULONG workspaceId = GetWorkspaceForDevice(event.deviceHandle);
    if (workspaceId == WORKSPACE_UNASSIGNED) {
        Workspace* ws = filter_.GetTargetWorkspace(event);
        if (!ws) return false;
        workspaceId = ws->GetId();
    }

    Workspace* workspace = workspaceManager_->GetWorkspace(workspaceId);
    if (!workspace) return false;

    auto windows = workspace->GetWindows();
    if (windows.empty()) {
        LOG_DEBUG("No windows in workspace to receive keyboard input");
        return false;
    }

    HWND targetWindow = NULL;
    HWND foregroundWindow = GetForegroundWindow();
    
    for (const auto& window : windows) {
        if (window.handle == foregroundWindow) {
            targetWindow = foregroundWindow;
            break;
        }
    }

    if (!targetWindow && !windows.empty()) {
        targetWindow = windows[0].handle;
    }

    return SendInputToWindow(event, targetWindow);
}

bool InputRouter::RouteMouseEvent(const InputEvent& event) {
    ULONG workspaceId = GetWorkspaceForDevice(event.deviceHandle);
    if (workspaceId == WORKSPACE_UNASSIGNED) {
        Workspace* ws = filter_.GetTargetWorkspace(event);
        if (!ws) return false;
        workspaceId = ws->GetId();
    }

    if (cursorEmulator_) {
        ULONG realWorkspace = cursorEmulator_->GetRealCursorWorkspace();
        
        if (workspaceId == realWorkspace) {
            return RouteMouseToRealCursor(event, workspaceId);
        } else {
            return RouteMouseToVirtualCursor(event, workspaceId);
        }
    }

    Workspace* workspace = workspaceManager_->GetWorkspace(workspaceId);
    if (!workspace) return false;

    POINT cursorPos;
    GetCursorPos(&cursorPos);
    HWND targetWindow = WindowFromPoint(cursorPos);

    bool isValid = false;
    auto windows = workspace->GetWindows();
    for (const auto& window : windows) {
        if (window.handle == targetWindow) {
            isValid = true;
            break;
        }
    }

    if (!isValid && !windows.empty()) {
        targetWindow = windows[0].handle;
    }

    return SendInputToWindow(event, targetWindow);
}

// ============================================================
// FIXED: RouteMouseToRealCursor - uses event.deltaX/deltaY (added to input_event.h)
// ============================================================
bool InputRouter::RouteMouseToRealCursor(const InputEvent& event, ULONG workspaceId) {
    if (!cursorEmulator_) return false;
    
    switch (event.type) {
        case InputEventType::MouseMove:
            // event.deltaX and event.deltaY are now in input_event.h
            cursorEmulator_->RouteMouseMove(workspaceId, event.deltaX, event.deltaY);
            return true;

        case InputEventType::MouseButtonDown:
            cursorEmulator_->RouteMouseButton(workspaceId, true, true);
            return true;

        case InputEventType::MouseButtonUp:
            cursorEmulator_->RouteMouseButton(workspaceId, true, false);
            return true;

        case InputEventType::MouseWheel:
            cursorEmulator_->RouteMouseWheel(workspaceId, event.wheelDelta);
            return true;

        default:
            return false;
    }
}

bool InputRouter::RouteMouseToVirtualCursor(const InputEvent& event, ULONG workspaceId) {
    if (!cursorEmulator_) return false;
    
    switch (event.type) {
        case InputEventType::MouseMove:
            cursorEmulator_->RouteMouseMove(workspaceId, event.deltaX, event.deltaY);
            return true;

        case InputEventType::MouseButtonDown:
            cursorEmulator_->RouteMouseButton(workspaceId, true, true);
            return true;

        case InputEventType::MouseButtonUp:
            cursorEmulator_->RouteMouseButton(workspaceId, true, false);
            return true;

        case InputEventType::MouseWheel:
            cursorEmulator_->RouteMouseWheel(workspaceId, event.wheelDelta);
            return true;

        default:
            return false;
    }
}

bool InputRouter::SendInputToWindow(const InputEvent& event, HWND targetWindow) {
    if (!targetWindow || !IsWindow(targetWindow)) {
        LOG_DEBUG("Invalid target window");
        return false;
    }

    switch(event.type) {
        case InputEventType::KeyDown:
        case InputEventType::KeyUp:
            return ProcessKeyEvent(event, targetWindow);
        
        case InputEventType::MouseMove:
        case InputEventType::MouseButtonDown:
        case InputEventType::MouseButtonUp:
        case InputEventType::MouseWheel:
            return ProcessMouseEvent(event, targetWindow);
        
        default:
            LOG_DEBUG("Unhandled event type");
            return false;
    }
}

bool InputRouter::ProcessKeyEvent(const InputEvent& event, HWND targetWindow) {
    UINT msg = (event.type == InputEventType::KeyDown) ? WM_KEYDOWN : WM_KEYUP;
    WPARAM wParam = event.keyCode;
    LPARAM lParam = (event.scanCode << 16) | 
                    (event.isExtended ? 0x01000000 : 0) |
                    (event.type == InputEventType::KeyDown ? 0 : 0xC0000000);

    return PostMessage(targetWindow, msg, wParam, lParam) != 0;
}

bool InputRouter::ProcessMouseEvent(const InputEvent& event, HWND targetWindow) {
    POINT pt;
    GetCursorPos(&pt);
    ScreenToClient(targetWindow, &pt);

    UINT msg = 0;
    WPARAM wParam = 0;
    LPARAM lParam = MAKELPARAM(pt.x, pt.y);

    switch(event.type) {
        case InputEventType::MouseMove:
            msg = WM_MOUSEMOVE;
            break;
        case InputEventType::MouseButtonDown:
            msg = WM_LBUTTONDOWN;
            wParam = MK_LBUTTON;
            break;
        case InputEventType::MouseButtonUp:
            msg = WM_LBUTTONUP;
            break;
        case InputEventType::MouseWheel:
            msg = WM_MOUSEWHEEL;
            wParam = MAKEWPARAM(0, event.wheelDelta);
            break;
        default:
            return false;
    }

    return PostMessage(targetWindow, msg, wParam, lParam) != 0;
}

void InputRouter::QueueEvent(const InputEvent& event) {
    eventQueue_.push(event);
}

void InputRouter::ProcessEventQueue() {
    while (!eventQueue_.empty()) {
        InputEvent event = eventQueue_.front();
        eventQueue_.pop();
        RouteInput(event);
    }
}

void InputRouter::ResetStatistics() {
    processedEventCount_ = 0;
    routedEventCount_ = 0;
    filteredEventCount_ = 0;
}

void InputRouter::SetRouteCallback(RouteCallback callback) {
    routeCallback_ = callback;
}

} // namespace dualdesk