#pragma once
#include <windows.h>
#include <vector>
#include <map>

namespace dualdesk {

struct VirtualCursor {
    ULONG workspaceId;
    ULONG deviceId;
    POINT position;
    bool visible;
    RECT bounds;
};

class VirtualCursorManager {
public:
    VirtualCursorManager();
    ~VirtualCursorManager();

    void Initialize(HWND overlayWnd, ULONG workspaceCount);
    void UpdateCursor(ULONG workspaceId, int x, int y);
    void ShowCursor(ULONG workspaceId, bool show);
    void Render(HDC hdc);
    void RemoveCursor(ULONG workspaceId);
    VirtualCursor* GetCursor(ULONG workspaceId);
    void EnableCursor(ULONG workspaceId, ULONG deviceId, const RECT& bounds);

private:
    HWND m_overlayWnd;
    std::map<ULONG, VirtualCursor> m_cursors;
};

} // namespace dualdesk