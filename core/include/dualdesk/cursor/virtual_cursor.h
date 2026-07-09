#pragma once
#include <windows.h>
#include <vector>
#include <map>
#include <string>
#include <mutex>

namespace dualdesk {

struct VirtualCursor {
    ULONG workspaceId;
    ULONG deviceHandle;
    POINT position;
    POINT lastPosition;
    RECT bounds;
    bool isVisible;
    bool isActive;
    bool isLeftDown;
    bool isRightDown;
    HWND targetWindow;
    HCURSOR cursorHandle;
    std::wstring name;
};

class VirtualCursorManager {
public:
    VirtualCursorManager();
    ~VirtualCursorManager();

    bool Initialize(HWND overlayWindow);
    void Shutdown();

    bool CreateCursor(ULONG workspaceId, ULONG deviceHandle, RECT monitorBounds);
    bool RemoveCursor(ULONG workspaceId);
    
    void MoveCursor(ULONG workspaceId, int deltaX, int deltaY);
    void SetCursorPosition(ULONG workspaceId, int x, int y);
    
    void Click(ULONG workspaceId, bool leftButton, bool down);
    void Wheel(ULONG workspaceId, int delta);
    
    HWND FindWindowUnderCursor(ULONG workspaceId);
    void Render(HDC hdc);
    
    VirtualCursor* GetCursor(ULONG workspaceId);
    bool IsCursorVisible(ULONG workspaceId);
    RECT GetWorkspaceBounds(ULONG workspaceId);
    
    // ============================================================
    // NEW: Border wall - draw barrier between workspaces
    // ============================================================
    void DrawBorderWall(HDC hdc, ULONG workspaceId, COLORREF color, int thickness);
    
    size_t GetCursorCount() const {
        std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(m_mutex));
        return m_cursors.size();
    }
    
    void SetEnabled(ULONG workspaceId, bool enabled);
    void SetVisible(ULONG workspaceId, bool visible);

private:
    std::map<ULONG, VirtualCursor> m_cursors;
    HWND m_overlayWindow;
    HDC m_memoryDC;
    HBITMAP m_backBuffer;
    SIZE m_windowSize;
    bool m_initialized;
    mutable std::mutex m_mutex;
    bool m_needRedraw;

    void DrawCursor(HDC hdc, const VirtualCursor& cursor);
    void UpdateOverlay();
    void SendMouseEvent(HWND hwnd, const POINT& pt, UINT msg, WPARAM wParam, LPARAM lParam);
};

} // namespace dualdesk