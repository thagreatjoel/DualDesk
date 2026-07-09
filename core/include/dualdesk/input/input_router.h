#pragma once

#include "input_event.h"
#include "input_filter.h"
#include "input_translator.h"
#include "../workspace/workspace_manager.h"
#include "../cursor/cursor_emulator.h"
#include <functional>
#include <queue>
#include <map>

namespace dualdesk {

class InputRouter {
public:
    InputRouter();
    ~InputRouter();

    bool Initialize(WorkspaceManager* workspaceManager);
    void Shutdown();

    void SetCursorEmulator(CursorEmulator* cursorEmulator);
    void SetDeviceWorkspaceMap(const std::map<HANDLE, ULONG>* deviceMap);

    bool RouteInput(const InputEvent& event);
    bool RouteKeyboardEvent(const InputEvent& event);
    bool RouteMouseEvent(const InputEvent& event);

    using RouteCallback = std::function<bool(const InputEvent&, Workspace*)>;
    void SetRouteCallback(RouteCallback callback);

    InputFilter* GetFilter() { return &filter_; }

    size_t GetProcessedEventCount() const { return processedEventCount_; }
    size_t GetRoutedEventCount() const { return routedEventCount_; }
    size_t GetFilteredEventCount() const { return filteredEventCount_; }
    void ResetStatistics();

    void QueueEvent(const InputEvent& event);
    void ProcessEventQueue();
    bool HasPendingEvents() const { return !eventQueue_.empty(); }
    size_t GetQueueSize() const { return eventQueue_.size(); }

    ULONG GetWorkspaceForDevice(HANDLE deviceHandle) const;

private:
    HWND FindTargetWindow(const InputEvent& event) const;
    bool SendInputToWindow(const InputEvent& event, HWND targetWindow);
    bool ProcessKeyEvent(const InputEvent& event, HWND targetWindow);
    bool ProcessMouseEvent(const InputEvent& event, HWND targetWindow);
    bool RouteMouseToRealCursor(const InputEvent& event, ULONG workspaceId);
    bool RouteMouseToVirtualCursor(const InputEvent& event, ULONG workspaceId);

    WorkspaceManager* workspaceManager_ = nullptr;
    CursorEmulator* cursorEmulator_ = nullptr;
    const std::map<HANDLE, ULONG>* deviceWorkspaceMap_ = nullptr;
    
    InputFilter filter_;
    InputTranslator translator_;
    RouteCallback routeCallback_;
    
    std::queue<InputEvent> eventQueue_;
    size_t processedEventCount_ = 0;
    size_t routedEventCount_ = 0;
    size_t filteredEventCount_ = 0;
    bool initialized_ = false;
    
    ULONG realCursorWorkspace_ = 0;
};

} // namespace dualdesk