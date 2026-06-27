#include "dualdesk/display/display_manager.h"
#include "dualdesk/core/logger.h"
#include <windows.h>

namespace dualdesk {

std::vector<DisplayInfo> DisplayManager::EnumerateDisplays() {
    displays_.clear();
    EnumDisplayMonitors(NULL, NULL, MonitorEnumProc, (LPARAM)this);
    return displays_;
}

BOOL CALLBACK DisplayManager::MonitorEnumProc(HMONITOR hMonitor, HDC hdc, LPRECT lprcMonitor, LPARAM dwData) {
    auto* manager = reinterpret_cast<DisplayManager*>(dwData);
    if (!manager) return TRUE;
    
    DisplayInfo info;
    info.monitor = hMonitor;
    info.bounds = *lprcMonitor;
    
    MONITORINFO mi = {sizeof(MONITORINFO)};
    if (GetMonitorInfo(hMonitor, &mi)) {
        info.workArea = mi.rcWork;
        info.isPrimary = (mi.dwFlags & MONITORINFOF_PRIMARY) != 0;
    }
    
    manager->displays_.push_back(info);
    return TRUE;
}

}
