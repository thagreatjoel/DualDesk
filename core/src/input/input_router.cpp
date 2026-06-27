#include "dualdesk/input/input_router.h"
#include "dualdesk/core/logger.h"
#include <windows.h>

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

bool InputRouter::RouteInput(const InputEvent& event) {
    processedEventCount_++;

    // Filter the event
    if (!filter_.ShouldProcessEvent(event)) {
        filteredEventCount_++;
        LOG_DEBUG("Event filtered");
        return false;
    }

    // Find target workspace
    Workspace* targetWorkspace = filter_.GetTargetWorkspace(event);
    if (!targetWorkspace) {
        LOG_WARN("No target workspace found");
        return false;
    }

    // Route based on event type
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

    // Callback
    if (routeCallback_) {
        routeCallback_(event, targetWorkspace);
    }

    return result;
}

bool InputRouter::RouteKeyboardEvent(const InputEvent& event) {
    Workspace* workspace = filter_.GetTargetWorkspace(event);
    if (!workspace) return false;

    // Get all windows in this workspace and send to active one
    auto windows = workspace->GetWindows();
    if (windows.empty()) {
        LOG_DEBUG("No windows in workspace to receive keyboard input");
        return false;
    }

    // Send to the foreground window in this workspace
    HWND targetWindow = GetForegroundWindow();
    
    // Check if foreground window belongs to this workspace
    bool isValid = false;
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

bool InputRouter::RouteMouseEvent(const InputEvent& event) {
    Workspace* workspace = filter_.GetTargetWorkspace(event);
    if (!workspace) return false;

    // Get window under cursor
    POINT cursorPos;
    GetCursorPos(&cursorPos);
    HWND targetWindow = WindowFromPoint(cursorPos);

    // Check if window belongs to this workspace
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

    // Try to post the message
    return PostMessage(targetWindow, msg, wParam, lParam) != 0;
}

bool InputRouter::ProcessMouseEvent(const InputEvent& event, HWND targetWindow) {
    // Get cursor position
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