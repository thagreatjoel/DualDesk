#pragma once
#include <windows.h>
#include <string>

namespace dualdesk {
struct DisplayInfo {
    HMONITOR monitor = nullptr;
    RECT bounds = {0,0,0,0};
    RECT workArea = {0,0,0,0};
    std::wstring deviceName;
    bool isPrimary = false;
    int dpiX = 96;
    int dpiY = 96;
    
    int Width() const { return bounds.right - bounds.left; }
    int Height() const { return bounds.bottom - bounds.top; }
};
}