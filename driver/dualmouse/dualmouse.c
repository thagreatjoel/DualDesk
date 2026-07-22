#include "dualmouse.h"
#include <ntddk.h>
#include <wdm.h>
#include <mouclass.h>

// Global driver object
PDRIVER_OBJECT g_DriverObject = NULL;
PDEVICE_OBJECT g_ControlDeviceObject = NULL;

// ============================================================
// DRIVER ENTRY
// ============================================================
NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath) {
    NTSTATUS status;
    PDEVICE_OBJECT deviceObject = NULL;
    UNICODE_STRING deviceName, symbolicLink;
    PDUALMOUSE_DEVICE_EXTENSION extension;
    
    DbgPrint("DualMouse: DriverEntry called\n");
    
    g_DriverObject = DriverObject;
    
    // Create control device for user-mode communication
    RtlInitUnicodeString(&deviceName, L"\\Device\\DualMouse");
    RtlInitUnicodeString(&symbolicLink, L"\\DosDevices\\DualMouse");
    
    status = IoCreateDevice(
        DriverObject,
        sizeof(DUALMOUSE_DEVICE_EXTENSION),
        &deviceName,
        FILE_DEVICE_UNKNOWN,
        FILE_DEVICE_SECURE_OPEN,
        FALSE,
        &deviceObject
    );
    
    if (!NT_SUCCESS(status)) {
        DbgPrint("DualMouse: IoCreateDevice failed (0x%X)\n", status);
        return status;
    }
    
    g_ControlDeviceObject = deviceObject;
    extension = (PDUALMOUSE_DEVICE_EXTENSION)deviceObject->DeviceExtension;
    RtlZeroMemory(extension, sizeof(DUALMOUSE_DEVICE_EXTENSION));
    
    extension->DeviceObject = deviceObject;
    extension->Enabled = TRUE;
    extension->Mouse1Color = RGB(255, 50, 50);   // Red
    extension->Mouse2Color = RGB(50, 150, 255);  // Blue
    extension->Mouse1X = 500;
    extension->Mouse1Y = 300;
    extension->Mouse2X = 600;
    extension->Mouse2Y = 300;
    
    KeInitializeSpinLock(&extension->Lock);
    
    // Create symbolic link
    status = IoCreateSymbolicLink(&symbolicLink, &deviceName);
    if (!NT_SUCCESS(status)) {
        DbgPrint("DualMouse: IoCreateSymbolicLink failed (0x%X)\n", status);
        IoDeleteDevice(deviceObject);
        return status;
    }
    
    // Set dispatch routines
    DriverObject->MajorFunction[IRP_MJ_CREATE] = DualMouseCreateClose;
    DriverObject->MajorFunction[IRP_MJ_CLOSE] = DualMouseCreateClose;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DualMouseDeviceControl;
    DriverObject->DriverUnload = DualMouseUnload;
    
    // Attach to mouse device
    status = DualMouseAddDevice(DriverObject, NULL);
    
    DbgPrint("DualMouse: Driver loaded successfully\n");
    return STATUS_SUCCESS;
}

// ============================================================
// ATTACH TO MOUSE DEVICE
// ============================================================
NTSTATUS DualMouseAddDevice(PDRIVER_OBJECT DriverObject, PDEVICE_OBJECT PhysicalDeviceObject) {
    // Find all mouse devices and attach filter
    // This is a simplified version - in production you'd enumerate all mouse devices
    
    // For now, we'll create a filter device that attaches to the mouse class driver
    // In practice, you'd use IoGetDeviceObjectPointer to get the mouse device
    
    DbgPrint("DualMouse: AddDevice called\n");
    return STATUS_SUCCESS;
}

// ============================================================
// MOUSE FILTER SERVICE CALLBACK
// ============================================================
NTSTATUS DualMouseServiceCallback(
    PDEVICE_OBJECT DeviceObject,
    PMOUSE_INPUT_DATA InputDataStart,
    PMOUSE_INPUT_DATA InputDataEnd,
    PULONG InputDataConsumed
) {
    PDUALMOUSE_DEVICE_EXTENSION extension = 
        (PDUALMOUSE_DEVICE_EXTENSION)DeviceObject->DeviceExtension;
    
    if (!extension || !extension->Enabled) {
        // Pass through
        *InputDataConsumed = (ULONG)(InputDataEnd - InputDataStart);
        return STATUS_SUCCESS;
    }
    
    HANDLE deviceHandle = (HANDLE)DeviceObject->DeviceObjectList.Blink;
    PMOUSE_INPUT_DATA current = InputDataStart;
    ULONG processed = 0;
    
    KeAcquireSpinLock(&extension->Lock, (PKIRQL)NULL);
    
    while (current < InputDataEnd) {
        // Determine which mouse this is
        if (deviceHandle == extension->Mouse1Handle) {
            extension->Mouse1X += current->LastX;
            extension->Mouse1Y += current->LastY;
            extension->Mouse1Active = TRUE;
            
            // Clamp
            if (extension->Mouse1X < 0) extension->Mouse1X = 0;
            if (extension->Mouse1Y < 0) extension->Mouse1Y = 0;
            if (extension->Mouse1X > 1920) extension->Mouse1X = 1920;
            if (extension->Mouse1Y > 1080) extension->Mouse1Y = 1080;
            
            // Route the input - we need to generate a new mouse event
            // This is where you'd inject the routed input
            // For now, we'll just track it
            
            extension->RoutedPackets++;
        } else if (deviceHandle == extension->Mouse2Handle) {
            extension->Mouse2X += current->LastX;
            extension->Mouse2Y += current->LastY;
            extension->Mouse2Active = TRUE;
            
            if (extension->Mouse2X < 0) extension->Mouse2X = 0;
            if (extension->Mouse2Y < 0) extension->Mouse2Y = 0;
            if (extension->Mouse2X > 1920) extension->Mouse2X = 1920;
            if (extension->Mouse2Y > 1080) extension->Mouse2Y = 1080;
            
            extension->RoutedPackets++;
        } else {
            // Unknown mouse - pass through
        }
        
        extension->TotalPackets++;
        current++;
        processed++;
    }
    
    KeReleaseSpinLock(&extension->Lock, (PKIRQL)NULL);
    
    *InputDataConsumed = processed;
    return STATUS_SUCCESS;
}

// ============================================================
// DEVICE CONTROL (IOCTL Handler)
// ============================================================
NTSTATUS DualMouseDeviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
    PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(Irp);
    PDUALMOUSE_DEVICE_EXTENSION extension = 
        (PDUALMOUSE_DEVICE_EXTENSION)DeviceObject->DeviceExtension;
    NTSTATUS status = STATUS_SUCCESS;
    ULONG info = 0;
    
    switch (stack->Parameters.DeviceIoControl.IoControlCode) {
        case IOCTL_DUALMOUSE_ENABLE:
            extension->Enabled = TRUE;
            DbgPrint("DualMouse: Enabled\n");
            break;
            
        case IOCTL_DUALMOUSE_DISABLE:
            extension->Enabled = FALSE;
            DbgPrint("DualMouse: Disabled\n");
            break;
            
        case IOCTL_DUALMOUSE_SET_COLOR: {
            if (stack->Parameters.DeviceIoControl.InputBufferLength >= sizeof(ULONG) * 3) {
                PULONG colors = (PULONG)Irp->AssociatedIrp.SystemBuffer;
                extension->Mouse1Color = colors[0];
                extension->Mouse2Color = colors[1];
                DbgPrint("DualMouse: Colors set - 1:%X 2:%X\n", colors[0], colors[1]);
            }
            break;
        }
            
        case IOCTL_DUALMOUSE_GET_STATUS: {
            if (stack->Parameters.DeviceIoControl.OutputBufferLength >= sizeof(ULONG) * 6) {
                PULONG statusData = (PULONG)Irp->AssociatedIrp.SystemBuffer;
                statusData[0] = extension->Enabled ? 1 : 0;
                statusData[1] = extension->TotalPackets;
                statusData[2] = extension->RoutedPackets;
                statusData[3] = extension->Mouse1Active ? 1 : 0;
                statusData[4] = extension->Mouse2Active ? 1 : 0;
                statusData[5] = extension->Mouse1Active && extension->Mouse2Active ? 1 : 0;
                info = sizeof(ULONG) * 6;
            }
            break;
        }
            
        default:
            status = STATUS_INVALID_DEVICE_REQUEST;
            break;
    }
    
    Irp->IoStatus.Status = status;
    Irp->IoStatus.Information = info;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return status;
}

// ============================================================
// CREATE/CLOSE HANDLERS
// ============================================================
NTSTATUS DualMouseCreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}

// ============================================================
// DRIVER UNLOAD
// ============================================================
VOID DualMouseUnload(PDRIVER_OBJECT DriverObject) {
    UNICODE_STRING symbolicLink;
    RtlInitUnicodeString(&symbolicLink, L"\\DosDevices\\DualMouse");
    IoDeleteSymbolicLink(&symbolicLink);
    
    if (g_ControlDeviceObject) {
        IoDeleteDevice(g_ControlDeviceObject);
    }
    
    DbgPrint("DualMouse: Driver unloaded\n");
}

// ============================================================
// RGB MACRO (for driver)
// ============================================================
#ifndef RGB
#define RGB(r,g,b) ((ULONG)(((BYTE)(r)|((WORD)((BYTE)(g))<<8))|(((DWORD)(BYTE)(b))<<16)))
#endif