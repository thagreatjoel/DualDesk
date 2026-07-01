#include "dualdesk/driver/driver_common.h"
#include "dualdesk/driver/ioctl_codes.h"
#include <ntddk.h>
#include <wdm.h>

extern PDEVICE_OBJECT g_DualDeskDevice;

// Handle IRP_MJ_CREATE - Application opens device
NTSTATUS OnCreate(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
    UNREFERENCED_PARAMETER(DeviceObject);
    
    DbgPrint("[DualDesk] OnCreate called - Application connected\n");
    
    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    
    return STATUS_SUCCESS;
}

// Handle IRP_MJ_CLOSE - Application closes device
NTSTATUS OnClose(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
    UNREFERENCED_PARAMETER(DeviceObject);
    
    DbgPrint("[DualDesk] OnClose called - Application disconnected\n");
    
    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    
    return STATUS_SUCCESS;
}

// Handle IRP_MJ_DEVICE_CONTROL - IOCTL requests from application
NTSTATUS OnDeviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
    PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(Irp);
    ULONG ioctlCode = stack->Parameters.DeviceIoControl.IoControlCode;
    NTSTATUS status = STATUS_SUCCESS;
    ULONG bytesReturned = 0;
    
    DbgPrint("[DualDesk] OnDeviceControl called - IOCTL: 0x%X\n", ioctlCode);
    
    switch (ioctlCode) {
        case IOCTL_GET_DRIVER_STATUS: {
            DbgPrint("[DualDesk] IOCTL_GET_DRIVER_STATUS\n");
            
            // Get output buffer
            DRIVER_STATUS* output = (DRIVER_STATUS*)Irp->AssociatedIrp.SystemBuffer;
            if (output) {
                output->IsRunning = TRUE;
                output->IsIsolatingInput = TRUE;
                output->ActiveMonitors = 2; // TODO: Get actual monitor count
                output->ConnectedKeyboards = 2;
                output->ConnectedMice = 2;
                bytesReturned = sizeof(DRIVER_STATUS);
                status = STATUS_SUCCESS;
            } else {
                status = STATUS_INSUFFICIENT_RESOURCES;
            }
            break;
        }
        
        case IOCTL_ROUTE_INPUT: {
            DbgPrint("[DualDesk] IOCTL_ROUTE_INPUT\n");
            
            INPUT_ROUTE* input = (INPUT_ROUTE*)Irp->AssociatedIrp.SystemBuffer;
            if (input) {
                DbgPrint("[DualDesk] Route input: DeviceId=%d, MonitorId=%d\n", 
                         input->DeviceId, input->MonitorId);
                // TODO: Actually route the input
                status = STATUS_SUCCESS;
            } else {
                status = STATUS_INVALID_PARAMETER;
            }
            break;
        }
        
        case IOCTL_LOCK_WINDOW: {
            DbgPrint("[DualDesk] IOCTL_LOCK_WINDOW\n");
            // TODO: Lock window to monitor
            status = STATUS_SUCCESS;
            break;
        }
        
        case IOCTL_CONFINE_MOUSE: {
            DbgPrint("[DualDesk] IOCTL_CONFINE_MOUSE\n");
            // TODO: Confine mouse to monitor
            status = STATUS_SUCCESS;
            break;
        }
        
        case IOCTL_SET_MONITOR_BOUNDS: {
            DbgPrint("[DualDesk] IOCTL_SET_MONITOR_BOUNDS\n");
            MONITOR_BOUNDS* bounds = (MONITOR_BOUNDS*)Irp->AssociatedIrp.SystemBuffer;
            if (bounds) {
                DbgPrint("[DualDesk] Monitor %d: (%d,%d)-(%d,%d)\n", 
                         bounds->MonitorId, bounds->Left, bounds->Top, 
                         bounds->Right, bounds->Bottom);
                // TODO: Store monitor bounds
                status = STATUS_SUCCESS;
            } else {
                status = STATUS_INVALID_PARAMETER;
            }
            break;
        }
        
        case IOCTL_GET_MONITOR_BOUNDS: {
            DbgPrint("[DualDesk] IOCTL_GET_MONITOR_BOUNDS\n");
            // TODO: Return monitor bounds
            status = STATUS_SUCCESS;
            break;
        }
        
        default: {
            DbgPrint("[DualDesk] Unknown IOCTL: 0x%X\n", ioctlCode);
            status = STATUS_INVALID_DEVICE_REQUEST;
            break;
        }
    }
    
    // Complete the request
    Irp->IoStatus.Status = status;
    Irp->IoStatus.Information = bytesReturned;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    
    return status;
}