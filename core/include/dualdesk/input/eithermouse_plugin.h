#pragma once

#include <windows.h>
#include <string>
#include <map>
#include <functional>

namespace dualdesk {

// ============================================================
// EITHERMOUSE PLUGIN SYSTEM
// ============================================================

class EitherMousePlugin {
public:
    static EitherMousePlugin& GetInstance() {
        static EitherMousePlugin instance;
        return instance;
    }

    // Initialize - register for EitherMouse messages
    bool Initialize();

    // Check if EitherMouse is running
    bool IsEitherMouseRunning();

    // Get the active mouse number (returns 1, 2, 3, etc.)
    int GetActiveMouse();

    // Assign a device to a specific monitor
    bool AssignDeviceToMonitor(int mouseId, int monitorIndex);

    // Lock the current mouse to a specific monitor
    bool LockMouseToMonitor(int monitorIndex);

    // Unlock the mouse (allow free movement)
    bool UnlockMouse();

    // Set callback for when mouse changes
    void SetMouseChangeCallback(std::function<void(int mouseId)> callback) {
        m_mouseChangeCallback = callback;
    }

    // Process messages - call this in your message loop
    void ProcessMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    // Get active mouse
    int GetActiveMouseId() const { return m_activeMouse; }

    // Get monitor rectangle
    bool GetMonitorRect(int monitorIndex, RECT* rect);

    // Get last error
    std::string GetLastError() const { return m_lastError; }

private:
    EitherMousePlugin();
    ~EitherMousePlugin();
    EitherMousePlugin(const EitherMousePlugin&) = delete;
    EitherMousePlugin& operator=(const EitherMousePlugin&) = delete;

    bool m_initialized;
    UINT m_eitherMouseMessage;
    int m_activeMouse;
    RECT m_clipRect;
    std::string m_lastError;
    std::map<int, int> m_mouseMonitorMap;
    std::function<void(int)> m_mouseChangeCallback;
};

} // namespace dualdesk