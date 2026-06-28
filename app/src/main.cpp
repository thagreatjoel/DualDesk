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
#include <algorithm>
#include <chrono>
#include <iomanip>

#pragma comment(lib, "comctl32.lib")

// Forward declarations
LRESULT CALLBACK MainWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK InputWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK DashboardProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK InfoPanelProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
void UpdateStatusBar();
void RefreshTree();
void ToggleTreeView();
void LayoutControls(HWND hwnd);
void BuildTree();
void CreateDashboard(HWND parent);
void CreateInfoPanel(HWND parent);

// Global instances
dualdesk::InputManager* g_inputManager = nullptr;
dualdesk::WindowMover* g_windowMover = nullptr;
dualdesk::DisplayManager* g_displayManager = nullptr;
dualdesk::WorkspaceManager* g_workspaceManager = nullptr;

// Main window controls
HWND g_mainWindow = nullptr;
HWND g_statusBar = nullptr;
HWND g_treeView = nullptr;
HWND g_dashboardWindow = nullptr;
HWND g_infoPanel = nullptr;
HWND g_toggleButton = nullptr;

// Fonts
HFONT g_font = nullptr;
HFONT g_fontBold = nullptr;
HFONT g_fontLarge = nullptr;
HFONT g_fontSmall = nullptr;
HIMAGELIST g_imageList = nullptr;

// State
bool g_running = true;
std::string g_lastEvent = "None";
bool g_driverConnected = false;
bool g_treeVisible = true;

// Root nodes
HTREEITEM g_nodeMonitors = nullptr;
HTREEITEM g_nodeWorkspaces = nullptr;
HTREEITEM g_nodeDevices = nullptr;

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
    ICON_DASHBOARD,
    ICON_SETTINGS,
    ICON_INFO,
    ICON_COUNT
};

// =====================================================================
// Tree Sync System
// =====================================================================

template <typename Key>
struct TreeSync {
    std::map<Key, HTREEITEM> liveNodes;

    void Sync(HTREEITEM parent,
              const std::vector<std::tuple<Key, std::wstring, int>>& entries) {
        std::set<Key> seen;
        HTREEITEM insertAfter = TVI_FIRST;

        for (const auto& entry : entries) {
            const Key& key = std::get<0>(entry);
            const std::wstring& label = std::get<1>(entry);
            int icon = std::get<2>(entry);
            seen.insert(key);

            auto it = liveNodes.find(key);
            if (it == liveNodes.end()) {
                TVINSERTSTRUCTW tvi = {0};
                tvi.hParent = parent;
                tvi.hInsertAfter = insertAfter;
                tvi.item.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE;
                tvi.item.pszText = const_cast<LPWSTR>(label.c_str());
                tvi.item.iImage = icon;
                tvi.item.iSelectedImage = icon;
                HTREEITEM newItem = TreeView_InsertItem(g_treeView, &tvi);
                liveNodes[key] = newItem;
                insertAfter = newItem;
            } else {
                HTREEITEM existing = it->second;
                wchar_t currentText[512];
                TVITEMW getItem = {0};
                getItem.mask = TVIF_TEXT;
                getItem.hItem = existing;
                getItem.pszText = currentText;
                getItem.cchTextMax = 512;
                TreeView_GetItem(g_treeView, &getItem);

                if (label != currentText) {
                    TVITEMW setItem = {0};
                    setItem.mask = TVIF_TEXT;
                    setItem.hItem = existing;
                    setItem.pszText = const_cast<LPWSTR>(label.c_str());
                    TreeView_SetItem(g_treeView, &setItem);
                }
                insertAfter = existing;
            }
        }

        for (auto it = liveNodes.begin(); it != liveNodes.end(); ) {
            if (seen.find(it->first) == seen.end()) {
                TreeView_DeleteItem(g_treeView, it->second);
                it = liveNodes.erase(it);
            } else {
                ++it;
            }
        }
    }

    void Clear() {
        for (auto& kv : liveNodes) {
            TreeView_DeleteItem(g_treeView, kv.second);
        }
        liveNodes.clear();
    }
};

std::map<void*, TreeSync<HWND>> g_workspaceWindowSync;
TreeSync<HMONITOR> g_monitorSync;
TreeSync<void*> g_workspaceSync;
TreeSync<std::wstring> g_deviceSync;

// =====================================================================
// Icon Generation
// =====================================================================

template <typename DrawFn>
HICON MakeGlyphIcon(DrawFn draw, int size = 16) {
    HDC screenDC = GetDC(NULL);
    HDC memDC = CreateCompatibleDC(screenDC);
    HBITMAP color_bmp = CreateCompatibleBitmap(screenDC, size, size);
    HBITMAP mask_bmp = CreateBitmap(size, size, 1, 1, NULL);

    HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, color_bmp);
    RECT full = {0, 0, size, size};
    HBRUSH bg = CreateSolidBrush(RGB(255, 255, 255));
    FillRect(memDC, &full, bg);
    DeleteObject(bg);

    draw(memDC, size);

    HDC maskDC = CreateCompatibleDC(screenDC);
    HBITMAP oldMaskBmp = (HBITMAP)SelectObject(maskDC, mask_bmp);
    BitBlt(maskDC, 0, 0, size, size, memDC, 0, 0, SRCCOPY);
    SelectObject(maskDC, oldMaskBmp);
    DeleteDC(maskDC);

    SelectObject(memDC, oldBmp);
    DeleteDC(memDC);
    ReleaseDC(NULL, screenDC);

    ICONINFO ii = {0};
    ii.fIcon = TRUE;
    ii.hbmColor = color_bmp;
    ii.hbmMask = mask_bmp;
    HICON icon = CreateIconIndirect(&ii);
    DeleteObject(color_bmp);
    DeleteObject(mask_bmp);
    return icon;
}

HICON MakeDashboardIcon() {
    return MakeGlyphIcon([](HDC memDC, int size) {
        HBRUSH brush = CreateSolidBrush(RGB(52, 152, 219));
        HPEN pen = CreatePen(PS_SOLID, 1, RGB(41, 128, 185));
        SelectObject(memDC, brush);
        SelectObject(memDC, pen);
        
        Rectangle(memDC, 1, 1, 7, 7);
        Rectangle(memDC, 9, 1, 15, 7);
        Rectangle(memDC, 1, 9, 7, 15);
        Rectangle(memDC, 9, 9, 15, 15);
        
        DeleteObject(brush);
        DeleteObject(pen);
    }, 16);
}

HICON MakeMonitorIcon(COLORREF color) {
    return MakeGlyphIcon([color](HDC memDC, int size) {
        HBRUSH brush = CreateSolidBrush(color);
        HBRUSH darkBrush = CreateSolidBrush(RGB(60, 60, 60));
        HPEN pen = CreatePen(PS_SOLID, 1, RGB(50, 50, 50));
        SelectObject(memDC, brush);
        SelectObject(memDC, pen);

        RoundRect(memDC, 1, 1, 15, 11, 2, 2);
        SelectObject(memDC, darkBrush);
        Rectangle(memDC, 7, 11, 9, 13);
        Rectangle(memDC, 4, 13, 12, 15);

        DeleteObject(brush);
        DeleteObject(darkBrush);
        DeleteObject(pen);
    }, 16);
}

HICON MakeWorkspaceIcon() {
    return MakeGlyphIcon([](HDC memDC, int size) {
        HBRUSH brush = CreateSolidBrush(RGB(155, 89, 182));
        HPEN pen = CreatePen(PS_SOLID, 1, RGB(142, 68, 173));
        SelectObject(memDC, brush);
        SelectObject(memDC, pen);
        
        Rectangle(memDC, 2, 2, 14, 6);
        Rectangle(memDC, 2, 8, 14, 12);
        Rectangle(memDC, 2, 8, 6, 12);
        
        DeleteObject(brush);
        DeleteObject(pen);
    }, 16);
}

HICON MakeKeyboardIcon(COLORREF color) {
    return MakeGlyphIcon([color](HDC memDC, int size) {
        HBRUSH brush = CreateSolidBrush(color);
        HBRUSH keyBrush = CreateSolidBrush(RGB(240, 240, 240));
        HPEN pen = CreatePen(PS_SOLID, 1, RGB(100, 100, 100));
        SelectObject(memDC, brush);
        SelectObject(memDC, pen);

        RoundRect(memDC, 1, 3, 15, 13, 2, 2);
        SelectObject(memDC, keyBrush);
        
        HPEN keyPen = CreatePen(PS_SOLID, 1, RGB(180, 180, 180));
        SelectObject(memDC, keyPen);
        for (int row = 0; row < 2; ++row) {
            for (int col = 0; col < 5; ++col) {
                int x = 2 + col * 3;
                int y = 5 + row * 3;
                Rectangle(memDC, x, y, x + 2, y + 2);
            }
        }
        Rectangle(memDC, 3, 11, 13, 12);
        DeleteObject(keyPen);

        DeleteObject(brush);
        DeleteObject(keyBrush);
        DeleteObject(pen);
    }, 16);
}

HICON MakeMouseIcon(COLORREF color) {
    return MakeGlyphIcon([color](HDC memDC, int size) {
        HBRUSH brush = CreateSolidBrush(color);
        HPEN pen = CreatePen(PS_SOLID, 1, RGB(100, 100, 100));
        SelectObject(memDC, brush);
        SelectObject(memDC, pen);

        RoundRect(memDC, 5, 1, 11, 15, 4, 6);
        MoveToEx(memDC, 8, 1, NULL);
        LineTo(memDC, 8, 6);

        HBRUSH wheelBrush = CreateSolidBrush(RGB(80, 80, 80));
        SelectObject(memDC, wheelBrush);
        Rectangle(memDC, 7, 3, 9, 6);
        DeleteObject(wheelBrush);

        DeleteObject(brush);
        DeleteObject(pen);
    }, 16);
}

HICON MakeTouchpadIcon(COLORREF color) {
    return MakeGlyphIcon([color](HDC memDC, int size) {
        HBRUSH brush = CreateSolidBrush(color);
        HPEN pen = CreatePen(PS_SOLID, 1, RGB(100, 100, 100));
        SelectObject(memDC, brush);
        SelectObject(memDC, pen);

        RoundRect(memDC, 1, 2, 15, 13, 2, 2);

        HBRUSH barBrush = CreateSolidBrush(RGB(80, 80, 80));
        SelectObject(memDC, barBrush);
        Rectangle(memDC, 1, 10, 15, 13);
        DeleteObject(barBrush);

        SelectObject(memDC, pen);
        MoveToEx(memDC, 8, 10, NULL);
        LineTo(memDC, 8, 13);

        DeleteObject(brush);
        DeleteObject(pen);
    }, 16);
}

HICON MakeWindowIcon(COLORREF color) {
    return MakeGlyphIcon([color](HDC memDC, int size) {
        HBRUSH brush = CreateSolidBrush(color);
        HPEN pen = CreatePen(PS_SOLID, 1, RGB(100, 100, 100));
        SelectObject(memDC, brush);
        SelectObject(memDC, pen);

        Rectangle(memDC, 2, 2, 14, 11);
        Rectangle(memDC, 4, 2, 12, 4);

        HPEN whitePen = CreatePen(PS_SOLID, 1, RGB(200, 200, 200));
        SelectObject(memDC, whitePen);
        Rectangle(memDC, 12, 1, 14, 3);
        DeleteObject(whitePen);

        DeleteObject(brush);
        DeleteObject(pen);
    }, 16);
}

HICON MakeColorIcon(COLORREF color, int shape) {
    return MakeGlyphIcon([color, shape](HDC memDC, int size) {
        HBRUSH brush = CreateSolidBrush(color);
        HPEN pen = CreatePen(PS_SOLID, 1, RGB(90, 90, 90));
        SelectObject(memDC, brush);
        SelectObject(memDC, pen);

        if (shape == 1) {
            Ellipse(memDC, 2, 2, 14, 14);
        } else if (shape == 2) {
            Ellipse(memDC, 4, 4, 12, 12);
        } else {
            RoundRect(memDC, 1, 1, 15, 15, 3, 3);
        }

        DeleteObject(brush);
        DeleteObject(pen);
    }, 16);
}

void BuildImageList() {
    g_imageList = ImageList_Create(16, 16, ILC_COLOR32 | ILC_MASK, ICON_COUNT, 0);

    HICON folder     = MakeColorIcon(RGB(255, 202, 92), 0);
    HICON monitor    = MakeMonitorIcon(RGB(52, 152, 219));
    HICON workspace  = MakeWorkspaceIcon();
    HICON keyboard   = MakeKeyboardIcon(RGB(120, 120, 120));
    HICON mouse      = MakeMouseIcon(RGB(120, 120, 120));
    HICON touchpad   = MakeTouchpadIcon(RGB(120, 120, 120));
    HICON ok         = MakeColorIcon(RGB(46, 204, 113), 2);
    HICON warn       = MakeColorIcon(RGB(231, 76, 60), 2);
    HICON window     = MakeWindowIcon(RGB(52, 152, 219));
    HICON dashboard  = MakeDashboardIcon();
    HICON settings   = MakeColorIcon(RGB(149, 165, 166), 0);
    HICON info       = MakeColorIcon(RGB(52, 152, 219), 1);

    ImageList_AddIcon(g_imageList, folder);
    ImageList_AddIcon(g_imageList, monitor);
    ImageList_AddIcon(g_imageList, workspace);
    ImageList_AddIcon(g_imageList, keyboard);
    ImageList_AddIcon(g_imageList, mouse);
    ImageList_AddIcon(g_imageList, touchpad);
    ImageList_AddIcon(g_imageList, ok);
    ImageList_AddIcon(g_imageList, warn);
    ImageList_AddIcon(g_imageList, window);
    ImageList_AddIcon(g_imageList, dashboard);
    ImageList_AddIcon(g_imageList, settings);
    ImageList_AddIcon(g_imageList, info);

    DestroyIcon(folder);
    DestroyIcon(monitor);
    DestroyIcon(workspace);
    DestroyIcon(keyboard);
    DestroyIcon(mouse);
    DestroyIcon(touchpad);
    DestroyIcon(ok);
    DestroyIcon(warn);
    DestroyIcon(window);
    DestroyIcon(dashboard);
    DestroyIcon(settings);
    DestroyIcon(info);

    TreeView_SetImageList(g_treeView, g_imageList, TVSIL_NORMAL);
}

// =====================================================================
// Helper Functions
// =====================================================================

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

void SetNodeLabel(HTREEITEM node, const std::wstring& label) {
    if (!node) return;
    
    wchar_t currentText[512];
    TVITEMW getItem = {0};
    getItem.mask = TVIF_TEXT;
    getItem.hItem = node;
    getItem.pszText = currentText;
    getItem.cchTextMax = 512;
    TreeView_GetItem(g_treeView, &getItem);

    if (label != currentText) {
        TVITEMW setItem = {0};
        setItem.mask = TVIF_TEXT;
        setItem.hItem = node;
        setItem.pszText = const_cast<LPWSTR>(label.c_str());
        TreeView_SetItem(g_treeView, &setItem);
    }
}

HTREEITEM AddRootNode(const std::wstring& text, int icon) {
    TVINSERTSTRUCTW tvi = {0};
    tvi.hParent = TVI_ROOT;
    tvi.hInsertAfter = TVI_LAST;
    tvi.item.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE;
    tvi.item.pszText = const_cast<LPWSTR>(text.c_str());
    tvi.item.iImage = icon;
    tvi.item.iSelectedImage = icon;
    HTREEITEM item = TreeView_InsertItem(g_treeView, &tvi);
    TreeView_Expand(g_treeView, item, TVE_EXPAND);
    return item;
}

void ToggleTreeView() {
    g_treeVisible = !g_treeVisible;
    ShowWindow(g_treeView, g_treeVisible ? SW_SHOW : SW_HIDE);
    
    // Update button text
    SetWindowTextW(g_toggleButton, g_treeVisible ? L"<< Hide Tree" : L">> Show Tree");
    
    // Re-layout
    LayoutControls(g_mainWindow);
}

// =====================================================================
// Dashboard Rendering
// =====================================================================

void DrawDashboard(HDC hdc, RECT rect) {
    // Background with gradient effect
    HBRUSH bgBrush = CreateSolidBrush(RGB(240, 242, 245));
    FillRect(hdc, &rect, bgBrush);
    DeleteObject(bgBrush);

    // Header with gradient
    RECT headerRect = rect;
    headerRect.bottom = 80;
    HBRUSH headerBrush = CreateSolidBrush(RGB(44, 62, 80));
    FillRect(hdc, &headerRect, headerBrush);
    DeleteObject(headerBrush);

    // Title with icon
    SetBkMode(hdc, TRANSPARENT);
    HFONT oldFont = (HFONT)SelectObject(hdc, g_fontLarge);
    SetTextColor(hdc, RGB(255, 255, 255));
    
    std::wstring title = L"DualDesk Dashboard";
    DrawTextW(hdc, title.c_str(), -1, &headerRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(hdc, oldFont);

    // Stats cards - bigger and more prominent
    int cardWidth = (rect.right - rect.left - 80) / 3;
    int cardHeight = 110;
    int startX = rect.left + 30;
    int startY = rect.top + 110;

    struct StatCard {
        const wchar_t* label;
        std::wstring value;
        COLORREF color;
        const wchar_t* icon;
    };

    std::vector<StatCard> cards;
    
    if (g_displayManager) {
        auto monitors = g_displayManager->EnumerateDisplays();
        cards.push_back({L"Monitors", std::to_wstring(monitors.size()), RGB(52, 152, 219), L"[M]"});
    }
    
    if (g_workspaceManager) {
        auto workspaces = g_workspaceManager->GetAllWorkspaces();
        int totalWindows = 0;
        for (auto* ws : workspaces) {
            totalWindows += ws->GetWindowCount();
        }
        cards.push_back({L"Workspaces", std::to_wstring(workspaces.size()), RGB(155, 89, 182), L"[W]"});
        cards.push_back({L"Windows", std::to_wstring(totalWindows), RGB(46, 204, 113), L"[ ]"});
    }

    for (size_t i = 0; i < cards.size() && i < 3; ++i) {
        RECT cardRect;
        cardRect.left = startX + i * (cardWidth + 30);
        cardRect.top = startY;
        cardRect.right = cardRect.left + cardWidth;
        cardRect.bottom = cardRect.top + cardHeight;

        // Card shadow
        RECT shadowRect = cardRect;
        shadowRect.left += 3;
        shadowRect.top += 3;
        shadowRect.right += 3;
        shadowRect.bottom += 3;
        HBRUSH shadowBrush = CreateSolidBrush(RGB(200, 200, 200));
        FillRect(hdc, &shadowRect, shadowBrush);
        DeleteObject(shadowBrush);

        // Card background
        HBRUSH cardBrush = CreateSolidBrush(RGB(255, 255, 255));
        HPEN borderPen = CreatePen(PS_SOLID, 1, RGB(230, 230, 230));
        SelectObject(hdc, cardBrush);
        SelectObject(hdc, borderPen);
        RoundRect(hdc, cardRect.left, cardRect.top, cardRect.right, cardRect.bottom, 10, 10);
        DeleteObject(cardBrush);
        DeleteObject(borderPen);

        // Color bar
        RECT colorBar = cardRect;
        colorBar.top = colorBar.bottom - 6;
        HBRUSH colorBrush = CreateSolidBrush(cards[i].color);
        FillRect(hdc, &colorBar, colorBrush);
        DeleteObject(colorBrush);

        // Icon
        RECT iconRect = cardRect;
        iconRect.left += 15;
        iconRect.top += 15;
        iconRect.right = iconRect.left + 40;
        iconRect.bottom = iconRect.top + 40;
        SelectObject(hdc, g_fontLarge);
        SetTextColor(hdc, cards[i].color);
        DrawTextW(hdc, cards[i].icon, -1, &iconRect, DT_LEFT | DT_TOP);

        // Value
        RECT valueRect = cardRect;
        valueRect.left += 65;
        valueRect.top += 15;
        valueRect.bottom = valueRect.top + 45;
        SelectObject(hdc, g_fontLarge);
        SetTextColor(hdc, RGB(44, 62, 80));
        DrawTextW(hdc, cards[i].value.c_str(), -1, &valueRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        // Label
        RECT labelRect = cardRect;
        labelRect.left += 65;
        labelRect.top = valueRect.bottom;
        labelRect.bottom = labelRect.top + 35;
        SelectObject(hdc, g_fontBold);
        SetTextColor(hdc, RGB(127, 140, 141));
        DrawTextW(hdc, cards[i].label, -1, &labelRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }

    // System Status Section - bigger
    int statusY = startY + cardHeight + 40;
    RECT statusRect;
    statusRect.left = rect.left + 30;
    statusRect.top = statusY;
    statusRect.right = rect.right - 30;
    statusRect.bottom = rect.bottom - 30;

    // Status shadow
    RECT statusShadow = statusRect;
    statusShadow.left += 3;
    statusShadow.top += 3;
    statusShadow.right += 3;
    statusShadow.bottom += 3;
    HBRUSH shadowBrush2 = CreateSolidBrush(RGB(200, 200, 200));
    FillRect(hdc, &statusShadow, shadowBrush2);
    DeleteObject(shadowBrush2);

    // Status background
    HBRUSH statusBrush = CreateSolidBrush(RGB(255, 255, 255));
    HPEN statusPen = CreatePen(PS_SOLID, 1, RGB(230, 230, 230));
    SelectObject(hdc, statusBrush);
    SelectObject(hdc, statusPen);
    RoundRect(hdc, statusRect.left, statusRect.top, statusRect.right, statusRect.bottom, 10, 10);
    DeleteObject(statusBrush);
    DeleteObject(statusPen);

    // Status header
    RECT statusHeader = statusRect;
    statusHeader.bottom = statusHeader.top + 45;
    HBRUSH headerBrush2 = CreateSolidBrush(RGB(52, 73, 94));
    FillRect(hdc, &statusHeader, headerBrush2);
    DeleteObject(headerBrush2);

    SelectObject(hdc, g_fontBold);
    SetTextColor(hdc, RGB(255, 255, 255));
    DrawTextW(hdc, L"System Status", -1, &statusHeader, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // Status content
    int contentY = statusHeader.bottom + 20;
    RECT contentRect = statusRect;
    contentRect.top = contentY;
    contentRect.bottom = statusRect.bottom - 20;
    contentRect.left += 30;
    contentRect.right -= 30;

    SelectObject(hdc, g_font);
    SetTextColor(hdc, RGB(44, 62, 80));

    // Status grid
    int rowHeight = 35;
    int col1 = 200;
    
    auto DrawStatusRow = [&](const wchar_t* label, const wchar_t* value, COLORREF color) {
        RECT labelRect = contentRect;
        labelRect.bottom = labelRect.top + rowHeight;
        labelRect.right = labelRect.left + col1;
        SetTextColor(hdc, RGB(127, 140, 141));
        DrawTextW(hdc, label, -1, &labelRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        RECT valueRect = contentRect;
        valueRect.left += col1;
        valueRect.bottom = valueRect.top + rowHeight;
        SetTextColor(hdc, color);
        DrawTextW(hdc, value, -1, &valueRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        contentRect.top += rowHeight;
    };

    DrawStatusRow(L"Driver Status:", 
                  g_driverConnected ? L"[OK] Connected" : L"[X] Disconnected",
                  g_driverConnected ? RGB(46, 204, 113) : RGB(231, 76, 60));

    DrawStatusRow(L"Last Event:", 
                  ToWide(g_lastEvent).c_str(),
                  RGB(52, 152, 219));

    DrawStatusRow(L"Application Status:", 
                  g_running ? L"[OK] Running" : L"[X] Stopped",
                  g_running ? RGB(46, 204, 113) : RGB(231, 76, 60));

    // Time - bigger and more prominent
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::wstringstream timeStr;
    timeStr << std::put_time(std::localtime(&time), L"%A, %B %d, %Y  %H:%M:%S");
    
    RECT timeRect = contentRect;
    timeRect.top += 10;
    SetTextColor(hdc, RGB(149, 165, 166));
    SelectObject(hdc, g_fontBold);
    DrawTextW(hdc, timeStr.str().c_str(), -1, &timeRect, DT_RIGHT | DT_TOP);

    SelectObject(hdc, oldFont);
}

// =====================================================================
// Dashboard Window Proc
// =====================================================================

LRESULT CALLBACK DashboardProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch(msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT rect;
            GetClientRect(hwnd, &rect);
            DrawDashboard(hdc, rect);
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_SIZE:
            InvalidateRect(hwnd, NULL, TRUE);
            return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// =====================================================================
// Info Panel Drawing
// =====================================================================

void DrawInfoPanel(HDC hdc, RECT rect) {
    // Background
    HBRUSH bgBrush = CreateSolidBrush(RGB(248, 249, 250));
    FillRect(hdc, &rect, bgBrush);
    DeleteObject(bgBrush);

    // Header
    RECT headerRect = rect;
    headerRect.bottom = 40;
    HBRUSH headerBrush = CreateSolidBrush(RGB(52, 73, 94));
    FillRect(hdc, &headerRect, headerBrush);
    DeleteObject(headerBrush);

    SetBkMode(hdc, TRANSPARENT);
    HFONT oldFont = (HFONT)SelectObject(hdc, g_fontBold);
    SetTextColor(hdc, RGB(255, 255, 255));
    DrawTextW(hdc, L"Quick Info", -1, &headerRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(hdc, oldFont);

    // Content
    int y = headerRect.bottom + 15;
    int x = 20;
    SelectObject(hdc, g_font);
    SetTextColor(hdc, RGB(44, 62, 80));

    std::wstringstream info;
    info << L"Hotkeys:\n\n";
    info << L"Win + Shift + Right\n";
    info << L"  Move window right\n\n";
    info << L"Win + Shift + Left\n";
    info << L"  Move window left\n\n";
    info << L"ESC\n  Exit application\n\n";
    info << L"Ctrl + R\n  Refresh view\n\n";
    info << L"Ctrl + T\n  Toggle Tree View\n\n";
    info << L"Toggle Tree:\n";
    info << L"  Click button above";

    RECT textRect = rect;
    textRect.left = x;
    textRect.top = y;
    textRect.right -= 10;
    textRect.bottom -= 10;
    DrawTextW(hdc, info.str().c_str(), -1, &textRect, DT_LEFT | DT_TOP);
}

// =====================================================================
// Info Panel Window Proc
// =====================================================================

LRESULT CALLBACK InfoPanelProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch(msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT rect;
            GetClientRect(hwnd, &rect);
            DrawInfoPanel(hdc, rect);
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_SIZE:
            InvalidateRect(hwnd, NULL, TRUE);
            return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// =====================================================================
// Main Application Functions
// =====================================================================

void RefreshTree() {
    if (!g_treeView) return;

    // --- Monitors ---
    if (g_displayManager) {
        auto monitors = g_displayManager->EnumerateDisplays();
        std::vector<std::tuple<HMONITOR, std::wstring, int>> monitorEntries;
        for (size_t i = 0; i < monitors.size(); ++i) {
            std::wstringstream label;
            label << L"Monitor " << (i + 1) << L"  -  "
                  << monitors[i].Width() << L"x" << monitors[i].Height();
            if (monitors[i].isPrimary) label << L"  (Primary)";
            monitorEntries.push_back({monitors[i].monitor, label.str(), ICON_MONITOR});
        }
        g_monitorSync.Sync(g_nodeMonitors, monitorEntries);
        std::wstring rootLabel = L"Monitors (" + std::to_wstring(monitors.size()) + L")";
        SetNodeLabel(g_nodeMonitors, rootLabel);
    }

    // --- Workspaces ---
    if (g_workspaceManager) {
        auto workspaces = g_workspaceManager->GetAllWorkspaces();
        std::vector<std::tuple<void*, std::wstring, int>> wsEntries;
        for (auto* ws : workspaces) {
            std::wstringstream label;
            label << ToWide(ws->GetName()) << L"  -  "
                  << ws->GetWindowCount() << L" window"
                  << (ws->GetWindowCount() == 1 ? L"" : L"s");
            wsEntries.push_back({(void*)ws, label.str(), ICON_WORKSPACE});
        }
        g_workspaceSync.Sync(g_nodeWorkspaces, wsEntries);

        for (auto* ws : workspaces) {
            auto nodeIt = g_workspaceSync.liveNodes.find((void*)ws);
            if (nodeIt == g_workspaceSync.liveNodes.end()) continue;
            HTREEITEM wsNode = nodeIt->second;
            TreeView_Expand(g_treeView, wsNode, TVE_EXPAND);

            std::vector<std::tuple<HWND, std::wstring, int>> winEntries;
            auto wsWindows = ws->GetWindows();
            for (const dualdesk::WindowInfo& winInfo : wsWindows) {
                if (!winInfo.isVisible || winInfo.isMinimized) continue;
                if (winInfo.title.empty()) continue;

                std::wstringstream label;
                label << winInfo.title;
                if (!winInfo.className.empty()) {
                    label << L"  [" << winInfo.className << L"]";
                }
                winEntries.push_back({winInfo.handle, label.str(), ICON_WINDOW});
            }
            g_workspaceWindowSync[(void*)ws].Sync(wsNode, winEntries);
        }

        for (auto it = g_workspaceWindowSync.begin(); it != g_workspaceWindowSync.end(); ) {
            bool stillExists = false;
            for (auto* ws : workspaces) {
                if ((void*)ws == it->first) { stillExists = true; break; }
            }
            if (!stillExists) {
                it->second.Clear();
                it = g_workspaceWindowSync.erase(it);
            } else {
                ++it;
            }
        }

        std::wstring rootLabel = L"Workspaces (" + std::to_wstring(workspaces.size()) + L")";
        SetNodeLabel(g_nodeWorkspaces, rootLabel);
    }

    // --- Devices ---
    {
        std::vector<std::tuple<std::wstring, std::wstring, int>> deviceEntries;
        int deviceCount = 0;

        if (g_inputManager) {
            auto keyboards = g_inputManager->GetKeyboards();
            for (const auto& kb : keyboards) {
                std::wstring cleanName = ToWide(GetCleanDeviceName(kb.deviceName));
                std::wstring key = L"kb:" + cleanName;
                deviceEntries.push_back({key, cleanName + L"  (Keyboard)", ICON_KEYBOARD});
                deviceCount++;
            }
            auto mice = g_inputManager->GetMice();
            for (const auto& mouse : mice) {
                std::string cleanNameStr = GetCleanDeviceName(mouse.deviceName);
                std::string lowerName = cleanNameStr;
                for (auto& c : lowerName) c = (char)tolower((unsigned char)c);
                bool isTouchpad = (lowerName.find("touchpad") != std::string::npos) ||
                                   (lowerName.find("trackpad") != std::string::npos);
                std::wstring cleanName = ToWide(cleanNameStr);
                std::wstring key = L"ms:" + cleanName;
                int icon = isTouchpad ? ICON_TOUCHPAD : ICON_MOUSE;
                const wchar_t* suffix = isTouchpad ? L"  (Touchpad)" : L"  (Mouse)";
                deviceEntries.push_back({key, cleanName + suffix, icon});
                deviceCount++;
            }
        }

        g_driverConnected = IsDriverConnected();
        deviceEntries.push_back({
            L"driver",
            g_driverConnected ? L"DualDesk Driver  -  Connected" : L"DualDesk Driver  -  Not connected",
            g_driverConnected ? ICON_OK : ICON_WARN
        });

        g_deviceSync.Sync(g_nodeDevices, deviceEntries);
        std::wstring rootLabel = L"Devices (" + std::to_wstring(deviceCount) + L")";
        SetNodeLabel(g_nodeDevices, rootLabel);
    }

    // Update dashboard
    if (g_dashboardWindow) {
        InvalidateRect(g_dashboardWindow, NULL, TRUE);
    }

    UpdateStatusBar();
}

void UpdateStatusBar() {
    if (!g_statusBar) return;
    
    std::wstring driverText = g_driverConnected ? L"Driver: Connected" : L"Driver: Not connected";
    SendMessageW(g_statusBar, SB_SETTEXTW, 0, (LPARAM)driverText.c_str());
    
    std::wstring eventText = L"Last event: " + ToWide(g_lastEvent);
    SendMessageW(g_statusBar, SB_SETTEXTW, 1, (LPARAM)eventText.c_str());
    
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::wstringstream timeStr;
    timeStr << std::put_time(std::localtime(&time), L"%H:%M:%S");
    SendMessageW(g_statusBar, SB_SETTEXTW, 2, (LPARAM)timeStr.str().c_str());
}

void BuildTree() {
    BuildImageList();
    g_nodeMonitors   = AddRootNode(L"Monitors", ICON_FOLDER);
    g_nodeWorkspaces = AddRootNode(L"Workspaces", ICON_FOLDER);
    g_nodeDevices    = AddRootNode(L"Devices", ICON_FOLDER);
    RefreshTree();
}

void CreateDashboard(HWND parent) {
    g_dashboardWindow = CreateWindowExW(
        0, L"STATIC", L"",
        WS_CHILD | WS_VISIBLE | SS_OWNERDRAW,
        0, 0, 100, 100,
        parent, NULL, GetModuleHandle(NULL), NULL
    );
    SetWindowLongPtr(g_dashboardWindow, GWLP_WNDPROC, (LONG_PTR)DashboardProc);
}

void CreateInfoPanel(HWND parent) {
    g_infoPanel = CreateWindowExW(
        WS_EX_CLIENTEDGE, L"STATIC", L"",
        WS_CHILD | WS_VISIBLE | SS_OWNERDRAW,
        0, 0, 200, 100,
        parent, NULL, GetModuleHandle(NULL), NULL
    );
    SetWindowLongPtr(g_infoPanel, GWLP_WNDPROC, (LONG_PTR)InfoPanelProc);
}

void LayoutControls(HWND hwnd) {
    RECT rc;
    GetClientRect(hwnd, &rc);

    int statusH = 28;
    SetWindowPos(g_statusBar, NULL, 0, rc.bottom - statusH, rc.right, statusH, SWP_NOZORDER);

    int contentY = 0;
    int contentH = rc.bottom - statusH - contentY;
    int contentW = rc.right;

    int treeWidth = g_treeVisible ? 280 : 0;
    int infoWidth = 220;

    // Toggle button position (top right corner)
    int buttonW = 120;
    int buttonH = 30;
    int buttonX = contentW - buttonW - 10;
    int buttonY = 5;
    SetWindowPos(g_toggleButton, NULL, buttonX, buttonY, buttonW, buttonH, SWP_NOZORDER);

    // Tree view
    SetWindowPos(g_treeView, NULL, 0, contentY, treeWidth, contentH, SWP_NOZORDER);
    
    // Dashboard
    int dashX = treeWidth;
    int dashW = contentW - treeWidth - infoWidth;
    SetWindowPos(g_dashboardWindow, NULL, dashX, contentY, dashW, contentH, SWP_NOZORDER);
    
    // Info panel
    SetWindowPos(g_infoPanel, NULL, contentW - infoWidth, contentY, infoWidth, contentH, SWP_NOZORDER);
}

// =====================================================================
// Window Procedures
// =====================================================================

LRESULT CALLBACK MainWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch(msg) {
        case WM_CREATE: {
            // Larger fonts
            g_font = CreateFontW(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                 DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                 DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
            g_fontBold = CreateFontW(16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                     DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                     DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
            g_fontLarge = CreateFontW(28, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                      DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                      DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
            g_fontSmall = CreateFontW(13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                      DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                      DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

            // Create toggle button
            g_toggleButton = CreateWindowExW(
                0, L"BUTTON", L"<< Hide Tree",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                0, 0, 120, 30,
                hwnd, (HMENU)300, GetModuleHandle(NULL), NULL
            );
            SendMessage(g_toggleButton, WM_SETFONT, (WPARAM)g_fontBold, TRUE);

            // Create tree view
            g_treeView = CreateWindowExW(WS_EX_CLIENTEDGE, WC_TREEVIEWW, L"",
                WS_CHILD | WS_VISIBLE | TVS_HASLINES | TVS_LINESATROOT | TVS_HASBUTTONS |
                TVS_SHOWSELALWAYS | TVS_FULLROWSELECT,
                0, 0, 100, 100, hwnd, (HMENU)200, GetModuleHandle(NULL), NULL);
            SendMessage(g_treeView, WM_SETFONT, (WPARAM)g_font, TRUE);
            SendMessage(g_treeView, TVM_SETBKCOLOR, 0, (LPARAM)RGB(248, 249, 250));
            SendMessage(g_treeView, TVM_SETITEMHEIGHT, (WPARAM)26, 0);

            CreateDashboard(hwnd);
            CreateInfoPanel(hwnd);

            // Create status bar
            g_statusBar = CreateWindowExW(0, STATUSCLASSNAMEW, L"",
                WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
                0, 0, 100, 28, hwnd, (HMENU)201, GetModuleHandle(NULL), NULL);
            SendMessage(g_statusBar, WM_SETFONT, (WPARAM)g_fontSmall, TRUE);
            
            int parts[3] = {200, 450, -1};
            SendMessage(g_statusBar, SB_SETPARTS, 3, (LPARAM)parts);

            BuildTree();
            LayoutControls(hwnd);
            SetTimer(hwnd, 1, 500, NULL);

            return 0;
        }
        case WM_COMMAND:
            if (LOWORD(wParam) == 300) { // Toggle button clicked
                ToggleTreeView();
                return 0;
            }
            break;
        case WM_SIZE:
            LayoutControls(hwnd);
            return 0;
        case WM_TIMER:
            RefreshTree();
            return 0;
        case WM_DEVICECHANGE:
            RefreshTree();
            return 0;
        case WM_GETMINMAXINFO: {
            MINMAXINFO* mmi = (MINMAXINFO*)lParam;
            mmi->ptMinTrackSize.x = 1000;
            mmi->ptMinTrackSize.y = 650;
            mmi->ptMaxTrackSize.x = 1000;
            mmi->ptMaxTrackSize.y = 650;
            return 0;
        }
        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) {
                g_running = false;
                DestroyWindow(hwnd);
                return 0;
            }
            if (wParam == 'R' && (GetAsyncKeyState(VK_CONTROL) & 0x8000)) {
                g_lastEvent = "Manual refresh";
                RefreshTree();
                return 0;
            }
            if (wParam == 'T' && (GetAsyncKeyState(VK_CONTROL) & 0x8000)) {
                ToggleTreeView();
                return 0;
            }
            break;
        case WM_DESTROY:
            KillTimer(hwnd, 1);
            if (g_font) DeleteObject(g_font);
            if (g_fontBold) DeleteObject(g_fontBold);
            if (g_fontLarge) DeleteObject(g_fontLarge);
            if (g_fontSmall) DeleteObject(g_fontSmall);
            if (g_imageList) ImageList_Destroy(g_imageList);
            PostQuitMessage(0);
            return 0;
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
                        }
                        return 0;
                    }
                    if (wParam == VK_LEFT) {
                        HWND activeWindow = GetForegroundWindow();
                        if (activeWindow && g_windowMover) {
                            g_windowMover->MoveWindowToPreviousMonitor(activeWindow);
                            g_lastEvent = "Window moved left";
                            RefreshTree();
                        }
                        return 0;
                    }
                }
            }
            break;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// =====================================================================
// Entry Point
// =====================================================================

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    LOG_INFO("DualDesk starting...");

    INITCOMMONCONTROLSEX icex = {0};
    icex.dwSize = sizeof(icex);
    icex.dwICC = ICC_TREEVIEW_CLASSES | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icex);

    WNDCLASSEXW wcInput = {0};
    wcInput.cbSize = sizeof(WNDCLASSEXW);
    wcInput.lpfnWndProc = InputWindowProc;
    wcInput.hInstance = hInstance;
    wcInput.lpszClassName = L"DualDeskInputWindow";
    wcInput.hCursor = LoadCursor(NULL, IDC_ARROW);

    if (!RegisterClassExW(&wcInput)) {
        MessageBoxA(NULL, "Failed to register input window class", "Error", MB_OK);
        return 1;
    }

    HWND hwndInput = CreateWindowExW(0, L"DualDeskInputWindow", L"DualDesk Input", 
                                      WS_OVERLAPPEDWINDOW, 0, 0, 100, 100, 
                                      NULL, NULL, hInstance, NULL);
    if (!hwndInput) {
        MessageBoxA(NULL, "Failed to create input window", "Error", MB_OK);
        return 1;
    }
    ShowWindow(hwndInput, SW_HIDE);

    dualdesk::InputManager inputManager;
    dualdesk::DisplayManager displayManager;
    dualdesk::WorkspaceManager workspaceManager;

    g_inputManager = &inputManager;
    g_displayManager = &displayManager;
    g_workspaceManager = &workspaceManager;

    inputManager.Initialize(hwndInput);
    displayManager.EnumerateDisplays();
    workspaceManager.Initialize(&displayManager, &inputManager);

    inputManager.SetDeviceChangeCallback([](const dualdesk::InputDevice& device, bool added) {
        std::string name = GetCleanDeviceName(device.deviceName);
        std::string typeStr;
        switch(device.type) {
            case dualdesk::InputDeviceType::Keyboard: typeStr = "Keyboard"; break;
            case dualdesk::InputDeviceType::Mouse: typeStr = "Mouse"; break;
            default: typeStr = "Unknown"; break;
        }
        g_lastEvent = (added ? "Plugged in: " : "Unplugged: ") + name + " (" + typeStr + ")";
        RefreshTree();
    });

    inputManager.SetInputEventCallback([](const dualdesk::InputEvent& event) {
        if (event.type == dualdesk::InputEventType::KeyDown ||
            event.type == dualdesk::InputEventType::KeyUp ||
            event.type == dualdesk::InputEventType::MouseButtonDown ||
            event.type == dualdesk::InputEventType::MouseButtonUp ||
            event.type == dualdesk::InputEventType::MouseWheel) {
            g_lastEvent = event.ToString();
            UpdateStatusBar();
        }
    });

    dualdesk::WindowMover windowMover;
    windowMover.Initialize(&workspaceManager);
    g_windowMover = &windowMover;

    windowMover.SetMoveCallback([](HWND hwnd, HMONITOR from, HMONITOR to) {
        g_lastEvent = "Window moved between monitors";
        RefreshTree();
    });

    WNDCLASSEXW wcMain = {0};
    wcMain.cbSize = sizeof(WNDCLASSEXW);
    wcMain.lpfnWndProc = MainWindowProc;
    wcMain.hInstance = hInstance;
    wcMain.lpszClassName = L"DualDeskMainWindow";
    wcMain.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcMain.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcMain.style = CS_HREDRAW | CS_VREDRAW;

    if (!RegisterClassExW(&wcMain)) {
        MessageBoxA(NULL, "Failed to register main window class", "Error", MB_OK);
        return 1;
    }

    int windowWidth = 1000;
    int windowHeight = 650;
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    int x = (screenWidth - windowWidth) / 2;
    int y = (screenHeight - windowHeight) / 2;

    g_mainWindow = CreateWindowExW(
        WS_EX_APPWINDOW,
        L"DualDeskMainWindow",
        L"DualDesk - Workspace Manager",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_CLIPCHILDREN,
        x, y, windowWidth, windowHeight,
        NULL, NULL, hInstance, NULL
    );

    if (!g_mainWindow) {
        MessageBoxA(NULL, "Failed to create main window", "Error", MB_OK);
        return 1;
    }

    ShowWindow(g_mainWindow, SW_SHOW);
    UpdateWindow(g_mainWindow);
    RefreshTree();

    g_lastEvent = "DualDesk started";
    UpdateStatusBar();

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

    if (g_mainWindow) {
        DestroyWindow(g_mainWindow);
        g_mainWindow = nullptr;
    }

    return 0;
}