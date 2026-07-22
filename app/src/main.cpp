#include "dualdesk/core/logger.h"
#include "dualdesk/display/display_manager.h"
#include "dualdesk/workspace/window_tracker.h"
#include "dualdesk/workspace/workspace_manager.h"
#include "dualdesk/workspace/window_mover.h"
#include "dualdesk/input/input_manager.h"
#include "dualdesk/input/input_router.h"
#include "dualdesk/core/driver_interface.h"
#include "dualdesk/cursor/cursor_emulator.h"
#include "dualdesk/cursor/virtual_cursor.h"
#include "dualdesk/input/eithermouse_plugin.h"
#include "dualdesk/display/monitor_border.h"
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <string>
#include <sstream>
#include <vector>
#include <map>
#include <algorithm>
#include <cmath>

#pragma comment(lib, "comctl32.lib")
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

using namespace dualdesk;

// ============================================================
// DASHBOARD COLORS
// ============================================================
#define COLOR_BG_DARK      RGB(15, 23, 42)
#define COLOR_BG_CARD      RGB(30, 41, 59)
#define COLOR_BG_CARD_HOVER RGB(51, 65, 85)
#define COLOR_TEXT_MAIN    RGB(241, 245, 249)
#define COLOR_TEXT_SUB     RGB(148, 163, 184)
#define COLOR_TEXT_MUTED   RGB(100, 116, 139)
#define COLOR_ACCENT       RGB(139, 92, 246)
#define COLOR_SUCCESS      RGB(16, 185, 129)
#define COLOR_WARNING      RGB(245, 158, 11)
#define COLOR_ERROR        RGB(239, 68, 68)
#define COLOR_BORDER       RGB(51, 65, 85)
#define COLOR_WS_A         RGB(139, 92, 246)
#define COLOR_WS_B         RGB(16, 185, 129)
#define COLOR_WS_C         RGB(245, 158, 11)
#define COLOR_WS_D         RGB(239, 68, 68)
#define COLOR_WS_E         RGB(59, 130, 246)

#define SIDEBAR_WIDTH      210
#define HEADER_HEIGHT      56
#define CONTENT_PADDING    20
#define STATUS_HEIGHT      36

// ============================================================
// GLOBAL VARIABLES
// ============================================================
InputManager* g_inputManager = nullptr;
WindowMover* g_windowMover = nullptr;
DisplayManager* g_displayManager = nullptr;
WorkspaceManager* g_workspaceManager = nullptr;
DriverInterface* g_driverInterface = nullptr;
InputRouter* g_inputRouter = nullptr;
CursorEmulator* g_cursorEmulator = nullptr;

HWND g_hMainWnd = nullptr;
HWND g_hOverlayWnd = nullptr;
HFONT g_hFontTitle = nullptr;
HFONT g_hFontSub = nullptr;
HFONT g_hFontStatus = nullptr;
HFONT g_hFontSmall = nullptr;
HBRUSH g_hBrushBg = nullptr;
HBRUSH g_hBrushCard = nullptr;
HBRUSH g_hBrushAccent = nullptr;
EitherMousePlugin& g_eitherMouse = EitherMousePlugin::GetInstance();
MonitorBorder& g_monitorBorder = MonitorBorder::GetInstance();

bool g_running = true;
bool g_driverConnected = false;
std::string g_lastEvent = "Ready";
int g_hoveredWorkspace = -1;
int g_hoveredDevice = -1;

std::map<HANDLE, DWORD> g_deviceWorkspaceMap;
std::map<HANDLE, std::string> g_deviceNames;
std::vector<HANDLE> g_availableDevices;

int g_currentView = 0;
int g_selectedWorkspace = -1;
int g_selectedDevice = -1;
int g_deviceScrollOffset = 0;
int g_maxDeviceScrollOffset = 0;

struct WorkspaceInfo {
    ULONG id;
    char letter;
    std::string name;
    int windowCount;
    int deviceCount;
    COLORREF color;
};
std::vector<WorkspaceInfo> g_workspaces;

// ============================================================
// FORWARD DECLARATIONS
// ============================================================
void RefreshUI();
void FillRoundedRect(HDC hdc, RECT rect, COLORREF fill, COLORREF border, int radius);
void FillSolidRect(HDC hdc, RECT rect, COLORREF color);
void DrawTextEx(HDC hdc, const std::wstring& text, RECT rect, UINT format, HFONT font, COLORREF color);
void ShowWorkspaceContextMenu(HWND hWnd, int wsIndex, POINT pt);
void ShowDeviceContextMenu(HWND hWnd, int deviceIndex, POINT pt);
void AssignDeviceToWorkspace(HANDLE deviceHandle, DWORD workspaceId);
void UnassignDevice(HANDLE deviceHandle);
void SwitchToDevicesView(int workspaceIndex);
void DrawSidebar(HDC hdc, RECT rect);
void DrawStatusBar(HDC hdc, RECT rect);
void DrawStatCard(HDC hdc, RECT rect, const wchar_t* label, int value, COLORREF color);
void DrawWorkspaceCard(HDC hdc, RECT rect, const WorkspaceInfo& ws, bool hovered);
void DrawDevicesView(HDC hdc, RECT rect);
void CreateOverlayWindow(HINSTANCE hInstance);
void InitializeMonitorBorders();
void InstallEitherMouse();
void LaunchEitherMouse();

LRESULT CALLBACK MainWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK InputWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK OverlayWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// ============================================================
// DRAWING HELPERS
// ============================================================
void FillRoundedRect(HDC hdc, RECT rect, COLORREF fill, COLORREF border, int radius) {
    HBRUSH brush = CreateSolidBrush(fill);
    HPEN pen = CreatePen(PS_SOLID, 1, border);
    HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, brush);
    HPEN oldPen = (HPEN)SelectObject(hdc, pen);
    RoundRect(hdc, rect.left, rect.top, rect.right, rect.bottom, radius, radius);
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(brush);
    DeleteObject(pen);
}

void FillSolidRect(HDC hdc, RECT rect, COLORREF color) {
    HBRUSH brush = CreateSolidBrush(color);
    FillRect(hdc, &rect, brush);
    DeleteObject(brush);
}

void DrawTextEx(HDC hdc, const std::wstring& text, RECT rect, UINT format, HFONT font, COLORREF color) {
    SelectObject(hdc, font);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, color);
    DrawTextW(hdc, text.c_str(), (int)text.length(), &rect, format | DT_END_ELLIPSIS);
}

// ============================================================
// HELPERS
// ============================================================
std::wstring ToWide(const std::string& s) {
    return std::wstring(s.begin(), s.end());
}

std::string ToNarrow(const std::wstring& s) {
    return std::string(s.begin(), s.end());
}

std::string GetCleanDeviceName(const std::wstring& deviceName) {
    std::string name;
    for (wchar_t c : deviceName) { if (c < 128) name += (char)c; }
    std::string result = name;
    size_t pos = result.find_last_of("\\#");
    if (pos != std::string::npos && pos + 1 < result.length()) { result = result.substr(pos + 1); }
    
    std::vector<std::string> removeSuffixes = {
        "&Col01", "&Col02", "&Col03", "&Col04", "&MI_00", "&MI_01", "&MI_02", "&MI_03",
        "&0000", "&0001", "&0002", "&0003", "#", "_"
    };
    for (const auto& suffix : removeSuffixes) {
        size_t found = result.find(suffix);
        if (found != std::string::npos) { result = result.substr(0, found); }
    }
    while (!result.empty() && (result.back() == '&' || result.back() == '#' || 
                               result.back() == '_' || result.back() == '\\')) {
        result.pop_back();
    }
    
    if (result.empty() || result.length() < 3 || result == "Keyboard" || result == "Mouse" ||
        result == "HID" || result == "USB") {
        std::string lowerName = name;
        for (auto& c : lowerName) c = tolower(c);
        size_t vidPos = lowerName.find("vid_");
        if (vidPos != std::string::npos) {
            std::string vid = name.substr(vidPos + 4, 4);
            size_t pidPos = lowerName.find("pid_");
            if (pidPos != std::string::npos) {
                std::string pid = name.substr(pidPos + 4, 4);
                result = "Device (VID:" + vid + ", PID:" + pid + ")";
            }
        }
        if (result.empty() || result == "Keyboard" || result == "Mouse") {
            if (lowerName.find("keyboard") != std::string::npos) { result = "USB Keyboard"; }
            else if (lowerName.find("mouse") != std::string::npos) { result = "USB Mouse"; }
            else if (lowerName.find("touchpad") != std::string::npos) { result = "Touchpad"; }
            else if (lowerName.find("bluetooth") != std::string::npos) { result = "Bluetooth Device"; }
        }
        if (result.empty() || result == "Keyboard" || result == "Mouse") {
            static int deviceCounter = 0; deviceCounter++;
            result = "Device " + std::to_string(deviceCounter);
        }
    }
    if (result.length() > 22) { result = result.substr(0, 19) + "..."; }
    return result;
}

bool IsDriverConnected() {
    HANDLE hDriver = CreateFileA("\\\\.\\DualDesk", GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (hDriver != INVALID_HANDLE_VALUE) { CloseHandle(hDriver); return true; }
    return false;
}

void UpdateDeviceCache() {
    if (!g_inputManager) return;
    auto keyboards = g_inputManager->GetKeyboards();
    auto mice = g_inputManager->GetMice();
    g_availableDevices.clear();
    for (const auto& kb : keyboards) {
        g_availableDevices.push_back(kb.deviceHandle);
        g_deviceNames[kb.deviceHandle] = GetCleanDeviceName(kb.deviceName);
    }
    for (const auto& mouse : mice) {
        g_availableDevices.push_back(mouse.deviceHandle);
        g_deviceNames[mouse.deviceHandle] = GetCleanDeviceName(mouse.deviceName);
    }
}

void UpdateWorkspaceInfo() {
    g_workspaces.clear();
    if (!g_workspaceManager) return;
    
    auto workspaces = g_workspaceManager->GetAllWorkspaces();
    COLORREF colors[] = {COLOR_WS_A, COLOR_WS_B, COLOR_WS_C, COLOR_WS_D, COLOR_WS_E};
    
    std::map<HMONITOR, int> windowCounts;
    EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
        auto* counts = reinterpret_cast<std::map<HMONITOR, int>*>(lParam);
        if (IsWindowVisible(hwnd) && !IsIconic(hwnd)) {
            wchar_t title[256];
            if (GetWindowTextW(hwnd, title, 256) > 0 && wcslen(title) > 0) {
                wchar_t cls[256];
                GetClassNameW(hwnd, cls, 256);
                std::wstring clsStr(cls);
                if (clsStr != L"Progman" && clsStr != L"WorkerW" && 
                    clsStr != L"Shell_TrayWnd" && clsStr != L"Button") {
                    HMONITOR mon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
                    (*counts)[mon]++;
                }
            }
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&windowCounts));
    
    std::map<DWORD, int> deviceCounts;
    for (const auto& pair : g_deviceWorkspaceMap) {
        deviceCounts[pair.second]++;
    }
    
    for (size_t i = 0; i < workspaces.size(); i++) {
        auto* ws = workspaces[i];
        WorkspaceInfo info;
        info.id = ws->GetId();
        info.letter = (char)('A' + i);
        info.name = "Workspace " + std::string(1, info.letter);
        info.color = colors[i % 5];
        info.windowCount = 0;
        info.deviceCount = 0;
        
        HMONITOR wsMon = ws->GetMonitor();
        auto it = windowCounts.find(wsMon);
        if (it != windowCounts.end()) {
            info.windowCount = it->second;
        }
        
        auto dit = deviceCounts.find(info.id);
        if (dit != deviceCounts.end()) {
            info.deviceCount = dit->second;
        }
        
        g_workspaces.push_back(info);
    }
}

void RefreshUI() {
    UpdateWorkspaceInfo();
    UpdateDeviceCache();
    InvalidateRect(g_hMainWnd, NULL, TRUE);
    UpdateWindow(g_hMainWnd);
}

void SwitchToDevicesView(int workspaceIndex) {
    g_selectedWorkspace = workspaceIndex;
    g_currentView = 2;
    InvalidateRect(g_hMainWnd, NULL, TRUE);
    UpdateWindow(g_hMainWnd);
}

// ============================================================
// CONTEXT MENU FUNCTIONS
// ============================================================
void ShowWorkspaceContextMenu(HWND hWnd, int wsIndex, POINT pt) {
    if (wsIndex < 0 || wsIndex >= (int)g_workspaces.size()) return;
    
    WorkspaceInfo& ws = g_workspaces[wsIndex];
    HMENU hMenu = CreatePopupMenu();
    
    std::wstring headerText = L"Workspace: " + ToWide(ws.name);
    AppendMenuW(hMenu, MF_STRING | MF_GRAYED, 0, headerText.c_str());
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenu, MF_STRING, 1002, L"Assign Device...");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    
    int deviceCount = 0;
    for (const auto& pair : g_deviceWorkspaceMap) {
        if (pair.second == ws.id) deviceCount++;
    }
    
    if (deviceCount > 0) {
        std::wstring countText = L"Devices assigned: " + std::to_wstring(deviceCount);
        AppendMenuW(hMenu, MF_STRING | MF_GRAYED, 0, countText.c_str());
        AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
        AppendMenuW(hMenu, MF_STRING, 1003, L"Unassign All Devices");
    } else {
        AppendMenuW(hMenu, MF_STRING | MF_GRAYED, 0, L"No devices assigned");
    }
    
    int selection = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_RIGHTBUTTON, 
                                   pt.x, pt.y, 0, hWnd, NULL);
    DestroyMenu(hMenu);
    
    if (selection == 1002) {
        SwitchToDevicesView(wsIndex);
    } else if (selection == 1003) {
        std::vector<HANDLE> toRemove;
        for (const auto& pair : g_deviceWorkspaceMap) {
            if (pair.second == ws.id) toRemove.push_back(pair.first);
        }
        for (HANDLE handle : toRemove) {
            g_deviceWorkspaceMap.erase(handle);
            if (g_driverInterface && g_driverInterface->IsConnected()) {
                g_driverInterface->UnassignDevice(handle);
            }
        }
        g_lastEvent = "All devices unassigned from " + ws.name;
        RefreshUI();
        MessageBoxA(hWnd, ("All devices unassigned from " + ws.name).c_str(), "Done", MB_OK);
    }
}

void ShowDeviceContextMenu(HWND hWnd, int deviceIndex, POINT pt) {
    if (deviceIndex < 0 || deviceIndex >= (int)g_availableDevices.size()) return;
    
    HANDLE handle = g_availableDevices[deviceIndex];
    std::string deviceName = g_deviceNames[handle];
    bool assigned = g_deviceWorkspaceMap.find(handle) != g_deviceWorkspaceMap.end();
    DWORD currentWs = assigned ? g_deviceWorkspaceMap[handle] : 0;
    
    HMENU hMenu = CreatePopupMenu();
    std::wstring headerText = L"Device: " + ToWide(deviceName);
    AppendMenuW(hMenu, MF_STRING | MF_GRAYED, 0, headerText.c_str());
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    
    if (assigned) {
        std::wstring wsName = L"Unknown";
        for (const auto& ws : g_workspaces) {
            if (ws.id == currentWs) { wsName = ToWide(ws.name); break; }
        }
        AppendMenuW(hMenu, MF_STRING | MF_GRAYED, 0, (L"Currently: " + wsName).c_str());
        AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
        AppendMenuW(hMenu, MF_STRING, 3001, L"Unassign Device");
        AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    }
    
    AppendMenuW(hMenu, MF_STRING | MF_GRAYED, 0, L"Assign to:");
    for (const auto& ws : g_workspaces) {
        if (!assigned || ws.id != currentWs) {
            std::wstring label = L"  " + ToWide(ws.name);
            AppendMenuW(hMenu, MF_STRING, 4000 + ws.id, label.c_str());
        }
    }
    
    int selection = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_RIGHTBUTTON, 
                                   pt.x, pt.y, 0, hWnd, NULL);
    DestroyMenu(hMenu);
    
    if (selection == 3001) {
        UnassignDevice(handle);
    } else if (selection >= 4000 && selection < 5000) {
        DWORD wsId = selection - 4000;
        if (assigned) UnassignDevice(handle);
        AssignDeviceToWorkspace(handle, wsId);
    }
}

void AssignDeviceToWorkspace(HANDLE deviceHandle, DWORD workspaceId) {
    if (!g_driverInterface) {
        MessageBoxA(g_hMainWnd, "Driver interface is NULL!", "Error", MB_OK | MB_ICONERROR);
        return;
    }
    
    if (!g_driverInterface->IsConnected()) {
        if (!g_driverInterface->Open()) {
            MessageBoxA(g_hMainWnd, "Driver not connected!", "Error", MB_OK | MB_ICONERROR);
            return;
        }
    }
    
    bool result = g_driverInterface->AssignDeviceToWorkspace(deviceHandle, workspaceId);
    if (result) {
        g_deviceWorkspaceMap[deviceHandle] = workspaceId;
        std::string wsName = "Workspace";
        for (const auto& ws : g_workspaces) {
            if (ws.id == workspaceId) { wsName = ws.name; break; }
        }
        g_lastEvent = "Device assigned to " + wsName;
        RefreshUI();
    } else {
        MessageBoxA(g_hMainWnd, "Failed to assign device!", "Error", MB_OK | MB_ICONERROR);
    }
}

void UnassignDevice(HANDLE deviceHandle) {
    g_deviceWorkspaceMap.erase(deviceHandle);
    if (g_driverInterface && g_driverInterface->IsConnected()) {
        g_driverInterface->UnassignDevice(deviceHandle);
    }
    g_lastEvent = "Device unassigned";
    RefreshUI();
}

// ============================================================
// UI DRAW FUNCTIONS
// ============================================================
void DrawSidebar(HDC hdc, RECT rect) {
    FillSolidRect(hdc, rect, COLOR_BG_CARD);
    
    DrawTextEx(hdc, L"[DD] DualDesk", {14, 14, rect.right - 14, 50}, 
               DT_LEFT | DT_VCENTER | DT_SINGLELINE, g_hFontTitle, COLOR_ACCENT);
    
    struct NavItem { const wchar_t* label; int view; };
    NavItem views[] = {
        {L"[D] Dashboard", 0},
        {L"[W] Workspaces", 1},
        {L"[K] Devices", 2},
        {L"[S] Settings", 3}
    };
    
    int y = 64;
    for (int i = 0; i < 4; i++) {
        RECT itemRect = {12, y, rect.right - 12, y + 38};
        if (i == g_currentView) {
            FillRoundedRect(hdc, itemRect, COLOR_BG_DARK, COLOR_ACCENT, 8);
            DrawTextEx(hdc, views[i].label, {itemRect.left + 12, itemRect.top, itemRect.right, itemRect.bottom}, 
                       DT_LEFT | DT_VCENTER | DT_SINGLELINE, g_hFontSub, COLOR_TEXT_MAIN);
        } else {
            DrawTextEx(hdc, views[i].label, {itemRect.left + 12, itemRect.top, itemRect.right, itemRect.bottom}, 
                       DT_LEFT | DT_VCENTER | DT_SINGLELINE, g_hFontStatus, COLOR_TEXT_SUB);
        }
        y += 42;
    }
}

void DrawStatusBar(HDC hdc, RECT rect) {
    FillSolidRect(hdc, rect, COLOR_BG_CARD);
    FillSolidRect(hdc, {rect.left, rect.top, rect.right, rect.top + 1}, COLOR_BORDER);
    
    COLORREF driverColor = g_driverConnected ? COLOR_SUCCESS : COLOR_ERROR;
    std::wstring status = L"Driver: " + std::wstring(g_driverConnected ? L"Connected" : L"Disconnected");
    DrawTextEx(hdc, status, {rect.left + 16, rect.top + 4, rect.left + 180, rect.bottom}, 
               DT_LEFT | DT_VCENTER | DT_SINGLELINE, g_hFontSmall, driverColor);
    
    std::wstring eventText = L"Event: " + ToWide(g_lastEvent);
    DrawTextEx(hdc, eventText, {rect.left + 200, rect.top + 4, rect.left + 500, rect.bottom}, 
               DT_LEFT | DT_VCENTER | DT_SINGLELINE, g_hFontSmall, COLOR_TEXT_SUB);
    
    wchar_t devCount[80];
    swprintf_s(devCount, 80, L"Devices: %d", (int)g_availableDevices.size());
    DrawTextEx(hdc, devCount, {rect.right - 160, rect.top + 4, rect.right - 16, rect.bottom}, 
               DT_RIGHT | DT_VCENTER | DT_SINGLELINE, g_hFontSmall, COLOR_TEXT_SUB);
}

void DrawStatCard(HDC hdc, RECT rect, const wchar_t* label, int value, COLORREF color) {
    FillRoundedRect(hdc, rect, COLOR_BG_CARD, COLOR_BORDER, 10);
    
    wchar_t valueStr[32];
    swprintf_s(valueStr, 32, L"%d", value);
    
    DrawTextEx(hdc, label, {rect.left + 16, rect.top + 10, rect.right - 16, rect.top + 32}, 
               DT_LEFT | DT_VCENTER | DT_SINGLELINE, g_hFontStatus, COLOR_TEXT_SUB);
    DrawTextEx(hdc, valueStr, {rect.left + 16, rect.top + 34, rect.right - 16, rect.bottom - 8}, 
               DT_LEFT | DT_VCENTER | DT_SINGLELINE, g_hFontTitle, color);
}

void DrawWorkspaceCard(HDC hdc, RECT rect, const WorkspaceInfo& ws, bool hovered) {
    COLORREF borderColor = hovered ? COLOR_ACCENT : COLOR_BORDER;
    FillRoundedRect(hdc, rect, COLOR_BG_CARD, borderColor, 10);
    
    wchar_t letter[2] = { (wchar_t)ws.letter, 0 };
    RECT badgeRect = {rect.left + 14, rect.top + 14, rect.left + 46, rect.top + 46};
    FillRoundedRect(hdc, badgeRect, ws.color, ws.color, 6);
    DrawTextEx(hdc, letter, badgeRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE, g_hFontSub, RGB(255,255,255));
    
    DrawTextEx(hdc, ToWide(ws.name), {badgeRect.right + 10, rect.top + 14, rect.right - 14, rect.top + 40}, 
               DT_LEFT | DT_VCENTER | DT_SINGLELINE, g_hFontSub, COLOR_TEXT_MAIN);
    
    wchar_t stats[64];
    swprintf_s(stats, 64, L"Windows: %d    Devices: %d", ws.windowCount, ws.deviceCount);
    DrawTextEx(hdc, stats, {rect.left + 14, rect.top + 52, rect.right - 14, rect.bottom - 14}, 
               DT_LEFT | DT_VCENTER | DT_SINGLELINE, g_hFontStatus, COLOR_TEXT_SUB);
    
    if (hovered) {
        DrawTextEx(hdc, L"Right-click for options", {rect.left + 14, rect.bottom - 18, rect.right - 14, rect.bottom - 6}, 
                   DT_CENTER | DT_VCENTER | DT_SINGLELINE, g_hFontSmall, COLOR_ACCENT);
    }
}

void DrawDevicesView(HDC hdc, RECT rect) {
    int y = rect.top + 14;
    int lineHeight = 34;
    int startIdx = g_deviceScrollOffset;
    
    if (g_selectedWorkspace >= 0 && g_selectedWorkspace < (int)g_workspaces.size()) {
        std::wstring headerText = L"Devices - Assigning to " + ToWide(g_workspaces[g_selectedWorkspace].name);
        DrawTextEx(hdc, headerText, {rect.left + 14, y, rect.right - 14, y + 32}, 
                   DT_LEFT | DT_VCENTER | DT_SINGLELINE, g_hFontSub, COLOR_TEXT_MAIN);
    } else {
        DrawTextEx(hdc, L"Devices", {rect.left + 14, y, rect.right - 14, y + 32}, 
                   DT_LEFT | DT_VCENTER | DT_SINGLELINE, g_hFontSub, COLOR_TEXT_MAIN);
    }
    y += 38;
    
    RECT headerRect = {rect.left + 14, y, rect.right - 14, y + 28};
    FillSolidRect(hdc, headerRect, COLOR_BG_DARK);
    DrawTextEx(hdc, L"Device", {headerRect.left + 10, headerRect.top, headerRect.left + 220, headerRect.bottom}, 
               DT_LEFT | DT_VCENTER | DT_SINGLELINE, g_hFontSmall, COLOR_TEXT_SUB);
    DrawTextEx(hdc, L"Workspace", {headerRect.left + 230, headerRect.top, headerRect.left + 370, headerRect.bottom}, 
               DT_LEFT | DT_VCENTER | DT_SINGLELINE, g_hFontSmall, COLOR_TEXT_SUB);
    DrawTextEx(hdc, L"Action", {headerRect.right - 120, headerRect.top, headerRect.right - 10, headerRect.bottom}, 
               DT_RIGHT | DT_VCENTER | DT_SINGLELINE, g_hFontSmall, COLOR_TEXT_SUB);
    
    y += 32;
    
    int visibleItems = (rect.bottom - y - 12) / lineHeight;
    if (visibleItems < 1) visibleItems = 1;
    g_maxDeviceScrollOffset = (int)g_availableDevices.size() - visibleItems;
    if (g_maxDeviceScrollOffset < 0) g_maxDeviceScrollOffset = 0;
    if (g_deviceScrollOffset > g_maxDeviceScrollOffset) g_deviceScrollOffset = g_maxDeviceScrollOffset;
    
    for (int i = startIdx; i < (int)g_availableDevices.size() && i < startIdx + visibleItems; i++) {
        HANDLE handle = g_availableDevices[i];
        std::string name = g_deviceNames[handle];
        bool assigned = g_deviceWorkspaceMap.find(handle) != g_deviceWorkspaceMap.end();
        DWORD wsId = assigned ? g_deviceWorkspaceMap[handle] : -1;
        
        std::string wsName = "Unassigned";
        COLORREF wsColor = COLOR_WARNING;
        if (assigned) {
            for (const auto& ws : g_workspaces) {
                if (ws.id == wsId) { wsName = ws.name; wsColor = COLOR_SUCCESS; break; }
            }
        }
        
        RECT itemRect = {rect.left + 14, y, rect.right - 14, y + lineHeight};
        bool hovered = (i == g_hoveredDevice);
        if (hovered || i == g_selectedDevice) {
            FillSolidRect(hdc, itemRect, COLOR_BG_CARD_HOVER);
        }
        
        RECT dotRect = {itemRect.left + 6, y + 12, itemRect.left + 16, y + 22};
        FillRoundedRect(hdc, dotRect, assigned ? COLOR_SUCCESS : COLOR_WARNING, 
                       assigned ? COLOR_SUCCESS : COLOR_WARNING, 4);
        
        DrawTextEx(hdc, ToWide(name), {itemRect.left + 22, itemRect.top, itemRect.left + 210, itemRect.bottom}, 
                   DT_LEFT | DT_VCENTER | DT_SINGLELINE, g_hFontStatus, COLOR_TEXT_MAIN);
        DrawTextEx(hdc, ToWide(wsName), {itemRect.left + 222, itemRect.top, itemRect.left + 360, itemRect.bottom}, 
                   DT_LEFT | DT_VCENTER | DT_SINGLELINE, g_hFontStatus, wsColor);
        
        if (hovered) {
            std::wstring action = assigned ? L"Right-click to unassign" : L"Right-click to assign";
            DrawTextEx(hdc, action, {itemRect.right - 180, itemRect.top, itemRect.right - 10, itemRect.bottom}, 
                       DT_RIGHT | DT_VCENTER | DT_SINGLELINE, g_hFontSmall, COLOR_ACCENT);
        } else {
            std::wstring action = assigned ? L"Unassign" : L"Assign";
            DrawTextEx(hdc, action, {itemRect.right - 120, itemRect.top, itemRect.right - 10, itemRect.bottom}, 
                       DT_RIGHT | DT_VCENTER | DT_SINGLELINE, g_hFontStatus, COLOR_ACCENT);
        }
        
        y += lineHeight;
    }
    
    if (g_availableDevices.empty()) {
        RECT emptyRect = {rect.left + 16, headerRect.bottom + 24, rect.right - 16, headerRect.bottom + 80};
        DrawTextEx(hdc, L"No input devices detected", emptyRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE, 
                   g_hFontSub, COLOR_TEXT_MUTED);
    }
}

// ============================================================
// EITHERMOUSE MANAGEMENT
// ============================================================

void InstallEitherMouse() {
    // Check if EitherMouse is already installed
    bool isInstalled = false;
    std::vector<std::wstring> installPaths = {
        L"C:\\Program Files\\EitherMouse\\EitherMouse.exe",
        L"C:\\Program Files (x86)\\EitherMouse\\EitherMouse.exe",
    };
    
    for (const auto& path : installPaths) {
        DWORD attrib = GetFileAttributesW(path.c_str());
        if (attrib != INVALID_FILE_ATTRIBUTES) {
            isInstalled = true;
            break;
        }
    }
    
    if (isInstalled) {
        LOG_INFO("EitherMouse already installed");
        g_lastEvent = "EitherMouse already installed";
        return;
    }
    
    // Find the installer
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(NULL, exePath, MAX_PATH);
    std::wstring exeDir = exePath;
    exeDir = exeDir.substr(0, exeDir.find_last_of(L"\\"));
    
    std::wstring installerPaths[] = {
        exeDir + L"\\EitherMouse_Setup.exe",
        exeDir + L"\\..\\EitherMouse_Setup.exe",
    };
    
    std::wstring installerPath;
    bool found = false;
    
    for (const auto& path : installerPaths) {
        DWORD attrib = GetFileAttributesW(path.c_str());
        if (attrib != INVALID_FILE_ATTRIBUTES) {
            installerPath = path;
            found = true;
            break;
        }
    }
    
    if (!found) {
        LOG_WARN("EitherMouse installer not found!");
        return;
    }
    
    // Install silently
    std::wstring cmdLine = L"\"" + installerPath + L"\" /S /VERYSILENT /SUPPRESSMSGBOXES /NORESTART";
    
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi = { 0 };
    
    BOOL result = CreateProcessW(
        NULL,
        (LPWSTR)cmdLine.c_str(),
        NULL,
        NULL,
        FALSE,
        CREATE_NO_WINDOW,
        NULL,
        NULL,
        &si,
        &pi
    );
    
    if (result) {
        LOG_INFO("EitherMouse installer launched");
        WaitForSingleObject(pi.hProcess, 30000);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        Sleep(2000);
    }
}

void LaunchEitherMouse() {
    if (g_eitherMouse.IsEitherMouseRunning()) {
        LOG_INFO("EitherMouse already running");
        return;
    }
    
    std::vector<std::wstring> installPaths = {
        L"C:\\Program Files\\EitherMouse\\EitherMouse.exe",
        L"C:\\Program Files (x86)\\EitherMouse\\EitherMouse.exe",
    };
    
    std::wstring exePath;
    bool found = false;
    
    for (const auto& path : installPaths) {
        DWORD attrib = GetFileAttributesW(path.c_str());
        if (attrib != INVALID_FILE_ATTRIBUTES) {
            exePath = path;
            found = true;
            break;
        }
    }
    
    if (!found) {
        InstallEitherMouse();
        Sleep(2000);
        for (const auto& path : installPaths) {
            DWORD attrib = GetFileAttributesW(path.c_str());
            if (attrib != INVALID_FILE_ATTRIBUTES) {
                exePath = path;
                found = true;
                break;
            }
        }
        if (!found) return;
    }
    
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi = { 0 };
    
    wchar_t cmdLine[MAX_PATH * 2];
    swprintf_s(cmdLine, L"\"%s\"", exePath.c_str());
    
    BOOL result = CreateProcessW(
        exePath.c_str(),
        cmdLine,
        NULL,
        NULL,
        FALSE,
        0,
        NULL,
        NULL,
        &si,
        &pi
    );
    
    if (result) {
        LOG_INFO("EitherMouse launched");
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        Sleep(1000);
    }
}
// ============================================================
// MONITOR BORDER MANAGEMENT - Using Windows API Directly
// ============================================================

void InitializeMonitorBorders() {
    std::vector<RECT> monitors;
    
    // Use Windows EnumDisplayMonitors directly
    EnumDisplayMonitors(NULL, NULL, [](HMONITOR hMon, HDC hdc, LPRECT rect, LPARAM lParam) -> BOOL {
        auto* monitors = reinterpret_cast<std::vector<RECT>*>(lParam);
        MONITORINFOEXW info;
        info.cbSize = sizeof(MONITORINFOEXW);
        if (GetMonitorInfoW(hMon, &info)) {
            monitors->push_back(info.rcMonitor);
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&monitors));
    
    g_monitorBorder.Initialize(monitors);
    g_monitorBorder.SetEnabled(true);
    
    LOG_INFO("Monitor borders initialized with %d monitors", (int)monitors.size());
}
// ============================================================
// CREATE OVERLAY WINDOW
// ============================================================

void CreateOverlayWindow(HINSTANCE hInstance) {
    WNDCLASSEXW wcOverlay = {0};
    wcOverlay.cbSize = sizeof(WNDCLASSEXW);
    wcOverlay.lpfnWndProc = OverlayWndProc;
    wcOverlay.hInstance = hInstance;
    wcOverlay.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
    wcOverlay.lpszClassName = L"DualDeskOverlayClass";
    RegisterClassExW(&wcOverlay);

    int vLeft = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int vTop = GetSystemMetrics(SM_YVIRTUALSCREEN);
    int vWidth = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int vHeight = GetSystemMetrics(SM_CYVIRTUALSCREEN);

    g_hOverlayWnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED | WS_EX_NOACTIVATE,
        L"DualDeskOverlayClass", L"", WS_POPUP,
        vLeft, vTop, vWidth, vHeight, NULL, NULL, hInstance, NULL);
    SetLayeredWindowAttributes(g_hOverlayWnd, RGB(0, 0, 0), 0, LWA_COLORKEY);
    ShowWindow(g_hOverlayWnd, SW_SHOWNOACTIVATE);
}

// ============================================================
// OVERLAY WINDOW PROCEDURE
// ============================================================

LRESULT CALLBACK OverlayWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            RECT rc;
            GetClientRect(hWnd, &rc);
            
            // Black background for transparency
            HBRUSH blackBrush = CreateSolidBrush(RGB(0, 0, 0));
            FillRect(hdc, &rc, blackBrush);
            DeleteObject(blackBrush);
            
            // Draw virtual cursors
            if (g_cursorEmulator) {
                auto* vcManager = g_cursorEmulator->GetVirtualCursorManager();
                if (vcManager) {
                    vcManager->Render(hdc);
                }
            }
            
            // Draw monitor border walls
            g_monitorBorder.DrawBorders(hdc, rc);
            
            EndPaint(hWnd, &ps);
            return 0;
        }
        case WM_ERASEBKGND:
            return 1;
        default:
            return DefWindowProc(hWnd, msg, wParam, lParam);
    }
}

// ============================================================
// INPUT WINDOW PROCEDURE
// ============================================================

LRESULT CALLBACK InputWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_INPUT) {
        if (g_inputManager) {
            HRAWINPUT handle = reinterpret_cast<HRAWINPUT>(lParam);
            g_inputManager->ProcessRawInput(handle);
        }
        return 0;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

// ============================================================
// MAIN WINDOW PROCEDURE
// ============================================================

LRESULT CALLBACK MainWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            g_hFontTitle = CreateFontW(24, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
            g_hFontSub = CreateFontW(16, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
            g_hFontStatus = CreateFontW(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
            g_hFontSmall = CreateFontW(12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
            
            g_hBrushBg = CreateSolidBrush(COLOR_BG_DARK);
            g_hBrushCard = CreateSolidBrush(COLOR_BG_CARD);
            g_hBrushAccent = CreateSolidBrush(COLOR_ACCENT);
            
            UpdateWorkspaceInfo();
            UpdateDeviceCache();
            SetTimer(hWnd, 1, 1000, NULL);
            return 0;
        }
        
        case WM_INPUT: {
            if (g_inputManager) {
                HRAWINPUT handle = reinterpret_cast<HRAWINPUT>(lParam);
                g_inputManager->ProcessRawInput(handle);
            }
            return 0;
        }
        
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            
            RECT rc;
            GetClientRect(hWnd, &rc);
            
            FillSolidRect(hdc, rc, COLOR_BG_DARK);
            
            RECT sidebar = {0, 0, SIDEBAR_WIDTH, rc.bottom};
            DrawSidebar(hdc, sidebar);
            
            int contentLeft = SIDEBAR_WIDTH + 20;
            int contentRight = rc.right - 20;
            int contentTop = HEADER_HEIGHT + 20;
            int contentBottom = rc.bottom - STATUS_HEIGHT - 20;
            
            const wchar_t* viewNames[] = {L"Dashboard", L"Workspaces", L"Devices", L"Settings"};
            DrawTextEx(hdc, viewNames[g_currentView], 
                       {contentLeft, 10, contentRight, HEADER_HEIGHT}, 
                       DT_LEFT | DT_VCENTER | DT_SINGLELINE, g_hFontTitle, COLOR_TEXT_MAIN);
            
            if (g_currentView == 0) {
                int statWidth = (contentRight - contentLeft - 28) / 4;
                int assignedCount = (int)g_deviceWorkspaceMap.size();
                int totalWindows = 0;
                for (const auto& ws : g_workspaces) totalWindows += ws.windowCount;
                
                RECT stat1 = {contentLeft, contentTop, contentLeft + statWidth, contentTop + 80};
                RECT stat2 = {stat1.right + 10, contentTop, stat1.right + 10 + statWidth, contentTop + 80};
                RECT stat3 = {stat2.right + 10, contentTop, stat2.right + 10 + statWidth, contentTop + 80};
                RECT stat4 = {stat3.right + 10, contentTop, stat3.right + 10 + statWidth, contentTop + 80};
                
                DrawStatCard(hdc, stat1, L"Workspaces", (int)g_workspaces.size(), COLOR_ACCENT);
                DrawStatCard(hdc, stat2, L"Devices", (int)g_availableDevices.size(), COLOR_SUCCESS);
                DrawStatCard(hdc, stat3, L"Assigned", assignedCount, COLOR_WARNING);
                DrawStatCard(hdc, stat4, L"Windows", totalWindows, COLOR_WS_E);
                
                int gridY = contentTop + 96;
                int cardWidth = (contentRight - contentLeft - 28) / 3;
                if (cardWidth < 160) cardWidth = 160;
                if (cardWidth > 200) cardWidth = 200;
                int cardsPerRow = (contentRight - contentLeft) / (cardWidth + 14);
                if (cardsPerRow < 1) cardsPerRow = 1;
                
                for (size_t i = 0; i < g_workspaces.size() && i < 6; i++) {
                    int row = i / cardsPerRow;
                    int col = i % cardsPerRow;
                    int x = contentLeft + col * (cardWidth + 14);
                    int y = gridY + row * (110 + 14);
                    RECT cardRect = {x, y, x + cardWidth, y + 106};
                    DrawWorkspaceCard(hdc, cardRect, g_workspaces[i], g_hoveredWorkspace == (int)i);
                }
            } else if (g_currentView == 1) {
                int cardWidth = 180;
                int cardsPerRow = (contentRight - contentLeft) / (cardWidth + 14);
                if (cardsPerRow < 1) cardsPerRow = 1;
                
                for (size_t i = 0; i < g_workspaces.size(); i++) {
                    int row = i / cardsPerRow;
                    int col = i % cardsPerRow;
                    int x = contentLeft + col * (cardWidth + 14);
                    int y = contentTop + row * (110 + 14);
                    RECT cardRect = {x, y, x + cardWidth, y + 106};
                    DrawWorkspaceCard(hdc, cardRect, g_workspaces[i], g_hoveredWorkspace == (int)i);
                }
            } else if (g_currentView == 2) {
                RECT deviceRect = {contentLeft, contentTop, contentRight, contentBottom};
                DrawDevicesView(hdc, deviceRect);
            } else if (g_currentView == 3) {
                int y = contentTop;
                RECT itemRect = {contentLeft, y, contentRight, y + 44};
                FillRoundedRect(hdc, itemRect, COLOR_BG_CARD, COLOR_BORDER, 8);
                DrawTextEx(hdc, L"Driver Status", {itemRect.left + 16, itemRect.top, itemRect.left + 160, itemRect.bottom}, 
                           DT_LEFT | DT_VCENTER | DT_SINGLELINE, g_hFontStatus, COLOR_TEXT_MAIN);
                DrawTextEx(hdc, g_driverConnected ? L"Connected" : L"Disconnected", 
                           {itemRect.right - 140, itemRect.top, itemRect.right - 16, itemRect.bottom}, 
                           DT_RIGHT | DT_VCENTER | DT_SINGLELINE, g_hFontStatus, 
                           g_driverConnected ? COLOR_SUCCESS : COLOR_ERROR);
                
                y += 54;
                RECT itemRect2 = {contentLeft, y, contentRight, y + 44};
                FillRoundedRect(hdc, itemRect2, COLOR_BG_CARD, COLOR_BORDER, 8);
                DrawTextEx(hdc, L"Last Event", {itemRect2.left + 16, itemRect2.top, itemRect2.left + 160, itemRect2.bottom}, 
                           DT_LEFT | DT_VCENTER | DT_SINGLELINE, g_hFontStatus, COLOR_TEXT_MAIN);
                DrawTextEx(hdc, ToWide(g_lastEvent), 
                           {itemRect2.right - 320, itemRect2.top, itemRect2.right - 16, itemRect2.bottom}, 
                           DT_RIGHT | DT_VCENTER | DT_SINGLELINE, g_hFontStatus, COLOR_TEXT_SUB);
            }
            
            RECT statusRect = {SIDEBAR_WIDTH, rc.bottom - STATUS_HEIGHT, rc.right, rc.bottom};
            DrawStatusBar(hdc, statusRect);
            
            EndPaint(hWnd, &ps);
            return 0;
        }
        
        case WM_LBUTTONDOWN: {
            POINT pt;
            GetCursorPos(&pt);
            ScreenToClient(hWnd, &pt);
            
            if (pt.x < SIDEBAR_WIDTH) {
                int y = 64;
                for (int i = 0; i < 4; i++) {
                    RECT itemRect = {12, y, SIDEBAR_WIDTH - 12, y + 38};
                    if (pt.y >= itemRect.top && pt.y <= itemRect.bottom) {
                        g_currentView = i;
                        if (i != 2) g_selectedWorkspace = -1;
                        InvalidateRect(hWnd, NULL, TRUE);
                        return 0;
                    }
                    y += 42;
                }
                return 0;
            }
            return 0;
        }
        
        case WM_RBUTTONDOWN: {
            POINT pt;
            GetCursorPos(&pt);
            ScreenToClient(hWnd, &pt);
            
            RECT rc;
            GetClientRect(hWnd, &rc);
            int contentLeft = SIDEBAR_WIDTH + 20;
            int contentRight = rc.right - 20;
            int contentTop = HEADER_HEIGHT + 20;
            
            if (g_currentView == 0 || g_currentView == 1) {
                int cardWidth = (g_currentView == 0) ? 160 : 180;
                int cardHeight = 106;
                int spacing = 14;
                int availableWidth = contentRight - contentLeft;
                int cardsPerRow = availableWidth / (cardWidth + spacing);
                if (cardsPerRow < 1) cardsPerRow = 1;
                
                int startX = contentLeft;
                int yPos = (g_currentView == 0) ? contentTop + 96 : contentTop;
                int startY = yPos;
                
                size_t maxCards = (g_currentView == 0) ? std::min(g_workspaces.size(), size_t(6)) : g_workspaces.size();
                
                for (size_t i = 0; i < maxCards; i++) {
                    int row = i / cardsPerRow;
                    int col = i % cardsPerRow;
                    int x = startX + col * (cardWidth + spacing);
                    int y = startY + row * (cardHeight + spacing);
                    
                    RECT cardRect = {x, y, x + cardWidth, y + cardHeight};
                    if (pt.x >= cardRect.left && pt.x <= cardRect.right &&
                        pt.y >= cardRect.top && pt.y <= cardRect.bottom) {
                        POINT screenPt = pt;
                        ClientToScreen(hWnd, &screenPt);
                        ShowWorkspaceContextMenu(hWnd, (int)i, screenPt);
                        return 0;
                    }
                }
            }
            
            if (g_currentView == 2) {
                int y = contentTop + 14 + 38 + 32;
                int lineHeight = 34;
                int visibleItems = (rc.bottom - STATUS_HEIGHT - 20 - y - 12) / lineHeight;
                if (visibleItems < 1) visibleItems = 1;
                
                int startIdx = g_deviceScrollOffset;
                int endIdx = startIdx + visibleItems;
                if (endIdx > (int)g_availableDevices.size()) endIdx = (int)g_availableDevices.size();
                
                for (int i = startIdx; i < endIdx; i++) {
                    RECT itemRect = {contentLeft + 14, y, contentRight - 14, y + lineHeight};
                    if (pt.x >= itemRect.left && pt.x <= itemRect.right &&
                        pt.y >= itemRect.top && pt.y <= itemRect.bottom) {
                        POINT screenPt = pt;
                        ClientToScreen(hWnd, &screenPt);
                        ShowDeviceContextMenu(hWnd, i, screenPt);
                        return 0;
                    }
                    y += lineHeight;
                }
            }
            return 0;
        }
        
        case WM_KEYDOWN: {
            if (wParam == VK_ESCAPE) {
                DestroyWindow(hWnd);
                return 0;
            }
            return 0;
        }
        
        case WM_MOUSEMOVE: {
            POINT pt;
            GetCursorPos(&pt);
            ScreenToClient(hWnd, &pt);
            
            RECT rc;
            GetClientRect(hWnd, &rc);
            int contentLeft = SIDEBAR_WIDTH + 20;
            int contentRight = rc.right - 20;
            int contentTop = HEADER_HEIGHT + 20;
            
            int newHoverWorkspace = -1;
            int newHoverDevice = -1;
            
            if (g_currentView == 0 || g_currentView == 1) {
                int cardWidth = (g_currentView == 0) ? 160 : 180;
                int cardHeight = 106;
                int spacing = 14;
                int availableWidth = contentRight - contentLeft;
                int cardsPerRow = availableWidth / (cardWidth + spacing);
                if (cardsPerRow < 1) cardsPerRow = 1;
                
                int startX = contentLeft;
                int yPos = (g_currentView == 0) ? contentTop + 96 : contentTop;
                int startY = yPos;
                
                size_t maxCards = (g_currentView == 0) ? std::min(g_workspaces.size(), size_t(6)) : g_workspaces.size();
                
                for (size_t i = 0; i < maxCards; i++) {
                    int row = i / cardsPerRow;
                    int col = i % cardsPerRow;
                    int x = startX + col * (cardWidth + spacing);
                    int y = startY + row * (cardHeight + spacing);
                    
                    RECT cardRect = {x, y, x + cardWidth, y + cardHeight};
                    if (pt.x >= cardRect.left && pt.x <= cardRect.right &&
                        pt.y >= cardRect.top && pt.y <= cardRect.bottom) {
                        newHoverWorkspace = (int)i;
                        break;
                    }
                }
            }
            
            if (g_currentView == 2) {
                int y = contentTop + 14 + 38 + 32;
                int lineHeight = 34;
                int visibleItems = (rc.bottom - STATUS_HEIGHT - 20 - y - 12) / lineHeight;
                if (visibleItems < 1) visibleItems = 1;
                
                int startIdx = g_deviceScrollOffset;
                int endIdx = startIdx + visibleItems;
                if (endIdx > (int)g_availableDevices.size()) endIdx = (int)g_availableDevices.size();
                
                for (int i = startIdx; i < endIdx; i++) {
                    RECT itemRect = {contentLeft + 14, y, contentRight - 14, y + lineHeight};
                    if (pt.x >= itemRect.left && pt.x <= itemRect.right &&
                        pt.y >= itemRect.top && pt.y <= itemRect.bottom) {
                        newHoverDevice = i;
                        break;
                    }
                    y += lineHeight;
                }
            }
            
            if (newHoverWorkspace != g_hoveredWorkspace || newHoverDevice != g_hoveredDevice) {
                g_hoveredWorkspace = newHoverWorkspace;
                g_hoveredDevice = newHoverDevice;
                InvalidateRect(hWnd, NULL, TRUE);
            }
            return 0;
        }
        
        case WM_TIMER: {
            bool changed = false;
            bool driverState = IsDriverConnected();
            if (driverState != g_driverConnected) {
                g_driverConnected = driverState;
                changed = true;
            }
            
            UpdateDeviceCache();
            UpdateWorkspaceInfo();
            
            int assignedCount = (int)g_deviceWorkspaceMap.size();
            static int lastAssigned = -1;
            if (assignedCount != lastAssigned) {
                lastAssigned = assignedCount;
                changed = true;
            }
            
            if (changed) {
                InvalidateRect(hWnd, NULL, TRUE);
            }
            return 0;
        }
        
        case WM_DESTROY:
            ShowCursor(TRUE);
            KillTimer(hWnd, 1);
            if (g_hFontTitle) DeleteObject(g_hFontTitle);
            if (g_hFontSub) DeleteObject(g_hFontSub);
            if (g_hFontStatus) DeleteObject(g_hFontStatus);
            if (g_hFontSmall) DeleteObject(g_hFontSmall);
            if (g_hBrushBg) DeleteObject(g_hBrushBg);
            if (g_hBrushCard) DeleteObject(g_hBrushCard);
            if (g_hBrushAccent) DeleteObject(g_hBrushAccent);
            if (g_cursorEmulator) { delete g_cursorEmulator; g_cursorEmulator = nullptr; }
            PostQuitMessage(0);
            return 0;
        
        default:
            return DefWindowProc(hWnd, msg, wParam, lParam);
    }
}

// ============================================================
// MAIN ENTRY
// ============================================================

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    try {
        LOG_INFO("DualDesk starting...");

        INITCOMMONCONTROLSEX icex = {0};
        icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
        icex.dwICC = ICC_STANDARD_CLASSES;
        InitCommonControlsEx(&icex);

        DisplayManager displayManager;
        WorkspaceManager workspaceManager;
        WindowMover windowMover;
        DriverInterface driverInterface;
        InputRouter inputRouter;
        InputManager inputManager;

        g_displayManager = &displayManager;
        g_workspaceManager = &workspaceManager;
        g_windowMover = &windowMover;
        g_driverInterface = &driverInterface;
        g_inputRouter = &inputRouter;
        g_inputManager = &inputManager;

        displayManager.EnumerateDisplays();
        workspaceManager.Initialize(&displayManager, &inputManager);
        inputRouter.Initialize(&workspaceManager);

        if (driverInterface.Open()) {
            g_driverConnected = true;
            driverInterface.SetRouteMode(TRUE);
            LOG_INFO("Driver connected");
        } else {
            g_driverConnected = false;
            LOG_WARN("Driver not available");
        }

        // Initialize monitor borders
        InitializeMonitorBorders();

        // Launch EitherMouse
        LaunchEitherMouse();

        // Initialize EitherMouse plugin
        if (g_eitherMouse.Initialize()) {
            LOG_INFO("EitherMouse plugin initialized");
            if (g_eitherMouse.IsEitherMouseRunning()) {
                LOG_INFO("EitherMouse is running");
                g_lastEvent = "EitherMouse ready";
            } else {
                LOG_WARN("EitherMouse not running");
                g_lastEvent = "EitherMouse not found";
            }
        }

        // Create overlay window
        CreateOverlayWindow(hInstance);

        // Cursor Emulation
        g_cursorEmulator = new CursorEmulator();
        g_cursorEmulator->Initialize(g_hOverlayWnd);
        g_cursorEmulator->LockRealCursor(true);
        g_cursorEmulator->SetRealCursorWorkspace(0);
        g_cursorEmulator->ShowCursorBorder(true);
        
        auto workspaces = workspaceManager.GetAllWorkspaces();
        for (size_t i = 1; i < workspaces.size(); i++) {
            RECT bounds = workspaces[i]->GetMonitorBounds();
            g_cursorEmulator->EnableVirtualCursor(i, 0, bounds);
        }

        g_inputRouter->SetCursorEmulator(g_cursorEmulator);
        g_inputRouter->SetDeviceWorkspaceMap(&g_deviceWorkspaceMap);

        // Input window
        WNDCLASSEXW wcInput = {0};
        wcInput.cbSize = sizeof(WNDCLASSEXW);
        wcInput.lpfnWndProc = InputWndProc;
        wcInput.hInstance = hInstance;
        wcInput.lpszClassName = L"DualDeskInputClass";
        RegisterClassExW(&wcInput);

        HWND hInputWnd = CreateWindowExW(0, L"DualDeskInputClass", L"", WS_POPUP,
            0, 0, 0, 0, HWND_MESSAGE, NULL, hInstance, NULL);
        inputManager.Initialize(hInputWnd);

        inputManager.SetDeviceChangeCallback([](const InputDevice& device, bool added) {
            if (added) {
                g_lastEvent = "Device plugged in";
                if (g_driverInterface && g_driverInterface->IsConnected()) {
                    g_driverInterface->AssignDeviceToWorkspace(device.deviceHandle, 0);
                    g_deviceWorkspaceMap[device.deviceHandle] = 0;
                }
            } else {
                g_lastEvent = "Device unplugged";
                g_deviceWorkspaceMap.erase(device.deviceHandle);
            }
            InvalidateRect(g_hMainWnd, NULL, TRUE);
        });

        inputManager.SetInputEventCallback([](const InputEvent& event) {
            if (g_inputRouter) g_inputRouter->RouteInput(event);
        });

        windowMover.Initialize(&workspaceManager);
        windowMover.SetMoveCallback([](HWND hwnd, HMONITOR from, HMONITOR to) {
            g_lastEvent = "Window moved";
            InvalidateRect(g_hMainWnd, NULL, TRUE);
        });

        // Register for raw input on the main window
        RAWINPUTDEVICE rid[1];
        rid[0].usUsagePage = 0x01;
        rid[0].usUsage = 0x02;
        rid[0].dwFlags = RIDEV_INPUTSINK;
        rid[0].hwndTarget = g_hMainWnd;
        RegisterRawInputDevices(rid, 1, sizeof(rid[0]));

        // Main window
        WNDCLASSEXW wc = {0};
        wc.cbSize = sizeof(WNDCLASSEXW);
        wc.lpfnWndProc = MainWndProc;
        wc.hInstance = hInstance;
        wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = CreateSolidBrush(COLOR_BG_DARK);
        wc.lpszClassName = L"DualDeskMainClass";
        RegisterClassExW(&wc);

        int screenWidth = GetSystemMetrics(SM_CXSCREEN);
        int screenHeight = GetSystemMetrics(SM_CYSCREEN);
        int windowWidth = 850;
        int windowHeight = 600;
        int x = (screenWidth - windowWidth) / 2;
        int y = (screenHeight - windowHeight) / 2;

        g_hMainWnd = CreateWindowExW(
            WS_EX_APPWINDOW,
            L"DualDeskMainClass",
            L"DualDesk",
            WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_CLIPCHILDREN,
            x, y, windowWidth, windowHeight,
            NULL, NULL, hInstance, NULL
        );

        if (!g_hMainWnd) {
            LOG_ERROR("Failed to create main window");
            return 1;
        }

        ShowWindow(g_hMainWnd, nCmdShow);
        UpdateWindow(g_hMainWnd);

        MSG msg;
        while (GetMessage(&msg, NULL, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        if (g_cursorEmulator) {
            delete g_cursorEmulator;
            g_cursorEmulator = nullptr;
        }

        LOG_INFO("DualDesk exited");
        return 0;
    }
    catch (const std::exception& e) {
        MessageBoxA(NULL, e.what(), "Fatal Error", MB_OK | MB_ICONERROR);
        return 1;
    }
}