#include <ntddk.h>
#include <wdm.h>

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

#define DUALDESK_DEVICE_NAME L"\\Device\\DualDesk"
#define DUALDESK_DOS_DEVICE_NAME L"\\DosDevices\\DualDesk"

typedef struct _DEVICE_EXTENSION {
    PDEVICE_OBJECT pDeviceObject;
    BOOLEAN routeModeEnabled;
    HANDLE deviceHandles[64];
    ULONG workspaceIds[64];
    ULONG deviceCount;
} DEVICE_EXTENSION, *PDEVICE_EXTENSION;

static PDEVICE_OBJECT g_pDeviceObject = NULL;

NTSTATUS DispatchCreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}

NTSTATUS DispatchDeviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
    PIO_STACK_LOCATION pStack = IoGetCurrentIrpStackLocation(Irp);
    ULONG controlCode = pStack->Parameters.DeviceIoControl.IoControlCode;
    PDEVICE_EXTENSION pExt = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;
    NTSTATUS status = STATUS_SUCCESS;
    ULONG information = 0;

    switch (controlCode) {
        case IOCTL_DUALDESK_ASSIGN_DEVICE: {
            if (pStack->Parameters.DeviceIoControl.InputBufferLength >= sizeof(ULONG) * 2) {
                ULONG* buffer = (ULONG*)Irp->AssociatedIrp.SystemBuffer;
                HANDLE deviceHandle = (HANDLE)buffer[0];
                ULONG workspaceId = buffer[1];
                DbgPrint("DualDesk: Assign device %p to workspace %lu\n", deviceHandle, workspaceId);
                status = STATUS_SUCCESS;
            }
            break;
        }
        case IOCTL_DUALDESK_GET_WORKSPACE: {
            if (pStack->Parameters.DeviceIoControl.OutputBufferLength >= sizeof(ULONG)) {
                ULONG workspaceId = 0;
                *(ULONG*)Irp->AssociatedIrp.SystemBuffer = workspaceId;
                information = sizeof(ULONG);
                status = STATUS_SUCCESS;
            }
            break;
        }
        case IOCTL_DUALDESK_SET_ROUTE_MODE: {
            if (pStack->Parameters.DeviceIoControl.InputBufferLength >= sizeof(BOOLEAN)) {
                BOOLEAN enable = *(BOOLEAN*)Irp->AssociatedIrp.SystemBuffer;
                pExt->routeModeEnabled = enable;
                DbgPrint("DualDesk: Route mode %s\n", enable ? "ENABLED" : "DISABLED");
                status = STATUS_SUCCESS;
            }
            break;
        }
        default:
            status = STATUS_INVALID_DEVICE_REQUEST;
            break;
    }

    Irp->IoStatus.Status = status;
    Irp->IoStatus.Information = information;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return status;
}

NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath) {
    NTSTATUS status;
    PDEVICE_OBJECT pDeviceObject = NULL;
    UNICODE_STRING deviceName, dosDeviceName;
    PDEVICE_EXTENSION pExt;

    DbgPrint("DualDesk: DriverEntry\n");

    RtlInitUnicodeString(&deviceName, DUALDESK_DEVICE_NAME);
    RtlInitUnicodeString(&dosDeviceName, DUALDESK_DOS_DEVICE_NAME);

    status = IoCreateDevice(DriverObject, sizeof(DEVICE_EXTENSION), &deviceName,
                            FILE_DEVICE_UNKNOWN, FILE_DEVICE_SECURE_OPEN, FALSE, &pDeviceObject);
    if (!NT_SUCCESS(status)) {
        DbgPrint("DualDesk: IoCreateDevice failed 0x%X\n", status);
        return status;
    }

    g_pDeviceObject = pDeviceObject;

    pExt = (PDEVICE_EXTENSION)pDeviceObject->DeviceExtension;
    RtlZeroMemory(pExt, sizeof(DEVICE_EXTENSION));
    pExt->pDeviceObject = pDeviceObject;
    pExt->routeModeEnabled = FALSE;

    status = IoCreateSymbolicLink(&dosDeviceName, &deviceName);
    if (!NT_SUCCESS(status)) {
        DbgPrint("DualDesk: IoCreateSymbolicLink failed 0x%X\n", status);
        IoDeleteDevice(pDeviceObject);
        return status;
    }

    DriverObject->MajorFunction[IRP_MJ_CREATE] = DispatchCreateClose;
    DriverObject->MajorFunction[IRP_MJ_CLOSE] = DispatchCreateClose;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DispatchDeviceControl;
    DriverObject->DriverUnload = DriverUnload;

    pDeviceObject->Flags &= ~DO_DEVICE_INITIALIZING;

    DbgPrint("DualDesk: Driver loaded successfully\n");
    return STATUS_SUCCESS;
}

VOID DriverUnload(PDRIVER_OBJECT DriverObject) {
    UNICODE_STRING dosDeviceName;
    RtlInitUnicodeString(&dosDeviceName, DUALDESK_DOS_DEVICE_NAME);

    DbgPrint("DualDesk: Driver unloading\n");

    IoDeleteSymbolicLink(&dosDeviceName);

    if (g_pDeviceObject) {
        IoDeleteDevice(g_pDeviceObject);
        g_pDeviceObject = NULL;
    }

    DbgPrint("DualDesk: Driver unloaded\n");
}