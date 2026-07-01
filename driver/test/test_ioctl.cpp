#include <windows.h>
#include <winioctl.h>
#include <iostream>

#define IOCTL_DUALDESK_GET_STATS \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800 + 5, METHOD_BUFFERED, FILE_ANY_ACCESS)

struct DUALDESK_STATS_OUTPUT {
    ULONG TotalDevices;
    ULONG TotalEventsRouted;
    ULONG TotalEventsBlocked;
    LARGE_INTEGER DriverStartTime;
};

int main() {
    const char* paths[] = {
        "\\\\.\\DualDeskDriver",
        "\\\\.\\DualDesk",
        "\\\\.\\DUALDESK",
        "\\\\.\\Global\\DualDeskDriver"
    };
    
    for (int i = 0; i < 4; i++) {
        HANDLE hDriver = CreateFileA(
            paths[i],
            GENERIC_READ | GENERIC_WRITE,
            0, NULL, OPEN_EXISTING, 0, NULL
        );
        
        if (hDriver != INVALID_HANDLE_VALUE) {
            std::cout << "✅ Opened driver at: " << paths[i] << std::endl;
            
            DUALDESK_STATS_OUTPUT stats = {};
            DWORD bytesReturned;
            
            if (DeviceIoControl(hDriver, IOCTL_DUALDESK_GET_STATS, NULL, 0,
                                &stats, sizeof(stats), &bytesReturned, NULL)) {
                std::cout << "✅ Driver communication successful!\n";
                std::cout << "   Events Routed: " << stats.TotalEventsRouted << "\n";
                std::cout << "   Events Blocked: " << stats.TotalEventsBlocked << "\n";
            }
            
            CloseHandle(hDriver);
            return 0;
        }
    }
    
    std::cout << "❌ Failed to open driver on any path. Error: " << GetLastError() << std::endl;
    return 1;
}