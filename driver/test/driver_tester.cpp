#include <windows.h>
#include <winioctl.h>
#include <iostream>
#include <string>
#include <vector>

// IOCTL codes - must match driver
#define DUALDESK_IOCTL_BASE 0x800

#define IOCTL_DUALDESK_ADD_DEVICE \
    CTL_CODE(FILE_DEVICE_UNKNOWN, DUALDESK_IOCTL_BASE + 1, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_DUALDESK_REMOVE_DEVICE \
    CTL_CODE(FILE_DEVICE_UNKNOWN, DUALDESK_IOCTL_BASE + 2, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_DUALDESK_ROUTE_INPUT \
    CTL_CODE(FILE_DEVICE_UNKNOWN, DUALDESK_IOCTL_BASE + 3, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_DUALDESK_SET_ROUTE_MODE \
    CTL_CODE(FILE_DEVICE_UNKNOWN, DUALDESK_IOCTL_BASE + 4, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_DUALDESK_GET_STATS \
    CTL_CODE(FILE_DEVICE_UNKNOWN, DUALDESK_IOCTL_BASE + 5, METHOD_BUFFERED, FILE_ANY_ACCESS)

typedef struct _DUALDESK_ADD_DEVICE_INPUT {
    HANDLE DeviceHandle;
    ULONG TargetProcessId;
    BOOLEAN IsKeyboard;
    BOOLEAN IsMouse;
} DUALDESK_ADD_DEVICE_INPUT;

typedef struct _DUALDESK_ROUTE_INPUT_INPUT {
    HANDLE DeviceHandle;
    ULONG TargetProcessId;
} DUALDESK_ROUTE_INPUT_INPUT;

typedef struct _DUALDESK_STATS_OUTPUT {
    ULONG TotalDevices;
    ULONG TotalEventsRouted;
    ULONG TotalEventsBlocked;
    LARGE_INTEGER DriverStartTime;
} DUALDESK_STATS_OUTPUT;

class DriverInterface {
public:
    DriverInterface() : hDriver(INVALID_HANDLE_VALUE) {}
    
    ~DriverInterface() {
        Close();
    }
    
    bool Open() {
        hDriver = CreateFile(
            L"\\\\.\\DualDeskDriver",
            GENERIC_READ | GENERIC_WRITE,
            0,
            NULL,
            OPEN_EXISTING,
            0,
            NULL
        );
        
        if (hDriver == INVALID_HANDLE_VALUE) {
            std::cerr << "Failed to open driver: " << GetLastError() << std::endl;
            return false;
        }
        
        std::cout << "✅ Driver opened successfully!" << std::endl;
        return true;
    }
    
    void Close() {
        if (hDriver != INVALID_HANDLE_VALUE) {
            CloseHandle(hDriver);
            hDriver = INVALID_HANDLE_VALUE;
        }
    }
    
    bool AddDevice(HANDLE deviceHandle, DWORD processId, bool isKeyboard, bool isMouse) {
        DUALDESK_ADD_DEVICE_INPUT input = {
            deviceHandle,
            processId,
            (BOOLEAN)isKeyboard,
            (BOOLEAN)isMouse
        };
        
        DWORD bytesReturned;
        BOOL result = DeviceIoControl(
            hDriver,
            IOCTL_DUALDESK_ADD_DEVICE,
            &input,
            sizeof(input),
            NULL,
            0,
            &bytesReturned,
            NULL
        );
        
        if (!result) {
            std::cerr << "Failed to add device: " << GetLastError() << std::endl;
        } else {
            std::cout << "✅ Device added successfully!" << std::endl;
        }
        
        return result != 0;
    }
    
    bool SetRouteMode(DWORD mode) {
        DWORD bytesReturned;
        BOOL result = DeviceIoControl(
            hDriver,
            IOCTL_DUALDESK_SET_ROUTE_MODE,
            &mode,
            sizeof(mode),
            NULL,
            0,
            &bytesReturned,
            NULL
        );
        
        if (!result) {
            std::cerr << "Failed to set route mode: " << GetLastError() << std::endl;
        } else {
            std::cout << "✅ Route mode set to: " << (mode ? "ISOLATED" : "NORMAL") << std::endl;
        }
        
        return result != 0;
    }
    
    bool RouteInput(HANDLE deviceHandle, DWORD targetProcessId) {
        DUALDESK_ROUTE_INPUT_INPUT input = {
            deviceHandle,
            targetProcessId
        };
        
        DWORD bytesReturned;
        BOOL result = DeviceIoControl(
            hDriver,
            IOCTL_DUALDESK_ROUTE_INPUT,
            &input,
            sizeof(input),
            NULL,
            0,
            &bytesReturned,
            NULL
        );
        
        if (!result) {
            std::cerr << "Failed to route input: " << GetLastError() << std::endl;
        } else {
            std::cout << "✅ Input routing set for process: " << targetProcessId << std::endl;
        }
        
        return result != 0;
    }
    
    bool GetStats() {
        DUALDESK_STATS_OUTPUT output = {0};
        DWORD bytesReturned;
        
        BOOL result = DeviceIoControl(
            hDriver,
            IOCTL_DUALDESK_GET_STATS,
            NULL,
            0,
            &output,
            sizeof(output),
            &bytesReturned,
            NULL
        );
        
        if (!result) {
            std::cerr << "Failed to get stats: " << GetLastError() << std::endl;
            return false;
        }
        
        std::cout << "\n📊 Driver Statistics:" << std::endl;
        std::cout << "  Total Devices: " << output.TotalDevices << std::endl;
        std::cout << "  Events Routed: " << output.TotalEventsRouted << std::endl;
        std::cout << "  Events Blocked: " << output.TotalEventsBlocked << std::endl;
        std::cout << "  Driver Uptime: " << output.DriverStartTime.QuadPart << " ticks" << std::endl;
        
        return true;
    }
    
private:
    HANDLE hDriver;
};

int main() {
    std::cout << "DualDesk Driver Tester v1.0" << std::endl;
    std::cout << "=============================" << std::endl << std::endl;
    
    DriverInterface driver;
    
    if (!driver.Open()) {
        std::cout << "❌ Failed to open driver. Is it installed?" << std::endl;
        std::cout << "Try running: sc start DualDesk" << std::endl;
        system("pause");
        return 1;
    }
    
    // Get current process ID
    DWORD currentPid = GetCurrentProcessId();
    std::cout << "Current Process ID: " << currentPid << std::endl << std::endl;
    
    // Get raw input devices for demonstration
    // You'll need to implement device enumeration
    
    // Set isolation mode
    std::cout << "Setting isolation mode..." << std::endl;
    if (driver.SetRouteMode(1)) {
        std::cout << "✅ Isolation mode ENABLED!" << std::endl;
    }
    
    // Get driver statistics
    driver.GetStats();
    
    std::cout << "\nPress any key to exit..." << std::endl;
    system("pause");
    
    return 0;
}