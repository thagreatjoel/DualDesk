#include "dualdesk/cursor/virtual_cursor.h"

namespace dualdesk {

VirtualCursorManager::VirtualCursorManager() : m_overlayWnd(nullptr) {}

VirtualCursorManager::~VirtualCursorManager() {}

void VirtualCursorManager::Initialize(HWND overlayWnd, ULONG workspaceCount) {
    m_overlayWnd = overlayWnd;
    // Pre-create cursors for each workspace
    for (ULONG i = 0; i < workspaceCount; i++) {
        VirtualCursor cursor;
        cursor.workspaceId = i;
        cursor.deviceId = 0;
        cursor.position.x = 100;
        cursor.position.y = 100;
        cursor.visible = false;
        SetRect(&cursor.bounds, 0, 0, 0, 0);
        m_cursors[i] = cursor;
    }
}

void VirtualCursorManager::EnableCursor(ULONG workspaceId, ULONG deviceId, const RECT& bounds) {
    VirtualCursor cursor;
    cursor.workspaceId = workspaceId;
    cursor.deviceId = deviceId;
    cursor.position.x = (bounds.left + bounds.right) / 2;
    cursor.position.y = (bounds.top + bounds.bottom) / 2;
    cursor.visible = true;
    cursor.bounds = bounds;
    m_cursors[workspaceId] = cursor;
}

void VirtualCursorManager::UpdateCursor(ULONG workspaceId, int x, int y) {
    auto it = m_cursors.find(workspaceId);
    if (it != m_cursors.end()) {
        it->second.position.x = x;
        it->second.position.y = y;
    }
}

void VirtualCursorManager::ShowCursor(ULONG workspaceId, bool show) {
    auto it = m_cursors.find(workspaceId);
    if (it != m_cursors.end()) {
        it->second.visible = show;
    }
}

void VirtualCursorManager::Render(HDC hdc) {
    if (!hdc) return;
    
    for (const auto& pair : m_cursors) {
        const auto& cursor = pair.second;
        if (!cursor.visible) continue;
        
        // Draw a simple crosshair cursor
        const int size = 10;
        int x = cursor.position.x;
        int y = cursor.position.y;
        
        // Green crosshair
        HPEN pen = CreatePen(PS_SOLID, 2, RGB(0, 255, 0));
        HPEN oldPen = (HPEN)SelectObject(hdc, pen);
        
        // Horizontal line
        MoveToEx(hdc, x - size, y, NULL);
        LineTo(hdc, x + size, y);
        
        // Vertical line
        MoveToEx(hdc, x, y - size, NULL);
        LineTo(hdc, x, y + size);
        
        // Center dot
        HBRUSH brush = CreateSolidBrush(RGB(0, 255, 0));
        HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, brush);
        Ellipse(hdc, x - 2, y - 2, x + 2, y + 2);
        
        SelectObject(hdc, oldPen);
        SelectObject(hdc, oldBrush);
        DeleteObject(pen);
        DeleteObject(brush);
    }
}

void VirtualCursorManager::RemoveCursor(ULONG workspaceId) {
    m_cursors.erase(workspaceId);
}

VirtualCursor* VirtualCursorManager::GetCursor(ULONG workspaceId) {
    auto it = m_cursors.find(workspaceId);
    if (it != m_cursors.end()) {
        return &it->second;
    }
    return nullptr;
}

} // namespace dualdesk