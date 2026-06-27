#include "dualdesk/driver/driver_common.h"
#include "dualdesk/driver/ioctl_codes.h"
#include <ntddk.h>
#include <wdm.h>

PDEVICE_OBJECT g_DualDeskDevice = nullptr;

NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath) {
    UNREFERENCED_PARAMETER(RegistryPath);
    
    KdPrint(("[DualDesk] DriverEntry called\n"));
    
    // Set driver unload routine
    DriverObject->DriverUnload = DriverUnload;
    
    // Set dispatch routines
    DriverObject->MajorFunction[IRP_MJ_CREATE] = OnCreate;
    DriverObject->MajorFunction[IRP_MJ_CLOSE] = OnClose;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = OnDeviceControl;
    
    // Create device
    NTSTATUS status = OnDeviceAdd(DriverObject, nullptr);
    if (!NT_SUCCESS(status)) {
        KdPrint(("[DualDesk] Failed to create device\n"));
        return status;
    }
    
    KdPrint(("[DualDesk] Driver initialized successfully\n"));
    return STATUS_SUCCESS;
}

VOID DriverUnload(PDRIVER_OBJECT DriverObject) {
    KdPrint(("[DualDesk] Driver unloading...\n"));
    
    UNICODE_STRING symlinkName;
    RtlInitUnicodeString(&symlinkName, DUALDESK_SYMLINK);
    IoDeleteSymbolicLink(&symlinkName);
    
    if (g_DualDeskDevice) {
        IoDeleteDevice(g_DualDeskDevice);
        g_DualDeskDevice = nullptr;
    }
    
    KdPrint(("[DualDesk] Driver unloaded\n"));
}

NTSTATUS OnDeviceAdd(PDRIVER_OBJECT DriverObject, PDEVICE_OBJECT DeviceObject) {
    UNREFERENCED_PARAMETER(DeviceObject);
    
    UNICODE_STRING deviceName, symlinkName;
    RtlInitUnicodeString(&deviceName, DUALDESK_DEVICE_NAME);
    RtlInitUnicodeString(&symlinkName, DUALDESK_SYMLINK);
    
    // Create device
    NTSTATUS status = IoCreateDevice(
        DriverObject,
        sizeof(DEVICE_EXTENSION),
        &deviceName,
        FILE_DEVICE_UNKNOWN,
        0,
        FALSE,
        &g_DualDeskDevice
    );
    
    if (!NT_SUCCESS(status)) {
        KdPrint(("[DualDesk] Failed to create device: 0x%X\n", status));
        return status;
    }
    
    // Create symbolic link
    status = IoCreateSymbolicLink(&symlinkName, &deviceName);
    if (!NT_SUCCESS(status)) {
        KdPrint(("[DualDesk] Failed to create symbolic link: 0x%X\n", status));
        IoDeleteDevice(g_DualDeskDevice);
        g_DualDeskDevice = nullptr;
        return status;
    }
    
    // Set device flags
    g_DualDeskDevice->Flags |= DO_BUFFERED_IO;
    g_DualDeskDevice->Flags &= ~DO_DEVICE_INITIALIZING;
    
    KdPrint(("[DualDesk] Device created successfully\n"));
    return STATUS_SUCCESS;
}