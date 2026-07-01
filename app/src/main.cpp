#include "dualdesk/core/logger.h"
#include "dualdesk/display/display_manager.h"
#include "dualdesk/workspace/window_tracker.h"
#include "dualdesk/workspace/workspace_manager.h"
#include "dualdesk/workspace/window_mover.h"
#include "dualdesk/input/input_manager.h"
#include "dualdesk/input/input_router.h"
#include <windows.h>
#include <commctrl.h>
#include <string>
#include <sstream>
#include <dbt.h>
#include <vector>
#include <set>
#include <map>
#include <cctype>

#pragma comment(lib, "comctl32.lib")

using namespace dualdesk;

// Global pointers
InputManager* g_inputManager = nullptr;
WindowMover* g_windowMover = nullptr;
DisplayManager* g_displayManager = nullptr;
WorkspaceManager* g_workspaceManager = nullptr;
HWND g_statusWindow = nullptr;
HWND g_treeView = nullptr;
HWND g_statusBar = nullptr;
HFONT g_font = nullptr;
HFONT g_fontBold = nullptr;
HIMAGELIST g_imageList = nullptr;
bool g_running = true;
std::string g_lastEvent = "None";
bool g_driverConnected = false;

// Root category nodes
HTREEITEM g_nodeMonitors = nullptr;
HTREEITEM g_nodeWorkspaces = nullptr;
HTREEITEM g_nodeDevices = nullptr;
HTREEITEM g_nodeWindows = nullptr;

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

// Helper functions
std::string GetCleanDeviceName(const std::wstring& deviceName) {
    std::string name;
    for (wchar_t c : deviceName) {
        if (c < 128) name += (char)c;
    }

    size_t pos = name.find_last_of("\\#");
    if (pos != std::string::npos) name = name.substr(pos + 1);

    std::vector<std::string> suffixes = {"&Col01", "&Col02", "&Col03", "&Col04", "&MI_00", "&MI_01"};
    for (const auto& suffix : suffixes) {
        size_t found = name.find(suffix);
        if (found != std::string::npos) name = name.substr(0, found);
    }

    while (!name.empty() && (name.back() == '&' || name.back() == '#' || name.back() == '_')) {
        name.pop_back();
    }

    if (name.empty()) return "Unknown Device";
    if (name.length() > 45) name = name.substr(0, 42) + "...";
    return name;
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

// Tree view functions
void BuildImageList() {
    g_imageList = ImageList_Create(16, 16, ILC_COLOR32 | ILC_MASK, ICON_COUNT, 0);
    
    for (int i = 0; i < ICON_COUNT; i++) {
        HICON icon = CreateIcon(GetModuleHandle(NULL), 16, 16, 1, 1, NULL, NULL);
        ImageList_AddIcon(g_imageList, icon);
        DestroyIcon(icon);
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

void RefreshTree() {
    if (!g_treeView) return;

    // --- Monitors ---
    ClearChildren(g_nodeMonitors);
    if (g_displayManager) {
        auto monitors = g_displayManager->EnumerateDisplays();
        for (size_t i = 0; i < monitors.size(); ++i) {
            std::wstringstream label;
            label << L"Monitor " << (i + 1) << L"  \u2014  "
                  << monitors[i].Width() << L"x" << monitors[i].Height();
            if (monitors[i].isPrimary) label << L"  (Primary)";
            AddNode(g_nodeMonitors, label.str(), ICON_MONITOR, false);
        }
        std::wstring rootLabel = L"Monitors (" + std::to_wstring(monitors.size()) + L")";
        TVITEMW item = {0};
        item.mask = TVIF_TEXT | TVIF_HANDLE;
        item.hItem = g_nodeMonitors;
        item.pszText = const_cast<LPWSTR>(rootLabel.c_str());
        TreeView_SetItem(g_treeView, &item);
    }

    // --- Workspaces ---
    ClearChildren(g_nodeWorkspaces);
    if (g_workspaceManager) {
        auto workspaces = g_workspaceManager->GetAllWorkspaces();
        for (auto* ws : workspaces) {
            std::wstringstream label;
            label << ToWide(ws->GetName()) << L"  \u2014  "
                  << ws->GetWindowCount() << L" window"
                  << (ws->GetWindowCount() == 1 ? L"" : L"s");
            AddNode(g_nodeWorkspaces, label.str(), ICON_WORKSPACE, false);
        }
        std::wstring rootLabel = L"Workspaces (" + std::to_wstring(workspaces.size()) + L")";
        TVITEMW item = {0};
        item.mask = TVIF_TEXT | TVIF_HANDLE;
        item.hItem = g_nodeWorkspaces;
        item.pszText = const_cast<LPWSTR>(rootLabel.c_str());
        TreeView_SetItem(g_treeView, &item);
    }

    // --- Windows ---
    ClearChildren(g_nodeWindows);
    int windowCount = 0;
    
    EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
        int* count = reinterpret_cast<int*>(lParam);
        
        if (!IsWindowTrackable(hwnd)) return TRUE;
        
        std::wstring title = GetWindowTitle(hwnd);
        std::wstring cls = GetWindowClassName(hwnd);
        
        if (!title.empty()) {
            std::wstringstream label;
            label << title;
            
            if (!cls.empty() && cls != L"") {
                label << L"  [" << cls << L"]";
            }
            
            HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
            MONITORINFO mi = {sizeof(MONITORINFO)};
            if (GetMonitorInfo(monitor, &mi)) {
                int monitorIndex = 1;
                if (g_displayManager) {
                    auto monitors = g_displayManager->EnumerateDisplays();
                    for (size_t i = 0; i < monitors.size(); ++i) {
                        if (monitors[i].monitor == monitor) {
                            monitorIndex = (int)i + 1;
                            break;
                        }
                    }
                }
                label << L"  (Monitor " << monitorIndex << L")";
            }
            
            AddNode(g_nodeWindows, label.str(), ICON_WINDOW, false);
            (*count)++;
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&windowCount));
    
    std::wstring rootLabel = L"Windows (" + std::to_wstring(windowCount) + L")";
    TVITEMW item = {0};
    item.mask = TVIF_TEXT | TVIF_HANDLE;
    item.hItem = g_nodeWindows;
    item.pszText = const_cast<LPWSTR>(rootLabel.c_str());
    TreeView_SetItem(g_treeView, &item);

    // --- Devices ---
    ClearChildren(g_nodeDevices);
    int deviceCount = 0;
    if (g_inputManager) {
        auto keyboards = g_inputManager->GetKeyboards();
        for (const auto& kb : keyboards) {
            AddNode(g_nodeDevices, ToWide(GetCleanDeviceName(kb.deviceName)) + L"  (Keyboard)", ICON_KEYBOARD, false);
            deviceCount++;
        }
        auto mice = g_inputManager->GetMice();
        for (const auto& mouse : mice) {
            std::string cleanName = GetCleanDeviceName(mouse.deviceName);
            std::string lowerName = cleanName;
            for (auto& c : lowerName) c = (char)tolower((unsigned char)c);
            bool isTouchpad = (lowerName.find("touchpad") != std::string::npos) ||
                               (lowerName.find("trackpad") != std::string::npos);
            int icon = isTouchpad ? ICON_TOUCHPAD : ICON_MOUSE;
            const wchar_t* suffix = isTouchpad ? L"  (Touchpad)" : L"  (Mouse)";
            AddNode(g_nodeDevices, ToWide(cleanName) + suffix, icon, false);
            deviceCount++;
        }
    }
    
    g_driverConnected = IsDriverConnected();
    
    AddNode(g_nodeDevices, g_driverConnected ? L"DualDesk Driver  \u2014  Connected" : L"DualDesk Driver  \u2014  Not connected",
            g_driverConnected ? ICON_OK : ICON_WARN, false);

    std::wstring rootLabelDevices = L"Devices (" + std::to_wstring(deviceCount) + L")";
    TVITEMW itemDevices = {0};
    itemDevices.mask = TVIF_TEXT | TVIF_HANDLE;
    itemDevices.hItem = g_nodeDevices;
    itemDevices.pszText = const_cast<LPWSTR>(rootLabelDevices.c_str());
    TreeView_SetItem(g_treeView, &itemDevices);

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
            RefreshTree();
            UpdateStatusBar();
            return 0;
        case WM_SIZE:
            LayoutControls(hwnd);
            return 0;
        case WM_DEVICECHANGE:
            RefreshTree();
            UpdateStatusBar();
            return 0;
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
            if (wParam == 'D') {
                g_lastEvent = "Status refreshed";
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
        icex.dwICC = ICC_TREEVIEW_CLASSES | ICC_STANDARD_CLASSES;
        InitCommonControlsEx(&icex);

        // Register input window class
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

        // Initialize managers
        InputManager inputManager;
        DisplayManager displayManager;
        WorkspaceManager workspaceManager;

        g_inputManager = &inputManager;
        g_displayManager = &displayManager;
        g_workspaceManager = &workspaceManager;

        inputManager.Initialize(hwndInput);
        displayManager.EnumerateDisplays();
        workspaceManager.Initialize(&displayManager, &inputManager);

        // Set up callbacks
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
            } else {
                g_lastEvent = "Unplugged: " + name + " (" + typeStr + ")";
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

        // Initialize WindowMover
        WindowMover windowMover;
        windowMover.Initialize(&workspaceManager);
        g_windowMover = &windowMover;

        windowMover.SetMoveCallback([](HWND hwnd, HMONITOR from, HMONITOR to) {
            g_lastEvent = "Window moved between monitors";
            RefreshTree();
            UpdateStatusBar();
        });

        // Register status window class
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

        // Create status window
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

        // Main message loop
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
        // Fixed: Use string concatenation instead of std::format
        LOG_CRITICAL("Unhandled exception: ");
        LOG_CRITICAL(e.what());
        std::string msg = "Fatal error: ";
        msg += e.what();
        MessageBoxA(NULL, 
                    msg.c_str(),
                    "DualDesk Fatal Error",
                    MB_OK | MB_ICONERROR);
        return 1;
    }
    catch (...) {
        LOG_CRITICAL("Unknown exception caught!");
        MessageBoxA(NULL,
                    "Unknown fatal error occurred!",
                    "DualDesk Fatal Error",
                    MB_OK | MB_ICONERROR);
        return 1;
    }
}