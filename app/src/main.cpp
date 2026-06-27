#include "dualdesk/core/logger.h"
#include "dualdesk/display/display_manager.h"
#include "dualdesk/workspace/window_tracker.h"
#include <windows.h>
#include <iostream>
#include <string>

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow) {
    // Create console and redirect output
    AllocConsole();
    AttachConsole(ATTACH_PARENT_PROCESS);
    
    // Simple redirect
    FILE* stream;
    freopen_s(&stream, "CONOUT$", "w", stdout);
    freopen_s(&stream, "CONOUT$", "w", stderr);
    
    // Turn off buffering
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    
    std::cout.clear();

    LOG_INFO("DualDesk starting...");

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
            std::string title(window.title.begin(), window.title.end());
            LOG_INFO("  - " + title);
        }
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
    
    message += "\nTotal windows: " + std::to_string(windows.size());

    MessageBoxA(NULL, message.c_str(), "DualDesk Monitor Info", MB_OK | MB_ICONINFORMATION);

    LOG_INFO("DualDesk exiting...");
    system("pause");

    FreeConsole();
    return 0;
}