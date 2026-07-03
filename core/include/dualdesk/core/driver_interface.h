#pragma once

#include <windows.h>
#include <winioctl.h>
#include <string>
#include <memory>
#include <vector>

namespace dualdesk {

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

#define IOCTL_DUALDESK_ASSIGN_DEVICE_TO_WORKSPACE \
    CTL_CODE(FILE_DEVICE_UNKNOWN, DUALDESK_IOCTL_BASE + 6, METHOD_BUFFERED, FILE_ANY_ACCESS)

// Data structures
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

typedef struct _DUALDESK_ASSIGN_DEVICE_INPUT {
    HANDLE DeviceHandle;
    ULONG WorkspaceId;
} DUALDESK_ASSIGN_DEVICE_INPUT;

typedef struct _DUALDESK_STATS_OUTPUT {
    ULONG TotalDevices;
    ULONG TotalEventsRouted;
    ULONG TotalEventsBlocked;
    LARGE_INTEGER DriverStartTime;
} DUALDESK_STATS_OUTPUT;

// Workspace assignment
struct DeviceAssignment {
    HANDLE deviceHandle;
    std::string deviceName;
    std::string deviceType;
    DWORD workspaceId;
    bool isAssigned;
};

/**
 * @brief Interface for communicating with the DualDesk kernel driver
 */
class DriverInterface {
public:
    DriverInterface();
    ~DriverInterface();

    bool Open();
    void Close();
    bool IsConnected() const { return hDriver_ != INVALID_HANDLE_VALUE; }

    bool AddDevice(HANDLE deviceHandle, DWORD processId, bool isKeyboard, bool isMouse);
    bool RemoveDevice(HANDLE deviceHandle);
    bool RouteInput(HANDLE deviceHandle, DWORD targetProcessId);
    bool SetRouteMode(DWORD mode);
    bool GetStats(DUALDESK_STATS_OUTPUT& stats);
    bool AssignDeviceToWorkspace(HANDLE deviceHandle, DWORD workspaceId);
    
    DWORD GetLastError() const { return lastError_; }

private:
    HANDLE hDriver_;
    DWORD lastError_;

    bool SendIoctl(DWORD ioctlCode, void* input, size_t inputSize, 
                   void* output, size_t outputSize, DWORD& bytesReturned);
};


#define IOCTL_DUALDESK_SET_ROUTE_MODE \
    CTL_CODE(FILE_DEVICE_UNKNOWN, DUALDESK_IOCTL_BASE + 4, METHOD_BUFFERED, FILE_ANY_ACCESS)

} // namespace dualdesk