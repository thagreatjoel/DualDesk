#pragma once
#include <windows.h>
#include <memory>
#include "virtual_cursor.h"

namespace dualdesk {

class CursorEmulator {
public:
    CursorEmulator();
    ~CursorEmulator();

    bool Initialize(HWND overlayWindow);
    void Shutdown();

    // Enable virtual cursor for a workspace
    bool EnableVirtualCursor(ULONG workspaceId, ULONG deviceHandle, RECT monitorBounds);
    bool DisableVirtualCursor(ULONG workspaceId);

    // Route mouse input to the correct cursor
    void RouteMouseMove(ULONG workspaceId, int deltaX, int deltaY);
    void RouteMouseButton(ULONG workspaceId, bool leftButton, bool down);
    void RouteMouseWheel(ULONG workspaceId, int delta);

    // Set which workspace has the real (physical) cursor
    void SetRealCursorWorkspace(ULONG workspaceId);
    ULONG GetRealCursorWorkspace() const;

    // Lock real cursor to its workspace
    void LockRealCursor(bool enabled);
    bool IsRealCursorLocked() const;

    // ============================================================
    // NEW: Border wall methods
    // ============================================================
    void ShowCursorBorder(bool show);
    void DrawBorder(HDC hdc);
    void SetBorderColor(COLORREF color);
    void SetBorderThickness(int thickness);

    // Get virtual cursor manager
    VirtualCursorManager* GetVirtualCursorManager();

private:
    std::unique_ptr<VirtualCursorManager> m_virtualCursorManager;
    ULONG m_realCursorWorkspace;
    bool m_cursorLocked;
    bool m_initialized;
    bool m_showBorder;
    COLORREF m_borderColor;
    int m_borderThickness;
    CRITICAL_SECTION m_cs;

    void ConstrainRealCursor();
    bool IsPointInWorkspace(ULONG workspaceId, const POINT& pt);
    void DrawBorderWall(HDC hdc);
};

} // namespace dualdesk