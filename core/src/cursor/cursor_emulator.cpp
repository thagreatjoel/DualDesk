#include "dualdesk/cursor/cursor_emulator.h"
#include "dualdesk/cursor/virtual_cursor.h"

namespace dualdesk {

CursorEmulator::CursorEmulator() 
    : m_realCursorWorkspace(0), m_cursorLocked(false), m_borderVisible(false), m_virtualManager(nullptr) {}

CursorEmulator::~CursorEmulator() {
    if (m_virtualManager) {
        delete m_virtualManager;
        m_virtualManager = nullptr;
    }
}

void CursorEmulator::Initialize(HWND overlayWnd) {
    m_virtualManager = new VirtualCursorManager();
    m_virtualManager->Initialize(overlayWnd, 4);
}

void CursorEmulator::LockRealCursor(bool locked) {
    m_cursorLocked = locked;
}

void CursorEmulator::SetRealCursorWorkspace(ULONG workspaceId) {
    m_realCursorWorkspace = workspaceId;
}

ULONG CursorEmulator::GetRealCursorWorkspace() const {
    return m_realCursorWorkspace;
}

void CursorEmulator::EnableVirtualCursor(ULONG workspaceId, ULONG deviceId, const RECT& bounds) {
    if (m_virtualManager) {
        m_virtualManager->EnableCursor(workspaceId, deviceId, bounds);
    }
}

void CursorEmulator::DisableVirtualCursor(ULONG workspaceId) {
    if (m_virtualManager) {
        m_virtualManager->ShowCursor(workspaceId, false);
    }
}

void CursorEmulator::ShowCursorBorder(bool show) {
    m_borderVisible = show;
}

void CursorEmulator::RouteMouseMove(ULONG workspaceId, int x, int y) {
    // Stub - will be implemented later
}

void CursorEmulator::RouteMouseButton(ULONG workspaceId, bool left, bool right) {
    // Stub - will be implemented later
}

void CursorEmulator::RouteMouseWheel(ULONG workspaceId, int delta) {
    // Stub - will be implemented later
}

VirtualCursorManager* CursorEmulator::GetVirtualCursorManager() {
    return m_virtualManager;
}

} // namespace dualdesk