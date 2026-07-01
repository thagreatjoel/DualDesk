#include "dualddesk/core/driver_interface.h"
#include "dualddesk/core/logger.h"
#include <iostream>

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

    // Try multiple possible device paths
    const std::vector<std::string> paths = {
        "\\\\.\\DualDesk",
        "\\\\.\\DualDeskDriver",
        "\\\\.\\DUALDESK",
        "\\\\.\\Global\\DualDeskDriver"
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
            LOG_INFO("Connected to driver at: {}", path);
            return true;
        }
    }

    lastError_ = GetLastError();
    LOG_ERROR("Failed to open driver. Error: {}", lastError_);
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
        LOG_ERROR("IOCTL 0x{:08X} failed. Error: {}", ioctlCode, lastError_);
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
        LOG_INFO("Added device: handle={}, pid={}, keyboard={}, mouse={}",
                 (void*)deviceHandle, processId, isKeyboard, isMouse);
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
        LOG_INFO("Routing input: handle={}, pid={}", (void*)deviceHandle, targetProcessId);
    }

    return result;
}

bool DriverInterface::SetRouteMode(DWORD mode) {
    DWORD bytesReturned;
    bool result = SendIoctl(IOCTL_DUALDESK_SET_ROUTE_MODE, &mode, sizeof(mode),
                            NULL, 0, bytesReturned);

    if (result) {
        LOG_INFO("Route mode set to: {}", mode);
    }

    return result;
}

bool DriverInterface::GetStats(DUALDESK_STATS_OUTPUT& stats) {
    DWORD bytesReturned;
    bool result = SendIoctl(IOCTL_DUALDESK_GET_STATS, NULL, 0,
                            &stats, sizeof(stats), bytesReturned);

    if (result) {
        LOG_INFO("Stats: devices={}, routed={}, blocked={}",
                 stats.TotalDevices, stats.TotalEventsRouted, stats.TotalEventsBlocked);
    }

    return result;
}

} // namespace dualdesk