#pragma once

#include "input_event.h"
#include "input_filter.h"
#include "input_translator.h"
#include "../workspace/workspace_manager.h"
#include <functional>
#include <queue>

namespace dualdesk {

/**
 * @brief Routes input events to the correct workspace
 * 
 * The InputRouter takes raw input events, filters them,
 * translates them if needed, and routes them to the target window.
 */
class InputRouter {
public:
    InputRouter();
    ~InputRouter();

    // Initialize with managers
    bool Initialize(WorkspaceManager* workspaceManager);
    void Shutdown();

    // Route an input event
    bool RouteInput(const InputEvent& event);
    bool RouteKeyboardEvent(const InputEvent& event);
    bool RouteMouseEvent(const InputEvent& event);

    // Set callbacks
    using RouteCallback = std::function<bool(const InputEvent&, Workspace*)>;
    void SetRouteCallback(RouteCallback callback);

    // Get the filter
    InputFilter* GetFilter() { return &filter_; }

    // Statistics
    size_t GetProcessedEventCount() const { return processedEventCount_; }
    size_t GetRoutedEventCount() const { return routedEventCount_; }
    size_t GetFilteredEventCount() const { return filteredEventCount_; }
    void ResetStatistics();

    // Event queue
    void QueueEvent(const InputEvent& event);
    void ProcessEventQueue();
    bool HasPendingEvents() const { return !eventQueue_.empty(); }
    size_t GetQueueSize() const { return eventQueue_.size(); }

private:
    // Find target window for event
    HWND FindTargetWindow(const InputEvent& event) const;

    // Send input to window
    bool SendInputToWindow(const InputEvent& event, HWND targetWindow);

    // Process key event
    bool ProcessKeyEvent(const InputEvent& event, HWND targetWindow);

    // Process mouse event
    bool ProcessMouseEvent(const InputEvent& event, HWND targetWindow);

    // Members
    WorkspaceManager* workspaceManager_ = nullptr;
    InputFilter filter_;
    InputTranslator translator_;
    RouteCallback routeCallback_;
    
    std::queue<InputEvent> eventQueue_;
    size_t processedEventCount_ = 0;
    size_t routedEventCount_ = 0;
    size_t filteredEventCount_ = 0;
    bool initialized_ = false;
};

} // namespace dualdesk