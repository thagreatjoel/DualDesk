#pragma once

#include <windows.h>
#include <vector>
#include <map>

namespace dualdesk {

class MonitorBorder {
public:
    static MonitorBorder& GetInstance() {
        static MonitorBorder instance;
        return instance;
    }

    void Initialize(const std::vector<RECT>& monitors);
    void SetEnabled(bool enabled);
    bool IsEnabled() const { return m_enabled; }
    void AssignMouseToMonitor(HANDLE mouseHandle, int monitorIndex);
    int GetMouseMonitor(HANDLE mouseHandle);
    POINT ConstrainToMonitor(HANDLE mouseHandle, POINT pos);
    const std::vector<RECT>& GetMonitors() const { return m_monitors; }
    void DrawBorders(HDC hdc, const RECT& virtualRect);
    int GetMonitorCount() const { return (int)m_monitors.size(); }

private:
    MonitorBorder() = default;
    ~MonitorBorder() = default;
    MonitorBorder(const MonitorBorder&) = delete;
    MonitorBorder& operator=(const MonitorBorder&) = delete;

    std::vector<RECT> m_monitors;
    std::map<HANDLE, int> m_mouseMonitorMap;
    bool m_enabled = true;
};

} // namespace dualdesk