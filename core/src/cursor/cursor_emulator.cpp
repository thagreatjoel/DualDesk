#include "dualdesk/cursor/cursor_emulator.h"
#include "dualdesk/core/logger.h"
#include <windowsx.h>
#include <algorithm>

namespace dualdesk {

CursorEmulator::CursorEmulator() 
    : m_realCursorWorkspace(0), m_cursorLocked(true), m_initialized(false),
      m_showBorder(true), m_borderColor(RGB(255, 0, 0)), m_borderThickness(3) {
    InitializeCriticalSection(&m_cs);
    m_virtualCursorManager = std::make_unique<VirtualCursorManager>();
}

CursorEmulator::~CursorEmulator() {
    Shutdown();
    DeleteCriticalSection(&m_cs);
}

bool CursorEmulator::Initialize(HWND overlayWindow) {
    if (m_initialized) return true;
    
    if (!m_virtualCursorManager->Initialize(overlayWindow)) {
        LOG_ERROR("Failed to initialize virtual cursor manager");
        return false;
    }
    
    m_realCursorWorkspace = 0;
    m_cursorLocked = true;
    m_initialized = true;
    m_showBorder = true;
    
    LOG_INFO("CursorEmulator initialized");
    return true;
}

void CursorEmulator::Shutdown() {
    if (!m_initialized) return;
    
    m_virtualCursorManager->Shutdown();
    m_initialized = false;
    LOG_INFO("CursorEmulator shutdown");
}

bool CursorEmulator::EnableVirtualCursor(ULONG workspaceId, ULONG deviceHandle, RECT monitorBounds) {
    if (!m_initialized) return false;
    return m_virtualCursorManager->CreateCursor(workspaceId, deviceHandle, monitorBounds);
}

bool CursorEmulator::DisableVirtualCursor(ULONG workspaceId) {
    if (!m_initialized) return false;
    return m_virtualCursorManager->RemoveCursor(workspaceId);
}

// ============================================================
// FIXED: Route Mouse Move with cursor cross-over prevention
// ============================================================
void CursorEmulator::RouteMouseMove(ULONG workspaceId, int deltaX, int deltaY) {
    if (!m_initialized) return;
    
    EnterCriticalSection(&m_cs);
    
    if (workspaceId == m_realCursorWorkspace) {
        // ============================================================
        // REAL CURSOR - Block from crossing workspace boundary
        // ============================================================
        if (m_cursorLocked) {
            POINT pos;
            GetCursorPos(&pos);
            
            RECT bounds = m_virtualCursorManager->GetWorkspaceBounds(workspaceId);
            
            int newX = pos.x + deltaX;
            int newY = pos.y + deltaY;
            
            bool blocked = false;
            
            // Block right edge
            if (newX >= bounds.right) {
                newX = bounds.right - 1;
                blocked = true;
                LOG_DEBUG("Cursor blocked at right border: %d", newX);
            }
            // Block left edge
            if (newX < bounds.left) {
                newX = bounds.left;
                blocked = true;
                LOG_DEBUG("Cursor blocked at left border: %d", newX);
            }
            // Block bottom edge
            if (newY >= bounds.bottom) {
                newY = bounds.bottom - 1;
                blocked = true;
                LOG_DEBUG("Cursor blocked at bottom border: %d", newY);
            }
            // Block top edge
            if (newY < bounds.top) {
                newY = bounds.top;
                blocked = true;
                LOG_DEBUG("Cursor blocked at top border: %d", newY);
            }
            
            if (newX != pos.x || newY != pos.y) {
                SetCursorPos(newX, newY);
            }
        }
    } else {
        // ============================================================
        // VIRTUAL CURSOR - Moves freely within its workspace
        // ============================================================
        m_virtualCursorManager->MoveCursor(workspaceId, deltaX, deltaY);
    }
    
    LeaveCriticalSection(&m_cs);
}

// ============================================================
// FIXED: Route Mouse Button
// ============================================================
void CursorEmulator::RouteMouseButton(ULONG workspaceId, bool leftButton, bool down) {
    if (!m_initialized) return;
    
    EnterCriticalSection(&m_cs);
    
    if (workspaceId != m_realCursorWorkspace) {
        m_virtualCursorManager->Click(workspaceId, leftButton, down);
    }
    
    LeaveCriticalSection(&m_cs);
}

// ============================================================
// FIXED: Route Mouse Wheel
// ============================================================
void CursorEmulator::RouteMouseWheel(ULONG workspaceId, int delta) {
    if (!m_initialized) return;
    
    EnterCriticalSection(&m_cs);
    
    if (workspaceId != m_realCursorWorkspace) {
        m_virtualCursorManager->Wheel(workspaceId, delta);
    }
    
    LeaveCriticalSection(&m_cs);
}

// ============================================================
// SET REAL CURSOR WORKSPACE
// ============================================================
void CursorEmulator::SetRealCursorWorkspace(ULONG workspaceId) {
    EnterCriticalSection(&m_cs);
    m_realCursorWorkspace = workspaceId;
    LeaveCriticalSection(&m_cs);
    LOG_INFO("Real cursor set to workspace %lu", workspaceId);
}

ULONG CursorEmulator::GetRealCursorWorkspace() const {
    return m_realCursorWorkspace;
}

// ============================================================
// LOCK REAL CURSOR
// ============================================================
void CursorEmulator::LockRealCursor(bool enabled) {
    EnterCriticalSection(&m_cs);
    m_cursorLocked = enabled;
    if (enabled) {
        ConstrainRealCursor();
    }
    LeaveCriticalSection(&m_cs);
    LOG_INFO("Cursor lock %s", enabled ? "enabled" : "disabled");
}

bool CursorEmulator::IsRealCursorLocked() const {
    return m_cursorLocked;
}

// ============================================================
// SHOW CURSOR BORDER
// ============================================================
void CursorEmulator::ShowCursorBorder(bool show) {
    EnterCriticalSection(&m_cs);
    m_showBorder = show;
    LeaveCriticalSection(&m_cs);
    LOG_INFO("Cursor border %s", show ? "shown" : "hidden");
}

void CursorEmulator::DrawBorder(HDC hdc) {
    if (!m_initialized || !m_showBorder) return;
    
    EnterCriticalSection(&m_cs);
    
    RECT bounds = m_virtualCursorManager->GetWorkspaceBounds(m_realCursorWorkspace);
    
    HPEN pen = CreatePen(PS_SOLID, m_borderThickness, m_borderColor);
    HPEN oldPen = (HPEN)SelectObject(hdc, pen);
    HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
    
    Rectangle(hdc, bounds.left, bounds.top, bounds.right, bounds.bottom);
    
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, m_borderColor);
    
    const wchar_t* wallText = L"===== CURSOR WALL =====";
    int textWidth = 200;
    TextOutW(hdc, (bounds.left + bounds.right) / 2 - textWidth/2, bounds.top + 2, wallText, (int)wcslen(wallText));
    TextOutW(hdc, (bounds.left + bounds.right) / 2 - textWidth/2, bounds.bottom - 22, wallText, (int)wcslen(wallText));
    
    for (int y = bounds.top + 40; y < bounds.bottom - 40; y += 20) {
        TextOutW(hdc, bounds.left + 2, y, L"|", 1);
    }
    
    for (int y = bounds.top + 40; y < bounds.bottom - 40; y += 20) {
        TextOutW(hdc, bounds.right - 12, y, L"|", 1);
    }
    
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(pen);
    
    LeaveCriticalSection(&m_cs);
}

void CursorEmulator::SetBorderColor(COLORREF color) {
    EnterCriticalSection(&m_cs);
    m_borderColor = color;
    LeaveCriticalSection(&m_cs);
}

void CursorEmulator::SetBorderThickness(int thickness) {
    EnterCriticalSection(&m_cs);
    if (thickness < 1) thickness = 1;
    if (thickness > 10) thickness = 10;
    m_borderThickness = thickness;
    LeaveCriticalSection(&m_cs);
}

VirtualCursorManager* CursorEmulator::GetVirtualCursorManager() {
    return m_virtualCursorManager.get();
}

// ============================================================
// PRIVATE METHODS
// ============================================================
void CursorEmulator::ConstrainRealCursor() {
    if (!m_cursorLocked) return;
    
    POINT pos;
    GetCursorPos(&pos);
    
    RECT bounds = m_virtualCursorManager->GetWorkspaceBounds(m_realCursorWorkspace);
    
    bool moved = false;
    
    if (pos.x < bounds.left) {
        pos.x = bounds.left;
        moved = true;
    } else if (pos.x >= bounds.right) {
        pos.x = bounds.right - 1;
        moved = true;
    }
    
    if (pos.y < bounds.top) {
        pos.y = bounds.top;
        moved = true;
    } else if (pos.y >= bounds.bottom) {
        pos.y = bounds.bottom - 1;
        moved = true;
    }
    
    if (moved) {
        SetCursorPos(pos.x, pos.y);
        LOG_DEBUG("Cursor constrained to workspace at (%d, %d)", pos.x, pos.y);
    }
}

bool CursorEmulator::IsPointInWorkspace(ULONG workspaceId, const POINT& pt) {
    RECT bounds = m_virtualCursorManager->GetWorkspaceBounds(workspaceId);
    return pt.x >= bounds.left && pt.x < bounds.right &&
           pt.y >= bounds.top && pt.y < bounds.bottom;
}

} // namespace dualdesk