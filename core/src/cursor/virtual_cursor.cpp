#include "dualdesk/cursor/virtual_cursor.h"
#include "dualdesk/core/logger.h"
#include <windowsx.h>
#include <algorithm>

namespace dualdesk {

// Helper function to clamp a value between min and max
template<typename T>
static T Clamp(T value, T min, T max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

VirtualCursorManager::VirtualCursorManager() 
    : m_overlayWindow(NULL), m_memoryDC(NULL), m_backBuffer(NULL),
      m_initialized(false), m_needRedraw(false) {
    m_windowSize.cx = 0;
    m_windowSize.cy = 0;
}

VirtualCursorManager::~VirtualCursorManager() {
    Shutdown();
}

bool VirtualCursorManager::Initialize(HWND overlayWindow) {
    if (m_initialized) return true;
    
    m_overlayWindow = overlayWindow;
    
    SetWindowLong(overlayWindow, GWL_EXSTYLE, 
                  GetWindowLong(overlayWindow, GWL_EXSTYLE) | 
                  WS_EX_TRANSPARENT | WS_EX_LAYERED | WS_EX_TOPMOST);
    
    SetLayeredWindowAttributes(overlayWindow, RGB(0, 0, 0), 0, LWA_COLORKEY);
    
    RECT rc;
    GetClientRect(overlayWindow, &rc);
    m_windowSize.cx = rc.right - rc.left;
    m_windowSize.cy = rc.bottom - rc.top;
    
    HDC hdc = GetDC(overlayWindow);
    m_memoryDC = CreateCompatibleDC(hdc);
    m_backBuffer = CreateCompatibleBitmap(hdc, m_windowSize.cx, m_windowSize.cy);
    SelectObject(m_memoryDC, m_backBuffer);
    ReleaseDC(overlayWindow, hdc);
    
    m_initialized = true;
    LOG_INFO("VirtualCursorManager initialized");
    return true;
}

void VirtualCursorManager::Shutdown() {
    if (!m_initialized) return;
    
    std::lock_guard<std::mutex> lock(m_mutex);
    m_cursors.clear();
    
    if (m_backBuffer) {
        DeleteObject(m_backBuffer);
        m_backBuffer = NULL;
    }
    if (m_memoryDC) {
        DeleteDC(m_memoryDC);
        m_memoryDC = NULL;
    }
    
    m_initialized = false;
    LOG_INFO("VirtualCursorManager shutdown");
}

bool VirtualCursorManager::CreateCursor(ULONG workspaceId, ULONG deviceHandle, RECT monitorBounds) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_cursors.find(workspaceId) != m_cursors.end()) {
        LOG_WARN("Cursor already exists for workspace %lu", workspaceId);
        return false;
    }
    
    VirtualCursor cursor;
    cursor.workspaceId = workspaceId;
    cursor.deviceHandle = deviceHandle;
    cursor.bounds = monitorBounds;
    cursor.isVisible = true;
    cursor.isActive = true;
    cursor.isLeftDown = false;
    cursor.isRightDown = false;
    cursor.targetWindow = NULL;
    cursor.cursorHandle = LoadCursor(NULL, IDC_ARROW);
    cursor.name = L"Cursor " + std::to_wstring(workspaceId);
    
    cursor.position.x = monitorBounds.left + (monitorBounds.right - monitorBounds.left) / 2;
    cursor.position.y = monitorBounds.top + (monitorBounds.bottom - monitorBounds.top) / 2;
    cursor.lastPosition = cursor.position;
    
    m_cursors[workspaceId] = cursor;
    m_needRedraw = true;
    
    LOG_INFO("Created virtual cursor for workspace %lu", workspaceId);
    return true;
}

bool VirtualCursorManager::RemoveCursor(ULONG workspaceId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_cursors.find(workspaceId);
    if (it == m_cursors.end()) {
        LOG_WARN("Cursor not found for workspace %lu", workspaceId);
        return false;
    }
    
    m_cursors.erase(it);
    m_needRedraw = true;
    LOG_INFO("Removed virtual cursor for workspace %lu", workspaceId);
    return true;
}

void VirtualCursorManager::MoveCursor(ULONG workspaceId, int deltaX, int deltaY) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_cursors.find(workspaceId);
    if (it == m_cursors.end() || !it->second.isActive) return;
    
    VirtualCursor& cursor = it->second;
    cursor.lastPosition = cursor.position;
    
    cursor.position.x += deltaX;
    cursor.position.y += deltaY;
    
    // ============================================================
    // FIX: Manual clamping instead of std::max/std::min
    // ============================================================
    if (cursor.position.x < cursor.bounds.left) {
        cursor.position.x = cursor.bounds.left;
    } else if (cursor.position.x >= cursor.bounds.right) {
        cursor.position.x = cursor.bounds.right - 1;
    }
    
    if (cursor.position.y < cursor.bounds.top) {
        cursor.position.y = cursor.bounds.top;
    } else if (cursor.position.y >= cursor.bounds.bottom) {
        cursor.position.y = cursor.bounds.bottom - 1;
    }
    
    cursor.targetWindow = FindWindowUnderCursor(workspaceId);
    m_needRedraw = true;
    UpdateOverlay();
}

void VirtualCursorManager::SetCursorPosition(ULONG workspaceId, int x, int y) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_cursors.find(workspaceId);
    if (it == m_cursors.end() || !it->second.isActive) return;
    
    VirtualCursor& cursor = it->second;
    
    // ============================================================
    // FIX: Manual clamping instead of std::max/std::min
    // ============================================================
    cursor.position.x = x;
    if (cursor.position.x < cursor.bounds.left) {
        cursor.position.x = cursor.bounds.left;
    } else if (cursor.position.x >= cursor.bounds.right) {
        cursor.position.x = cursor.bounds.right - 1;
    }
    
    cursor.position.y = y;
    if (cursor.position.y < cursor.bounds.top) {
        cursor.position.y = cursor.bounds.top;
    } else if (cursor.position.y >= cursor.bounds.bottom) {
        cursor.position.y = cursor.bounds.bottom - 1;
    }
    
    cursor.targetWindow = FindWindowUnderCursor(workspaceId);
    m_needRedraw = true;
    UpdateOverlay();
}

void VirtualCursorManager::Click(ULONG workspaceId, bool leftButton, bool down) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_cursors.find(workspaceId);
    if (it == m_cursors.end() || !it->second.isActive) return;
    
    VirtualCursor& cursor = it->second;
    HWND hwnd = cursor.targetWindow;
    if (!hwnd) return;
    
    POINT pt = cursor.position;
    ScreenToClient(hwnd, &pt);
    
    UINT msg;
    if (leftButton) {
        msg = down ? WM_LBUTTONDOWN : WM_LBUTTONUP;
        cursor.isLeftDown = down;
    } else {
        msg = down ? WM_RBUTTONDOWN : WM_RBUTTONUP;
        cursor.isRightDown = down;
    }
    
    SendMouseEvent(hwnd, pt, msg, 0, MAKELPARAM(pt.x, pt.y));
}

void VirtualCursorManager::Wheel(ULONG workspaceId, int delta) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_cursors.find(workspaceId);
    if (it == m_cursors.end() || !it->second.isActive) return;
    
    VirtualCursor& cursor = it->second;
    HWND hwnd = cursor.targetWindow;
    if (!hwnd) return;
    
    POINT pt = cursor.position;
    ScreenToClient(hwnd, &pt);
    
    SendMouseEvent(hwnd, pt, WM_MOUSEWHEEL, MAKEWPARAM(0, delta), MAKELPARAM(pt.x, pt.y));
}

HWND VirtualCursorManager::FindWindowUnderCursor(ULONG workspaceId) {
    auto it = m_cursors.find(workspaceId);
    if (it == m_cursors.end()) return NULL;
    
    POINT pt = it->second.position;
    HWND hwnd = WindowFromPoint(pt);
    
    if (!hwnd) return NULL;
    if (!IsWindowVisible(hwnd)) return NULL;
    
    RECT windowRect;
    GetWindowRect(hwnd, &windowRect);
    
    if (pt.x < windowRect.left || pt.x > windowRect.right ||
        pt.y < windowRect.top || pt.y > windowRect.bottom) {
        return NULL;
    }
    
    wchar_t className[256];
    GetClassNameW(hwnd, className, 256);
    std::wstring cls(className);
    if (cls == L"Progman" || cls == L"WorkerW" || cls == L"Shell_TrayWnd") {
        return NULL;
    }
    
    return hwnd;
}

void VirtualCursorManager::Render(HDC hdc) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    RECT rc = {0, 0, m_windowSize.cx, m_windowSize.cy};
    HBRUSH blackBrush = CreateSolidBrush(RGB(0, 0, 0));
    FillRect(m_memoryDC, &rc, blackBrush);
    DeleteObject(blackBrush);
    
    for (auto& pair : m_cursors) {
        if (pair.second.isVisible && pair.second.isActive) {
            DrawCursor(m_memoryDC, pair.second);
        }
    }
    
    POINT pt = {0, 0};
    SIZE sz = m_windowSize;
    BLENDFUNCTION blend = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    
    UpdateLayeredWindow(m_overlayWindow, hdc, &pt, &sz, m_memoryDC, &pt, 0, &blend, ULW_ALPHA);
    m_needRedraw = false;
}

VirtualCursor* VirtualCursorManager::GetCursor(ULONG workspaceId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_cursors.find(workspaceId);
    if (it == m_cursors.end()) return nullptr;
    return &it->second;
}

bool VirtualCursorManager::IsCursorVisible(ULONG workspaceId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_cursors.find(workspaceId);
    if (it == m_cursors.end()) return false;
    return it->second.isVisible;
}

RECT VirtualCursorManager::GetWorkspaceBounds(ULONG workspaceId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_cursors.find(workspaceId);
    if (it == m_cursors.end()) {
        RECT empty = {0, 0, 0, 0};
        return empty;
    }
    return it->second.bounds;
}

void VirtualCursorManager::SetEnabled(ULONG workspaceId, bool enabled) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_cursors.find(workspaceId);
    if (it != m_cursors.end()) {
        it->second.isActive = enabled;
        m_needRedraw = true;
        UpdateOverlay();
    }
}

void VirtualCursorManager::SetVisible(ULONG workspaceId, bool visible) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_cursors.find(workspaceId);
    if (it != m_cursors.end()) {
        it->second.isVisible = visible;
        m_needRedraw = true;
        UpdateOverlay();
    }
}

// ===== PRIVATE METHODS =====

void VirtualCursorManager::DrawCursor(HDC hdc, const VirtualCursor& cursor) {
    if (cursor.cursorHandle) {
        DrawIcon(hdc, cursor.position.x - 2, cursor.position.y - 2, cursor.cursorHandle);
    } else {
        HBRUSH brush = CreateSolidBrush(RGB(255, 255, 255));
        HPEN pen = CreatePen(PS_SOLID, 1, RGB(0, 0, 0));
        SelectObject(hdc, brush);
        SelectObject(hdc, pen);
        Ellipse(hdc, cursor.position.x - 8, cursor.position.y - 8,
                cursor.position.x + 8, cursor.position.y + 8);
        DeleteObject(brush);
        DeleteObject(pen);
    }
    
    if (!cursor.name.empty()) {
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(255, 255, 255));
        TextOutW(hdc, cursor.position.x + 12, cursor.position.y - 8,
                 cursor.name.c_str(), (int)cursor.name.length());
    }
}

void VirtualCursorManager::UpdateOverlay() {
    if (m_needRedraw && m_initialized) {
        HDC hdc = GetDC(m_overlayWindow);
        Render(hdc);
        ReleaseDC(m_overlayWindow, hdc);
    }
}

void VirtualCursorManager::SendMouseEvent(HWND hwnd, const POINT& pt, UINT msg, 
                                          WPARAM wParam, LPARAM lParam) {
    if (!hwnd || !IsWindow(hwnd)) return;
    PostMessage(hwnd, msg, wParam, lParam);
}

// ============================================================
// NEW: Draw border wall
// ============================================================
void VirtualCursorManager::DrawBorderWall(HDC hdc, ULONG workspaceId, COLORREF color, int thickness) {
    auto it = m_cursors.find(workspaceId);
    if (it == m_cursors.end()) return;
    
    RECT bounds = it->second.bounds;
    
    // Create pen for the border wall
    HPEN pen = CreatePen(PS_SOLID, thickness, color);
    HPEN oldPen = (HPEN)SelectObject(hdc, pen);
    HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
    
    // Draw the border wall
    Rectangle(hdc, bounds.left, bounds.top, bounds.right, bounds.bottom);
    
    // Draw warning text on each edge
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, color);
    
    const wchar_t* wallText = L"🚫 BORDER WALL 🚫";
    
    // Draw text centered on each edge
    int textWidth = 200;
    int textHeight = 20;
    
    // Top edge
    TextOutW(hdc, (bounds.left + bounds.right) / 2 - textWidth/2, 
             bounds.top + 2, wallText, (int)wcslen(wallText));
    
    // Bottom edge
    TextOutW(hdc, (bounds.left + bounds.right) / 2 - textWidth/2, 
             bounds.bottom - textHeight - 2, wallText, (int)wcslen(wallText));
    
    // Restore GDI objects
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(pen);
}


} // namespace dualdesk