#include "dualdesk/input/eithermouse_plugin.h"
#include <map>

namespace dualdesk {

// ============================================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================================

EitherMousePlugin::EitherMousePlugin() 
    : m_initialized(false), 
      m_activeMouse(0), 
      m_eitherMouseMessage(0) {
    m_clipRect = {0, 0, 0, 0};
}

EitherMousePlugin::~EitherMousePlugin() {
    UnlockMouse();
}

// ============================================================
// INITIALIZATION
// ============================================================

bool EitherMousePlugin::Initialize() {
    if (m_initialized) return true;
    
    m_eitherMouseMessage = RegisterWindowMessageW(L"EitherMouse");
    
    if (!m_eitherMouseMessage) {
        m_lastError = "Failed to register EitherMouse message";
        return false;
    }
    
    m_initialized = true;
    return true;
}

// ============================================================
// EITHERMOUSE DETECTION
// ============================================================

bool EitherMousePlugin::IsEitherMouseRunning() {
    HWND hWnd = FindWindowW(NULL, L"EitherMouse");
    return hWnd != NULL;
}

int EitherMousePlugin::GetActiveMouse() {
    if (!m_initialized) return -1;
    
    HWND hWnd = FindWindowW(NULL, L"EitherMouse");
    if (!hWnd) return -1;
    
    DWORD_PTR result = 0;
    SendMessageTimeoutW(
        hWnd,
        m_eitherMouseMessage,
        m_eitherMouseMessage,
        0,
        SMTO_NORMAL,
        1000,
        &result
    );
    
    return (int)result;
}

// ============================================================
// MOUSE ASSIGNMENT
// ============================================================

bool EitherMousePlugin::AssignDeviceToMonitor(int mouseId, int monitorIndex) {
    if (!m_initialized) return false;
    
    m_mouseMonitorMap[mouseId] = monitorIndex;
    
    int activeMouse = GetActiveMouse();
    if (activeMouse == mouseId) {
        LockMouseToMonitor(monitorIndex);
    }
    
    return true;
}

bool EitherMousePlugin::LockMouseToMonitor(int monitorIndex) {
    RECT monitorRect;
    if (!GetMonitorRect(monitorIndex, &monitorRect)) {
        return false;
    }
    
    m_clipRect = monitorRect;
    return ClipCursor(&monitorRect) != FALSE;
}

bool EitherMousePlugin::UnlockMouse() {
    m_clipRect = {0, 0, 0, 0};
    return ClipCursor(NULL) != FALSE;
}

// ============================================================
// MONITOR HELPERS
// ============================================================

bool EitherMousePlugin::GetMonitorRect(int monitorIndex, RECT* rect) {
    if (!rect) return false;
    
    HMONITOR hMonitor = NULL;
    
    if (monitorIndex == 0) {
        hMonitor = MonitorFromPoint({0, 0}, MONITOR_DEFAULTTOPRIMARY);
    } else {
        // Enumerate monitors
        struct MonitorData {
            int index;
            MONITORINFOEXW info;
            bool found;
        };
        
        MonitorData data = {monitorIndex, {}, false};
        
        EnumDisplayMonitors(NULL, NULL, [](HMONITOR hMon, HDC hdc, LPRECT rect, LPARAM lParam) -> BOOL {
            auto* pData = (MonitorData*)lParam;
            if (pData->index == 0) {
                MONITORINFOEXW info;
                info.cbSize = sizeof(MONITORINFOEXW);
                if (GetMonitorInfoW(hMon, &info)) {
                    pData->info = info;
                    pData->found = true;
                    return FALSE;
                }
            }
            pData->index--;
            return TRUE;
        }, (LPARAM)&data);
        
        if (data.found) {
            *rect = data.info.rcMonitor;
            return true;
        }
        return false;
    }
    
    if (hMonitor) {
        MONITORINFOEXW info;
        info.cbSize = sizeof(MONITORINFOEXW);
        if (GetMonitorInfoW(hMonitor, &info)) {
            *rect = info.rcMonitor;
            return true;
        }
    }
    return false;
}

// ============================================================
// MESSAGE PROCESSING
// ============================================================

void EitherMousePlugin::ProcessMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    UNREFERENCED_PARAMETER(hWnd);
    UNREFERENCED_PARAMETER(wParam);
    
    if (msg == m_eitherMouseMessage && m_initialized) {
        int mouseId = (int)lParam;
        m_activeMouse = mouseId;
        
        auto it = m_mouseMonitorMap.find(mouseId);
        if (it != m_mouseMonitorMap.end()) {
            LockMouseToMonitor(it->second);
        } else {
            UnlockMouse();
        }
        
        if (m_mouseChangeCallback) {
            m_mouseChangeCallback(mouseId);
        }
    }
}

} // namespace dualdesk