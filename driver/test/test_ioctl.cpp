#include <windows.h>
#include <winioctl.h>
#include <iostream>
#include <string>

// ============================================================
// IOCTL Definitions - Match driver
// ============================================================
#define IOCTL_DUALDESK_GET_STATUS \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x805, METHOD_BUFFERED, FILE_ANY_ACCESS)

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

// ============================================================
// Helper function to print error
// ============================================================
void PrintError(const char* testName, DWORD error) {
    std::cout << "❌ " << testName << " failed. Error: " << error;
    
    switch (error) {
        case ERROR_INVALID_FUNCTION: std::cout << " (ERROR_INVALID_FUNCTION)"; break;
        case ERROR_INVALID_PARAMETER: std::cout << " (ERROR_INVALID_PARAMETER)"; break;
        case ERROR_INSUFFICIENT_BUFFER: std::cout << " (ERROR_INSUFFICIENT_BUFFER)"; break;
        case ERROR_FILE_NOT_FOUND: std::cout << " (ERROR_FILE_NOT_FOUND)"; break;
        case ERROR_ACCESS_DENIED: std::cout << " (ERROR_ACCESS_DENIED)"; break;
        default: break;
    }
    std::cout << std::endl;
}

// ============================================================
// Main Test
// ============================================================
int main() {
    // Try different device paths
    const char* paths[] = {
        "\\\\.\\DualDesk",
        "\\\\.\\DualDeskDriver",
        "\\\\.\\DualDeskNew",
        "\\\\.\\DualDeskReady",
        "\\\\.\\DualDeskNow"
    };

    HANDLE hDriver = INVALID_HANDLE_VALUE;
    const char* foundPath = nullptr;

    std::cout << "========================================" << std::endl;
    std::cout << "DualDesk Driver Test" << std::endl;
    std::cout << "========================================" << std::endl;

    // Try to open driver
    for (int i = 0; i < 5; i++) {
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
        std::cout << "❌ Failed to open driver. Error: " << GetLastError() << std::endl;
        return 1;
    }

    std::cout << "✅ Opened driver at: " << foundPath << std::endl;
    std::cout << "========================================" << std::endl;

    DWORD bytesReturned = 0;
    BOOL result = FALSE;

    // ============================================================
    // Test 1: Get Status (4 ULONGs output)
    // ============================================================
    std::cout << "\n[Test 1] Getting driver status..." << std::endl;
    
    ULONG statusOutput[4] = {0, 0, 0, 0};
    
    result = DeviceIoControl(
        hDriver,
        IOCTL_DUALDESK_GET_STATUS,
        NULL,
        0,
        statusOutput,
        sizeof(statusOutput),
        &bytesReturned,
        NULL
    );

    if (result) {
        std::cout << "✅ GET_STATUS succeeded!" << std::endl;
        std::cout << "   Device count: " << statusOutput[0] << std::endl;
        std::cout << "   Routed inputs: " << statusOutput[1] << std::endl;
        std::cout << "   Blocked inputs: " << statusOutput[2] << std::endl;
        std::cout << "   Reserved: " << statusOutput[3] << std::endl;
    } else {
        PrintError("GET_STATUS", GetLastError());
    }

    // ============================================================
    // Test 2: Set Route Mode
    // ============================================================
    std::cout << "\n[Test 2] Setting route mode..." << std::endl;
    
    BOOLEAN enable = TRUE;
    result = DeviceIoControl(
        hDriver,
        IOCTL_DUALDESK_SET_ROUTE_MODE,
        &enable,
        sizeof(enable),
        NULL,
        0,
        &bytesReturned,
        NULL
    );

    if (result) {
        std::cout << "✅ SET_ROUTE_MODE succeeded (ENABLED)" << std::endl;
    } else {
        PrintError("SET_ROUTE_MODE", GetLastError());
    }

    // ============================================================
    // Test 3: Assign Device
    // ============================================================
    std::cout << "\n[Test 3] Testing device assignment..." << std::endl;
    
    HANDLE dummyHandle = (HANDLE)0x12345678;
    ULONG assignBuffer[2] = { (ULONG)(ULONG_PTR)dummyHandle, 0 };  // Assign to workspace 0
    
    result = DeviceIoControl(
        hDriver,
        IOCTL_DUALDESK_ASSIGN_DEVICE,
        assignBuffer,
        sizeof(assignBuffer),
        NULL,
        0,
        &bytesReturned,
        NULL
    );

    if (result) {
        std::cout << "✅ ASSIGN_DEVICE succeeded!" << std::endl;
    } else {
        PrintError("ASSIGN_DEVICE", GetLastError());
    }

    // ============================================================
    // Test 4: Get Workspace
    // ============================================================
    std::cout << "\n[Test 4] Getting workspace for device..." << std::endl;
    
    ULONG handleInput = (ULONG)(ULONG_PTR)dummyHandle;
    ULONG workspaceId = 0xFFFFFFFF;
    
    result = DeviceIoControl(
        hDriver,
        IOCTL_DUALDESK_GET_WORKSPACE,
        &handleInput,
        sizeof(handleInput),
        &workspaceId,
        sizeof(workspaceId),
        &bytesReturned,
        NULL
    );

    if (result) {
        std::cout << "✅ GET_WORKSPACE succeeded! Workspace: " << workspaceId << std::endl;
    } else {
        PrintError("GET_WORKSPACE", GetLastError());
    }

    // ============================================================
    // Test 5: Unassign Device
    // ============================================================
    std::cout << "\n[Test 5] Unassigning device..." << std::endl;
    
    result = DeviceIoControl(
        hDriver,
        IOCTL_DUALDESK_UNASSIGN_DEVICE,
        &handleInput,
        sizeof(handleInput),
        NULL,
        0,
        &bytesReturned,
        NULL
    );

    if (result) {
        std::cout << "✅ UNASSIGN_DEVICE succeeded!" << std::endl;
    } else {
        PrintError("UNASSIGN_DEVICE", GetLastError());
    }

    // ============================================================
    // Test 6: Reset All Assignments
    // ============================================================
    std::cout << "\n[Test 6] Resetting all assignments..." << std::endl;
    
    result = DeviceIoControl(
        hDriver,
        IOCTL_DUALDESK_RESET,
        NULL,
        0,
        NULL,
        0,
        &bytesReturned,
        NULL
    );

    if (result) {
        std::cout << "✅ RESET succeeded!" << std::endl;
    } else {
        PrintError("RESET", GetLastError());
    }

    // ============================================================
    // Summary
    // ============================================================
    std::cout << "\n========================================" << std::endl;
    std::cout << "Test Complete!" << std::endl;
    std::cout << "========================================" << std::endl;

    CloseHandle(hDriver);
    return 0;
}