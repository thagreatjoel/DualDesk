#include "dualdesk/core/logger.h"
#include "dualdesk/display/display_manager.h"
#include "dualdesk/workspace/window_tracker.h"
#include "dualdesk/workspace/workspace_manager.h"
#include "dualdesk/input/input_manager.h"
#include "dualdesk/input/input_router.h"
#include <windows.h>
#include <iostream>
#include <string>
#include <thread>
#include <chrono>

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
dualdesk::DisplayManager* g_displayManager = nullptr;
dualdesk::WorkspaceManager* g_workspaceManager = nullptr;
HWND g_statusWindow = nullptr;
bool g_running = true;
bool g_showStatusWindow = true;

// Forward declarations
LRESULT CALLBACK StatusWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK InputWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
void UpdateStatusWindow();

// ==================== STATUS WINDOW ====================
LRESULT CALLBACK StatusWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch(msg) {
        case WM_CREATE:
            // Set a timer to update every 500ms
            SetTimer(hwnd, 1, 500, NULL);
            return 0;
        case WM_TIMER:
            UpdateStatusWindow();
            return 0;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            // Background
            RECT rect;
            GetClientRect(hwnd, &rect);
            FillRect(hdc, &rect, (HBRUSH)(COLOR_WINDOW + 1));
            
            // Draw text
            std::string text = "DualDesk - Real-Time Monitor\n";
            text += "=============================\n\n";
            
            if (g_displayManager) {
                auto monitors = g_displayManager->EnumerateDisplays();
                text += "Monitors: " + std::to_string(monitors.size()) + "\n";
                for (size_t i = 0; i < monitors.size(); ++i) {
                    text += "  Monitor " + std::to_string(i+1) + ": ";
                    text += std::to_string(monitors[i].Width()) + "x";
                    text += std::to_string(monitors[i].Height());
                    if (monitors[i].isPrimary) text += " (Primary)";
                    text += "\n";
                }
            }
            text += "\n";
            
            if (g_inputManager) {
                auto keyboards = g_inputManager->GetKeyboards();
                auto mice = g_inputManager->GetMice();
                text += "Keyboards: " + std::to_string(keyboards.size()) + "\n";
                for (const auto& kb : keyboards) {
                    text += "  - " + WStringToString(kb.deviceName) + "\n";
                }
                text += "\n";
                text += "Mice: " + std::to_string(mice.size()) + "\n";
                for (const auto& mouse : mice) {
                    text += "  - " + WStringToString(mouse.deviceName) + "\n";
                }
            }
            
            text += "\nPress ESC to exit";
            
            // Draw text in window
            RECT textRect = rect;
            textRect.left += 10;
            textRect.top += 10;
            DrawTextA(hdc, text.c_str(), -1, &textRect, DT_LEFT);
            
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_DESTROY:
            KillTimer(hwnd, 1);
            g_showStatusWindow = false;
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
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// ==================== MAIN ====================
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow) {
    // Create console
    AllocConsole();
    AttachConsole(ATTACH_PARENT_PROCESS);
    FILE* stream;
    freopen_s(&stream, "CONOUT$", "w", stdout);
    freopen_s(&stream, "CONOUT$", "w", stderr);
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    std::cout.clear();

    LOG_INFO("DualDesk starting...");

    // ==================== CREATE INPUT WINDOW ====================
    WNDCLASSEX wcInput = {0};
    wcInput.cbSize = sizeof(WNDCLASSEX);
    wcInput.lpfnWndProc = InputWindowProc;
    wcInput.hInstance = hInstance;
    wcInput.lpszClassName = L"DualDeskInputWindow";
    wcInput.hCursor = LoadCursor(NULL, IDC_ARROW);

    if (!RegisterClassEx(&wcInput)) {
        LOG_ERROR("Failed to register input window class");
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
        LOG_ERROR("Failed to create input window");
        return 1;
    }

    LOG_INFO("Input window created");
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
            LOG_INFO("🔌 DEVICE PLUGGED IN: " + name + " (" + typeStr + ")");
        } else {
            LOG_INFO("🔌 DEVICE UNPLUGGED: " + name + " (" + typeStr + ")");
        }
        
        // Update the status window
        UpdateStatusWindow();
    });

    inputManager.SetInputEventCallback([](const dualdesk::InputEvent& event) {
        if (event.type == dualdesk::InputEventType::KeyDown ||
            event.type == dualdesk::InputEventType::KeyUp ||
            event.type == dualdesk::InputEventType::MouseButtonDown ||
            event.type == dualdesk::InputEventType::MouseButtonUp ||
            event.type == dualdesk::InputEventType::MouseWheel) {
            LOG_INFO("EVENT: " + event.ToString());
        }
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
        LOG_ERROR("Failed to register status window class");
        return 1;
    }

    // Calculate window size
    int windowWidth = 450;
    int windowHeight = 400;
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    int x = (screenWidth - windowWidth) / 2;
    int y = (screenHeight - windowHeight) / 2;

    g_statusWindow = CreateWindowEx(
        WS_EX_TOPMOST,
        L"DualDeskStatusWindow",
        L"DualDesk - Real-Time Monitor",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        x, y, windowWidth, windowHeight,
        NULL, NULL, hInstance, NULL
    );

    if (!g_statusWindow) {
        LOG_ERROR("Failed to create status window");
        return 1;
    }

    LOG_INFO("Status window created");
    ShowWindow(g_statusWindow, SW_SHOW);
    UpdateWindow(g_statusWindow);

    // ==================== SHOW INITIAL STATUS ====================
    LOG_INFO("========================================");
    LOG_INFO("DUALDESK IS RUNNING");
    LOG_INFO("Real-time status window is open");
    LOG_INFO("Plug/unplug devices to see updates");
    LOG_INFO("Press ESC in status window to exit");
    LOG_INFO("========================================");

    // Update status window immediately
    UpdateStatusWindow();

    // ==================== MAIN MESSAGE LOOP ====================
    MSG msg;
    DWORD lastCheckTime = GetTickCount();

    while (g_running && GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
        
        // Check for device changes
        DWORD now = GetTickCount();
        if (now - lastCheckTime >= 500) {
            lastCheckTime = now;
            inputManager.CheckForDeviceChanges();
        }
    }

    LOG_INFO("DualDesk exiting...");
    
    // Cleanup
    if (g_statusWindow) {
        DestroyWindow(g_statusWindow);
        g_statusWindow = nullptr;
    }
    
    FreeConsole();
    return 0;
}