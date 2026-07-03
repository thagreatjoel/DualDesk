#include "dualdesk/core/driver_interface.h"
#include "dualdesk/core/logger.h"
#include <iostream>
#include <sstream>

namespace dualdesk {

DriverInterface::DriverInterface() 
    : hDriver_(INVALID_HANDLE_VALUE), lastError_(0) {}

DriverInterface::~DriverInterface() {
    Close();
}

bool DriverInterface::Open() {
    if (IsConnected()) {
        return true;
    }

    const std::vector<std::string> paths = {
        "\\\\.\\DualDesk",
        "\\\\.\\DualDeskDriver",
        "\\\\.\\DUALDESK",
        "\\\\.\\Global\\DualDeskDriver",
        "\\\\.\\DualDeskFilter"
    };

    for (const auto& path : paths) {
        hDriver_ = CreateFileA(
            path.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            0,
            NULL,
            OPEN_EXISTING,
            0,
            NULL
        );

        if (hDriver_ != INVALID_HANDLE_VALUE) {
            std::string msg = "Connected to driver at: " + path;
            LOG_INFO(msg);
            return true;
        }
    }

    lastError_ = GetLastError();
    std::string msg = "Failed to open driver. Error: " + std::to_string(lastError_);
    LOG_ERROR(msg);
    return false;
}

void DriverInterface::Close() {
    if (hDriver_ != INVALID_HANDLE_VALUE) {
        CloseHandle(hDriver_);
        hDriver_ = INVALID_HANDLE_VALUE;
    }
}

bool DriverInterface::SendIoctl(DWORD ioctlCode, void* input, size_t inputSize,
                                 void* output, size_t outputSize, DWORD& bytesReturned) {
    if (!IsConnected()) {
        lastError_ = ERROR_NOT_CONNECTED;
        return false;
    }

    BOOL result = DeviceIoControl(
        hDriver_,
        ioctlCode,
        input,
        static_cast<DWORD>(inputSize),
        output,
        static_cast<DWORD>(outputSize),
        &bytesReturned,
        NULL
    );

    if (!result) {
        lastError_ = GetLastError();
        std::string msg = "IOCTL 0x" + std::to_string(ioctlCode) + " failed. Error: " + std::to_string(lastError_);
        LOG_ERROR(msg);
        return false;
    }

    return true;
}

bool DriverInterface::AddDevice(HANDLE deviceHandle, DWORD processId, 
                                 bool isKeyboard, bool isMouse) {
    DUALDESK_ADD_DEVICE_INPUT input = {
        deviceHandle,
        processId,
        (BOOLEAN)isKeyboard,
        (BOOLEAN)isMouse
    };

    DWORD bytesReturned;
    bool result = SendIoctl(IOCTL_DUALDESK_ADD_DEVICE, &input, sizeof(input),
                            NULL, 0, bytesReturned);

    if (result) {
        std::string msg = "Added device: handle=" + std::to_string((uintptr_t)deviceHandle) + 
                          ", pid=" + std::to_string(processId) +
                          ", keyboard=" + std::to_string(isKeyboard) +
                          ", mouse=" + std::to_string(isMouse);
        LOG_INFO(msg);
    }

    return result;
}

bool DriverInterface::RemoveDevice(HANDLE deviceHandle) {
    DUALDESK_ADD_DEVICE_INPUT input = {
        deviceHandle,
        0,
        FALSE,
        FALSE
    };

    DWORD bytesReturned;
    return SendIoctl(IOCTL_DUALDESK_REMOVE_DEVICE, &input, sizeof(input),
                     NULL, 0, bytesReturned);
}

bool DriverInterface::RouteInput(HANDLE deviceHandle, DWORD targetProcessId) {
    DUALDESK_ROUTE_INPUT_INPUT input = {
        deviceHandle,
        targetProcessId
    };

    DWORD bytesReturned;
    bool result = SendIoctl(IOCTL_DUALDESK_ROUTE_INPUT, &input, sizeof(input),
                            NULL, 0, bytesReturned);

    if (result) {
        std::string msg = "Routing input: handle=" + std::to_string((uintptr_t)deviceHandle) + 
                          ", pid=" + std::to_string(targetProcessId);
        LOG_INFO(msg);
    }

    return result;
}

bool DriverInterface::SetRouteMode(DWORD mode) {
    DWORD bytesReturned;
    bool result = SendIoctl(IOCTL_DUALDESK_SET_ROUTE_MODE, &mode, sizeof(mode),
                            NULL, 0, bytesReturned);

    if (result) {
        std::string msg = "Route mode set to: " + std::to_string(mode);
        LOG_INFO(msg);
    }

    return result;
}

bool DriverInterface::GetStats(DUALDESK_STATS_OUTPUT& stats) {
    DWORD bytesReturned;
    bool result = SendIoctl(IOCTL_DUALDESK_GET_STATS, NULL, 0,
                            &stats, sizeof(stats), bytesReturned);

    if (result) {
        std::string msg = "Stats: devices=" + std::to_string(stats.TotalDevices) + 
                          ", routed=" + std::to_string(stats.TotalEventsRouted) +
                          ", blocked=" + std::to_string(stats.TotalEventsBlocked);
        LOG_INFO(msg);
    }

    return result;
}

bool DriverInterface::AssignDeviceToWorkspace(HANDLE deviceHandle, DWORD workspaceId) {
    DUALDESK_ASSIGN_DEVICE_INPUT input = {
        deviceHandle,
        workspaceId
    };

    DWORD bytesReturned;
    bool result = SendIoctl(IOCTL_DUALDESK_ASSIGN_DEVICE_TO_WORKSPACE, &input, sizeof(input),
                            NULL, 0, bytesReturned);

    if (result) {
        std::string msg = "Device assigned to workspace: handle=" + std::to_string((uintptr_t)deviceHandle) + 
                          ", workspace=" + std::to_string(workspaceId);
        LOG_INFO(msg);
    }

    return result;
}

} // namespace dualdesk