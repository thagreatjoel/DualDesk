#pragma once

#include <windows.h>
#include <vector>
#include <functional>
#include "../core/types.h"  // ADD THIS - for WorkspaceId

namespace dualdesk {

class WorkspaceManager;

/**
 * @brief Handles moving windows between workspaces/monitors
 */
class WindowMover {
public:
    WindowMover();
    ~WindowMover();

    // Initialize with workspace manager
    void Initialize(WorkspaceManager* manager);
    void Shutdown();

    // Move window to specific monitor
    bool MoveWindowToMonitor(HWND hwnd, HMONITOR targetMonitor);
    bool MoveWindowToWorkspace(HWND hwnd, WorkspaceId targetWorkspace);

    // Move window to next/previous monitor
    bool MoveWindowToNextMonitor(HWND hwnd);
    bool MoveWindowToPreviousMonitor(HWND hwnd);

    // Move all windows from one workspace to another
    bool MoveAllWindows(WorkspaceId fromWorkspace, WorkspaceId toWorkspace);

    // Get window's current monitor
    HMONITOR GetWindowMonitor(HWND hwnd) const;

    // Get window's current workspace
    WorkspaceId GetWindowWorkspace(HWND hwnd) const;

    // Check if window can be moved
    bool CanMoveWindow(HWND hwnd) const;

    // Move with position preservation
    bool MoveWindowWithPosition(HWND hwnd, HMONITOR targetMonitor, 
                                int offsetX = 0, int offsetY = 0);

    // Callbacks
    using MoveCallback = std::function<void(HWND hwnd, HMONITOR from, HMONITOR to)>;
    void SetMoveCallback(MoveCallback callback);

private:
    WorkspaceManager* workspaceManager_ = nullptr;
    MoveCallback moveCallback_;
    bool initialized_ = false;

    // Helper functions
    RECT GetWindowRect(HWND hwnd) const;
    RECT GetMonitorWorkArea(HMONITOR monitor) const;
    POINT GetWindowCenter(HWND hwnd) const;
    bool IsWindowOnMonitor(HWND hwnd, HMONITOR monitor) const;
    bool IsWindowVisible(HWND hwnd) const;

    // Calculate new position
    RECT CalculateNewPosition(const RECT& windowRect, 
                              HMONITOR fromMonitor, 
                              HMONITOR toMonitor);
};

} // namespace dualdesk