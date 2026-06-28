#pragma once

#include <ntddk.h>
#include <wdm.h>

#define DUALDESK_DEVICE_NAME L"\\Device\\DualDesk"
#define DUALDESK_SYMLINK L"\\DosDevices\\DualDesk"

typedef struct _DEVICE_EXTENSION {
    PDEVICE_OBJECT DeviceObject;
    UNICODE_STRING DeviceName;
    UNICODE_STRING SymlinkName;
    ULONG DeviceId;
} DEVICE_EXTENSION, *PDEVICE_EXTENSION;

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