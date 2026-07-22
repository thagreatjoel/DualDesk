#pragma once
#include <windows.h>
#include <vector>
#include <map>
#include <memory>

namespace dualdesk {

// Forward declaration
class VirtualCursorManager;

class CursorEmulator {
public:
    CursorEmulator();
    ~CursorEmulator();

    void Initialize(HWND overlayWnd);
    void LockRealCursor(bool locked);
    void SetRealCursorWorkspace(ULONG workspaceId);
    ULONG GetRealCursorWorkspace() const;
    void EnableVirtualCursor(ULONG workspaceId, ULONG deviceId, const RECT& bounds);
    void DisableVirtualCursor(ULONG workspaceId);
    void ShowCursorBorder(bool show);
    void RouteMouseMove(ULONG workspaceId, int x, int y);
    void RouteMouseButton(ULONG workspaceId, bool left, bool right);
    void RouteMouseWheel(ULONG workspaceId, int delta);
    VirtualCursorManager* GetVirtualCursorManager();

private:
    ULONG m_realCursorWorkspace;
    bool m_cursorLocked;
    bool m_borderVisible;
    VirtualCursorManager* m_virtualManager;  // Forward declaration works here
};

} // namespace dualdesk