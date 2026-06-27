#include "dualdesk/core/logger.h"
#include "dualdesk/display/display_manager.h"
#include "dualdesk/workspace/window_tracker.h"
#include "dualdesk/input/input_manager.h"
#include <windows.h>
#include <iostream>
#include <string>

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

// Global InputManager for window procedure
dualdesk::InputManager* g_inputManager = nullptr;

LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
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

    // Create hidden window for raw input
    WNDCLASSEX wc = {0};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"DualDeskInputWindow";
    RegisterClassEx(&wc);

    HWND hwnd = CreateWindowEx(
        0,
        L"DualDeskInputWindow",
        L"DualDesk Input",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        400, 300,
        nullptr, nullptr, hInstance, nullptr
    );

    // Initialize InputManager
    dualdesk::InputManager inputManager;
    g_inputManager = &inputManager;
    inputManager.Initialize(hwnd);

    // Set up event callback with filtering
    inputManager.SetInputEventCallback([](const dualdesk::InputEvent& event) {
        if (event.type == dualdesk::InputEventType::KeyDown ||
            event.type == dualdesk::InputEventType::KeyUp ||
            event.type == dualdesk::InputEventType::MouseButtonDown ||
            event.type == dualdesk::InputEventType::MouseButtonUp ||
            event.type == dualdesk::InputEventType::MouseWheel) {
            LOG_INFO(event.ToString());
        }
    });

    // Detect monitors
    dualdesk::DisplayManager dm;
    auto monitors = dm.EnumerateDisplays();
    
    std::string monitorMsg = "Found " + std::to_string(monitors.size()) + " monitors";
    LOG_INFO(monitorMsg);
    
    for (size_t i = 0; i < monitors.size(); ++i) {
        std::string msg = "Monitor " + std::to_string(i+1) + ": " + 
                          std::to_string(monitors[i].Width()) + "x" + 
                          std::to_string(monitors[i].Height());
        LOG_INFO(msg);
    }

    // Track windows
    dualdesk::WindowTracker tracker;
    auto windows = tracker.GetAllWindows();
    std::string windowMsg = "Found " + std::to_string(windows.size()) + " trackable windows";
    LOG_INFO(windowMsg);

    // Show windows per monitor
    for (size_t i = 0; i < monitors.size(); ++i) {
        std::string msg = "Monitor " + std::to_string(i+1) + " windows:";
        LOG_INFO(msg);
        
        auto windowsOnMonitor = tracker.GetWindowsOnMonitor(monitors[i].monitor);
        for (const auto& window : windowsOnMonitor) {
            std::string title = WStringToString(window.title);
            LOG_INFO("  - " + title);
        }
    }

    // Show input devices
    LOG_INFO("=== INPUT DEVICES ===");
    
    auto keyboards = inputManager.GetKeyboards();
    auto mice = inputManager.GetMice();
    
    LOG_INFO("Keyboards: " + std::to_string(keyboards.size()));
    for (const auto& kb : keyboards) {
        std::string name = WStringToString(kb.deviceName);
        LOG_INFO("  - " + name);
    }
    
    LOG_INFO("Mice: " + std::to_string(mice.size()));
    for (const auto& mouse : mice) {
        std::string name = WStringToString(mouse.deviceName);
        LOG_INFO("  - " + name);
    }

    // Build message for MessageBox
    std::string message = "DualDesk is running!\n\n";
    message += "Detected " + std::to_string(monitors.size()) + " monitor(s):\n";
    for (size_t i = 0; i < monitors.size(); ++i) {
        message += "  Monitor " + std::to_string(i+1) + ": " + 
                   std::to_string(monitors[i].Width()) + "x" + 
                   std::to_string(monitors[i].Height());
        if (monitors[i].isPrimary) message += " (Primary)";
        message += "\n";
    }
    
    message += "\nInput Devices:\n";
    message += "  Keyboards: " + std::to_string(keyboards.size()) + "\n";
    message += "  Mice: " + std::to_string(mice.size()) + "\n";
    
    message += "\nTotal windows: " + std::to_string(windows.size());

    MessageBoxA(NULL, message.c_str(), "DualDesk Monitor Info", MB_OK | MB_ICONINFORMATION);

    LOG_INFO("DualDesk exiting...");
    system("pause");

    FreeConsole();
    return 0;
}