#include "dualdesk/core/logger.h"
#include <windows.h>
#include <iostream>

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, 
                   LPSTR lpCmdLine, int nCmdShow) {
    // Allocate console window for debugging
    AllocConsole();
    FILE* fDummy;
    freopen_s(&fDummy, "CONOUT$", "w", stdout);
    freopen_s(&fDummy, "CONOUT$", "w", stderr);
    std::cout.clear();
    
    LOG_INFO("DualDesk v0.1.0 starting...");
    LOG_DEBUG("Debug message - this should appear");
    LOG_WARN("Warning: This is a test warning");
    
    MessageBoxA(NULL, "DualDesk is running!\nCheck the console for logs.", 
                "DualDesk", MB_OK | MB_ICONINFORMATION);
    
    LOG_INFO("DualDesk exiting...");
    
    // KEEP CONSOLE OPEN - press any key to close
    system("pause");
    
    // Cleanup
    fclose(stdout);
    fclose(stderr);
    FreeConsole();
    
    return 0;
}