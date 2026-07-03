#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <cstdint>

namespace dualdesk {

class DriverInterface {
public:
    DriverInterface();
    ~DriverInterface();

    bool Open();
    void Close();
    bool IsConnected() const;

    // Device assignment
    bool AssignDeviceToWorkspace(HANDLE deviceHandle, ULONG workspaceId);
    bool UnassignDevice(HANDLE deviceHandle);
    ULONG GetWorkspaceForDevice(HANDLE deviceHandle);
    
    // Route mode - ONLY ONE overload to avoid ambiguity
    bool SetRouteMode(BOOLEAN enable);  // Use TRUE/FALSE, not 1/0
    
    bool ResetDeviceAssignments();
    ULONG GetDeviceCount() const;

    // Additional methods for compatibility
    bool AddDevice(HANDLE deviceHandle, ULONG workspaceId);
    bool RemoveDevice(HANDLE deviceHandle);
    bool RouteInput(HANDLE deviceHandle, const std::vector<uint8_t>& inputData);

private:
    HANDLE m_hDevice;
    bool m_connected;

    bool SendIoctl(DWORD ioctl, PVOID input, DWORD inputSize, PVOID output, DWORD outputSize, DWORD* bytesReturned);
};

} // namespace dualdesk