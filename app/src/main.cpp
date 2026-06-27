#include "dualdesk/core/logger.h"
#include "dualdesk/display/display_manager.h"
#include "dualdesk/workspace/window_tracker.h"
#include "dualdesk/workspace/workspace_manager.h"
#include "dualdesk/workspace/window_mover.h"
#include "dualdesk/input/input_manager.h"
#include "dualdesk/input/input_router.h"
#include <windows.h>
#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <sstream>

// Helper to convert wstring to string
std::string WStringToString(const std::wstring& wstr) {
    if (wstr.empty()) return std::string();
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), 
                                          NULL, 0, NULL, NULL);
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), 
                        &strTo[0], size_needed, NULL, NULL);
    return strTo;
}

// Global pointers
dualdesk::InputManager* g_inputManager = nullptr;
dualdesk::WindowMover* g_windowMover = nullptr;
dualdesk::DisplayManager* g_displayManager = nullptr;
dualdesk::WorkspaceManager* g_workspaceManager = nullptr;
HWND g_statusWindow = nullptr;
bool g_running = true;
std::string g_lastEvent = "None";

// Forward declarations
LRESULT CALLBACK StatusWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK InputWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
void UpdateStatusWindow();

// ==================== STATUS WINDOW ====================
LRESULT CALLBACK StatusWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch(msg) {
        case WM_CREATE:
            SetTimer(hwnd, 1, 500, NULL);
            return 0;
        case WM_TIMER:
            UpdateStatusWindow();
            return 0;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT rect;
            GetClientRect(hwnd, &rect);
            
            // White background
            HBRUSH whiteBrush = CreateSolidBrush(RGB(255, 255, 255));
            FillRect(hdc, &rect, whiteBrush);
            DeleteObject(whiteBrush);
            
            // Create font
            HFONT hFont = CreateFont(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                     DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                     DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Consolas");
            SelectObject(hdc, hFont);
            
            std::stringstream ss;
            
            // Header
            ss << "+----------------------------------------------------+\n";
            ss << "|                DUALDESK STATUS                     |\n";
            ss << "+----------------------------------------------------+\n\n";
            
            // Monitors
            if (g_displayManager) {
                auto monitors = g_displayManager->EnumerateDisplays();
                ss << "MONITORS: " << monitors.size() << "\n";
                for (size_t i = 0; i < monitors.size(); ++i) {
                    ss << "   " << (i+1) << ". ";
                    ss << monitors[i].Width() << "x" << monitors[i].Height();
                    if (monitors[i].isPrimary) ss << " (Primary)";
                    ss << "\n";
                }
            }
            ss << "\n";
            
            // Workspaces
            if (g_workspaceManager) {
                auto workspaces = g_workspaceManager->GetAllWorkspaces();
                ss << "WORKSPACES: " << workspaces.size() << "\n";
                for (auto* ws : workspaces) {
                    ss << "   - " << ws->GetName() << ": ";
                    ss << ws->GetWindowCount() << " windows\n";
                }
            }
            ss << "\n";
            
            // Input Devices
            if (g_inputManager) {
                auto keyboards = g_inputManager->GetKeyboards();
                auto mice = g_inputManager->GetMice();
                ss << "KEYBOARDS: " << keyboards.size() << "\n";
                for (const auto& kb : keyboards) {
                    ss << "   - " << WStringToString(kb.deviceName) << "\n";
                }
                ss << "\n";
                ss << "MICE: " << mice.size() << "\n";
                for (const auto& mouse : mice) {
                    ss << "   - " << WStringToString(mouse.deviceName) << "\n";
                }
            }
            ss << "\n";
            
            // Last Event
            ss << "LAST EVENT: " << g_lastEvent << "\n\n";
            
            // Help
            ss << "----------------------------------------------------\n";
            ss << "  Win+Shift+Left   -> Move window left\n";
            ss << "  Win+Shift+Right  -> Move window right\n";
            ss << "  ESC              -> Exit\n";
            
            std::string text = ss.str();
            
            // Draw text
            RECT textRect = rect;
            textRect.left += 15;
            textRect.top += 15;
            textRect.right -= 15;
            textRect.bottom -= 15;
            
            DrawTextA(hdc, text.c_str(), -1, &textRect, DT_LEFT | DT_TOP | DT_EXPANDTABS);
            
            DeleteObject(hFont);
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_DESTROY:
            KillTimer(hwnd, 1);
            PostQuitMessage(0);
            return 0;
        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) {
                g_running = false;
                DestroyWindow(hwnd);
                return 0;
            }
            break;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

void UpdateStatusWindow() {
    if (g_statusWindow) {
        InvalidateRect(g_statusWindow, NULL, TRUE);
    }
}

// ==================== INPUT WINDOW ====================
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
        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) {
                g_running = false;
                PostQuitMessage(0);
                return 0;
            }
            // Win+Shift+Left/Right for window movement
            if ((GetAsyncKeyState(VK_LWIN) & 0x8000) || (GetAsyncKeyState(VK_RWIN) & 0x8000)) {
                if (GetAsyncKeyState(VK_SHIFT) & 0x8000) {
                    if (wParam == VK_RIGHT) {
                        HWND activeWindow = GetForegroundWindow();
                        if (activeWindow && g_windowMover) {
                            g_windowMover->MoveWindowToNextMonitor(activeWindow);
                            g_lastEvent = "Window moved right";
                            UpdateStatusWindow();
                        }
                        return 0;
                    }
                    if (wParam == VK_LEFT) {
                        HWND activeWindow = GetForegroundWindow();
                        if (activeWindow && g_windowMover) {
                            g_windowMover->MoveWindowToPreviousMonitor(activeWindow);
                            g_lastEvent = "Window moved left";
                            UpdateStatusWindow();
                        }
                        return 0;
                    }
                }
            }
            break;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// ==================== MAIN ====================
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow) {
    
    LOG_INFO("DualDesk starting...");

    // ==================== CREATE INPUT WINDOW ====================
    WNDCLASSEX wcInput = {0};
    wcInput.cbSize = sizeof(WNDCLASSEX);
    wcInput.lpfnWndProc = InputWindowProc;
    wcInput.hInstance = hInstance;
    wcInput.lpszClassName = L"DualDeskInputWindow";
    wcInput.hCursor = LoadCursor(NULL, IDC_ARROW);

    if (!RegisterClassEx(&wcInput)) {
        MessageBoxA(NULL, "Failed to register input window class", "Error", MB_OK);
        return 1;
    }

    HWND hwndInput = CreateWindowEx(
        0,
        L"DualDeskInputWindow",
        L"DualDesk Input",
        WS_OVERLAPPEDWINDOW,
        0, 0, 100, 100,
        NULL, NULL, hInstance, NULL
    );

    if (!hwndInput) {
        MessageBoxA(NULL, "Failed to create input window", "Error", MB_OK);
        return 1;
    }

    ShowWindow(hwndInput, SW_HIDE);

    // ==================== INITIALIZE MANAGERS ====================
    dualdesk::InputManager inputManager;
    dualdesk::DisplayManager displayManager;
    dualdesk::WorkspaceManager workspaceManager;
    
    g_inputManager = &inputManager;
    g_displayManager = &displayManager;
    g_workspaceManager = &workspaceManager;
    
    inputManager.Initialize(hwndInput);
    displayManager.EnumerateDisplays();
    workspaceManager.Initialize(&displayManager, &inputManager);

    // ==================== SETUP CALLBACKS ====================
    inputManager.SetDeviceChangeCallback([](const dualdesk::InputDevice& device, bool added) {
        std::string name(device.deviceName.begin(), device.deviceName.end());
        std::string typeStr;
        switch(device.type) {
            case dualdesk::InputDeviceType::Keyboard: typeStr = "Keyboard"; break;
            case dualdesk::InputDeviceType::Mouse: typeStr = "Mouse"; break;
            default: typeStr = "Unknown"; break;
        }
        
        if (added) {
            g_lastEvent = "PLUGGED IN: " + name + " (" + typeStr + ")";
        } else {
            g_lastEvent = "UNPLUGGED: " + name + " (" + typeStr + ")";
        }
        UpdateStatusWindow();
    });

    inputManager.SetInputEventCallback([](const dualdesk::InputEvent& event) {
        if (event.type == dualdesk::InputEventType::KeyDown ||
            event.type == dualdesk::InputEventType::KeyUp ||
            event.type == dualdesk::InputEventType::MouseButtonDown ||
            event.type == dualdesk::InputEventType::MouseButtonUp ||
            event.type == dualdesk::InputEventType::MouseWheel) {
            g_lastEvent = event.ToString();
            UpdateStatusWindow();
        }
    });

    // ==================== WINDOW MOVEMENT ====================
    dualdesk::WindowMover windowMover;
    windowMover.Initialize(&workspaceManager);
    g_windowMover = &windowMover;
    
    windowMover.SetMoveCallback([](HWND hwnd, HMONITOR from, HMONITOR to) {
        g_lastEvent = "Window moved between monitors";
        UpdateStatusWindow();
    });

    // ==================== CREATE STATUS WINDOW ====================
    WNDCLASSEX wcStatus = {0};
    wcStatus.cbSize = sizeof(WNDCLASSEX);
    wcStatus.lpfnWndProc = StatusWindowProc;
    wcStatus.hInstance = hInstance;
    wcStatus.lpszClassName = L"DualDeskStatusWindow";
    wcStatus.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcStatus.hCursor = LoadCursor(NULL, IDC_ARROW);

    if (!RegisterClassEx(&wcStatus)) {
        MessageBoxA(NULL, "Failed to register status window class", "Error", MB_OK);
        return 1;
    }

    int windowWidth = 550;
    int windowHeight = 600;
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    int x = (screenWidth - windowWidth) / 2;
    int y = (screenHeight - windowHeight) / 2;

    g_statusWindow = CreateWindowEx(
        WS_EX_TOPMOST,
        L"DualDeskStatusWindow",
        L"DualDesk - Status Monitor",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_CLIPCHILDREN,
        x, y, windowWidth, windowHeight,
        NULL, NULL, hInstance, NULL
    );

    if (!g_statusWindow) {
        MessageBoxA(NULL, "Failed to create status window", "Error", MB_OK);
        return 1;
    }

    ShowWindow(g_statusWindow, SW_SHOW);
    UpdateWindow(g_statusWindow);
    UpdateStatusWindow();

    // Show startup message
    g_lastEvent = "DualDesk started";
    UpdateStatusWindow();

    // ==================== MAIN MESSAGE LOOP ====================
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
    
    return 0;
}