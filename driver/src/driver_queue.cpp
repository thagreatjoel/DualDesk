#include "dualdesk/driver/driver_common.h"
#include "dualdesk/driver/ioctl_codes.h"
#include <ntddk.h>
#include <wdm.h>

// Forward declarations
NTSTATUS HandleGetDevices(PIRP Irp);
NTSTATUS HandleRouteInput(PIRP Irp);

// These are defined in driver_filter.cpp
extern NTSTATUS HandleAssignDevice(PIRP Irp);
extern NTSTATUS HandleLockCursor(PIRP Irp);

NTSTATUS OnDeviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
    UNREFERENCED_PARAMETER(DeviceObject);
    
    PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation(Irp);
    NTSTATUS status = STATUS_INVALID_DEVICE_REQUEST;
    ULONG ioctlCode = irpSp->Parameters.DeviceIoControl.IoControlCode;
    
    switch (ioctlCode) {
        case IOCTL_DUALDESK_GET_DEVICES:
            status = HandleGetDevices(Irp);
            break;
            
        case IOCTL_DUALDESK_ASSIGN_DEVICE:
            status = HandleAssignDevice(Irp);
            break;
            
        case IOCTL_DUALDESK_ROUTE_INPUT:
            status = HandleRouteInput(Irp);
            break;
            
        case IOCTL_DUALDESK_LOCK_CURSOR:
            status = HandleLockCursor(Irp);
            break;
            
        default:
            status = STATUS_INVALID_DEVICE_REQUEST;
            break;
    }
    
    Irp->IoStatus.Status = status;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return status;
}

NTSTATUS HandleGetDevices(PIRP Irp) {
    DbgPrint("[DualDesk] HandleGetDevices called\n");
    return STATUS_SUCCESS;
}

NTSTATUS HandleRouteInput(PIRP Irp) {
    DbgPrint("[DualDesk] HandleRouteInput called\n");
    return STATUS_SUCCESS;
}