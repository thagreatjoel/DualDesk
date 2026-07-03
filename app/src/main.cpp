#include "dualdesk/core/logger.h"
#include "dualdesk/display/display_manager.h"
#include "dualdesk/workspace/window_tracker.h"
#include "dualdesk/workspace/workspace_manager.h"
#include "dualdesk/workspace/window_mover.h"
#include "dualdesk/input/input_manager.h"
#include "dualdesk/input/input_router.h"
#include "dualdesk/core/driver_interface.h"
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <string>
#include <sstream>
#include <dbt.h>
#include <vector>
#include <set>
#include <map>
#include <cctype>
#include <fstream>

#pragma comment(lib, "comctl32.lib")

using namespace dualdesk;

// Global pointers
InputManager* g_inputManager = nullptr;
WindowMover* g_windowMover = nullptr;
DisplayManager* g_displayManager = nullptr;
WorkspaceManager* g_workspaceManager = nullptr;
DriverInterface* g_driverInterface = nullptr;
HWND g_statusWindow = nullptr;
HWND g_treeView = nullptr;
HWND g_statusBar = nullptr;
HFONT g_font = nullptr;
HFONT g_fontBold = nullptr;
HIMAGELIST g_imageList = nullptr;
bool g_running = true;
std::string g_lastEvent = "None";
bool g_driverConnected = false;
bool g_refreshPaused = false;

// Root category nodes
HTREEITEM g_nodeMonitors = nullptr;
HTREEITEM g_nodeWorkspaces = nullptr;
HTREEITEM g_nodeDevices = nullptr;
HTREEITEM g_nodeWindows = nullptr;

// Device assignment tracking
std::map<HANDLE, DWORD> g_deviceWorkspaceMap;

// Cache for change detection
int g_cachedMonitorCount = -1;
int g_cachedWindowCount = -1;
int g_cachedDeviceCount = -1;
bool g_cachedDriverState = false;

// Icon indices
enum IconIndex {
    ICON_FOLDER = 0,
    ICON_MONITOR,
    ICON_WORKSPACE,
    ICON_KEYBOARD,
    ICON_MOUSE,
    ICON_TOUCHPAD,
    ICON_OK,
    ICON_WARN,
    ICON_WINDOW,
    ICON_COUNT
};

// Forward declarations
LRESULT CALLBACK StatusWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK InputWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
void UpdateStatusWindow();
void BuildTree(HWND parent);
void RefreshTree();
void LayoutControls(HWND hwnd);
void UpdateStatusBar();
void ShowDeviceContextMenu(HWND hwnd, POINT pt);
void AssignDeviceToWorkspace(HANDLE deviceHandle, DWORD workspaceId);
bool CheckForChanges();

// ===================== DEVICE IDENTIFICATION =====================

std::string GetCleanDeviceName(const std::wstring& deviceName) {
    std::string name;
    for (wchar_t c : deviceName) {
        if (c < 128) name += (char)c;
    }
    
    std::string result = name;
    
    size_t pos = result.find_last_of("\\#");
    if (pos != std::string::npos && pos + 1 < result.length()) {
        result = result.substr(pos + 1);
    }
    
    std::vector<std::string> removeSuffixes = {
        "&Col01", "&Col02", "&Col03", "&Col04", 
        "&MI_00", "&MI_01", "&MI_02", "&MI_03",
        "&0000", "&0001", "&0002", "&0003",
        "#", "_"
    };
    
    for (const auto& suffix : removeSuffixes) {
        size_t found = result.find(suffix);
        if (found != std::string::npos) {
            result = result.substr(0, found);
        }
    }
    
    while (!result.empty() && (result.back() == '&' || result.back() == '#' || 
                               result.back() == '_' || result.back() == '\\')) {
        result.pop_back();
    }
    
    if (result.empty() || result.length() < 3 || 
        result == "Keyboard" || result == "Mouse" || 
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
            if (lowerName.find("keyboard") != std::string::npos) {
                result = "USB Keyboard";
            } else if (lowerName.find("mouse") != std::string::npos) {
                result = "USB Mouse";
            } else if (lowerName.find("touchpad") != std::string::npos) {
                result = "Touchpad";
            } else if (lowerName.find("bluetooth") != std::string::npos) {
                result = "Bluetooth Device";
            }
        }
        
        if (result.empty() || result == "Keyboard" || result == "Mouse") {
            static int deviceCounter = 0;
            deviceCounter++;
            result = "Device " + std::to_string(deviceCounter);
        }
    }
    
    if (result.length() > 35) {
        result = result.substr(0, 32) + "...";
    }
    
    return result;
}

std::string GetDeviceInstanceId(const std::wstring& devicePath) {
    std::string result;
    for (wchar_t c : devicePath) {
        if (c < 128) result += (char)c;
    }
    
    size_t pos = result.find_last_of("\\#");
    if (pos != std::string::npos) {
        std::string instance = result.substr(pos + 1);
        size_t endPos = instance.find("&");
        if (endPos != std::string::npos) {
            instance = instance.substr(0, endPos);
        }
        if (instance.length() > 8) {
            return instance.substr(0, 12);
        }
        return instance;
    }
    return "";
}

std::wstring ToWide(const std::string& s) {
    return std::wstring(s.begin(), s.end());
}

bool IsDriverConnected() {
    HANDLE hDriver = CreateFileA("\\\\.\\DualDesk", GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (hDriver != INVALID_HANDLE_VALUE) {
        CloseHandle(hDriver);
        return true;
    }
    return false;
}

// ===================== TREE VIEW FUNCTIONS =====================

void BuildImageList() {
    g_imageList = ImageList_Create(16, 16, ILC_COLOR32 | ILC_MASK, ICON_COUNT, 0);
    
    for (int i = 0; i < ICON_COUNT; i++) {
        HICON icon = LoadIcon(NULL, IDI_APPLICATION);
        if (icon) {
            ImageList_AddIcon(g_imageList, icon);
        } else {
            HBITMAP bmp = CreateBitmap(16, 16, 1, 1, NULL);
            ImageList_Add(g_imageList, bmp, (HBITMAP)NULL);
            DeleteObject(bmp);
        }
    }
    
    TreeView_SetImageList(g_treeView, g_imageList, TVSIL_NORMAL);
}

HTREEITEM AddNode(HTREEITEM parent, const std::wstring& text, int icon, bool expand) {
    TVINSERTSTRUCTW tvi = {0};
    tvi.hParent = parent;
    tvi.hInsertAfter = TVI_LAST;
    tvi.item.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE;
    tvi.item.pszText = const_cast<LPWSTR>(text.c_str());
    tvi.item.iImage = icon;
    tvi.item.iSelectedImage = icon;
    HTREEITEM item = TreeView_InsertItem(g_treeView, &tvi);
    if (expand && parent == TVI_ROOT) {
        TreeView_Expand(g_treeView, item, TVE_EXPAND);
    }
    return item;
}

void BuildTree(HWND parent) {
    HINSTANCE hInst = (HINSTANCE)GetWindowLongPtr(parent, GWLP_HINSTANCE);

    g_treeView = CreateWindowExW(WS_EX_CLIENTEDGE, WC_TREEVIEWW, L"",
        WS_CHILD | WS_VISIBLE | TVS_HASLINES | TVS_LINESATROOT | TVS_HASBUTTONS |
        TVS_SHOWSELALWAYS | TVS_FULLROWSELECT,
        0, 0, 100, 100, parent, (HMENU)200, hInst, NULL);
    SendMessage(g_treeView, WM_SETFONT, (WPARAM)g_font, TRUE);
    SendMessage(g_treeView, TVM_SETBKCOLOR, 0, (LPARAM)RGB(255, 255, 255));
    SendMessage(g_treeView, TVM_SETITEMHEIGHT, (WPARAM)22, 0);

    BuildImageList();

    g_nodeMonitors   = AddNode(TVI_ROOT, L"Monitors",   ICON_FOLDER, true);
    g_nodeWorkspaces = AddNode(TVI_ROOT, L"Workspaces", ICON_FOLDER, true);
    g_nodeWindows    = AddNode(TVI_ROOT, L"Windows",    ICON_FOLDER, true);
    g_nodeDevices    = AddNode(TVI_ROOT, L"Devices",    ICON_FOLDER, true);

    g_statusBar = CreateWindowExW(0, STATUSCLASSNAMEW, L"",
        WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
        0, 0, 100, 24, parent, (HMENU)201, hInst, NULL);
    SendMessage(g_statusBar, WM_SETFONT, (WPARAM)g_font, TRUE);
    
    int parts[3] = {180, 360, -1};
    SendMessage(g_statusBar, SB_SETPARTS, 3, (LPARAM)parts);

    RefreshTree();
}

void ClearChildren(HTREEITEM parent) {
    HTREEITEM child = TreeView_GetChild(g_treeView, parent);
    while (child) {
        HTREEITEM next = TreeView_GetNextSibling(g_treeView, child);
        TreeView_DeleteItem(g_treeView, child);
        child = next;
    }
}

std::wstring GetWindowClassName(HWND hwnd) {
    wchar_t className[256];
    if (GetClassNameW(hwnd, className, 256)) {
        return std::wstring(className);
    }
    return L"";
}

std::wstring GetWindowTitle(HWND hwnd) {
    wchar_t title[256];
    if (GetWindowTextW(hwnd, title, 256)) {
        return std::wstring(title);
    }
    return L"";
}

bool IsWindowTrackable(HWND hwnd) {
    if (!IsWindowVisible(hwnd)) return false;
    if (IsIconic(hwnd)) return false;
    
    wchar_t title[256];
    if (GetWindowTextW(hwnd, title, 256) == 0) return false;
    if (wcslen(title) == 0) return false;
    
    std::wstring cls = GetWindowClassName(hwnd);
    if (cls == L"Progman" || cls == L"WorkerW" || 
        cls == L"Shell_TrayWnd" || cls == L"Button") {
        return false;
    }
    
    return true;
}

void AssignDeviceToWorkspace(HANDLE deviceHandle, DWORD workspaceId) {
    if (g_driverInterface && g_driverInterface->IsConnected()) {
        if (g_driverInterface->AssignDeviceToWorkspace(deviceHandle, workspaceId)) {
            g_deviceWorkspaceMap[deviceHandle] = workspaceId;
            std::string msg = "Device assigned to workspace " + std::to_string(workspaceId);
            g_lastEvent = msg;
            LOG_INFO(msg.c_str());
            
            // FORCE REFRESH
            RefreshTree();
            UpdateStatusBar();
            InvalidateRect(g_treeView, NULL, TRUE);
        }
    }
}
bool CheckForChanges() {
    bool changed = false;
    
    int monitorCount = 0;
    if (g_displayManager) {
        auto monitors = g_displayManager->EnumerateDisplays();
        monitorCount = (int)monitors.size();
    }
    if (monitorCount != g_cachedMonitorCount) {
        g_cachedMonitorCount = monitorCount;
        changed = true;
    }
    
    int windowCount = 0;
    EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
        int* count = reinterpret_cast<int*>(lParam);
        if (IsWindowTrackable(hwnd)) {
            (*count)++;
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&windowCount));
    if (windowCount != g_cachedWindowCount) {
        g_cachedWindowCount = windowCount;
        changed = true;
    }
    
    int deviceCount = 0;
    if (g_inputManager) {
        deviceCount += (int)g_inputManager->GetKeyboards().size();
        deviceCount += (int)g_inputManager->GetMice().size();
    }
    if (deviceCount != g_cachedDeviceCount) {
        g_cachedDeviceCount = deviceCount;
        changed = true;
    }
    
    bool driverState = IsDriverConnected();
    if (driverState != g_cachedDriverState) {
        g_cachedDriverState = driverState;
        changed = true;
    }
    
    return changed;
}

void RefreshTree() {
    if (!g_treeView || g_refreshPaused) return;

    // --- Monitors ---
    ClearChildren(g_nodeMonitors);
    int monitorCount = 0;
    if (g_displayManager) {
        auto monitors = g_displayManager->EnumerateDisplays();
        monitorCount = (int)monitors.size();
        for (size_t i = 0; i < monitors.size(); ++i) {
            std::wstringstream label;
            label << L"Monitor " << (i + 1) << L"  \u2014  "
                  << monitors[i].Width() << L"x" << monitors[i].Height();
            if (monitors[i].isPrimary) label << L"  (Primary)";
            AddNode(g_nodeMonitors, label.str(), ICON_MONITOR, false);
        }
    }
    std::wstring monitorLabel = L"Monitors (" + std::to_wstring(monitorCount) + L")";
    TVITEMW item = {0};
    item.mask = TVIF_TEXT | TVIF_HANDLE;
    item.hItem = g_nodeMonitors;
    item.pszText = const_cast<LPWSTR>(monitorLabel.c_str());
    TreeView_SetItem(g_treeView, &item);

    // --- Workspaces ---
    ClearChildren(g_nodeWorkspaces);
    int workspaceCount = 0;
    if (g_workspaceManager) {
        auto workspaces = g_workspaceManager->GetAllWorkspaces();
        workspaceCount = (int)workspaces.size();
        for (auto* ws : workspaces) {
            std::wstringstream label;
            label << ToWide(ws->GetName()) << L"  \u2014  "
                  << ws->GetWindowCount() << L" window"
                  << (ws->GetWindowCount() == 1 ? L"" : L"s");
            AddNode(g_nodeWorkspaces, label.str(), ICON_WORKSPACE, false);
        }
    }
    std::wstring workspaceLabel = L"Workspaces (" + std::to_wstring(workspaceCount) + L")";
    item.hItem = g_nodeWorkspaces;
    item.pszText = const_cast<LPWSTR>(workspaceLabel.c_str());
    TreeView_SetItem(g_treeView, &item);

    
 // --- Workspaces ---
ClearChildren(g_nodeWorkspaces);
int workspaceCount = 0;
if (g_workspaceManager) {
    auto workspaces = g_workspaceManager->GetAllWorkspaces();
    workspaceCount = (int)workspaces.size();
    for (auto* ws : workspaces) {
        std::wstringstream label;
        label << ToWide(ws->GetName()) << L"  \u2014  "
              << ws->GetWindowCount() << L" window"
              << (ws->GetWindowCount() == 1 ? L"" : L"s");
        
        // Count devices assigned to this workspace
        int deviceCount = 0;
        for (const auto& [handle, wsId] : g_deviceWorkspaceMap) {
            if (wsId == ws->GetId()) deviceCount++;
        }
        if (deviceCount > 0) {
            label << L"  (" << deviceCount << L" devices)";
        }
        
        AddNode(g_nodeWorkspaces, label.str(), ICON_WORKSPACE, false);
    }
}
std::wstring workspaceLabel = L"Workspaces (" + std::to_wstring(workspaceCount) + L")";
item.hItem = g_nodeWorkspaces;
item.pszText = const_cast<LPWSTR>(workspaceLabel.c_str());
TreeView_SetItem(g_treeView, &item);
    // --- Devices ---
    ClearChildren(g_nodeDevices);
    int deviceCount = 0;
    
    if (g_inputManager) {
        auto keyboards = g_inputManager->GetKeyboards();
        int kbIndex = 1;
        for (const auto& kb : keyboards) {
            std::string cleanName = GetCleanDeviceName(kb.deviceName);
            std::string instanceId = GetDeviceInstanceId(kb.deviceName);
            
            std::wstring label;
            if (!instanceId.empty() && instanceId != "Keyboard") {
                label = ToWide(cleanName + " [" + instanceId + "]") + L"  (Keyboard " + std::to_wstring(kbIndex) + L")";
            } else {
                label = ToWide(cleanName) + L"  (Keyboard " + std::to_wstring(kbIndex) + L")";
            }
            
            auto it = g_deviceWorkspaceMap.find(kb.deviceHandle);
            if (it != g_deviceWorkspaceMap.end()) {
                label += L"  → Workspace " + std::to_wstring(it->second);
            }
            
            AddNode(g_nodeDevices, label, ICON_KEYBOARD, false);
            deviceCount++;
            kbIndex++;
        }
        
        auto mice = g_inputManager->GetMice();
        int mouseIndex = 1;
        for (const auto& mouse : mice) {
            std::string cleanName = GetCleanDeviceName(mouse.deviceName);
            std::string instanceId = GetDeviceInstanceId(mouse.deviceName);
            
            std::string lowerName = cleanName;
            for (auto& c : lowerName) c = (char)tolower((unsigned char)c);
            bool isTouchpad = (lowerName.find("touchpad") != std::string::npos) ||
                              (lowerName.find("trackpad") != std::string::npos);
            int icon = isTouchpad ? ICON_TOUCHPAD : ICON_MOUSE;
            
            std::wstring label;
            if (!instanceId.empty() && instanceId != "Mouse") {
                label = ToWide(cleanName + " [" + instanceId + "]") + L"  (" + (isTouchpad ? L"Touchpad" : L"Mouse") + L" " + std::to_wstring(mouseIndex) + L")";
            } else {
                label = ToWide(cleanName) + L"  (" + (isTouchpad ? L"Touchpad" : L"Mouse") + L" " + std::to_wstring(mouseIndex) + L")";
            }
            
            auto it = g_deviceWorkspaceMap.find(mouse.deviceHandle);
            if (it != g_deviceWorkspaceMap.end()) {
                label += L"  → Workspace " + std::to_wstring(it->second);
            }
            
            AddNode(g_nodeDevices, label, icon, false);
            deviceCount++;
            mouseIndex++;
        }
    }
    
    g_driverConnected = IsDriverConnected();
    
    AddNode(g_nodeDevices, g_driverConnected ? L"DualDesk Driver  \u2014  Connected" : L"DualDesk Driver  \u2014  Not connected",
            g_driverConnected ? ICON_OK : ICON_WARN, false);

    std::wstring deviceLabel = L"Devices (" + std::to_wstring(deviceCount) + L")";
    item.hItem = g_nodeDevices;
    item.pszText = const_cast<LPWSTR>(deviceLabel.c_str());
    TreeView_SetItem(g_treeView, &item);

    TreeView_Expand(g_treeView, g_nodeMonitors, TVE_EXPAND);
    TreeView_Expand(g_treeView, g_nodeWorkspaces, TVE_EXPAND);
    TreeView_Expand(g_treeView, g_nodeWindows, TVE_EXPAND);
    TreeView_Expand(g_treeView, g_nodeDevices, TVE_EXPAND);
    
    UpdateStatusBar();
    InvalidateRect(g_treeView, NULL, TRUE);
}

void UpdateStatusBar() {
    if (!g_statusBar) return;
    std::wstring driverText = g_driverConnected ? L"Driver: Connected" : L"Driver: Not connected";
    SendMessageW(g_statusBar, SB_SETTEXTW, 0, (LPARAM)driverText.c_str());
    std::wstring eventText = L"Last event: " + ToWide(g_lastEvent);
    SendMessageW(g_statusBar, SB_SETTEXTW, 1, (LPARAM)eventText.c_str());
    SendMessageW(g_statusBar, SB_SETTEXTW, 2, (LPARAM)L"Live");
}

void LayoutControls(HWND hwnd) {
    RECT rc;
    GetClientRect(hwnd, &rc);

    int statusH = 24;
    SetWindowPos(g_statusBar, NULL, 0, rc.bottom - statusH, rc.right, statusH, SWP_NOZORDER);

    int treeY = 6;
    int treeH = (rc.bottom - statusH) - treeY - 6;
    int treeW = rc.right - 12;
    SetWindowPos(g_treeView, NULL, 6, treeY, treeW, treeH, SWP_NOZORDER);
}

void ShowDeviceContextMenu(HWND hwnd, POINT pt) {
    HTREEITEM selected = TreeView_GetSelection(g_treeView);
    if (!selected) return;
    
    HTREEITEM parent = TreeView_GetParent(g_treeView, selected);
    if (parent != g_nodeDevices) return;
    
    wchar_t text[256];
    TVITEMW item = {0};
    item.hItem = selected;
    item.mask = TVIF_TEXT | TVIF_PARAM;
    item.pszText = text;
    item.cchTextMax = 256;
    TreeView_GetItem(g_treeView, &item);
    
    std::wstring itemText = text;
    
    bool isInputDevice = (itemText.find(L"Keyboard") != std::string::npos ||
                          itemText.find(L"Mouse") != std::string::npos ||
                          itemText.find(L"Touchpad") != std::string::npos);
    
    bool isDriverStatus = (itemText.find(L"Driver") != std::string::npos);
    
    if (!isInputDevice || isDriverStatus) {
        return;
    }
    
    HANDLE deviceHandle = (HANDLE)item.lParam;
    
    auto workspaces = g_workspaceManager->GetAllWorkspaces();
    if (workspaces.empty()) {
        MessageBoxA(hwnd, "No workspaces available!", "Info", MB_OK);
        return;
    }
    
    HMENU hMenu = CreatePopupMenu();
    
    int menuId = 1;
    for (auto* ws : workspaces) {
        std::wstring menuText = L"Assign to " + ToWide(ws->GetName());
        AppendMenuW(hMenu, MF_STRING, menuId, menuText.c_str());
        menuId++;
    }
    
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenu, MF_STRING, 999, L"Remove Assignment");
    
    int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD, pt.x, pt.y, 0, hwnd, NULL);
    DestroyMenu(hMenu);
    
    if (cmd == 999) {
        g_deviceWorkspaceMap.erase(deviceHandle);
        g_lastEvent = "Device unassigned";
        RefreshTree();
        UpdateStatusBar();
        return;
    }
    
    if (cmd >= 1 && cmd <= (int)workspaces.size()) {
        int workspaceIndex = cmd - 1;
        auto* ws = workspaces[workspaceIndex];
        if (g_driverInterface && g_driverInterface->IsConnected()) {
            if (g_driverInterface->AssignDeviceToWorkspace(deviceHandle, ws->GetId())) {
                g_deviceWorkspaceMap[deviceHandle] = ws->GetId();
                std::string msg = "Device assigned to " + ws->GetName();
                g_lastEvent = msg;
                RefreshTree();
                UpdateStatusBar();
            }
        }
    }
}

LRESULT CALLBACK StatusWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch(msg) {
        case WM_CREATE: {
            g_font = CreateFontW(15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                 DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                 DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
            g_fontBold = CreateFontW(15, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                 DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                 DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

            BuildTree(hwnd);
            LayoutControls(hwnd);

            SetTimer(hwnd, 1, 500, NULL);
            return 0;
        }
        case WM_TIMER:
            if (CheckForChanges()) {
                RefreshTree();
                UpdateStatusBar();
            }
            return 0;
        case WM_SIZE:
            LayoutControls(hwnd);
            return 0;
        case WM_DEVICECHANGE:
            RefreshTree();
            UpdateStatusBar();
            return 0;
        case WM_CONTEXTMENU: {
            POINT pt;
            pt.x = GET_X_LPARAM(lParam);
            pt.y = GET_Y_LPARAM(lParam);
            g_refreshPaused = true;
            ShowDeviceContextMenu(hwnd, pt);
            g_refreshPaused = false;
            return 0;
        }
        case WM_DESTROY:
            KillTimer(hwnd, 1);
            if (g_font) DeleteObject(g_font);
            if (g_fontBold) DeleteObject(g_fontBold);
            if (g_imageList) ImageList_Destroy(g_imageList);
            PostQuitMessage(0);
            return 0;
        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) {
                g_running = false;
                DestroyWindow(hwnd);
                return 0;
            }
            if (wParam == 'R') {
                g_lastEvent = "Manual refresh";
                RefreshTree();
                UpdateStatusBar();
                return 0;
            }
            break;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK InputWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch(msg) {
        case WM_INPUT: {
            if (g_inputManager) {
                HRAWINPUT handle = reinterpret_cast<HRAWINPUT>(lParam);
                g_inputManager->ProcessRawInput(handle);
            }
            return 0;
        }
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        case WM_DEVICECHANGE:
            RefreshTree();
            UpdateStatusBar();
            return 0;
        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) {
                g_running = false;
                PostQuitMessage(0);
                return 0;
            }
            if ((GetAsyncKeyState(VK_LWIN) & 0x8000) || (GetAsyncKeyState(VK_RWIN) & 0x8000)) {
                if (GetAsyncKeyState(VK_SHIFT) & 0x8000) {
                    if (wParam == VK_RIGHT) {
                        HWND activeWindow = GetForegroundWindow();
                        if (activeWindow && g_windowMover) {
                            g_windowMover->MoveWindowToNextMonitor(activeWindow);
                            g_lastEvent = "Window moved right";
                            RefreshTree();
                            UpdateStatusBar();
                        }
                        return 0;
                    }
                    if (wParam == VK_LEFT) {
                        HWND activeWindow = GetForegroundWindow();
                        if (activeWindow && g_windowMover) {
                            g_windowMover->MoveWindowToPreviousMonitor(activeWindow);
                            g_lastEvent = "Window moved left";
                            RefreshTree();
                            UpdateStatusBar();
                        }
                        return 0;
                    }
                }
            }
            break;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// ===================== MAIN ENTRY POINT =====================

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);
    UNREFERENCED_PARAMETER(nCmdShow);

    try {
        LOG_INFO("DualDesk starting...");

        INITCOMMONCONTROLSEX icex = {0};
        icex.dwSize = sizeof(icex);
        icex.dwICC = ICC_TREEVIEW_CLASSES | ICC_STANDARD_CLASSES | ICC_BAR_CLASSES;
        InitCommonControlsEx(&icex);

        WNDCLASSEXW wcInput = {0};
        wcInput.cbSize = sizeof(WNDCLASSEXW);
        wcInput.lpfnWndProc = InputWindowProc;
        wcInput.hInstance = hInstance;
        wcInput.lpszClassName = L"DualDeskInputWindow";
        wcInput.hCursor = LoadCursor(NULL, IDC_ARROW);

        if (!RegisterClassExW(&wcInput)) {
            LOG_ERROR("Failed to register input window class");
            MessageBoxA(NULL, "Failed to register input window class", "Error", MB_OK);
            return 1;
        }

        HWND hwndInput = CreateWindowExW(0, L"DualDeskInputWindow", L"DualDesk Input", 
                                         WS_OVERLAPPEDWINDOW, 0, 0, 100, 100, 
                                         NULL, NULL, hInstance, NULL);
        if (!hwndInput) {
            LOG_ERROR("Failed to create input window");
            MessageBoxA(NULL, "Failed to create input window", "Error", MB_OK);
            return 1;
        }
        ShowWindow(hwndInput, SW_HIDE);

        InputManager inputManager;
        DisplayManager displayManager;
        WorkspaceManager workspaceManager;
        DriverInterface driverInterface;

        g_inputManager = &inputManager;
        g_displayManager = &displayManager;
        g_workspaceManager = &workspaceManager;
        g_driverInterface = &driverInterface;

        inputManager.Initialize(hwndInput);
        displayManager.EnumerateDisplays();
        workspaceManager.Initialize(&displayManager, &inputManager);

        auto workspaces = workspaceManager.GetAllWorkspaces();
        std::string logMsg = "Number of workspaces: " + std::to_string(workspaces.size());
        LOG_INFO(logMsg.c_str());
        
        if (driverInterface.Open()) {
            LOG_INFO("Driver connected");
            driverInterface.SetRouteMode(1);
            
        } else {
            LOG_WARN("Driver not available");
        }

        g_cachedMonitorCount = (int)displayManager.EnumerateDisplays().size();
        g_cachedWindowCount = 0;
        EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
            int* count = reinterpret_cast<int*>(lParam);
            if (IsWindowTrackable(hwnd)) {
                (*count)++;
            }
            return TRUE;
        }, reinterpret_cast<LPARAM>(&g_cachedWindowCount));
        g_cachedDeviceCount = (int)inputManager.GetKeyboards().size() + (int)inputManager.GetMice().size();
        g_cachedDriverState = IsDriverConnected();

        inputManager.SetDeviceChangeCallback([](const InputDevice& device, bool added) {
            std::string name = GetCleanDeviceName(device.deviceName);
            std::string typeStr;
            switch(device.type) {
                case InputDeviceType::Keyboard: typeStr = "Keyboard"; break;
                case InputDeviceType::Mouse: typeStr = "Mouse"; break;
                default: typeStr = "Unknown"; break;
            }
            if (added) {
                g_lastEvent = "Plugged in: " + name + " (" + typeStr + ")";
                if (g_driverInterface && g_driverInterface->IsConnected()) {
                    g_driverInterface->AssignDeviceToWorkspace(device.deviceHandle, 0);
                    g_deviceWorkspaceMap[device.deviceHandle] = 0;
                }
            } else {
                g_lastEvent = "Unplugged: " + name + " (" + typeStr + ")";
                g_deviceWorkspaceMap.erase(device.deviceHandle);
            }
            RefreshTree();
            UpdateStatusBar();
        });

        inputManager.SetInputEventCallback([](const InputEvent& event) {
            if (event.type == InputEventType::KeyDown ||
                event.type == InputEventType::KeyUp ||
                event.type == InputEventType::MouseButtonDown ||
                event.type == InputEventType::MouseButtonUp ||
                event.type == InputEventType::MouseWheel) {
                g_lastEvent = event.ToString();
                UpdateStatusBar();
            }
        });

        WindowMover windowMover;
        windowMover.Initialize(&workspaceManager);
        g_windowMover = &windowMover;

        windowMover.SetMoveCallback([](HWND hwnd, HMONITOR from, HMONITOR to) {
            g_lastEvent = "Window moved between monitors";
            RefreshTree();
            UpdateStatusBar();
        });

        WNDCLASSEXW wcStatus = {0};
        wcStatus.cbSize = sizeof(WNDCLASSEXW);
        wcStatus.lpfnWndProc = StatusWindowProc;
        wcStatus.hInstance = hInstance;
        wcStatus.lpszClassName = L"DualDeskStatusWindow";
        wcStatus.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wcStatus.hCursor = LoadCursor(NULL, IDC_ARROW);

        if (!RegisterClassExW(&wcStatus)) {
            LOG_ERROR("Failed to register status window class");
            MessageBoxA(NULL, "Failed to register status window class", "Error", MB_OK);
            return 1;
        }

        int windowWidth = 700;
        int windowHeight = 550;
        int screenWidth = GetSystemMetrics(SM_CXSCREEN);
        int screenHeight = GetSystemMetrics(SM_CYSCREEN);
        int x = (screenWidth - windowWidth) / 2;
        int y = (screenHeight - windowHeight) / 2;

        g_statusWindow = CreateWindowExW(
            WS_EX_APPWINDOW,
            L"DualDeskStatusWindow",
            L"DualDesk Status Monitor",
            WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_CLIPCHILDREN | WS_SIZEBOX | WS_MINIMIZEBOX | WS_MAXIMIZEBOX,
            x, y, windowWidth, windowHeight,
            NULL, NULL, hInstance, NULL
        );

        if (!g_statusWindow) {
            LOG_ERROR("Failed to create status window");
            MessageBoxA(NULL, "Failed to create status window", "Error", MB_OK);
            return 1;
        }

        ShowWindow(g_statusWindow, SW_SHOW);
        UpdateWindow(g_statusWindow);
        RefreshTree();
        UpdateStatusBar();

        g_lastEvent = "DualDesk started";
        UpdateStatusBar();

        LOG_INFO("DualDesk running...");

        MSG msg;
        DWORD lastCheckTime = GetTickCount();

        while (g_running && GetMessage(&msg, NULL, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            
            DWORD now = GetTickCount();
            if (now - lastCheckTime >= 500) {
                lastCheckTime = now;
                inputManager.CheckForDeviceChanges();
            }
        }

        if (g_statusWindow) {
            DestroyWindow(g_statusWindow);
            g_statusWindow = nullptr;
        }

        LOG_INFO("DualDesk exiting...");
        return 0;
    }
    catch (const std::exception& e) {
        std::string errMsg = "Unhandled exception: ";
        errMsg += e.what();
        LOG_ERROR(errMsg.c_str());
        std::string errMsg2 = "Fatal error: ";
        errMsg2 += e.what();
        MessageBoxA(NULL, 
                    errMsg2.c_str(),
                    "DualDesk Fatal Error",
                    MB_OK | MB_ICONERROR);
        return 1;
    }
    catch (...) {
        LOG_ERROR("Unknown exception caught!");
        MessageBoxA(NULL,
                    "Unknown fatal error occurred!",
                    "DualDesk Fatal Error",
                    MB_OK | MB_ICONERROR);
        return 1;
    }
}