#pragma once

#include <ntddk.h>
#include <wdm.h>

#define DUALDESK_DEVICE_NAME L"\\Device\\DualDesk"
#define DUALDESK_SYMLINK L"\\DosDevices\\DualDesk"

// IOCTL Codes
#define IOCTL_GET_DRIVER_STATUS    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_ROUTE_INPUT          CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_LOCK_WINDOW          CTL_CODE(FILE_DEVICE_UNKNOWN, 0x802, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_CONFINE_MOUSE        CTL_CODE(FILE_DEVICE_UNKNOWN, 0x803, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_SET_MONITOR_BOUNDS   CTL_CODE(FILE_DEVICE_UNKNOWN, 0x804, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_GET_MONITOR_BOUNDS   CTL_CODE(FILE_DEVICE_UNKNOWN, 0x805, METHOD_BUFFERED, FILE_ANY_ACCESS)

typedef struct _DEVICE_EXTENSION {
    PDEVICE_OBJECT DeviceObject;
    UNICODE_STRING DeviceName;
    UNICODE_STRING SymlinkName;
    ULONG DeviceId;
    BOOLEAN IsInputIsolationActive;
    ULONG MonitorCount;
} DEVICE_EXTENSION, *PDEVICE_EXTENSION;

// Status structure
typedef struct _DRIVER_STATUS {
    BOOLEAN IsRunning;
    BOOLEAN IsIsolatingInput;
    ULONG ActiveMonitors;
    ULONG ConnectedKeyboards;
    ULONG ConnectedMice;
} DRIVER_STATUS;

// Monitor bounds
typedef struct _MONITOR_BOUNDS {
    ULONG MonitorId;
    LONG Left;
    LONG Top;
    LONG Right;
    LONG Bottom;
} MONITOR_BOUNDS;

// Input route info
typedef struct _INPUT_ROUTE {
    ULONG DeviceId;
    ULONG MonitorId;
    BOOLEAN IsKeyboard;
    BOOLEAN IsMouse;
} INPUT_ROUTE;

// Driver functions - declared as extern "C" to prevent C++ name mangling
#ifdef __cplusplus
extern "C" {
#endif

NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath);
VOID DriverUnload(PDRIVER_OBJECT DriverObject);

// Dispatch functions
NTSTATUS OnCreate(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS OnClose(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS OnDeviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp);

#ifdef __cplusplus
}
#endif