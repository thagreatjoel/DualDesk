#include <ntddk.h>
#include <wdm.h>

#define DEVICE_NAME L"\\Device\\DualDesk"
#define DOS_DEVICE_NAME L"\\DosDevices\\DualDesk"

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

#define WORKSPACE_UNASSIGNED 0xFFFFFFFF
#define MAX_DEVICES 64

typedef struct _DEVICE_EXTENSION {
    PDEVICE_OBJECT pDeviceObject;
    BOOLEAN routeModeEnabled;
    HANDLE deviceHandles[MAX_DEVICES];
    ULONG workspaceIds[MAX_DEVICES];
    ULONG deviceCount;
    ULONG blockedInputs;
    ULONG routedInputs;
} DEVICE_EXTENSION, *PDEVICE_EXTENSION;

static PDEVICE_OBJECT g_pDeviceObject = NULL;

extern "C" {

// Forward declarations
NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath);
VOID DualDeskDriverUnload(PDRIVER_OBJECT DriverObject);
NTSTATUS DispatchCreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS DispatchDeviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp);

// Helper functions
ULONG FindDeviceWorkspace(PDEVICE_EXTENSION pExt, HANDLE deviceHandle) {
    if (!pExt) return WORKSPACE_UNASSIGNED;
    for (ULONG i = 0; i < pExt->deviceCount; i++) {
        if (pExt->deviceHandles[i] == deviceHandle) {
            return pExt->workspaceIds[i];
        }
    }
    return WORKSPACE_UNASSIGNED;
}

NTSTATUS AddDeviceMapping(PDEVICE_EXTENSION pExt, HANDLE deviceHandle, ULONG workspaceId) {
    if (!pExt) return STATUS_INVALID_PARAMETER;
    
    for (ULONG i = 0; i < pExt->deviceCount; i++) {
        if (pExt->deviceHandles[i] == deviceHandle) {
            pExt->workspaceIds[i] = workspaceId;
            return STATUS_SUCCESS;
        }
    }
    
    if (pExt->deviceCount >= MAX_DEVICES) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    
    pExt->deviceHandles[pExt->deviceCount] = deviceHandle;
    pExt->workspaceIds[pExt->deviceCount] = workspaceId;
    pExt->deviceCount++;
    
    return STATUS_SUCCESS;
}

NTSTATUS RemoveDeviceMapping(PDEVICE_EXTENSION pExt, HANDLE deviceHandle) {
    if (!pExt) return STATUS_INVALID_PARAMETER;
    
    for (ULONG i = 0; i < pExt->deviceCount; i++) {
        if (pExt->deviceHandles[i] == deviceHandle) {
            for (ULONG j = i; j < pExt->deviceCount - 1; j++) {
                pExt->deviceHandles[j] = pExt->deviceHandles[j + 1];
                pExt->workspaceIds[j] = pExt->workspaceIds[j + 1];
            }
            pExt->deviceCount--;
            return STATUS_SUCCESS;
        }
    }
    return STATUS_NOT_FOUND;
}

NTSTATUS DispatchCreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
    UNREFERENCED_PARAMETER(DeviceObject);
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
    
    DbgPrint("DualDesk: IOCTL 0x%X received\n", controlCode);
    
    switch (controlCode) {
        case IOCTL_DUALDESK_ASSIGN_DEVICE: {
            DbgPrint("DualDesk: ASSIGN_DEVICE called\n");
            
            if (pStack->Parameters.DeviceIoControl.InputBufferLength < sizeof(ULONG) * 2) {
                DbgPrint("DualDesk: Input buffer too small\n");
                status = STATUS_INVALID_PARAMETER;
                break;
            }
            
            ULONG* buffer = (ULONG*)Irp->AssociatedIrp.SystemBuffer;
            HANDLE deviceHandle = (HANDLE)buffer[0];
            ULONG workspaceId = buffer[1];
            
            status = AddDeviceMapping(pExt, deviceHandle, workspaceId);
            DbgPrint("DualDesk: ASSIGN device %p -> workspace %lu (status: %X)\n", deviceHandle, workspaceId, status);
            break;
        }
        
        case IOCTL_DUALDESK_UNASSIGN_DEVICE: {
            DbgPrint("DualDesk: UNASSIGN_DEVICE called\n");
            
            if (pStack->Parameters.DeviceIoControl.InputBufferLength < sizeof(ULONG)) {
                DbgPrint("DualDesk: Input buffer too small (need %zu, have %lu)\n", 
                    sizeof(ULONG), pStack->Parameters.DeviceIoControl.InputBufferLength);
                status = STATUS_INVALID_PARAMETER;
                break;
            }
            
            ULONG deviceHandleUlong = *(ULONG*)Irp->AssociatedIrp.SystemBuffer;
            HANDLE deviceHandle = (HANDLE)(ULONG_PTR)deviceHandleUlong;
            
            status = RemoveDeviceMapping(pExt, deviceHandle);
            DbgPrint("DualDesk: UNASSIGN device %p (status: %X)\n", deviceHandle, status);
            break;
        }
        
        case IOCTL_DUALDESK_GET_WORKSPACE: {
            DbgPrint("DualDesk: GET_WORKSPACE called\n");
            
            if (pStack->Parameters.DeviceIoControl.InputBufferLength < sizeof(ULONG)) {
                DbgPrint("DualDesk: Input buffer too small (need %zu, have %lu)\n", 
                    sizeof(ULONG), pStack->Parameters.DeviceIoControl.InputBufferLength);
                status = STATUS_INVALID_PARAMETER;
                break;
            }
            
            if (pStack->Parameters.DeviceIoControl.OutputBufferLength < sizeof(ULONG)) {
                DbgPrint("DualDesk: Output buffer too small\n");
                status = STATUS_INVALID_PARAMETER;
                break;
            }
            
            ULONG deviceHandleUlong = *(ULONG*)Irp->AssociatedIrp.SystemBuffer;
            HANDLE deviceHandle = (HANDLE)(ULONG_PTR)deviceHandleUlong;
            
            ULONG workspaceId = FindDeviceWorkspace(pExt, deviceHandle);
            *(ULONG*)Irp->AssociatedIrp.SystemBuffer = workspaceId;
            information = sizeof(ULONG);
            status = STATUS_SUCCESS;
            
            DbgPrint("DualDesk: GET_WORKSPACE device %p -> workspace %lu\n", deviceHandle, workspaceId);
            break;
        }
        
        case IOCTL_DUALDESK_SET_ROUTE_MODE: {
            DbgPrint("DualDesk: SET_ROUTE_MODE called\n");
            
            if (pStack->Parameters.DeviceIoControl.InputBufferLength < sizeof(BOOLEAN)) {
                DbgPrint("DualDesk: Input buffer too small\n");
                status = STATUS_INVALID_PARAMETER;
                break;
            }
            
            BOOLEAN enable = *(BOOLEAN*)Irp->AssociatedIrp.SystemBuffer;
            pExt->routeModeEnabled = enable;
            DbgPrint("DualDesk: ROUTE MODE %s\n", enable ? "ENABLED" : "DISABLED");
            status = STATUS_SUCCESS;
            break;
        }
        
        case IOCTL_DUALDESK_RESET: {
            DbgPrint("DualDesk: RESET called\n");
            
            pExt->deviceCount = 0;
            pExt->routeModeEnabled = FALSE;
            pExt->blockedInputs = 0;
            pExt->routedInputs = 0;
            for (int i = 0; i < MAX_DEVICES; i++) {
                pExt->deviceHandles[i] = NULL;
                pExt->workspaceIds[i] = WORKSPACE_UNASSIGNED;
            }
            
            DbgPrint("DualDesk: RESET all mappings\n");
            status = STATUS_SUCCESS;
            break;
        }
        
        case IOCTL_DUALDESK_GET_STATUS: {
            DbgPrint("DualDesk: GET_STATUS called\n");
            
            if (pStack->Parameters.DeviceIoControl.OutputBufferLength < sizeof(ULONG) * 4) {
                DbgPrint("DualDesk: Output buffer too small (need %zu, have %lu)\n", 
                    sizeof(ULONG) * 4, pStack->Parameters.DeviceIoControl.OutputBufferLength);
                status = STATUS_BUFFER_TOO_SMALL;
                break;
            }
            
            ULONG* output = (ULONG*)Irp->AssociatedIrp.SystemBuffer;
            output[0] = pExt->deviceCount;
            output[1] = pExt->routedInputs;
            output[2] = pExt->blockedInputs;
            output[3] = 0;
            
            information = sizeof(ULONG) * 4;
            status = STATUS_SUCCESS;
            
            DbgPrint("DualDesk: GET_STATUS devices=%lu, routed=%lu, blocked=%lu\n", 
                output[0], output[1], output[2]);
            break;
        }
        
        default: {
            DbgPrint("DualDesk: Unknown IOCTL 0x%X - accepting as SUCCESS\n", controlCode);
            status = STATUS_SUCCESS;
            information = 0;
            break;
        }
    }
    
    Irp->IoStatus.Status = status;
    Irp->IoStatus.Information = information;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return status;
}

VOID DualDeskDriverUnload(PDRIVER_OBJECT DriverObject) {
    UNICODE_STRING dosDeviceName;
    RtlInitUnicodeString(&dosDeviceName, DOS_DEVICE_NAME);
    
    DbgPrint("DualDesk: Driver unloading\n");
    
    IoDeleteSymbolicLink(&dosDeviceName);
    
    if (g_pDeviceObject) {
        IoDeleteDevice(g_pDeviceObject);
        g_pDeviceObject = NULL;
    }
    
    DbgPrint("DualDesk: Driver unloaded\n");
}

NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath) {
    NTSTATUS status;
    PDEVICE_OBJECT deviceObject = NULL;
    UNICODE_STRING deviceName, dosDeviceName;
    PDEVICE_EXTENSION pExt = NULL;
    
    UNREFERENCED_PARAMETER(RegistryPath);
    
    DbgPrint("DualDesk: DriverEntry called\n");
    
    RtlInitUnicodeString(&deviceName, DEVICE_NAME);
    RtlInitUnicodeString(&dosDeviceName, DOS_DEVICE_NAME);
    
    status = IoCreateDevice(
        DriverObject,
        sizeof(DEVICE_EXTENSION),
        &deviceName,
        FILE_DEVICE_UNKNOWN,
        FILE_DEVICE_SECURE_OPEN,
        FALSE,
        &deviceObject
    );
    
    if (!NT_SUCCESS(status)) {
        DbgPrint("DualDesk: IoCreateDevice failed 0x%X\n", status);
        return status;
    }
    
    g_pDeviceObject = deviceObject;
    
    pExt = (PDEVICE_EXTENSION)deviceObject->DeviceExtension;
    pExt->pDeviceObject = deviceObject;
    pExt->routeModeEnabled = FALSE;
    pExt->deviceCount = 0;
    pExt->blockedInputs = 0;
    pExt->routedInputs = 0;
    for (int i = 0; i < MAX_DEVICES; i++) {
        pExt->deviceHandles[i] = NULL;
        pExt->workspaceIds[i] = WORKSPACE_UNASSIGNED;
    }
    
    status = IoCreateSymbolicLink(&dosDeviceName, &deviceName);
    if (!NT_SUCCESS(status)) {
        DbgPrint("DualDesk: IoCreateSymbolicLink failed 0x%X\n", status);
        IoDeleteDevice(deviceObject);
        return status;
    }
    
    DriverObject->MajorFunction[IRP_MJ_CREATE] = DispatchCreateClose;
    DriverObject->MajorFunction[IRP_MJ_CLOSE] = DispatchCreateClose;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DispatchDeviceControl;
    DriverObject->DriverUnload = DualDeskDriverUnload;
    
    deviceObject->Flags &= ~DO_DEVICE_INITIALIZING;
    
    DbgPrint("DualDesk: Driver loaded SUCCESSFULLY!\n");
    return STATUS_SUCCESS;
}

} // extern "C"