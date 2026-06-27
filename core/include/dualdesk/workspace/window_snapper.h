#pragma once

#include <windows.h>
#include <vector>

namespace dualdesk {

/**
 * @brief Snaps windows to edges and grid positions
 */
class WindowSnapper {
public:
    WindowSnapper();
    ~WindowSnapper();

    // Snap to edges
    bool SnapToLeft(HWND hwnd);
    bool SnapToRight(HWND hwnd);
    bool SnapToTop(HWND hwnd);
    bool SnapToBottom(HWND hwnd);
    bool SnapToCenter(HWND hwnd);

    // Snap to corners
    bool SnapToTopLeft(HWND hwnd);
    bool SnapToTopRight(HWND hwnd);
    bool SnapToBottomLeft(HWND hwnd);
    bool SnapToBottomRight(HWND hwnd);

    // Snap to half screen
    bool SnapToLeftHalf(HWND hwnd);
    bool SnapToRightHalf(HWND hwnd);

    // Snap to grid
    bool SnapToGrid(HWND hwnd, int gridSize = 10);

    // Snap to other windows
    bool SnapToWindow(HWND hwnd, HWND targetWindow);

    // Get snap position
    RECT GetSnapPosition(HWND hwnd, int snapType) const;

    // Settings
    void SetSnapDistance(int distance) { snapDistance_ = distance; }
    void SetGridSize(int size) { gridSize_ = size; }
    void EnableSnapOnDrag(bool enable) { snapOnDrag_ = enable; }

private:
    int snapDistance_ = 10;
    int gridSize_ = 10;
    bool snapOnDrag_ = true;

    // Helper functions
    RECT GetWindowRect(HWND hwnd) const;
    RECT GetMonitorWorkArea(HWND hwnd) const;
    bool IsNearEdge(const RECT& rect, const RECT& monitor, int edge) const;
    RECT SnapRectToEdge(const RECT& rect, const RECT& monitor, int edge) const;
};

} // namespace dualdesk