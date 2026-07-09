#include "dualdesk/core/logger.h"
#include "dualdesk/display/display_manager.h"
#include "dualdesk/workspace/window_tracker.h"
#include "dualdesk/workspace/workspace_manager.h"
#include "dualdesk/workspace/window_mover.h"
#include "dualdesk/input/input_manager.h"
#include "dualdesk/input/input_router.h"
#include "dualdesk/core/driver_interface.h"
#include "dualdesk/cursor/cursor_emulator.h"
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <string>
#include <sstream>
#include <vector>
#include <map>
#include <algorithm>

#pragma comment(lib, "comctl32.lib")
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

using namespace dualdesk;

// ============================================================
// DASHBOARD COLORS
// ============================================================
#define COLOR_BG_DARK      RGB(248, 250, 252)
#define COLOR_BG_CARD      RGB(255, 255, 255)
#define COLOR_BG_CARD_HOVER RGB(241, 245, 249)
#define COLOR_TEXT_MAIN    RGB(30, 41, 59)
#define COLOR_TEXT_SUB     RGB(71, 85, 105)
#define COLOR_TEXT_MUTED   RGB(148, 163, 184)
#define COLOR_ACCENT       RGB(139, 92, 246)
#define COLOR_ACCENT_HOVER RGB(124, 58, 237)
#define COLOR_SUCCESS      RGB(16, 185, 129)
#define COLOR_WARNING      RGB(245, 158, 11)
#define COLOR_ERROR        RGB(239, 68, 68)
#define COLOR_BORDER       RGB(226, 232, 240)
#define COLOR_WS_A         RGB(139, 92, 246)
#define COLOR_WS_B         RGB(16, 185, 129)
#define COLOR_WS_C         RGB(245, 158, 11)
#define COLOR_WS_D         RGB(239, 68, 68)
#define COLOR_WS_E         RGB(59, 130, 246)

#define SIDEBAR_WIDTH      162
#define HEADER_HEIGHT      40
#define CONTENT_PADDING    16
#define SCALE 1.6f
#define SCALE_INT(x) ((int)((x) * SCALE))

// ============================================================
// VIEW TYPES
// ============================================================
enum ViewType {
    VIEW_DASHBOARD = 0,
    VIEW_TREE,
    VIEW_WORKSPACES,
    VIEW_DEVICES,
    VIEW_MONITORS,
    VIEW_SETTINGS
};

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
HWND g_hStatusBar = nullptr;
HWND g_hTreeView = nullptr;
HWND g_btnToggleBorder = nullptr;
HWND g_btnRefresh = nullptr;
HWND g_btnTestDriver = nullptr;
HWND g_btnUnassignAll = nullptr;

HFONT g_hFontTitle = nullptr;
HFONT g_hFontSub = nullptr;
HFONT g_hFontCard = nullptr;
HFONT g_hFontButton = nullptr;
HFONT g_hFontStatus = nullptr;
HBRUSH g_hBrushBg = nullptr;
HBRUSH g_hBrushCard = nullptr;
HBRUSH g_hBrushCardHover = nullptr;
HBRUSH g_hBrushAccent = nullptr;
HIMAGELIST g_hImageList = nullptr;

bool g_running = true;
bool g_borderVisible = true;
bool g_driverConnected = false;
std::string g_lastEvent = "Ready";
int g_selectedWorkspace = -1;
int g_selectedDevice = -1;
int g_hoveredDevice = -1;
int g_hoveredWorkspace = -1;
ViewType g_currentView = VIEW_DASHBOARD;

// Cache for change detection
int g_cachedMonitorCount = -1;
int g_cachedWindowCount = -1;
int g_cachedDeviceCount = -1;
int g_cachedWorkspaceCount = -1;
int g_cachedAssignedCount = -1;
bool g_cachedDriverState = false;

std::map<HANDLE, DWORD> g_deviceWorkspaceMap;
std::map<HANDLE, std::string> g_deviceNames;
std::vector<HANDLE> g_availableDevices;

HTREEITEM g_nodeMonitors = nullptr;
HTREEITEM g_nodeWorkspaces = nullptr;
HTREEITEM g_nodeDevices = nullptr;
HTREEITEM g_nodeWindows = nullptr;

int g_deviceScrollOffset = 0;
int g_maxDeviceScrollOffset = 0;
int g_dashboardScrollOffset = 0;
int g_maxDashboardScrollOffset = 0; 


struct WorkspaceInfo {
    ULONG id;
    char letter;
    std::string name;
    RECT bounds;
    int windowCount;
    int deviceCount;
    bool hasRealCursor;
    bool hasVirtualCursor;
    COLORREF color;
};
std::vector<WorkspaceInfo> g_workspaces;

enum IconIndex {
    ICON_FOLDER = 0, ICON_MONITOR, ICON_WORKSPACE, ICON_KEYBOARD,
    ICON_MOUSE, ICON_TOUCHPAD, ICON_OK, ICON_WARN, ICON_WINDOW, ICON_CURSOR, ICON_COUNT
};

// ============================================================
// FORWARD DECLARATIONS
// ============================================================
LRESULT CALLBACK MainWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK OverlayWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK InputWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

void UpdateWorkspaceInfo();
void RefreshDashboard();
void RefreshTree();
void BuildTree();
void DrawDashboardView(HDC hdc, RECT rc);
void DrawTreeView(HDC hdc, RECT rc);
void DrawWorkspacesView(HDC hdc, RECT rc);
void DrawDevicesView(HDC hdc, RECT rc);
void DrawMonitorsView(HDC hdc, RECT rc);
void DrawSettingsView(HDC hdc, RECT rc);
void DrawWorkspaceCard(HDC hdc, RECT rect, const WorkspaceInfo& ws, bool selected);
void DrawDeviceList(HDC hdc, RECT rect, bool showAll);
void DrawDeviceListInteractive(HDC hdc, RECT rect);
void DrawStatusBar(HDC hdc, RECT rect);
void DrawDashboardShell(HDC hdc, RECT rc);
void DrawStatCard(HDC hdc, RECT rect, const wchar_t* title, const wchar_t* value, const wchar_t* change, COLORREF accent, const wchar_t* glyph);
void DrawSidebar(HDC hdc, RECT rect);
void DrawBorderWall(HDC hdc);  // ← ADD THIS LINE HERE
void ToggleBorderWall();
void TestDriverConnection();
void UnassignAllDevices();
void AssignDeviceToWorkspace(HANDLE deviceHandle, DWORD workspaceId);
void AssignDeviceToWorkspaceById(int deviceIndex, int workspaceId);
void SelectWorkspace(int index);
void SwitchView(ViewType view);
void BuildImageList();
HTREEITEM AddNode(HTREEITEM parent, const std::wstring& text, int icon, bool expand);
void ClearChildren(HTREEITEM parent);
void ShowDeviceContextMenu(HWND hwnd, POINT pt);
void ShowAssignDevicePopup(HWND hWnd, POINT pt, ULONG workspaceId);
bool CheckForChanges();
void UpdateDeviceCache();
void FlashBorderWall();

std::string GetCleanDeviceName(const std::wstring& deviceName);
std::wstring ToWide(const std::string& s);
bool IsDriverConnected();
std::wstring GetWindowClassName(HWND hwnd);
std::wstring GetWindowTitle(HWND hwnd);
bool IsWindowTrackable(HWND hwnd);

// ============================================================
// DEVICE HELPERS
// ============================================================
std::string GetCleanDeviceName(const std::wstring& deviceName) {
    std::string name;
    for (wchar_t c : deviceName) { if (c < 128) name += (char)c; }
    std::string result = name;
    size_t pos = result.find_last_of("\\#");
    if (pos != std::string::npos && pos + 1 < result.length()) { result = result.substr(pos + 1); }
    
    std::vector<std::string> removeSuffixes = {
        "&Col01", "&Col02", "&Col03", "&Col04", "&MI_00", "&MI_01", "&MI_02", "&MI_03", "&0000", "&0001", "&0002", "&0003", "#", "_"
    };
    for (const auto& suffix : removeSuffixes) {
        size_t found = result.find(suffix);
        if (found != std::string::npos) { result = result.substr(0, found); }
    }
    while (!result.empty() && (result.back() == '&' || result.back() == '#' || result.back() == '_' || result.back() == '\\')) {
        result.pop_back();
    }
    if (result.empty() || result.length() < 3 || result == "Keyboard" || result == "Mouse" || result == "HID" || result == "USB") {
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
    if (result.length() > 20) { result = result.substr(0, 17) + "..."; }
    return result;
}

std::wstring ToWide(const std::string& s) { return std::wstring(s.begin(), s.end()); }

bool IsDriverConnected() {
    HANDLE hDriver = CreateFileA("\\\\.\\DualDesk", GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (hDriver != INVALID_HANDLE_VALUE) { CloseHandle(hDriver); return true; }
    return false;
}

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

void DrawClippedText(HDC hdc, const std::wstring& text, RECT rect, UINT format, HFONT font, COLORREF color) {
    SelectObject(hdc, font);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, color);
    DrawTextW(hdc, text.c_str(), (int)text.length(), &rect, format | DT_END_ELLIPSIS);
}

void DrawIconBadge(HDC hdc, RECT rect, COLORREF color, const wchar_t* glyph) {
    FillRoundedRect(hdc, rect, color, color, SCALE_INT(10));
    DrawClippedText(hdc, glyph, rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE, g_hFontSub, RGB(255, 255, 255));
}

// ============================================================
// CHECK FOR CHANGES - FIXED WINDOW COUNT
// ============================================================
bool CheckForChanges() {
    bool changed = false;
    
    UpdateDeviceCache();
    
    int monitorCount = 0;
    if (g_displayManager) {
        auto monitors = g_displayManager->EnumerateDisplays();
        monitorCount = (int)monitors.size();
    }
    if (monitorCount != g_cachedMonitorCount) {
        g_cachedMonitorCount = monitorCount;
        changed = true;
    }
    
    // Count ONLY user windows (with titles, not system windows)
    int windowCount = 0;
    EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
        int* count = reinterpret_cast<int*>(lParam);
        if (IsWindowVisible(hwnd) && !IsIconic(hwnd)) {
            wchar_t title[256];
            GetWindowTextW(hwnd, title, 256);
            wchar_t className[256];
            GetClassNameW(hwnd, className, 256);
            
            std::wstring cls(className);
            std::wstring ttl(title);
            
            // Skip system windows
            if (cls == L"Progman" || cls == L"WorkerW" || 
                cls == L"Shell_TrayWnd" || cls == L"Button" ||
                cls == L"SysListView32" || cls == L"DesktopWindowXamlSource" ||
                cls == L"ApplicationFrameWindow" || cls == L"Windows.UI.Core.CoreWindow" ||
                ttl == L"") {
                return TRUE;
            }
            
            // Only count windows with titles (real applications)
            if (ttl.length() > 0) {
                (*count)++;
            }
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&windowCount));
    
    if (windowCount != g_cachedWindowCount) {
        g_cachedWindowCount = windowCount;
        changed = true;
    }
    
    int deviceCount = (int)g_availableDevices.size();
    if (deviceCount != g_cachedDeviceCount) {
        g_cachedDeviceCount = deviceCount;
        changed = true;
    }
    
    int workspaceCount = (int)g_workspaces.size();
    if (workspaceCount != g_cachedWorkspaceCount) {
        g_cachedWorkspaceCount = workspaceCount;
        changed = true;
    }
    
    int assignedCount = (int)g_deviceWorkspaceMap.size();
    if (assignedCount != g_cachedAssignedCount) {
        g_cachedAssignedCount = assignedCount;
        changed = true;
    }
    
    bool driverState = IsDriverConnected();
    if (driverState != g_cachedDriverState) {
        g_cachedDriverState = driverState;
        changed = true;
    }
    
    return changed;
}

// ============================================================
// UPDATE DEVICE CACHE
// ============================================================
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

// ============================================================
// TREE VIEW FUNCTIONS
// ============================================================
void BuildImageList() {
    g_hImageList = ImageList_Create(16, 16, ILC_COLOR32 | ILC_MASK, ICON_COUNT, 0);
    for (int i = 0; i < ICON_COUNT; i++) {
        HICON icon = LoadIcon(NULL, IDI_APPLICATION);
        if (icon) { ImageList_AddIcon(g_hImageList, icon); }
        else {
            HBITMAP bmp = CreateBitmap(16, 16, 1, 1, NULL);
            ImageList_Add(g_hImageList, bmp, (HBITMAP)NULL);
            DeleteObject(bmp);
        }
    }
    if (g_hTreeView) {
        TreeView_SetImageList(g_hTreeView, g_hImageList, TVSIL_NORMAL);
    }
}

HTREEITEM AddNode(HTREEITEM parent, const std::wstring& text, int icon, bool expand) {
    if (!g_hTreeView) return NULL;
    TVINSERTSTRUCTW tvi = {0};
    tvi.hParent = parent;
    tvi.hInsertAfter = TVI_LAST;
    tvi.item.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE;
    tvi.item.pszText = const_cast<LPWSTR>(text.c_str());
    tvi.item.iImage = icon;
    tvi.item.iSelectedImage = icon;
    HTREEITEM item = TreeView_InsertItem(g_hTreeView, &tvi);
    if (expand && parent == TVI_ROOT) {
        TreeView_Expand(g_hTreeView, item, TVE_EXPAND);
    }
    return item;
}

void ClearChildren(HTREEITEM parent) {
    if (!g_hTreeView) return;
    HTREEITEM child = TreeView_GetChild(g_hTreeView, parent);
    while (child) {
        HTREEITEM next = TreeView_GetNextSibling(g_hTreeView, child);
        TreeView_DeleteItem(g_hTreeView, child);
        child = next;
    }
}

void BuildTree() {
    if (!g_hTreeView) return;
    TreeView_DeleteAllItems(g_hTreeView);
    g_nodeMonitors = AddNode(TVI_ROOT, L"Monitors", ICON_FOLDER, true);
    g_nodeWorkspaces = AddNode(TVI_ROOT, L"Workspaces", ICON_FOLDER, true);
    g_nodeWindows = AddNode(TVI_ROOT, L"Windows", ICON_FOLDER, true);
    g_nodeDevices = AddNode(TVI_ROOT, L"Devices", ICON_FOLDER, true);
}

void RefreshTree() {
    if (!g_hTreeView || !IsWindowVisible(g_hTreeView)) return;
    
    ClearChildren(g_nodeMonitors);
    int monitorCount = 0;
    if (g_displayManager) {
        auto monitors = g_displayManager->EnumerateDisplays();
        monitorCount = (int)monitors.size();
        for (size_t i = 0; i < monitors.size(); ++i) {
            std::wstringstream label;
            label << L"Monitor " << (i + 1) << L" - "
                  << monitors[i].Width() << L"x" << monitors[i].Height();
            if (monitors[i].isPrimary) label << L" (Primary)";
            AddNode(g_nodeMonitors, label.str(), ICON_MONITOR, false);
        }
    }
    
    ClearChildren(g_nodeWorkspaces);
    for (const auto& ws : g_workspaces) {
        std::wstringstream label;
        label << L"Workspace " << ws.letter << L" - " << ws.windowCount << L" windows";
        if (ws.deviceCount > 0) {
            label << L" (" << ws.deviceCount << L" devices)";
        }
        if (ws.hasRealCursor) {
            label << L" [REAL]";
        } else if (ws.hasVirtualCursor) {
            label << L" [VIRTUAL]";
        }
        AddNode(g_nodeWorkspaces, label.str(), ICON_WORKSPACE, false);
    }
    
     ClearChildren(g_nodeWindows);
    std::vector<HWND> windowsList;
    EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
        auto* pData = reinterpret_cast<std::vector<HWND>*>(lParam);
        if (IsWindowVisible(hwnd) && !IsIconic(hwnd)) {
            wchar_t title[256];
            GetWindowTextW(hwnd, title, 256);
            wchar_t className[256];
            GetClassNameW(hwnd, className, 256);
            
            std::wstring cls(className);
            std::wstring ttl(title);
            
            // Skip system windows
            if (cls == L"Progman" || cls == L"WorkerW" || 
                cls == L"Shell_TrayWnd" || cls == L"Button" ||
                cls == L"SysListView32" || cls == L"DesktopWindowXamlSource" ||
                cls == L"ApplicationFrameWindow" || cls == L"Windows.UI.Core.CoreWindow" ||
                ttl == L"") {
                return TRUE;
            }
            
            // Only add windows with titles (real applications)
            if (ttl.length() > 0) {
                pData->push_back(hwnd);
            }
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&windowsList));
    
    for (HWND hwnd : windowsList) {
        std::wstring title = GetWindowTitle(hwnd);
        std::wstring className = GetWindowClassName(hwnd);
        if (title.length() > 30) { title = title.substr(0, 27) + L"..."; }
        std::wstringstream label;
        label << title << L" [" << className << L"]";
        AddNode(g_nodeWindows, label.str(), ICON_WINDOW, false);
    }
    
    ClearChildren(g_nodeDevices);
    UpdateDeviceCache();
    if (g_inputManager) {
        auto keyboards = g_inputManager->GetKeyboards();
        for (const auto& kb : keyboards) {
            std::string cleanName = GetCleanDeviceName(kb.deviceName);
            std::wstring label = L"Keyboard: " + ToWide(cleanName);
            auto it = g_deviceWorkspaceMap.find(kb.deviceHandle);
            if (it != g_deviceWorkspaceMap.end()) {
                label += L" -> WS " + std::wstring(1, 'A' + (char)it->second);
            }
            TVINSERTSTRUCTW tvi = {0};
            tvi.hParent = g_nodeDevices;
            tvi.hInsertAfter = TVI_LAST;
            tvi.item.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM;
            tvi.item.pszText = const_cast<LPWSTR>(label.c_str());
            tvi.item.iImage = ICON_KEYBOARD;
            tvi.item.iSelectedImage = ICON_KEYBOARD;
            tvi.item.lParam = (LPARAM)kb.deviceHandle;
            TreeView_InsertItem(g_hTreeView, &tvi);
        }
        auto mice = g_inputManager->GetMice();
        for (const auto& mouse : mice) {
            std::string cleanName = GetCleanDeviceName(mouse.deviceName);
            std::wstring label = L"Mouse: " + ToWide(cleanName);
            auto it = g_deviceWorkspaceMap.find(mouse.deviceHandle);
            if (it != g_deviceWorkspaceMap.end()) {
                label += L" -> WS " + std::wstring(1, 'A' + (char)it->second);
            }
            TVINSERTSTRUCTW tvi = {0};
            tvi.hParent = g_nodeDevices;
            tvi.hInsertAfter = TVI_LAST;
            tvi.item.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM;
            tvi.item.pszText = const_cast<LPWSTR>(label.c_str());
            tvi.item.iImage = ICON_MOUSE;
            tvi.item.iSelectedImage = ICON_MOUSE;
            tvi.item.lParam = (LPARAM)mouse.deviceHandle;
            TreeView_InsertItem(g_hTreeView, &tvi);
        }
    }
    
    AddNode(g_nodeDevices, g_driverConnected ? L"Driver: Connected" : L"Driver: Not connected",
            g_driverConnected ? ICON_OK : ICON_WARN, false);
    
    TreeView_Expand(g_hTreeView, g_nodeMonitors, TVE_EXPAND);
    TreeView_Expand(g_hTreeView, g_nodeWorkspaces, TVE_EXPAND);
    TreeView_Expand(g_hTreeView, g_nodeWindows, TVE_EXPAND);
    TreeView_Expand(g_hTreeView, g_nodeDevices, TVE_EXPAND);
}

std::wstring GetWindowClassName(HWND hwnd) {
    wchar_t className[256];
    if (GetClassNameW(hwnd, className, 256)) { return std::wstring(className); }
    return L"";
}

std::wstring GetWindowTitle(HWND hwnd) {
    wchar_t title[256];
    if (GetWindowTextW(hwnd, title, 256)) { return std::wstring(title); }
    return L"";
}

bool IsWindowTrackable(HWND hwnd) {
    if (!IsWindowVisible(hwnd)) return false;
    if (IsIconic(hwnd)) return false;
    wchar_t title[256];
    if (GetWindowTextW(hwnd, title, 256) == 0) return false;
    if (wcslen(title) == 0) return false;
    return true;
}

void ShowDeviceContextMenu(HWND hwnd, POINT pt) {
    if (!g_hTreeView) return;
    
    TVHITTESTINFO ht = {0};
    ht.pt = pt;
    ScreenToClient(g_hTreeView, &ht.pt);
    HTREEITEM hItem = TreeView_HitTest(g_hTreeView, &ht);
    if (!hItem) return;
    
    TVITEMW item = {0};
    item.hItem = hItem;
    item.mask = TVIF_PARAM | TVIF_HANDLE;
    if (!TreeView_GetItem(g_hTreeView, &item) || !item.lParam) return;
    
    HANDLE hDevice = (HANDLE)item.lParam;
    HMENU hMenu = CreatePopupMenu();
    if (g_workspaceManager) {
        auto workspaces = g_workspaceManager->GetAllWorkspaces();
        for (auto* ws : workspaces) {
            wchar_t label[64];
            swprintf_s(label, 64, L"Assign to Workspace %c", 'A' + (char)ws->GetId());
            AppendMenuW(hMenu, MF_STRING, 1000 + ws->GetId(), label);
        }
    }
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenu, MF_STRING, 999, L"Remove Assignment");
    SetForegroundWindow(hwnd);
    int selection = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
    DestroyMenu(hMenu);
    
    if (selection == 999) {
        g_deviceWorkspaceMap.erase(hDevice);
        if (g_driverInterface && g_driverInterface->IsConnected()) {
            g_driverInterface->UnassignDevice(hDevice);
        }
        g_lastEvent = "Device unassigned";
        RefreshDashboard();
    } else if (selection >= 1000 && selection < 1020) {
        AssignDeviceToWorkspace(hDevice, selection - 1000);
    }
}

// ============================================================
// UPDATE WORKSPACE INFO - FIXED WINDOW COUNT
// ============================================================
void UpdateWorkspaceInfo() {
    g_workspaces.clear();
    
    if (!g_workspaceManager) return;
    
    auto workspaces = g_workspaceManager->GetAllWorkspaces();
    
    // Count windows per workspace - ONLY user windows
    std::map<HMONITOR, int> windowCounts;
    EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
        auto* counts = reinterpret_cast<std::map<HMONITOR, int>*>(lParam);
        // Check if this is a valid user window
        if (IsWindowVisible(hwnd) && !IsIconic(hwnd)) {
            // Skip desktop and system windows
            wchar_t title[256];
            GetWindowTextW(hwnd, title, 256);
            wchar_t className[256];
            GetClassNameW(hwnd, className, 256);
            
            std::wstring cls(className);
            std::wstring ttl(title);
            
            // Skip system windows
            if (cls == L"Progman" || cls == L"WorkerW" || 
                cls == L"Shell_TrayWnd" || cls == L"Button" ||
                cls == L"SysListView32" || cls == L"DesktopWindowXamlSource" ||
                cls == L"ApplicationFrameWindow" || cls == L"Windows.UI.Core.CoreWindow" ||
                ttl == L"") {
                return TRUE;
            }
            
            // Only count windows with titles (real applications)
            if (ttl.length() > 0) {
                HMONITOR mon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
                (*counts)[mon]++;
            }
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&windowCounts));
    
    std::map<DWORD, int> deviceCounts;
    for (const auto& pair : g_deviceWorkspaceMap) {
        deviceCounts[pair.second]++;
    }
    
    ULONG realCursorWs = g_cursorEmulator ? g_cursorEmulator->GetRealCursorWorkspace() : 0;
    
    COLORREF colors[] = {COLOR_WS_A, COLOR_WS_B, COLOR_WS_C, COLOR_WS_D, COLOR_WS_E};
    
    for (size_t i = 0; i < workspaces.size(); i++) {
        auto* ws = workspaces[i];
        WorkspaceInfo info;
        info.id = ws->GetId();
        info.letter = (char)ws->GetId() + 'A';
        info.name = "Workspace " + std::string(1, info.letter);
        info.bounds = ws->GetMonitorBounds();
        info.color = colors[i % 5];
        
        info.windowCount = 0;
        HMONITOR wsMon = ws->GetMonitor();
        for (const auto& pair : windowCounts) {
            if (pair.first == wsMon) {
                info.windowCount = pair.second;
                break;
            }
        }
        
        info.deviceCount = 0;
        auto it = deviceCounts.find(info.id);
        if (it != deviceCounts.end()) {
            info.deviceCount = it->second;
        }
        
        info.hasRealCursor = (info.id == realCursorWs);
        info.hasVirtualCursor = g_cursorEmulator && 
            g_cursorEmulator->GetVirtualCursorManager()->GetCursor(info.id) != nullptr;
        
        g_workspaces.push_back(info);
    }
    
    UpdateDeviceCache();
}

// ============================================================
// SWITCH VIEW
// ============================================================
void SwitchView(ViewType view) {
    g_currentView = view;
    if (g_hTreeView) {
        ShowWindow(g_hTreeView, (view == VIEW_TREE) ? SW_SHOW : SW_HIDE);
        if (view == VIEW_TREE) {
            RefreshTree();
        }
    }
    RefreshDashboard();
}

// ============================================================
// DRAW SIDEBAR
// ============================================================
void DrawSidebar(HDC hdc, RECT rect) {
    FillSolidRect(hdc, rect, COLOR_BG_CARD);
    
    // Logo
    DrawClippedText(hdc, L"DualDesk", 
        {SCALE_INT(48), SCALE_INT(12), SIDEBAR_WIDTH - SCALE_INT(12), SCALE_INT(34)},
        DT_LEFT | DT_VCENTER | DT_SINGLELINE, 
        g_hFontSub, 
        COLOR_TEXT_MAIN);
    DrawClippedText(hdc, L"|||", 
        {SCALE_INT(18), SCALE_INT(12), SCALE_INT(42), SCALE_INT(34)},
        DT_CENTER | DT_VCENTER | DT_SINGLELINE, 
        g_hFontSub, 
        COLOR_ACCENT);

    // Navigation items
    struct NavItem { const wchar_t* label; ViewType view; };
    NavItem items[] = {
        {L"Dashboard", VIEW_DASHBOARD},
        {L"Tree View", VIEW_TREE},
        {L"Workspaces", VIEW_WORKSPACES},
        {L"Devices", VIEW_DEVICES},
        {L"Monitors", VIEW_MONITORS},
        {L"Settings", VIEW_SETTINGS}
    };

    int navY = SCALE_INT(58);
    for (const auto& item : items) {
        RECT itemRect = {SCALE_INT(10), navY, SIDEBAR_WIDTH - SCALE_INT(10), navY + SCALE_INT(34)};
        bool active = (g_currentView == item.view);
        
        if (active) {
            FillRoundedRect(hdc, itemRect, COLOR_BG_CARD_HOVER, COLOR_BG_CARD_HOVER, SCALE_INT(10));
            FillSolidRect(hdc, {itemRect.left, itemRect.top + 4, itemRect.left + 3, itemRect.bottom - 4}, COLOR_ACCENT);
        }
        
        DrawClippedText(hdc, item.label, 
            {itemRect.left + SCALE_INT(14), itemRect.top, itemRect.right - SCALE_INT(8), itemRect.bottom},
            DT_LEFT | DT_VCENTER | DT_SINGLELINE, 
            g_hFontStatus, 
            active ? COLOR_TEXT_MAIN : COLOR_TEXT_SUB);
        navY += SCALE_INT(38);
    }
}

// ============================================================
// DRAW STAT CARD
// ============================================================
void DrawStatCard(HDC hdc, RECT rect, const wchar_t* title, const wchar_t* value, const wchar_t* change, COLORREF accent, const wchar_t* glyph) {
    FillRoundedRect(hdc, rect, COLOR_BG_CARD, COLOR_BORDER, SCALE_INT(12));

    int padding = SCALE_INT(14);
    RECT titleRect = {rect.left + padding, rect.top + SCALE_INT(12), rect.right - SCALE_INT(56), rect.top + SCALE_INT(30)};
    DrawClippedText(hdc, title, titleRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE, g_hFontStatus, COLOR_TEXT_SUB);

    RECT iconRect = {rect.right - SCALE_INT(46), rect.top + SCALE_INT(12), rect.right - SCALE_INT(14), rect.top + SCALE_INT(44)};
    FillRoundedRect(hdc, iconRect, RGB(245, 243, 255), RGB(245, 243, 255), SCALE_INT(10));
    DrawClippedText(hdc, glyph, iconRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE, g_hFontSub, accent);

    RECT valueRect = {rect.left + padding, rect.top + SCALE_INT(36), rect.right - padding, rect.top + SCALE_INT(72)};
    DrawClippedText(hdc, value, valueRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE, g_hFontTitle, COLOR_TEXT_MAIN);

    RECT changeRect = {rect.left + padding, rect.bottom - SCALE_INT(28), rect.right - padding, rect.bottom - SCALE_INT(10)};
    DrawClippedText(hdc, change, changeRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE, g_hFontStatus, accent);
}
// ============================================================
// DRAW DASHBOARD VIEW - SIMPLIFIED, NO DEVICE SECTION
// ============================================================
void DrawDashboardView(HDC hdc, RECT rc) {
    int margin = CONTENT_PADDING;
    int contentLeft = SIDEBAR_WIDTH + margin;
    int contentRight = rc.right - margin;
    int contentTop = HEADER_HEIGHT + margin;
    int cardWidth = SCALE_INT(158);
    int statHeight = SCALE_INT(98);
    int spacing = SCALE_INT(12);

    int assignedDevices = 0;
    for (const auto& pair : g_deviceWorkspaceMap) assignedDevices++;

    // ============================================================
    // DRAW CONTENT (NO SCROLLING NEEDED)
    // ============================================================
    int yPos = contentTop;

    // --- Workspaces Title ---
    DrawClippedText(hdc, L"Workspaces", {contentLeft, yPos, contentRight, yPos + SCALE_INT(24)},
        DT_LEFT | DT_VCENTER | DT_SINGLELINE, g_hFontSub, COLOR_TEXT_MAIN);
    yPos += SCALE_INT(34);
    
    // --- Workspace Cards ---
    int cardHeight = SCALE_INT(126);
    int availableWidth = contentRight - contentLeft;
    int cardsPerRow = availableWidth / (cardWidth + spacing);
    if (cardsPerRow < 1) cardsPerRow = 1;
    
    int startX = contentLeft;
    int startY = yPos;
    
    for (size_t i = 0; i < g_workspaces.size() && i < 10; i++) {
        int row = i / cardsPerRow;
        int col = i % cardsPerRow;
        int x = startX + col * (cardWidth + spacing);
        int y = startY + row * (cardHeight + spacing);
        
        RECT cardRect = {x, y, x + cardWidth, y + cardHeight};
        DrawWorkspaceCard(hdc, cardRect, g_workspaces[i], g_selectedWorkspace == (int)i);
    }
    
    int workspaceRows = (g_workspaces.size() + cardsPerRow - 1) / cardsPerRow;
    yPos = startY + workspaceRows * (cardHeight + spacing) + margin;
    
    // --- Overview Title ---
    DrawClippedText(hdc, L"Overview", {contentLeft, yPos, contentRight, yPos + SCALE_INT(24)},
        DT_LEFT | DT_VCENTER | DT_SINGLELINE, g_hFontSub, COLOR_TEXT_MAIN);
    yPos += SCALE_INT(34);
    
    // --- Stat Cards ---
    int statWidth = (contentRight - contentLeft - spacing * 2) / 3;
    
    wchar_t workspacesValue[16];
    swprintf_s(workspacesValue, 16, L"%d", (int)g_workspaces.size());
    wchar_t devicesValue[16];
    swprintf_s(devicesValue, 16, L"%d", (int)g_availableDevices.size());
    wchar_t assignedValue[16];
    swprintf_s(assignedValue, 16, L"%d", assignedDevices);
    
    RECT stat1 = {contentLeft, yPos, contentLeft + statWidth, yPos + statHeight};
    RECT stat2 = {stat1.right + spacing, yPos, stat1.right + spacing + statWidth, yPos + statHeight};
    RECT stat3 = {stat2.right + spacing, yPos, contentRight, yPos + statHeight};
    
    DrawStatCard(hdc, stat1, L"Workspaces", workspacesValue, L"Active", COLOR_ACCENT, L"");
    DrawStatCard(hdc, stat2, L"Devices", devicesValue, L"Detected", COLOR_SUCCESS, L"");
    DrawStatCard(hdc, stat3, L"Assigned", assignedValue, L"Routed", COLOR_WARNING, L"");
}


// ============================================================
// DRAW INTERACTIVE DEVICE LIST
// ============================================================
void DrawDeviceListInteractive(HDC hdc, RECT rect) {
    FillRoundedRect(hdc, rect, COLOR_BG_CARD, COLOR_BORDER, SCALE_INT(12));

    int padding = SCALE_INT(14);
    int headerHeight = SCALE_INT(30);
    int lineHeight = SCALE_INT(34);
    
    // Title
    RECT titleRect = {rect.left + padding, rect.top + SCALE_INT(12), rect.right - padding, rect.top + SCALE_INT(34)};
    DrawClippedText(hdc, L"Devices", titleRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE, g_hFontSub, COLOR_TEXT_MAIN);

    wchar_t countText[32];
    swprintf_s(countText, 32, L"%d connected", (int)g_availableDevices.size());
    RECT countRect = {rect.right - SCALE_INT(120), rect.top + SCALE_INT(12), rect.right - padding, rect.top + SCALE_INT(34)};
    DrawClippedText(hdc, countText, countRect, DT_RIGHT | DT_VCENTER | DT_SINGLELINE, g_hFontStatus, COLOR_TEXT_SUB);

    // Header
    RECT headerRect = {rect.left + 1, rect.top + SCALE_INT(46), rect.right - 1, rect.top + SCALE_INT(46) + headerHeight};
    FillSolidRect(hdc, headerRect, COLOR_BG_DARK);
    DrawClippedText(hdc, L"Device", {rect.left + padding, headerRect.top, rect.left + SCALE_INT(260), headerRect.bottom},
        DT_LEFT | DT_VCENTER | DT_SINGLELINE, g_hFontStatus, COLOR_TEXT_SUB);
    DrawClippedText(hdc, L"Workspace", {rect.left + SCALE_INT(270), headerRect.top, rect.left + SCALE_INT(390), headerRect.bottom},
        DT_LEFT | DT_VCENTER | DT_SINGLELINE, g_hFontStatus, COLOR_TEXT_SUB);
    DrawClippedText(hdc, L"Actions", {rect.right - SCALE_INT(170), headerRect.top, rect.right - padding, headerRect.bottom},
        DT_RIGHT | DT_VCENTER | DT_SINGLELINE, g_hFontStatus, COLOR_TEXT_SUB);

    // Calculate visible area
    int contentTop = headerRect.bottom;
    int contentBottom = rect.bottom - SCALE_INT(8);
    int contentHeight = contentBottom - contentTop;
    
    int totalItems = (int)g_availableDevices.size();
    int visibleItems = contentHeight / lineHeight;
    if (visibleItems < 1) visibleItems = 1;
    
    // Calculate max scroll offset
    g_maxDeviceScrollOffset = totalItems - visibleItems;
    if (g_maxDeviceScrollOffset < 0) g_maxDeviceScrollOffset = 0;
    
    // Clamp scroll offset
    if (g_deviceScrollOffset > g_maxDeviceScrollOffset) {
        g_deviceScrollOffset = g_maxDeviceScrollOffset;
    }
    if (g_deviceScrollOffset < 0) {
        g_deviceScrollOffset = 0;
    }
    
    int startIndex = g_deviceScrollOffset;
    int endIndex = startIndex + visibleItems;
       if (endIndex > totalItems) endIndex = totalItems;
    
    int y = contentTop;
    
    // Draw visible devices
    for (int i = startIndex; i < endIndex && i < totalItems; i++) {
        HANDLE handle = g_availableDevices[i];
        std::string name = "Device";
        auto it = g_deviceNames.find(handle);
        if (it != g_deviceNames.end()) {
            name = it->second;
        }
        
        bool assigned = g_deviceWorkspaceMap.find(handle) != g_deviceWorkspaceMap.end();
        COLORREF color = assigned ? COLOR_SUCCESS : COLOR_WARNING;
        
        bool isHovered = (g_hoveredDevice == i);
        bool isSelected = (g_selectedDevice == i);
        
        RECT rowRect = {rect.left + 1, y, rect.right - 1, y + lineHeight};
        if (isHovered || isSelected) {
            FillSolidRect(hdc, rowRect, COLOR_BG_CARD_HOVER);
        }

        RECT dotRect = {rect.left + padding, y + SCALE_INT(12), rect.left + padding + SCALE_INT(8), y + SCALE_INT(20)};
        FillRoundedRect(hdc, dotRect, color, color, SCALE_INT(4));

        std::wstring deviceText = ToWide(name);
        DrawClippedText(hdc, deviceText, {rect.left + padding + SCALE_INT(18), y, rect.left + SCALE_INT(255), y + lineHeight},
            DT_LEFT | DT_VCENTER | DT_SINGLELINE, g_hFontStatus, COLOR_TEXT_MAIN);
        
        if (assigned) {
            DWORD wsId = g_deviceWorkspaceMap[handle];
            char letter = 'A' + (char)wsId;
            wchar_t wsText[16];
            swprintf_s(wsText, 16, L"WS %c", letter);
            DrawClippedText(hdc, wsText, {rect.left + SCALE_INT(270), y, rect.left + SCALE_INT(390), y + lineHeight},
                DT_LEFT | DT_VCENTER | DT_SINGLELINE, g_hFontStatus, COLOR_SUCCESS);
        } else {
            DrawClippedText(hdc, L"Unassigned", {rect.left + SCALE_INT(270), y, rect.left + SCALE_INT(390), y + lineHeight},
                DT_LEFT | DT_VCENTER | DT_SINGLELINE, g_hFontStatus, COLOR_WARNING);
        }
        
        // In DrawDeviceListInteractive, when a device is hovered, show right-click hint


        if (isHovered) {  // Show right-click hint

          RECT hintRect = {rect.left + padding + SCALE_INT(18), y, rect.left + SCALE_INT(255), y + lineHeight};
            SetTextColor(hdc, COLOR_ACCENT);
            DrawClippedText(hdc, L"Right-click to assign", hintRect, DT_RIGHT | DT_VCENTER | DT_SINGLELINE, 
              g_hFontStatus, COLOR_ACCENT);
}
        // Hover actions - click to assign
        if (isHovered) {
            int btnX = rect.right - SCALE_INT(140);
            int btnY = y + SCALE_INT(7);
            int btnWidth = SCALE_INT(20);
            int btnHeight = SCALE_INT(20);
            int btnSpacing = SCALE_INT(4);
            
            std::vector<WorkspaceInfo> availableWS;
            for (const auto& ws : g_workspaces) {
                bool alreadyAssigned = false;
                for (const auto& pair : g_deviceWorkspaceMap) {
                    if (pair.first == handle && pair.second == ws.id) {
                        alreadyAssigned = true;
                        break;
                    }
                }
                if (!alreadyAssigned) {
                    availableWS.push_back(ws);
                }
            }
            
            if (assigned) {
                RECT btnRect = {btnX, btnY, btnX + btnWidth * 2, btnY + btnHeight};
                FillRoundedRect(hdc, btnRect, COLOR_ERROR, COLOR_ERROR, SCALE_INT(8));
                DrawClippedText(hdc, L"X", btnRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE, g_hFontStatus, RGB(255, 255, 255));
            }
            
            int maxBtns = (int)availableWS.size();
            if (maxBtns > 4) maxBtns = 4;
            
            for (int j = 0; j < maxBtns; j++) {
                int x = btnX - (j + 1) * (btnWidth + btnSpacing);
                RECT btnRect = {x, btnY, x + btnWidth, btnY + btnHeight};
                
                COLORREF btnColor = availableWS[j].color;
                wchar_t btnLetter[2] = { (wchar_t)availableWS[j].letter, 0 };
                FillRoundedRect(hdc, btnRect, btnColor, btnColor, SCALE_INT(8));
                DrawClippedText(hdc, btnLetter, btnRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE, g_hFontStatus, RGB(255, 255, 255));
            }
        }
        
        y += lineHeight;
    }
    
    // ============================================================
    // DRAW SCROLLBAR
    // ============================================================
    if (totalItems > visibleItems) {
        int scrollBarWidth = SCALE_INT(12);
        int scrollBarX = rect.right - scrollBarWidth - 2;
        int scrollBarHeight = contentHeight;
        int scrollBarY = contentTop;
        
        // Scroll bar background
        RECT scrollBg = {scrollBarX, scrollBarY, scrollBarX + scrollBarWidth, scrollBarY + scrollBarHeight};
        FillSolidRect(hdc, scrollBg, COLOR_BORDER);
        
        // Scroll bar thumb
        float thumbRatio = (float)visibleItems / (float)totalItems;
        float thumbHeight = scrollBarHeight * thumbRatio;
        if (thumbHeight < 20) thumbHeight = 20;
        
        float thumbPos = (float)g_deviceScrollOffset / (float)(totalItems - visibleItems);
        float thumbY = scrollBarY + (scrollBarHeight - thumbHeight) * thumbPos;
        
        RECT thumbRect = {scrollBarX + 1, (int)thumbY, scrollBarX + scrollBarWidth - 1, (int)(thumbY + thumbHeight)};
        FillRoundedRect(hdc, thumbRect, COLOR_ACCENT, COLOR_ACCENT, SCALE_INT(4));
    }
    
    // "No devices" message
    if (g_availableDevices.empty()) {
        RECT emptyRect = {rect.left + padding, headerRect.bottom + SCALE_INT(20), rect.right - padding, headerRect.bottom + SCALE_INT(70)};
        DrawClippedText(hdc, L"No input devices detected", emptyRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE, g_hFontSub, COLOR_TEXT_MUTED);
    }
}

// ============================================================
// DRAW TREE VIEW
// ============================================================
void DrawTreeView(HDC hdc, RECT rc) {
    if (g_hTreeView) {
        RECT treeRect = {rc.left, rc.top, rc.right, rc.bottom};
        MoveWindow(g_hTreeView, treeRect.left, treeRect.top, 
                   treeRect.right - treeRect.left, treeRect.bottom - treeRect.top, TRUE);
        ShowWindow(g_hTreeView, SW_SHOW);
        RefreshTree();
    }
}

// ============================================================
// DRAW WORKSPACES VIEW
// ============================================================
void DrawWorkspacesView(HDC hdc, RECT rc) {
    int margin = CONTENT_PADDING;
    int contentLeft = SIDEBAR_WIDTH + margin;
    int contentRight = rc.right - margin;
    int contentTop = HEADER_HEIGHT + margin;

    DrawClippedText(hdc, L"Workspace Management", {contentLeft, contentTop, contentRight, contentTop + SCALE_INT(28)},
        DT_LEFT | DT_VCENTER | DT_SINGLELINE, g_hFontSub, COLOR_TEXT_MAIN);

    int cardWidth = SCALE_INT(180);
    int cardHeight = SCALE_INT(140);
    int spacing = SCALE_INT(12);
    int availableWidth = contentRight - contentLeft;
    int cardsPerRow = availableWidth / (cardWidth + spacing);
    if (cardsPerRow < 1) cardsPerRow = 1;
    
    int startX = contentLeft;
    int yPos = contentTop + SCALE_INT(40);
    
    for (size_t i = 0; i < g_workspaces.size() && i < 10; i++) {
        int row = i / cardsPerRow;
        int col = i % cardsPerRow;
        int x = startX + col * (cardWidth + spacing);
        int y = yPos + row * (cardHeight + spacing);
        
        RECT cardRect = {x, y, x + cardWidth, y + cardHeight};
        DrawWorkspaceCard(hdc, cardRect, g_workspaces[i], g_selectedWorkspace == (int)i);
    }
}

// ============================================================
// DRAW DEVICES VIEW - With right-click assignment
// ============================================================
void DrawDevicesView(HDC hdc, RECT rc) {
    int margin = CONTENT_PADDING;
    int contentLeft = SIDEBAR_WIDTH + margin;
    int contentRight = rc.right - margin;
    int contentTop = HEADER_HEIGHT + margin;

    // Show which workspace we're assigning to (if selected from dashboard)
    if (g_selectedWorkspace >= 0 && g_selectedWorkspace < (int)g_workspaces.size()) {
        char letter = 'A' + (char)g_selectedWorkspace;
        wchar_t headerText[128];
        swprintf_s(headerText, 128, L"Device Management - Assigning to Workspace %c", letter);
        DrawClippedText(hdc, headerText, 
            {contentLeft, contentTop, contentRight, contentTop + SCALE_INT(28)},
            DT_LEFT | DT_VCENTER | DT_SINGLELINE, g_hFontSub, COLOR_TEXT_MAIN);
        
        DrawClippedText(hdc, L"Right-click on a device to assign it to this workspace", 
            {contentLeft, contentTop + SCALE_INT(30), contentRight, contentTop + SCALE_INT(54)},
            DT_LEFT | DT_VCENTER | DT_SINGLELINE, g_hFontStatus, COLOR_TEXT_SUB);
    } else {
        DrawClippedText(hdc, L"Device Management", 
            {contentLeft, contentTop, contentRight, contentTop + SCALE_INT(28)},
            DT_LEFT | DT_VCENTER | DT_SINGLELINE, g_hFontSub, COLOR_TEXT_MAIN);
        
        DrawClippedText(hdc, L"Right-click on a device to assign or unassign it", 
            {contentLeft, contentTop + SCALE_INT(30), contentRight, contentTop + SCALE_INT(54)},
            DT_LEFT | DT_VCENTER | DT_SINGLELINE, g_hFontStatus, COLOR_TEXT_SUB);
    }

    RECT deviceRect = {contentLeft, contentTop + SCALE_INT(56), contentRight, rc.bottom - SCALE_INT(30)};
    DrawDeviceListInteractive(hdc, deviceRect);
}

// ============================================================
// DRAW MONITORS VIEW
// ============================================================
void DrawMonitorsView(HDC hdc, RECT rc) {
    int margin = CONTENT_PADDING;
    int contentLeft = SIDEBAR_WIDTH + margin;
    int contentRight = rc.right - margin;
    int contentTop = HEADER_HEIGHT + margin;

    DrawClippedText(hdc, L"Monitors", {contentLeft, contentTop, contentRight, contentTop + SCALE_INT(28)},
        DT_LEFT | DT_VCENTER | DT_SINGLELINE, g_hFontSub, COLOR_TEXT_MAIN);

    if (g_displayManager) {
        auto monitors = g_displayManager->EnumerateDisplays();
        int yPos = contentTop + SCALE_INT(40);
        int monitorHeight = SCALE_INT(80);
        int spacing = SCALE_INT(10);
        
        for (size_t i = 0; i < monitors.size(); i++) {
            RECT monRect = {contentLeft, yPos, contentRight, yPos + monitorHeight};
            FillRoundedRect(hdc, monRect, COLOR_BG_CARD, COLOR_BORDER, SCALE_INT(12));
            
            wchar_t label[128];
            swprintf_s(label, 128, L"Monitor %d - %dx%d", (int)(i + 1), 
                monitors[i].Width(), monitors[i].Height());
            if (monitors[i].isPrimary) {
                wcscat_s(label, 128, L" (Primary)");
            }
            DrawClippedText(hdc, label, {monRect.left + SCALE_INT(14), monRect.top + SCALE_INT(12), monRect.right - SCALE_INT(14), monRect.bottom - SCALE_INT(12)},
                DT_LEFT | DT_VCENTER | DT_SINGLELINE, g_hFontCard, COLOR_TEXT_MAIN);
            
            if (i < g_workspaces.size()) {
                const auto& ws = g_workspaces[i];
                wchar_t wsText[64];
                swprintf_s(wsText, 64, L"Workspace %c - %d windows, %d devices", 
                    ws.letter, ws.windowCount, ws.deviceCount);
                DrawClippedText(hdc, wsText, {monRect.left + SCALE_INT(14), monRect.top + SCALE_INT(44), monRect.right - SCALE_INT(14), monRect.bottom - SCALE_INT(12)},
                    DT_LEFT | DT_VCENTER | DT_SINGLELINE, g_hFontStatus, COLOR_ACCENT);
            }
            
            yPos += monitorHeight + spacing;
        }
    }
}

// ============================================================
// DRAW SETTINGS VIEW
// ============================================================
void DrawSettingsView(HDC hdc, RECT rc) {
    int margin = CONTENT_PADDING;
    int contentLeft = SIDEBAR_WIDTH + margin;
    int contentRight = rc.right - margin;
    int contentTop = HEADER_HEIGHT + margin;

    DrawClippedText(hdc, L"Settings", {contentLeft, contentTop, contentRight, contentTop + SCALE_INT(28)},
        DT_LEFT | DT_VCENTER | DT_SINGLELINE, g_hFontSub, COLOR_TEXT_MAIN);

    int yPos = contentTop + SCALE_INT(40);
    int itemHeight = SCALE_INT(50);
    int spacing = SCALE_INT(8);

    struct SettingItem { const wchar_t* label; const wchar_t* value; bool toggle; };
    SettingItem items[] = {
        {L"Border Wall", g_borderVisible ? L"Enabled" : L"Disabled", false},
        {L"Cursor Lock", g_cursorEmulator ? (g_cursorEmulator->IsRealCursorLocked() ? L"Enabled" : L"Disabled") : L"N/A", false},
        {L"Driver Status", g_driverConnected ? L"Connected" : L"Disconnected", false},
        {L"Auto Refresh", L"Enabled", false},
        {L"Show Hidden Devices", L"Disabled", false}
    };

    for (const auto& item : items) {
        RECT itemRect = {contentLeft, yPos, contentRight, yPos + itemHeight};
        FillRoundedRect(hdc, itemRect, COLOR_BG_CARD, COLOR_BORDER, SCALE_INT(10));
        
        DrawClippedText(hdc, item.label, {itemRect.left + SCALE_INT(14), itemRect.top, itemRect.right - SCALE_INT(160), itemRect.bottom},
            DT_LEFT | DT_VCENTER | DT_SINGLELINE, g_hFontCard, COLOR_TEXT_MAIN);
        DrawClippedText(hdc, item.value, {itemRect.right - SCALE_INT(140), itemRect.top, itemRect.right - SCALE_INT(14), itemRect.bottom},
            DT_RIGHT | DT_VCENTER | DT_SINGLELINE, g_hFontStatus, 
            wcscmp(item.value, L"Enabled") == 0 || wcscmp(item.value, L"Connected") == 0 ? COLOR_SUCCESS : COLOR_WARNING);
        
        yPos += itemHeight + spacing;
    }
}

// ============================================================
// DRAW WORKSPACE CARD - With Assign Button
// ============================================================
void DrawWorkspaceCard(HDC hdc, RECT rect, const WorkspaceInfo& ws, bool selected) {
    // Store workspace ID for click detection
    static ULONG clickedWorkspaceId = 0;
    
    FillRoundedRect(hdc, rect, selected ? COLOR_BG_CARD_HOVER : COLOR_BG_CARD, selected ? COLOR_ACCENT : COLOR_BORDER, SCALE_INT(12));

    int padding = SCALE_INT(14);
    int circleSize = SCALE_INT(30);
    int circleX = rect.left + padding;
    int circleY = rect.top + padding;
    
    RECT iconRect = {circleX, circleY, circleX + circleSize, circleY + circleSize};
    wchar_t letter[2] = { (wchar_t)ws.letter, 0 };
    DrawIconBadge(hdc, iconRect, ws.color, letter);

    std::wstring name = L"Workspace " + std::wstring(1, ws.letter);
    RECT nameRect = {circleX + circleSize + SCALE_INT(10), rect.top + padding, rect.right - padding, rect.top + padding + SCALE_INT(20)};
    DrawClippedText(hdc, name, nameRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE, g_hFontCard, COLOR_TEXT_MAIN);

    std::wstring cursorType;
    if (ws.hasRealCursor) {
        cursorType = L"Real cursor";
    } else if (ws.hasVirtualCursor) {
        cursorType = L"Virtual cursor";
    } else {
        cursorType = L"No cursor";
    }
    RECT cursorRect = {nameRect.left, nameRect.bottom + SCALE_INT(2), rect.right - padding, nameRect.bottom + SCALE_INT(18)};
    DrawClippedText(hdc, cursorType, cursorRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE, g_hFontStatus,
        ws.hasRealCursor ? COLOR_ACCENT : (ws.hasVirtualCursor ? COLOR_SUCCESS : COLOR_WARNING));

    // Window count
    wchar_t windowText[32];
    swprintf_s(windowText, 32, L"%d", ws.windowCount);
    RECT valueRect = {rect.left + padding, rect.top + SCALE_INT(68), rect.left + SCALE_INT(80), rect.top + SCALE_INT(102)};
    DrawClippedText(hdc, windowText, valueRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE, g_hFontTitle, COLOR_TEXT_MAIN);

    RECT labelRect = {rect.left + padding, valueRect.bottom - SCALE_INT(2), rect.left + SCALE_INT(110), rect.bottom - padding};
    DrawClippedText(hdc, L"windows", labelRect, DT_LEFT | DT_TOP | DT_SINGLELINE, g_hFontStatus, COLOR_TEXT_SUB);

    // ============================================================
    // ASSIGN BUTTON - Click to go to Devices view
    // ============================================================
    int btnWidth = SCALE_INT(70);
    int btnHeight = SCALE_INT(24);
    int btnX = rect.right - btnWidth - padding;
    int btnY = rect.bottom - btnHeight - SCALE_INT(8);
    
    RECT btnRect = {btnX, btnY, btnX + btnWidth, btnY + btnHeight};
    FillRoundedRect(hdc, btnRect, COLOR_ACCENT, COLOR_ACCENT, SCALE_INT(6));
    DrawClippedText(hdc, L"Assign Device", btnRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE, 
        g_hFontButton, RGB(255, 255, 255));
    
    // Store button rect for click detection (we'll use a global or static)
    // We'll detect clicks in WM_LBUTTONDOWN
    
    // ============================================================
    // ASSIGNED DEVICES LIST
    // ============================================================
    std::wstring assignedDevicesText;
    int deviceCount = 0;
    for (const auto& pair : g_deviceWorkspaceMap) {
        if (pair.second == ws.id) {
            if (deviceCount > 0) assignedDevicesText += L", ";
            auto it = g_deviceNames.find(pair.first);
            if (it != g_deviceNames.end()) {
                assignedDevicesText += ToWide(it->second);
            } else {
                assignedDevicesText += L"Device";
            }
            deviceCount++;
        }
    }
    
    if (deviceCount > 0) {
        RECT devicesRect = {rect.left + padding, rect.bottom - SCALE_INT(48), rect.right - btnWidth - SCALE_INT(8), rect.bottom - SCALE_INT(32)};
        std::wstring label = L"Devices: " + assignedDevicesText;
        DrawClippedText(hdc, label, devicesRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE, g_hFontStatus, COLOR_TEXT_SUB);
    } else {
        RECT hintRect = {rect.left + padding, rect.bottom - SCALE_INT(48), rect.right - btnWidth - SCALE_INT(8), rect.bottom - SCALE_INT(32)};
        DrawClippedText(hdc, L"No devices assigned", hintRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE, g_hFontStatus, COLOR_WARNING);
    }
}

// ============================================================
// TOGGLE BORDER WALL - FIXED
// ============================================================
void ToggleBorderWall() {
    g_borderVisible = !g_borderVisible;
    if (g_cursorEmulator) {
        g_cursorEmulator->ShowCursorBorder(g_borderVisible);
    }
    
    g_lastEvent = g_borderVisible ? "Border ON" : "Border OFF";
    
    // Update button text
    if (g_btnToggleBorder) {
        SetWindowTextW(g_btnToggleBorder, g_borderVisible ? L"Hide Border" : L"Show Border");
    }
    
    // Force overlay window to redraw
    if (g_hOverlayWnd) {
        InvalidateRect(g_hOverlayWnd, NULL, TRUE);
        UpdateWindow(g_hOverlayWnd);
        RedrawWindow(g_hOverlayWnd, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ERASE);
    }
    
    // Also force main window to refresh
    RefreshDashboard();
}

void TestDriverConnection() {
    if (!g_driverInterface) {
        MessageBoxA(g_hMainWnd, "Driver interface is NULL!", "Error", MB_OK | MB_ICONERROR);
        return;
    }
    
    if (g_driverInterface->Open()) {
        g_driverConnected = true;
        g_lastEvent = "Driver connected";
        RefreshDashboard();
        MessageBoxA(g_hMainWnd, "Driver connected successfully!", "Success", MB_OK | MB_ICONINFORMATION);
    } else {
        g_driverConnected = false;
        g_lastEvent = "Driver connection failed";
        RefreshDashboard();
        MessageBoxA(g_hMainWnd, "Failed to connect to driver!", "Error", MB_OK | MB_ICONERROR);
    }
}

void UnassignAllDevices() {
    g_deviceWorkspaceMap.clear();
    if (g_driverInterface && g_driverInterface->IsConnected()) {
        g_driverInterface->ResetDeviceAssignments();
    }
    g_lastEvent = "All devices unassigned";
    RefreshDashboard();
    MessageBoxA(g_hMainWnd, "All devices unassigned!", "Done", MB_OK | MB_ICONINFORMATION);
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
        char letter = 'A' + (char)workspaceId;
        g_lastEvent = "Device -> WS " + std::string(1, letter);
        
        if (g_cursorEmulator) {
            ULONG realWorkspace = g_cursorEmulator->GetRealCursorWorkspace();
            if (workspaceId != realWorkspace) {
                auto* ws = g_workspaceManager->GetWorkspace(workspaceId);
                if (ws) {
                    RECT bounds = ws->GetMonitorBounds();
                    g_cursorEmulator->EnableVirtualCursor(workspaceId, (ULONG)(ULONG_PTR)deviceHandle, bounds);
                }
            }
        }
        
        RefreshDashboard();
    } else {
        MessageBoxA(g_hMainWnd, "Failed to assign device!", "Error", MB_OK | MB_ICONERROR);
    }
}

void AssignDeviceToWorkspaceById(int deviceIndex, int workspaceId) {
    if (deviceIndex < 0 || deviceIndex >= (int)g_availableDevices.size()) {
        return;
    }
    
    if (workspaceId < 0 || workspaceId >= (int)g_workspaces.size()) {
        return;
    }
    
    HANDLE device = g_availableDevices[deviceIndex];
    DWORD wsId = g_workspaces[workspaceId].id;
    AssignDeviceToWorkspace(device, wsId);
}

void RefreshDashboard() {
    UpdateWorkspaceInfo();
    if (g_hTreeView && IsWindowVisible(g_hTreeView)) {
        RefreshTree();
    }
    InvalidateRect(g_hMainWnd, NULL, TRUE);
    UpdateWindow(g_hMainWnd);
}

// ============================================================
// OVERLAY WINDOW PROCEDURE - FIXED
// ============================================================
LRESULT CALLBACK OverlayWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            
            // Clear the overlay with transparent black
            RECT rc;
            GetClientRect(hWnd, &rc);
            HBRUSH blackBrush = CreateSolidBrush(RGB(0, 0, 0));
            FillRect(hdc, &rc, blackBrush);
            DeleteObject(blackBrush);
            
            // Render virtual cursors
            if (g_cursorEmulator) {
                auto* vcManager = g_cursorEmulator->GetVirtualCursorManager();
                if (vcManager) {
                    vcManager->Render(hdc);
                }
            }
            
            // Draw border wall - only if visible
            if (g_borderVisible) {
                DrawBorderWall(hdc);
            }
            
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
            g_hFontTitle = CreateFontW(SCALE_INT(22), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
            g_hFontSub = CreateFontW(SCALE_INT(14), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
            g_hFontCard = CreateFontW(SCALE_INT(12), 0, 0, 0, FW_MEDIUM, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
            g_hFontButton = CreateFontW(SCALE_INT(12), 0, 0, 0, FW_MEDIUM, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
            g_hFontStatus = CreateFontW(SCALE_INT(11), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
            
            g_hBrushBg = CreateSolidBrush(COLOR_BG_DARK);
            g_hBrushCard = CreateSolidBrush(COLOR_BG_CARD);
            g_hBrushCardHover = CreateSolidBrush(COLOR_BG_CARD_HOVER);
            g_hBrushAccent = CreateSolidBrush(COLOR_ACCENT);
            
            HINSTANCE hInst = (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE);
            
            // Tree View
            g_hTreeView = CreateWindowExW(
                WS_EX_CLIENTEDGE,
                WC_TREEVIEWW,
                L"",
                WS_CHILD | WS_VISIBLE | TVS_HASLINES | TVS_LINESATROOT | TVS_HASBUTTONS |
                TVS_SHOWSELALWAYS | TVS_FULLROWSELECT,
                0, 0, 100, 100,
                hWnd,
                (HMENU)300,
                hInst,
                NULL
            );
            SendMessage(g_hTreeView, WM_SETFONT, (WPARAM)g_hFontStatus, TRUE);
            BuildImageList();
            BuildTree();
            ShowWindow(g_hTreeView, SW_HIDE);
            
            // Buttons
            g_btnToggleBorder = CreateWindowExW(0, L"BUTTON", L"Hide Border",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_FLAT,
                0, 0, SCALE_INT(85), SCALE_INT(24), hWnd, (HMENU)101, hInst, NULL);
            SendMessage(g_btnToggleBorder, WM_SETFONT, (WPARAM)g_hFontButton, TRUE);
            
            g_btnRefresh = CreateWindowExW(0, L"BUTTON", L"Refresh",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_FLAT,
                0, 0, SCALE_INT(65), SCALE_INT(24), hWnd, (HMENU)102, hInst, NULL);
            SendMessage(g_btnRefresh, WM_SETFONT, (WPARAM)g_hFontButton, TRUE);
            
            g_btnTestDriver = CreateWindowExW(0, L"BUTTON", L"Test Driver",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_FLAT,
                0, 0, SCALE_INT(75), SCALE_INT(24), hWnd, (HMENU)103, hInst, NULL);
            SendMessage(g_btnTestDriver, WM_SETFONT, (WPARAM)g_hFontButton, TRUE);
            
            g_btnUnassignAll = CreateWindowExW(0, L"BUTTON", L"Unassign All",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_FLAT,
                0, 0, SCALE_INT(75), SCALE_INT(24), hWnd, (HMENU)104, hInst, NULL);
            SendMessage(g_btnUnassignAll, WM_SETFONT, (WPARAM)g_hFontButton, TRUE);
            
            g_hStatusBar = CreateWindowExW(0, STATUSCLASSNAMEW, L"",
                WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
                0, 0, 100, SCALE_INT(24), hWnd, (HMENU)200, hInst, NULL);
            SendMessage(g_hStatusBar, WM_SETFONT, (WPARAM)g_hFontStatus, TRUE);
            ShowWindow(g_hStatusBar, SW_HIDE);
            
            UpdateWorkspaceInfo();
            SetTimer(hWnd, 1, 1000, NULL);
            
            return 0;
        }
        
        case WM_NOTIFY: {
            LPNMHDR nmhdr = (LPNMHDR)lParam;
            if (nmhdr->idFrom == 300 && nmhdr->hwndFrom == g_hTreeView) {
                if (nmhdr->code == NM_RCLICK) {
                    POINT pt;
                    GetCursorPos(&pt);
                    ShowDeviceContextMenu(hWnd, pt);
                    return 1;
                }
            }
            return 0;
        }
        
        case WM_COMMAND: {
            int wmId = LOWORD(wParam);
            switch (wmId) {
                case 101: ToggleBorderWall(); break;
                case 102: RefreshDashboard(); break;
                case 103: TestDriverConnection(); break;
                case 104: UnassignAllDevices(); break;
            }
            return 0;
        }
        
        case WM_SIZE: {
            RECT rc;
            GetClientRect(hWnd, &rc);
            
            int btnX = SCALE_INT(14);
            int btnW = SIDEBAR_WIDTH - SCALE_INT(28);
            int btnH = SCALE_INT(24);
            int btnY = rc.bottom - SCALE_INT(142);

            MoveWindow(g_btnToggleBorder, btnX, btnY, btnW, btnH, TRUE);
            MoveWindow(g_btnRefresh, btnX, btnY + SCALE_INT(30), btnW, btnH, TRUE);
            MoveWindow(g_btnTestDriver, btnX, btnY + SCALE_INT(60), btnW, btnH, TRUE);
            MoveWindow(g_btnUnassignAll, btnX, btnY + SCALE_INT(90), btnW, btnH, TRUE);
            MoveWindow(g_hStatusBar, 0, rc.bottom - SCALE_INT(24), rc.right, SCALE_INT(24), TRUE);
            
            InvalidateRect(hWnd, NULL, TRUE);
            return 0;
        }
        
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            
            RECT rc;
            GetClientRect(hWnd, &rc);
            DrawDashboardShell(hdc, rc);
            
            EndPaint(hWnd, &ps);
            return 0;
        }
        
        case WM_TIMER: {
            if (CheckForChanges()) {
                UpdateWorkspaceInfo();
                if (g_hTreeView && IsWindowVisible(g_hTreeView)) {
                    RefreshTree();
                }
                InvalidateRect(hWnd, NULL, TRUE);
                UpdateWindow(hWnd);
            }
            return 0;
        }
        

        case WM_MOUSEWHEEL: {
    // Handle mouse wheel scrolling for the dashboard
    if (g_currentView == VIEW_DASHBOARD) {
        int delta = GET_WHEEL_DELTA_WPARAM(wParam);
        int scrollAmount = -delta / WHEEL_DELTA * SCALE_INT(20);  // 20px per scroll
        
        int newOffset = g_dashboardScrollOffset + scrollAmount;
        if (newOffset < 0) newOffset = 0;
        if (newOffset > g_maxDashboardScrollOffset) newOffset = g_maxDashboardScrollOffset;
        
        if (newOffset != g_dashboardScrollOffset) {
            g_dashboardScrollOffset = newOffset;
            InvalidateRect(hWnd, NULL, TRUE);
        }
        return 0;
    }
    // Handle scrolling in devices view
    if (g_currentView == VIEW_DEVICES) {
        int delta = GET_WHEEL_DELTA_WPARAM(wParam);
        int scrollAmount = -delta / WHEEL_DELTA;
        
        int newOffset = g_deviceScrollOffset + scrollAmount;
        if (newOffset < 0) newOffset = 0;
        if (newOffset > g_maxDeviceScrollOffset) newOffset = g_maxDeviceScrollOffset;
        
        if (newOffset != g_deviceScrollOffset) {
            g_deviceScrollOffset = newOffset;
            InvalidateRect(hWnd, NULL, TRUE);
        }
        return 0;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}



       case WM_RBUTTONDOWN: {
    RECT rc;
    GetClientRect(hWnd, &rc);
    
    POINT pt;
    GetCursorPos(&pt);
    ScreenToClient(hWnd, &pt);
    
    // Check if right-clicked on a device in Devices view
    if (g_currentView == VIEW_DEVICES || g_currentView == VIEW_DASHBOARD) {
        int margin = CONTENT_PADDING;
        int contentLeft = SIDEBAR_WIDTH + margin;
        int contentRight = rc.right - margin;
        int contentTop = HEADER_HEIGHT + margin;
        
        // Find which device was clicked
        int clickedDeviceIndex = -1;
        int deviceListX = contentLeft;
        int deviceListY = contentTop + SCALE_INT(56);
        int deviceListHeight = rc.bottom - deviceListY - SCALE_INT(24) - margin * 2;
        
        if (deviceListHeight > SCALE_INT(50)) {
            RECT deviceRect = {deviceListX, deviceListY, contentRight, deviceListY + deviceListHeight};
            int y = deviceRect.top + SCALE_INT(76);
            int lineHeight = SCALE_INT(34);
            int maxDisplay = (int)g_availableDevices.size();
            int availableHeight2 = (deviceRect.bottom - y) / lineHeight;
            if (maxDisplay > availableHeight2) maxDisplay = availableHeight2;
            
            for (int i = 0; i < maxDisplay && i < (int)g_availableDevices.size(); i++) {
                RECT itemRect = {deviceRect.left + 2, y - 2, deviceRect.right - 2, y + lineHeight - 2};
                if (pt.x >= itemRect.left && pt.x <= itemRect.right &&
                    pt.y >= itemRect.top && pt.y <= itemRect.bottom) {
                    clickedDeviceIndex = i;
                    break;
                }
                y += lineHeight;
            }
        }
        
        if (clickedDeviceIndex >= 0 && clickedDeviceIndex < (int)g_availableDevices.size()) {
            HANDLE deviceHandle = g_availableDevices[clickedDeviceIndex];
            
            // Create context menu with available workspaces
            HMENU hMenu = CreatePopupMenu();
            
            // Check if device is already assigned
            bool assigned = false;
            ULONG currentWs = 0;
            for (const auto& pair : g_deviceWorkspaceMap) {
                if (pair.first == deviceHandle) {
                    assigned = true;
                    currentWs = pair.second;
                    break;
                }
            }
            
            if (assigned) {
                char letter = 'A' + (char)currentWs;
                wchar_t header[64];
                swprintf_s(header, 64, L"Currently assigned to Workspace %c", letter);
                AppendMenuW(hMenu, MF_STRING | MF_GRAYED, 0, header);
                AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
                AppendMenuW(hMenu, MF_STRING, 999, L"Remove Assignment");
                AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
            }
            
            // Add workspace options
            AppendMenuW(hMenu, MF_STRING | MF_GRAYED, 0, L"Assign to Workspace:");
            for (const auto& ws : g_workspaces) {
                // Check if this workspace already has this device
                bool alreadyAssigned = false;
                for (const auto& pair : g_deviceWorkspaceMap) {
                    if (pair.first == deviceHandle && pair.second == ws.id) {
                        alreadyAssigned = true;
                        break;
                    }
                }
                if (!alreadyAssigned) {
                    wchar_t menuText[32];
                    swprintf_s(menuText, 32, L"Workspace %c", ws.letter);
                    AppendMenuW(hMenu, MF_STRING, 1000 + ws.id, menuText);
                }
            }
            
            POINT screenPt = pt;
            ClientToScreen(hWnd, &screenPt);
            int selection = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_RIGHTBUTTON, 
                                          screenPt.x, screenPt.y, 0, hWnd, NULL);
            DestroyMenu(hMenu);
            
            if (selection == 999) {
                // Remove assignment
                g_deviceWorkspaceMap.erase(deviceHandle);
                if (g_driverInterface && g_driverInterface->IsConnected()) {
                    g_driverInterface->UnassignDevice(deviceHandle);
                }
                g_lastEvent = "Device unassigned";
                RefreshDashboard();
            } else if (selection >= 1000 && selection < 1020) {
                // Assign to workspace
                DWORD wsId = selection - 1000;
                AssignDeviceToWorkspace(deviceHandle, wsId);
                // If we're in Devices view with a selected workspace, switch back to dashboard
                if (g_selectedWorkspace >= 0) {
                    SwitchView(VIEW_DASHBOARD);
                }
            }
            return 0;
        }
    }
    
    return DefWindowProc(hWnd, msg, wParam, lParam);
}



        case WM_MOUSEMOVE: {
            POINT pt;
            GetCursorPos(&pt);
            ScreenToClient(hWnd, &pt);
            
            int newHoverDevice = -1;
            int newHoverWorkspace = -1;
            
            // Check device hover (only in dashboard view)
            if (g_currentView == VIEW_DASHBOARD || g_currentView == VIEW_DEVICES) {
                RECT rc;
                GetClientRect(hWnd, &rc);
                int margin = CONTENT_PADDING;
                int contentLeft = SIDEBAR_WIDTH + margin;
                int contentRight = rc.right - margin;
                int contentTop = HEADER_HEIGHT + margin;
                int cardWidth = SCALE_INT(158);
                int statHeight = SCALE_INT(98);
                int cardHeight = SCALE_INT(126);
                int spacing = SCALE_INT(12);

                int availableWidth = contentRight - contentLeft;
                int cardsPerRow = availableWidth / (cardWidth + spacing);
                if (cardsPerRow < 1) cardsPerRow = 1;
                
                int startX = contentLeft;
                int workspaceTitleY = contentTop + SCALE_INT(34);
                int yPos = workspaceTitleY + ((g_workspaces.size() + cardsPerRow - 1) / cardsPerRow) * (cardHeight + spacing) + margin;
                int deviceListX = contentLeft;
                int deviceListY = yPos + SCALE_INT(34) + margin;
                int deviceListHeight = rc.bottom - deviceListY - SCALE_INT(24) - margin * 2;
                
                if (deviceListHeight > SCALE_INT(50)) {
                    RECT deviceRect = {deviceListX, deviceListY, contentRight, deviceListY + deviceListHeight};
                    
                    int y = deviceRect.top + SCALE_INT(76);
                    int lineHeight = SCALE_INT(34);
                    int maxDisplay = (int)g_availableDevices.size();
                    int availableHeight2 = (deviceRect.bottom - y) / lineHeight;
                    if (maxDisplay > availableHeight2) maxDisplay = availableHeight2;
                    
                    for (int i = 0; i < maxDisplay && i < (int)g_availableDevices.size(); i++) {
                        RECT itemRect = {deviceRect.left + 2, y - 2, deviceRect.right - 2, y + lineHeight - 2};
                        if (pt.x >= itemRect.left && pt.x <= itemRect.right &&
                            pt.y >= itemRect.top && pt.y <= itemRect.bottom) {
                            newHoverDevice = i;
                            break;
                        }
                        y += lineHeight;
                    }
                }
            }
            
            if (newHoverDevice != g_hoveredDevice) {
                g_hoveredDevice = newHoverDevice;
                InvalidateRect(hWnd, NULL, TRUE);
            }
            return 0;
        }
        
// ============================================================
// DRAW DEVICES VIEW - With right-click assignment
// ============================================================
void DrawDevicesView(HDC hdc, RECT rc) {
    int margin = CONTENT_PADDING;
    int contentLeft = SIDEBAR_WIDTH + margin;
    int contentRight = rc.right - margin;
    int contentTop = HEADER_HEIGHT + margin;

    // Show which workspace we're assigning to (if selected)
    if (g_selectedWorkspace >= 0 && g_selectedWorkspace < (int)g_workspaces.size()) {
        char letter = 'A' + (char)g_selectedWorkspace;
        wchar_t headerText[128];
        swprintf_s(headerText, 128, L"Device Management - Assigning to Workspace %c", letter);
        DrawClippedText(hdc, headerText, {contentLeft, contentTop, contentRight, contentTop + SCALE_INT(28)},
            DT_LEFT | DT_VCENTER | DT_SINGLELINE, g_hFontSub, COLOR_TEXT_MAIN);
        
        // Show hint
        DrawClippedText(hdc, L"Right-click on a device to assign it to this workspace", 
            {contentLeft, contentTop + SCALE_INT(30), contentRight, contentTop + SCALE_INT(54)},
            DT_LEFT | DT_VCENTER | DT_SINGLELINE, g_hFontStatus, COLOR_TEXT_SUB);
    } else {
        DrawClippedText(hdc, L"Device Management", {contentLeft, contentTop, contentRight, contentTop + SCALE_INT(28)},
            DT_LEFT | DT_VCENTER | DT_SINGLELINE, g_hFontSub, COLOR_TEXT_MAIN);
        DrawClippedText(hdc, L"Right-click on a device to assign it to a workspace", 
            {contentLeft, contentTop + SCALE_INT(30), contentRight, contentTop + SCALE_INT(54)},
            DT_LEFT | DT_VCENTER | DT_SINGLELINE, g_hFontStatus, COLOR_TEXT_SUB);
    }

    RECT deviceRect = {contentLeft, contentTop + SCALE_INT(56), contentRight, rc.bottom - SCALE_INT(30)};
    DrawDeviceListInteractive(hdc, deviceRect);
}

    RECT deviceRect = {contentLeft, contentTop + SCALE_INT(56), contentRight, rc.bottom - SCALE_INT(30)};
    DrawDeviceListInteractive(hdc, deviceRect);
}
    RECT rc;
    GetClientRect(hWnd, &rc);
    
    POINT pt;
    GetCursorPos(&pt);
    ScreenToClient(hWnd, &pt);
    
    // Check if right-clicked on a device in Devices view
    if (g_currentView == VIEW_DEVICES || g_currentView == VIEW_DASHBOARD) {
        int margin = CONTENT_PADDING;
        int contentLeft = SIDEBAR_WIDTH + margin;
        int contentRight = rc.right - margin;
        int contentTop = HEADER_HEIGHT + margin;
        
        // Find which device was clicked
        int clickedDeviceIndex = -1;
        int deviceListX = contentLeft;
        int deviceListY = contentTop + SCALE_INT(56);
        int deviceListHeight = rc.bottom - deviceListY - SCALE_INT(24) - margin * 2;
        
        // Check if device list is visible
        if (deviceListHeight > SCALE_INT(50)) {
            RECT deviceRect = {deviceListX, deviceListY, contentRight, deviceListY + deviceListHeight};
            int y = deviceRect.top + SCALE_INT(76);
            int lineHeight = SCALE_INT(34);
            
            // Get visible devices (considering scroll offset if any)
            int totalItems = (int)g_availableDevices.size();
            int visibleItems = (deviceRect.bottom - y) / lineHeight;
            if (visibleItems < 1) visibleItems = 1;
            
            int startIndex = g_deviceScrollOffset;
            int endIndex = startIndex + visibleItems;
            if (endIndex > totalItems) endIndex = totalItems;
            
            for (int i = startIndex; i < endIndex && i < totalItems; i++) {
                RECT itemRect = {deviceRect.left + 2, y - 2, deviceRect.right - 2, y + lineHeight - 2};
                if (pt.x >= itemRect.left && pt.x <= itemRect.right &&
                    pt.y >= itemRect.top && pt.y <= itemRect.bottom) {
                    clickedDeviceIndex = i;
                    break;
                }
                y += lineHeight;
            }
        }
        
        if (clickedDeviceIndex >= 0 && clickedDeviceIndex < (int)g_availableDevices.size()) {
            HANDLE deviceHandle = g_availableDevices[clickedDeviceIndex];
            
            // Get device name
            std::string deviceName = "Device";
            auto it = g_deviceNames.find(deviceHandle);
            if (it != g_deviceNames.end()) {
                deviceName = it->second;
            }
            
            // Create context menu
            HMENU hMenu = CreatePopupMenu();
            
            // Header - show device name
            std::wstring headerText = L"Device: " + ToWide(deviceName);
            AppendMenuW(hMenu, MF_STRING | MF_GRAYED, 0, headerText.c_str());
            AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
            
            // Check if device is already assigned
            bool assigned = false;
            ULONG currentWs = 0;
            for (const auto& pair : g_deviceWorkspaceMap) {
                if (pair.first == deviceHandle) {
                    assigned = true;
                    currentWs = pair.second;
                    break;
                }
            }
            
            if (assigned) {
                char letter = 'A' + (char)currentWs;
                wchar_t assignedText[64];
                swprintf_s(assignedText, 64, L"Currently: Workspace %c", letter);
                AppendMenuW(hMenu, MF_STRING | MF_GRAYED, 0, assignedText);
                AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
                AppendMenuW(hMenu, MF_STRING, 999, L"Remove Assignment");
                AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
            }
            
            // Add "Assign to Workspace" options
            AppendMenuW(hMenu, MF_STRING | MF_GRAYED, 0, L"Assign to:");
            
            bool hasAvailableWorkspace = false;
            for (const auto& ws : g_workspaces) {
                // Check if this workspace already has this device
                bool alreadyAssigned = false;
                for (const auto& pair : g_deviceWorkspaceMap) {
                    if (pair.first == deviceHandle && pair.second == ws.id) {
                        alreadyAssigned = true;
                        break;
                    }
                }
                if (!alreadyAssigned) {
                    wchar_t menuText[32];
                    swprintf_s(menuText, 32, L"Workspace %c", ws.letter);
                    AppendMenuW(hMenu, MF_STRING, 1000 + ws.id, menuText);
                    hasAvailableWorkspace = true;
                }
            }
            
            if (!hasAvailableWorkspace) {
                AppendMenuW(hMenu, MF_STRING | MF_GRAYED, 0, L"(All workspaces assigned)");
            }
            
            // Show the menu
            POINT screenPt = pt;
            ClientToScreen(hWnd, &screenPt);
            int selection = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_RIGHTBUTTON, 
                                          screenPt.x, screenPt.y, 0, hWnd, NULL);
            DestroyMenu(hMenu);
            
            // Process selection
            if (selection == 999) {
                // Remove assignment
                g_deviceWorkspaceMap.erase(deviceHandle);
                if (g_driverInterface && g_driverInterface->IsConnected()) {
                    g_driverInterface->UnassignDevice(deviceHandle);
                }
                g_lastEvent = "Device unassigned";
                RefreshDashboard();
                MessageBoxA(hWnd, ("Device unassigned: " + deviceName).c_str(), "Unassigned", MB_OK | MB_ICONINFORMATION);
            } else if (selection >= 1000 && selection < 1020) {
                // Assign to workspace
                DWORD wsId = selection - 1000;
                
                // Check if device is already assigned to another workspace
                if (assigned) {
                    // Unassign first
                    g_deviceWorkspaceMap.erase(deviceHandle);
                    if (g_driverInterface && g_driverInterface->IsConnected()) {
                        g_driverInterface->UnassignDevice(deviceHandle);
                    }
                }
                
                AssignDeviceToWorkspace(deviceHandle, wsId);
                
                // If we came from dashboard with a selected workspace, switch back
                if (g_selectedWorkspace >= 0) {
                    SwitchView(VIEW_DASHBOARD);
                }
            }
            return 0;
        }
    }
    
    return DefWindowProc(hWnd, msg, wParam, lParam);
}
        
        case WM_DESTROY:
            KillTimer(hWnd, 1);
            if (g_hFontTitle) DeleteObject(g_hFontTitle);
            if (g_hFontSub) DeleteObject(g_hFontSub);
            if (g_hFontCard) DeleteObject(g_hFontCard);
            if (g_hFontButton) DeleteObject(g_hFontButton);
            if (g_hFontStatus) DeleteObject(g_hFontStatus);
            if (g_hBrushBg) DeleteObject(g_hBrushBg);
            if (g_hBrushCard) DeleteObject(g_hBrushCard);
            if (g_hBrushCardHover) DeleteObject(g_hBrushCardHover);
            if (g_hBrushAccent) DeleteObject(g_hBrushAccent);
            if (g_hImageList) ImageList_Destroy(g_hImageList);
            if (g_cursorEmulator) { delete g_cursorEmulator; g_cursorEmulator = nullptr; }
            PostQuitMessage(0);
            return 0;
        
        default:
            return DefWindowProc(hWnd, msg, wParam, lParam);
    }
}

// ============================================================
// MAIN ENTRY POINT
// ============================================================
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    try {
        LOG_INFO("DualDesk Dashboard starting...");

        INITCOMMONCONTROLSEX icex = {0};
        icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
        icex.dwICC = ICC_TREEVIEW_CLASSES | ICC_STANDARD_CLASSES | ICC_BAR_CLASSES;
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
            driverInterface.RegisterFilters();
            LOG_INFO("Driver connected");
        } else {
            g_driverConnected = false;
            LOG_WARN("Driver not available");
        }

        // Overlay Window
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
            WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED,
            L"DualDeskOverlayClass", L"", WS_POPUP,
            vLeft, vTop, vWidth, vHeight, NULL, NULL, hInstance, NULL);
        SetLayeredWindowAttributes(g_hOverlayWnd, RGB(0, 0, 0), 0, LWA_COLORKEY);
        ShowWindow(g_hOverlayWnd, SW_SHOWNOACTIVATE);

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

        // Main Window
        WNDCLASSEXW wc = {0};
        wc.cbSize = sizeof(WNDCLASSEXW);
        wc.lpfnWndProc = MainWndProc;
        wc.hInstance = hInstance;
        wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = CreateSolidBrush(COLOR_BG_DARK);
        wc.lpszClassName = L"DualDeskDashboardClass";
        RegisterClassExW(&wc);

        int screenWidth = GetSystemMetrics(SM_CXSCREEN);
        int screenHeight = GetSystemMetrics(SM_CYSCREEN);
        int windowWidth = SCALE_INT(700);
        int windowHeight = SCALE_INT(500);
        int x = (screenWidth - windowWidth) / 2;
        int y = (screenHeight - windowHeight) / 2;

        g_hMainWnd = CreateWindowExW(
            WS_EX_APPWINDOW,
            L"DualDeskDashboardClass",
            L"DualDesk",
            WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_VISIBLE | WS_CLIPCHILDREN,
            x, y, windowWidth, windowHeight,
            NULL, NULL, hInstance, NULL
        );

        if (!g_hMainWnd) {
            LOG_ERROR("Failed to create main window");
            return 1;
        }

        HMENU hMenu = GetSystemMenu(g_hMainWnd, FALSE);
        if (hMenu) {
            DeleteMenu(hMenu, SC_MAXIMIZE, MF_BYCOMMAND);
        }

        ShowWindow(g_hMainWnd, nCmdShow);
        UpdateWindow(g_hMainWnd);

        // Input Window
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
            RefreshDashboard();
        });

        inputManager.SetInputEventCallback([](const InputEvent& event) {
            if (g_inputRouter) {
                g_inputRouter->RouteInput(event);
            }
        });

        windowMover.Initialize(&workspaceManager);
        windowMover.SetMoveCallback([](HWND hwnd, HMONITOR from, HMONITOR to) {
            g_lastEvent = "Window moved";
            RefreshDashboard();
        });

        MSG msg;
        DWORD lastCheck = GetTickCount();

        while (g_running && GetMessage(&msg, NULL, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            
            DWORD now = GetTickCount();
            if (now - lastCheck >= 500) {
                lastCheck = now;
                inputManager.CheckForDeviceChanges();
            }
        }

        if (g_cursorEmulator) {
            delete g_cursorEmulator;
            g_cursorEmulator = nullptr;
        }
        
        if (g_hMainWnd) DestroyWindow(g_hMainWnd);
        if (g_hOverlayWnd) DestroyWindow(g_hOverlayWnd);

        LOG_INFO("DualDesk exited");
        return 0;
    }
    catch (const std::exception& e) {
        MessageBoxA(NULL, e.what(), "Fatal Error", MB_OK | MB_ICONERROR);
        return 1;
    }
}


// ============================================================
// DRAW BORDER WALL - Add this function
// ============================================================
void DrawBorderWall(HDC hdc) {
    if (!g_borderVisible) return;
    if (!g_cursorEmulator) return;
    if (!g_workspaceManager) return;
    
    auto workspaces = g_workspaceManager->GetAllWorkspaces();
    if (workspaces.empty()) return;
    
    ULONG realWorkspace = g_cursorEmulator->GetRealCursorWorkspace();
    
    for (auto* ws : workspaces) {
        ULONG wsId = ws->GetId();
        RECT bounds = ws->GetMonitorBounds();
        
        if (wsId != realWorkspace) continue;
        
        HPEN pen = CreatePen(PS_SOLID, 4, RGB(255, 0, 0));
        HPEN oldPen = (HPEN)SelectObject(hdc, pen);
        HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
        
        Rectangle(hdc, bounds.left, bounds.top, bounds.right, bounds.bottom);
        
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(255, 0, 0));
        
        const wchar_t* topText = L"===== CURSOR WALL =====";
        int textWidth = 200;
        TextOutW(hdc, (bounds.left + bounds.right) / 2 - textWidth/2, 
                 bounds.top + 2, topText, (int)wcslen(topText));
        
        TextOutW(hdc, (bounds.left + bounds.right) / 2 - textWidth/2, 
                 bounds.bottom - 22, topText, (int)wcslen(topText));
        
        wchar_t wsNumber[10];
        swprintf_s(wsNumber, 10, L"WS %lu", wsId + 1);
        
        HFONT hFont = CreateFontW(48, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                  DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                  DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Arial");
        HFONT oldFont = (HFONT)SelectObject(hdc, hFont);
        
        SetTextColor(hdc, RGB(255, 0, 0));
        int xPos = bounds.left + 20;
        int yPos = bounds.bottom - 60;
        
        SetTextColor(hdc, RGB(0, 0, 0));
        TextOutW(hdc, xPos + 2, yPos + 2, wsNumber, (int)wcslen(wsNumber));
        SetTextColor(hdc, RGB(255, 0, 0));
        TextOutW(hdc, xPos, yPos, wsNumber, (int)wcslen(wsNumber));
        
        SelectObject(hdc, oldFont);
        DeleteObject(hFont);
        
        for (int y = bounds.top + 40; y < bounds.bottom - 40; y += 20) {
            TextOutW(hdc, bounds.left + 2, y, L"|", 1);
        }
        
        for (int y = bounds.top + 40; y < bounds.bottom - 40; y += 20) {
            TextOutW(hdc, bounds.right - 12, y, L"|", 1);
        }
        
        SelectObject(hdc, oldPen);
        SelectObject(hdc, oldBrush);
        DeleteObject(pen);
    }
}

// ============================================================
// FLASH BORDER WALL - Add this to main.cpp
// ============================================================
void FlashBorderWall() {
    if (!g_hOverlayWnd || !g_borderVisible || !g_cursorEmulator) return;
    
    static bool flashState = false;
    flashState = !flashState;
    
    HDC hdc = GetDC(g_hOverlayWnd);
    if (hdc) {
        auto workspaces = g_workspaceManager->GetAllWorkspaces();
        for (auto* ws : workspaces) {
            ULONG wsId = ws->GetId();
            if (wsId == g_cursorEmulator->GetRealCursorWorkspace()) {
                RECT bounds = ws->GetMonitorBounds();
                
                HPEN pen = CreatePen(PS_SOLID, 6, flashState ? RGB(255, 255, 0) : RGB(255, 0, 0));
                HPEN oldPen = (HPEN)SelectObject(hdc, pen);
                HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
                
                Rectangle(hdc, bounds.left, bounds.top, bounds.right, bounds.bottom);
                
                SelectObject(hdc, oldPen);
                SelectObject(hdc, oldBrush);
                DeleteObject(pen);
            }
        }
        ReleaseDC(g_hOverlayWnd, hdc);
    }
}

// ============================================================
// SHOW ASSIGN DEVICE POPUP
// ============================================================
void ShowAssignDevicePopup(HWND hWnd, POINT pt, ULONG workspaceId) {
    // Get unassigned devices
    std::vector<std::pair<HANDLE, std::string>> unassignedDevices;
    for (const auto& handle : g_availableDevices) {
        // Check if device is already assigned to any workspace
        bool assigned = false;
        for (const auto& pair : g_deviceWorkspaceMap) {
            if (pair.first == handle) {
                assigned = true;
                break;
            }
        }
        if (!assigned) {
            auto it = g_deviceNames.find(handle);
            if (it != g_deviceNames.end()) {
                unassignedDevices.push_back({handle, it->second});
            } else {
                unassignedDevices.push_back({handle, "Unknown Device"});
            }
        }
    }
    
    if (unassignedDevices.empty()) {
        MessageBoxA(hWnd, "No unassigned devices available!", "Info", MB_OK | MB_ICONINFORMATION);
        return;
    }
    
    // Create popup menu
    HMENU hMenu = CreatePopupMenu();
    
    // Add header
    AppendMenuW(hMenu, MF_STRING | MF_GRAYED, 0, L"--- Select a device to assign ---");
    
    int menuId = 1000;
    for (const auto& device : unassignedDevices) {
        std::wstring menuText = ToWide(device.second);
        AppendMenuW(hMenu, MF_STRING, menuId, menuText.c_str());
        menuId++;
    }
    
    // Show popup
    int selection = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_RIGHTBUTTON, 
                                   pt.x, pt.y, 0, hWnd, NULL);
    DestroyMenu(hMenu);
    
    // Process selection
    if (selection >= 1000 && selection < 1000 + (int)unassignedDevices.size()) {
        int index = selection - 1000;
        HANDLE deviceHandle = unassignedDevices[index].first;
        AssignDeviceToWorkspace(deviceHandle, workspaceId);
    }
}

// ============================================================
// DRAW DASHBOARD SHELL - Main layout function
// ============================================================
void DrawDashboardShell(HDC hdc, RECT rc) {
    // Draw sidebar
    RECT sidebar = {0, 0, SIDEBAR_WIDTH, rc.bottom};
    DrawSidebar(hdc, sidebar);

    // Draw header
    RECT header = {SIDEBAR_WIDTH, 0, rc.right, HEADER_HEIGHT};
    FillSolidRect(hdc, header, COLOR_BG_CARD);
    FillSolidRect(hdc, {SIDEBAR_WIDTH, HEADER_HEIGHT - 1, rc.right, HEADER_HEIGHT}, COLOR_BORDER);

    // Draw content area
    RECT content = {SIDEBAR_WIDTH, HEADER_HEIGHT, rc.right, rc.bottom - SCALE_INT(24)};
    FillSolidRect(hdc, content, COLOR_BG_DARK);

    // Draw the current view
    switch (g_currentView) {
        case VIEW_DASHBOARD: 
            DrawDashboardView(hdc, rc); 
            break;
        case VIEW_TREE: 
            DrawTreeView(hdc, content); 
            break;
        case VIEW_WORKSPACES: 
            DrawWorkspacesView(hdc, rc); 
            break;
        case VIEW_DEVICES: 
            DrawDevicesView(hdc, rc); 
            break;
        case VIEW_MONITORS: 
            DrawMonitorsView(hdc, rc); 
            break;
        case VIEW_SETTINGS: 
            DrawSettingsView(hdc, rc); 
            break;
    }

    // Draw status bar at bottom
    DrawStatusBar(hdc, {SIDEBAR_WIDTH, rc.bottom - SCALE_INT(24), rc.right, rc.bottom});
}

// ============================================================
// DRAW STATUS BAR
// ============================================================
void DrawStatusBar(HDC hdc, RECT rect) {
    // Background
    FillSolidRect(hdc, rect, COLOR_BG_CARD);
    
    // Top border line
    FillSolidRect(hdc, {rect.left, rect.top, rect.right, rect.top + 1}, COLOR_BORDER);

    int x = rect.left + SCALE_INT(14);
    int y = rect.top + SCALE_INT(5);
    int spacing = SCALE_INT(128);

    // ============================================================
    // DRIVER STATUS
    // ============================================================
    std::wstring status = L"Driver: " + std::wstring(g_driverConnected ? L"Connected" : L"Disconnected");
    DrawClippedText(hdc, status, 
        {x, y, x + spacing, rect.bottom}, 
        DT_LEFT | DT_TOP | DT_SINGLELINE,
        g_hFontStatus, 
        g_driverConnected ? COLOR_SUCCESS : COLOR_ERROR);

    // ============================================================
    // BORDER STATUS
    // ============================================================
    x += spacing;
    std::wstring border = L"Border: " + std::wstring(g_borderVisible ? L"ON" : L"OFF");
    DrawClippedText(hdc, border, 
        {x, y, x + spacing, rect.bottom}, 
        DT_LEFT | DT_TOP | DT_SINGLELINE,
        g_hFontStatus, 
        g_borderVisible ? COLOR_WARNING : COLOR_TEXT_SUB);

    // ============================================================
    // LAST EVENT
    // ============================================================
    x += spacing;
    std::wstring eventText = L"Event: " + ToWide(g_lastEvent);
    DrawClippedText(hdc, eventText, 
        {x, y, x + spacing * 2, rect.bottom}, 
        DT_LEFT | DT_TOP | DT_SINGLELINE,
        g_hFontStatus, 
        COLOR_TEXT_SUB);

    // ============================================================
    // DEVICE COUNT
    // ============================================================
    x += spacing * 2;
    wchar_t devCount[32];
    swprintf_s(devCount, 32, L"Devices: %d", (int)g_availableDevices.size());
    DrawClippedText(hdc, devCount, 
        {x, y, rect.right - SCALE_INT(10), rect.bottom}, 
        DT_LEFT | DT_TOP | DT_SINGLELINE,
        g_hFontStatus, 
        COLOR_TEXT_SUB);
}