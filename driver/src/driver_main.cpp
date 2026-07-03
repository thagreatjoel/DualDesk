#include <ntddk.h>

#define DEVICE_NAME L"\\Device\\DualDesk"
#define DOS_DEVICE_NAME L"\\DosDevices\\DualDesk"

VOID DriverUnload(PDRIVER_OBJECT DriverObject) {
    UNICODE_STRING dosDeviceName;
    RtlInitUnicodeString(&dosDeviceName, DOS_DEVICE_NAME);
    DbgPrint("DualDesk: Unloading...\n");
    IoDeleteSymbolicLink(&dosDeviceName);
    if (DriverObject->DeviceObject) {
        IoDeleteDevice(DriverObject->DeviceObject);
    }
}

NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath) {
    NTSTATUS status;
    PDEVICE_OBJECT deviceObject = NULL;
    UNICODE_STRING deviceName, dosDeviceName;

    UNREFERENCED_PARAMETER(RegistryPath);

    DbgPrint("DualDesk: Loading...\n");

    RtlInitUnicodeString(&deviceName, DEVICE_NAME);
    RtlInitUnicodeString(&dosDeviceName, DOS_DEVICE_NAME);

    status = IoCreateDevice(DriverObject, 0, &deviceName, FILE_DEVICE_UNKNOWN, 0, FALSE, &deviceObject);
    if (!NT_SUCCESS(status)) {
        DbgPrint("DualDesk: IoCreateDevice failed 0x%X\n", status);
        return status;
    }

    status = IoCreateSymbolicLink(&dosDeviceName, &deviceName);
    if (!NT_SUCCESS(status)) {
        DbgPrint("DualDesk: IoCreateSymbolicLink failed 0x%X\n", status);
        IoDeleteDevice(deviceObject);
        return status;
    }

    DriverObject->DriverUnload = DriverUnload;
    deviceObject->Flags &= ~DO_DEVICE_INITIALIZING;

    DbgPrint("DualDesk: Loaded successfully!\n");
    return STATUS_SUCCESS;
}