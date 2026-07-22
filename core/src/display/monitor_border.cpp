#include "dualdesk/display/monitor_border.h"
#include <algorithm>

namespace dualdesk {

void MonitorBorder::Initialize(const std::vector<RECT>& monitors) {
    m_monitors = monitors;
    m_mouseMonitorMap.clear();
    m_enabled = true;
}

void MonitorBorder::SetEnabled(bool enabled) {
    m_enabled = enabled;
    if (!enabled) {
        ClipCursor(NULL);
    }
}

void MonitorBorder::AssignMouseToMonitor(HANDLE mouseHandle, int monitorIndex) {
    if (monitorIndex < 0 || monitorIndex >= (int)m_monitors.size()) return;
    m_mouseMonitorMap[mouseHandle] = monitorIndex;
}

int MonitorBorder::GetMouseMonitor(HANDLE mouseHandle) {
    auto it = m_mouseMonitorMap.find(mouseHandle);
    if (it != m_mouseMonitorMap.end()) return it->second;
    return -1;
}

POINT MonitorBorder::ConstrainToMonitor(HANDLE mouseHandle, POINT pos) {
    if (!m_enabled) return pos;
    
    int monitorIndex = GetMouseMonitor(mouseHandle);
    if (monitorIndex < 0 || monitorIndex >= (int)m_monitors.size()) return pos;
    
    const RECT& monitor = m_monitors[monitorIndex];
    pos.x = std::max(monitor.left, std::min(pos.x, monitor.right - 1));
    pos.y = std::max(monitor.top, std::min(pos.y, monitor.bottom - 1));
    
    return pos;
}

void MonitorBorder::DrawBorders(HDC hdc, const RECT& virtualRect) {
    if (!m_enabled || m_monitors.size() < 2) return;
    
    COLORREF borderColor = RGB(255, 0, 0);
    
    for (const auto& monitor : m_monitors) {
        HPEN pen = CreatePen(PS_SOLID, 4, borderColor);
        HPEN oldPen = (HPEN)SelectObject(hdc, pen);
        HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
        
        Rectangle(hdc, monitor.left, monitor.top, monitor.right, monitor.bottom);
        
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, borderColor);
        
        HFONT hFont = CreateFontW(24, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                  DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                  DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Arial");
        HFONT oldFont = (HFONT)SelectObject(hdc, hFont);
        
        int centerX = (monitor.left + monitor.right) / 2;
        const wchar_t* wallText = L"═══ WALL ═══";
        
        RECT topRect = {centerX - 120, monitor.top + 4, centerX + 120, monitor.top + 32};
        DrawTextW(hdc, wallText, -1, &topRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        
        RECT bottomRect = {centerX - 120, monitor.bottom - 32, centerX + 120, monitor.bottom - 4};
        DrawTextW(hdc, wallText, -1, &bottomRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        
        for (int y = monitor.top + 40; y < monitor.bottom - 40; y += 30) {
            TextOutW(hdc, monitor.left + 2, y, L"║", 1);
            TextOutW(hdc, monitor.right - 12, y, L"║", 1);
        }
        
        SelectObject(hdc, oldFont);
        DeleteObject(hFont);
        SelectObject(hdc, oldPen);
        SelectObject(hdc, oldBrush);
        DeleteObject(pen);
    }
}

} // namespace dualdesk