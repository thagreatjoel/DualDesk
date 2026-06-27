#pragma once
#include "display_info.h"
#include <vector>

namespace dualdesk {
class DisplayManager {
public:
    std::vector<DisplayInfo> EnumerateDisplays();
private:
    static BOOL CALLBACK MonitorEnumProc(HMONITOR hMonitor, HDC hdc, LPRECT lprcMonitor, LPARAM dwData);
    std::vector<DisplayInfo> displays_;
};
}