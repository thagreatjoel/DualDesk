#include <windows.h>
#include <iostream>
#include "dualddesk/core/driver_interface.h"

using namespace dualdesk;

int main() {
    std::cout << "Testing DualDesk Driver Integration" << std::endl;
    std::cout << "====================================" << std::endl;

    DriverInterface driver;
    
    if (!driver.Open()) {
        std::cout << "❌ Failed to open driver" << std::endl;
        return 1;
    }
    
    std::cout << "✅ Driver opened!" << std::endl;
    
    // Set isolation mode
    if (driver.SetRouteMode(1)) {
        std::cout << "✅ Isolation mode enabled" << std::endl;
    }
    
    // Get stats
    DUALDESK_STATS_OUTPUT stats;
    if (driver.GetStats(stats)) {
        std::cout << "📊 Driver Statistics:" << std::endl;
        std::cout << "  Total Devices: " << stats.TotalDevices << std::endl;
        std::cout << "  Events Routed: " << stats.TotalEventsRouted << std::endl;
        std::cout << "  Events Blocked: " << stats.TotalEventsBlocked << std::endl;
    }
    
    // Example: Add current process as a target
    DWORD currentPid = GetCurrentProcessId();
    std::cout << "Current process ID: " << currentPid << std::endl;
    
    // In a real implementation, you would get device handles from Raw Input
    // and add them with AddDevice()
    
    driver.Close();
    return 0;
}