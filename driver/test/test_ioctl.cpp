#include <windows.h>
#include <winioctl.h>
#include <iostream>

#define IOCTL_DUALDESK_GET_STATS \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800 + 5, METHOD_BUFFERED, FILE_ANY_ACCESS)

typedef struct _DUALDESK_STATS_OUTPUT {
    ULONG TotalDevices;
    ULONG TotalEventsRouted;
    ULONG TotalEventsBlocked;
    LARGE_INTEGER DriverStartTime;
} DUALDESK_STATS_OUTPUT;

int main() {
    
   const char* paths[] = {
    "\\\\.\\DualDeskDriver", 
};

    HANDLE hDriver = INVALID_HANDLE_VALUE;
    const char* foundPath = nullptr;

    for (int i = 0; i < 3; i++) {
        hDriver = CreateFileA(
            paths[i],
            GENERIC_READ | GENERIC_WRITE,
            0,
            NULL,
            OPEN_EXISTING,
            0,
            NULL
        );

        if (hDriver != INVALID_HANDLE_VALUE) {
            foundPath = paths[i];
            break;
        }
    }

    if (hDriver == INVALID_HANDLE_VALUE) {
        std::cout << "Failed to open driver. Error: " << GetLastError() << std::endl;
        return 1;
    }

    std::cout << "✅ Opened driver at: " << foundPath << std::endl;

    DUALDESK_STATS_OUTPUT stats = {};
    DWORD bytesReturned;

    if (DeviceIoControl(hDriver, IOCTL_DUALDESK_GET_STATS, NULL, 0,
                        &stats, sizeof(stats), &bytesReturned, NULL)) {
        std::cout << "✅ Driver communication successful!\n";
        std::cout << "   Total Devices: " << stats.TotalDevices << "\n";
        std::cout << "   Events Routed: " << stats.TotalEventsRouted << "\n";
        std::cout << "   Events Blocked: " << stats.TotalEventsBlocked << "\n";
        std::cout << "   Driver Start Time: " << stats.DriverStartTime.QuadPart << "\n";
    } else {
        std::cout << "Failed to get stats. Error: " << GetLastError() << std::endl;
    }

    CloseHandle(hDriver);
    return 0;
}