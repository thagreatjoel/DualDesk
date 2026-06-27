#pragma once

#include <ntddk.h>

#define DUALDESK_DEVICE_NAME L"\\Device\\DualDesk"
#define DUALDESK_SYMLINK L"\\DosDevices\\DualDesk"

typedef struct _DEVICE_EXTENSION {
    HANDLE hDevice;
    ULONG DeviceId;
    // Add more fields as needed
} DEVICE_EXTENSION, *PDEVICE_EXTENSION;

// Driver functions
DRIVER_INITIALIZE DriverEntry;
DRIVER_UNLOAD DriverUnload;
NTSTATUS OnDeviceAdd(PDRIVER_OBJECT DriverObject, PDEVICE_OBJECT DeviceObject);

// Dispatch functions
NTSTATUS OnCreate(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS OnClose(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS OnDeviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp);