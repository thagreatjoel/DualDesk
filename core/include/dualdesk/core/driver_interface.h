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

typedef struct _DUALDESK_STATS_OUTPUT {
    ULONG TotalDevices;
    ULONG TotalEventsRouted;
    ULONG TotalEventsBlocked;
    LARGE_INTEGER DriverStartTime;
} DUALDESK_STATS_OUTPUT;

/**
 * @brief Interface for communicating with the DualDesk kernel driver
 */
class DriverInterface {
public:
    DriverInterface();
    ~DriverInterface();

    /**
     * @brief Opens connection to the driver
     * @return true if successful
     */
    bool Open();

    /**
     * @brief Closes connection to the driver
     */
    void Close();

    /**
     * @brief Checks if driver is connected
     */
    bool IsConnected() const { return hDriver_ != INVALID_HANDLE_VALUE; }

    /**
     * @brief Adds a device to be monitored by the driver
     * @param deviceHandle Handle to the input device
     * @param processId Target process ID
     * @param isKeyboard true if keyboard
     * @param isMouse true if mouse
     */
    bool AddDevice(HANDLE deviceHandle, DWORD processId, bool isKeyboard, bool isMouse);

    /**
     * @brief Removes a device from monitoring
     * @param deviceHandle Handle to the input device
     */
    bool RemoveDevice(HANDLE deviceHandle);

    /**
     * @brief Routes input from a device to a specific process
     * @param deviceHandle Handle to the input device
     * @param targetProcessId Target process ID
     */
    bool RouteInput(HANDLE deviceHandle, DWORD targetProcessId);

    /**
     * @brief Sets the route mode
     * @param mode 0 = normal, 1 = isolated
     */
    bool SetRouteMode(DWORD mode);

    /**
     * @brief Gets driver statistics
     */
    bool GetStats(DUALDESK_STATS_OUTPUT& stats);

    /**
     * @brief Gets the last error code
     */
    DWORD GetLastError() const { return lastError_; }

private:
    HANDLE hDriver_;
    DWORD lastError_;

    bool SendIoctl(DWORD ioctlCode, void* input, size_t inputSize, 
                   void* output, size_t outputSize, DWORD& bytesReturned);
};

} // namespace dualdesk