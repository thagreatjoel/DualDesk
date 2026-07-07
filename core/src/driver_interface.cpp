#include "dualdesk/core/driver_interface.h"
#include "dualdesk/core/logger.h"
#include <winioctl.h>
#include <string>
#include <vector>

namespace dualdesk {

// IOCTL definitions
#define IOCTL_DUALDESK_ASSIGN_DEVICE \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_DUALDESK_UNASSIGN_DEVICE \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_DUALDESK_GET_WORKSPACE \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x802, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_DUALDESK_SET_ROUTE_MODE \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x803, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_DUALDESK_RESET \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x804, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_DUALDESK_GET_STATUS \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x805, METHOD_BUFFERED, FILE_ANY_ACCESS)

DriverInterface::DriverInterface() : m_hDevice(INVALID_HANDLE_VALUE), m_connected(false) {}

DriverInterface::~DriverInterface() { Close(); }

bool DriverInterface::Open() {
    if (m_connected) return true;
    
    m_hDevice = CreateFileW(
        L"\\\\.\\DualDesk",
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );
    
    if (m_hDevice == INVALID_HANDLE_VALUE) {
        LOG_ERROR("Failed to open driver: %d", GetLastError());
        return false;
    }
    
    m_connected = true;
    LOG_INFO("Driver connected");
    return true;
}

void DriverInterface::Close() {
    if (m_connected && m_hDevice != INVALID_HANDLE_VALUE) {
        CloseHandle(m_hDevice);
        m_hDevice = INVALID_HANDLE_VALUE;
        m_connected = false;
        LOG_INFO("Driver closed");
    }
}

bool DriverInterface::IsConnected() const {
    return m_connected && m_hDevice != INVALID_HANDLE_VALUE;
}

bool DriverInterface::SendIoctl(DWORD ioctl, PVOID input, DWORD inputSize, PVOID output, DWORD outputSize, DWORD* bytesReturned) {
    if (!IsConnected()) return false;
    
    DWORD bytes = 0;
    BOOL result = DeviceIoControl(
        m_hDevice,
        ioctl,
        input,
        inputSize,
        output,
        outputSize,
        bytesReturned ? bytesReturned : &bytes,
        NULL
    );
    
    if (!result) {
        LOG_ERROR("IOCTL 0x%X failed: %d", ioctl, GetLastError());
    }
    return result != FALSE;
}

bool DriverInterface::AssignDeviceToWorkspace(HANDLE deviceHandle, ULONG workspaceId) {
    ULONG buffer[2] = { (ULONG)(ULONG_PTR)deviceHandle, workspaceId };
    return SendIoctl(IOCTL_DUALDESK_ASSIGN_DEVICE, buffer, sizeof(buffer), NULL, 0, NULL);
}

bool DriverInterface::UnassignDevice(HANDLE deviceHandle) {
    ULONG handle = (ULONG)(ULONG_PTR)deviceHandle;
    DWORD bytesReturned = 0;
    
    // Send UNASSIGN IOCTL to driver
    BOOL result = DeviceIoControl(
        m_hDevice,
        IOCTL_DUALDESK_UNASSIGN_DEVICE,  // IOCTL code
        &handle,                         // Input buffer
        sizeof(handle),                  // Input size
        NULL,                            // Output buffer
        0,                               // Output size
        &bytesReturned,                  // Bytes returned
        NULL                             // Overlapped
    );
    
    if (!result) {
        DWORD error = GetLastError();
        LOG_ERROR("UnassignDevice failed: %d", error);
        return false;
    }
    
    LOG_INFO("Device unassigned successfully");
    return true;
}

ULONG DriverInterface::GetWorkspaceForDevice(HANDLE deviceHandle) {
    ULONG handle = (ULONG)(ULONG_PTR)deviceHandle;
    ULONG workspaceId = 0xFFFFFFFF;
    SendIoctl(IOCTL_DUALDESK_GET_WORKSPACE, &handle, sizeof(handle), &workspaceId, sizeof(workspaceId), NULL);
    return workspaceId;
}

// ONLY ONE SetRouteMode - no ambiguity
bool DriverInterface::SetRouteMode(BOOLEAN enable) {
    return SendIoctl(IOCTL_DUALDESK_SET_ROUTE_MODE, &enable, sizeof(enable), NULL, 0, NULL);
}

bool DriverInterface::ResetDeviceAssignments() {
    DWORD bytesReturned = 0;
    
    BOOL result = DeviceIoControl(
        m_hDevice,
        IOCTL_DUALDESK_RESET,  // RESET IOCTL code
        NULL,                  // No input
        0,                     // No input size
        NULL,                  // No output
        0,                     // No output size
        &bytesReturned,        // Bytes returned
        NULL                   // Overlapped
    );
    
    if (!result) {
        DWORD error = GetLastError();
        LOG_ERROR("ResetDeviceAssignments failed: %d", error);
        return false;
    }
    
    LOG_INFO("All device assignments reset");
    return true;
}

ULONG DriverInterface::GetDeviceCount() const {
    ULONG count = 0;
    if (m_connected && m_hDevice != INVALID_HANDLE_VALUE) {
        const_cast<DriverInterface*>(this)->SendIoctl(IOCTL_DUALDESK_GET_STATUS, NULL, 0, &count, sizeof(count), NULL);
    }
    return count;
}

// Additional methods for compatibility
bool DriverInterface::AddDevice(HANDLE deviceHandle, ULONG workspaceId) {
    return AssignDeviceToWorkspace(deviceHandle, workspaceId);
}

bool DriverInterface::RemoveDevice(HANDLE deviceHandle) {
    return UnassignDevice(deviceHandle);
}

bool DriverInterface::RouteInput(HANDLE deviceHandle, const std::vector<uint8_t>& inputData) {
    LOG_DEBUG("Routing input from device %p, size %llu", deviceHandle, (unsigned long long)inputData.size());
    return true;
}

} // namespace dualdesk