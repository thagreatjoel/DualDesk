#include "dualdesk/core/logger.h"
#include "dualdesk/display/display_manager.h"
#include <windows.h>
#include <iostream>
#include <sstream>
#include <string>

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow) {
    AllocConsole();
    FILE* fDummy;
    freopen_s(&fDummy, "CONOUT$", "w", stdout);
    freopen_s(&fDummy, "CONOUT$", "w", stderr);

    LOG_INFO("DualDesk starting...");

    dualdesk::DisplayManager dm;
    auto monitors = dm.EnumerateDisplays();

    // Build message with stringstream and convert to string
    std::stringstream ss;
    ss << "Found " << monitors.size() << " monitors";
    LOG_INFO(ss.str());
    
    for (size_t i = 0; i < monitors.size(); ++i) {
        std::stringstream msg;
        msg << "Monitor " << (i+1) << ": " 
            << monitors[i].Width() << "x" << monitors[i].Height();
        LOG_INFO(msg.str());
    }

    // Build message for MessageBox
    std::string message = "DualDesk is running!\n\nDetected ";
    message += std::to_string(monitors.size());
    message += " monitor(s):\n";
    
    for (size_t i = 0; i < monitors.size(); ++i) {
        message += "  Monitor ";
        message += std::to_string(i + 1);
        message += ": ";
        message += std::to_string(monitors[i].Width());
        message += "x";
        message += std::to_string(monitors[i].Height());
        if (monitors[i].isPrimary) {
            message += " (Primary)";
        }
        message += "\n";
    }

    MessageBoxA(NULL, message.c_str(), "DualDesk Monitor Info", MB_OK | MB_ICONINFORMATION);

    LOG_INFO("DualDesk exiting...");
    system("pause");

    fclose(stdout);
    fclose(stderr);
    FreeConsole();
    return 0;
}