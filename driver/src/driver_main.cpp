#include <ntddk.h>
#include <wdm.h>
#include <kbdclass.h>
#include <mouclass.h>
#include <ntstrsafe.h>

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
#define IOCTL_DUALDESK_REGISTER_FILTER \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x806, METHOD_BUFFERED, FILE_ANY_ACCESS)

// ============================================================
// ADD: DUAL MOUSE IOCTL CODES
// ============================================================
#define IOCTL_DUALMOUSE_ENABLE \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x900, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_DUALMOUSE_DISABLE \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x901, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_DUALMOUSE_SET_COLORS \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x902, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_DUALMOUSE_GET_POSITIONS \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x903, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_DUALMOUSE_SET_DEVICE \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x904, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_DUALMOUSE_GET_STATUS \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x905, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define WORKSPACE_UNASSIGNED 0xFFFFFFFF
#define MAX_DEVICES 64
#define MAX_FILTERS 32

#define DEVICE_TYPE_KEYBOARD 1
#define DEVICE_TYPE_MOUSE 2

// ============================================================
// DUAL MOUSE DATA STRUCTURES
// ============================================================
typedef struct _DUALMOUSE_POSITIONS {
    LONG X1;
    LONG Y1;
    LONG X2;
    LONG Y2;
} DUALMOUSE_POSITIONS, *PDUALMOUSE_POSITIONS;

typedef struct _DUALMOUSE_COLORS {
    ULONG Color1;
    ULONG Color2;
} DUALMOUSE_COLORS, *PDUALMOUSE_COLORS;

typedef struct _DUALMOUSE_DEVICE_ASSIGN {
    HANDLE DeviceHandle;
    ULONG DeviceNumber;
} DUALMOUSE_DEVICE_ASSIGN, *PDUALMOUSE_DEVICE_ASSIGN;

typedef struct _DUALMOUSE_STATUS {
    BOOLEAN Enabled;
    ULONG TotalPackets;
    ULONG RoutedPackets;
    BOOLEAN Device1Active;
    BOOLEAN Device2Active;
} DUALMOUSE_STATUS, *PDUALMOUSE_STATUS;

// ============================================================
// DEVICE EXTENSION (Add DualMouse field)
// ============================================================
typedef struct _DEVICE_EXTENSION {
    PDEVICE_OBJECT pDeviceObject;
    BOOLEAN routeModeEnabled;
    BOOLEAN filtersRegistered;
    HANDLE deviceHandles[MAX_DEVICES];
    ULONG workspaceIds[MAX_DEVICES];
    ULONG deviceTypes[MAX_DEVICES];
    ULONG deviceCount;
    ULONG blockedInputs;
    ULONG routedInputs;
    ULONG totalInputs;
    KSPIN_LOCK lock;
    
    PDEVICE_OBJECT keyboardFilterDevice;
    PDEVICE_OBJECT mouseFilterDevice;
    PDEVICE_OBJECT keyboardTargetDevice;
    PDEVICE_OBJECT mouseTargetDevice;
    
    PKEYBOARD_CALLBACK keyboardCallback;
    PMOUSE_CALLBACK mouseCallback;
    
    // ============================================================
    // ADD: DUAL MOUSE CONTEXT
    // ============================================================
    struct {
        BOOLEAN Enabled;
        LONG Cursor1X;
        LONG Cursor1Y;
        LONG Cursor2X;
        LONG Cursor2Y;
        ULONG Cursor1Color;
        ULONG Cursor2Color;
        HANDLE Device1Handle;
        HANDLE Device2Handle;
        BOOLEAN Device1Assigned;
        BOOLEAN Device2Assigned;
        ULONG TotalPackets;
        ULONG RoutedPackets;
    } DualMouse;
    
} DEVICE_EXTENSION, *PDEVICE_EXTENSION;

static PDEVICE_OBJECT g_pDeviceObject = NULL;

// ============================================================
// EXISTING FILTER CALLBACKS (Keep as is)
// ============================================================
VOID KeyboardFilterCallback(
    PDEVICE_OBJECT DeviceObject,
    PKEYBOARD_INPUT_DATA InputDataStart,
    PKEYBOARD_INPUT_DATA InputDataEnd,
    PULONG InputDataConsumed
) {
    // ... EXISTING CODE - Keep unchanged ...
    PDEVICE_EXTENSION pExt = (PDEVICE_EXTENSION)g_pDeviceObject->DeviceExtension;
    if (!pExt) {
        *InputDataConsumed = (ULONG)((ULONG_PTR)InputDataEnd - (ULONG_PTR)InputDataStart) / sizeof(KEYBOARD_INPUT_DATA);
        return;
    }
    
    PFILE_OBJECT fileObject = IoGetCurrentIrpStackLocation(IoGetCurrentIrp(DeviceObject))->FileObject;
    HANDLE deviceHandle = NULL;
    if (fileObject) {
        deviceHandle = (HANDLE)fileObject;
    }
    
    ULONG workspaceId = WORKSPACE_UNASSIGNED;
    KeAcquireSpinLock(&pExt->lock, (PKIRQL)NULL);
    
    for (ULONG i = 0; i < pExt->deviceCount; i++) {
        if (pExt->deviceHandles[i] == deviceHandle && pExt->deviceTypes[i] == DEVICE_TYPE_KEYBOARD) {
            workspaceId = pExt->workspaceIds[i];
            break;
        }
    }
    
    KeReleaseSpinLock(&pExt->lock, (PKIRQL)NULL);
    
    PKEYBOARD_INPUT_DATA current = InputDataStart;
    ULONG processedCount = 0;
    ULONG blockedCount = 0;
    ULONG routedCount = 0;
    
    while (current < InputDataEnd) {
        if (pExt->routeModeEnabled) {
            if (workspaceId != WORKSPACE_UNASSIGNED) {
                routedCount++;
            } else {
                blockedCount++;
            }
        } else {
            routedCount++;
        }
        
        InterlockedIncrement(&pExt->totalInputs);
        current++;
        processedCount++;
    }
    
    InterlockedAdd(&pExt->routedInputs, routedCount);
    InterlockedAdd(&pExt->blockedInputs, blockedCount);
    
    *InputDataConsumed = processedCount;
}

VOID MouseFilterCallback(
    PDEVICE_OBJECT DeviceObject,
    PMOUSE_INPUT_DATA InputDataStart,
    PMOUSE_INPUT_DATA InputDataEnd,
    PULONG InputDataConsumed
) {
    PDEVICE_EXTENSION pExt = (PDEVICE_EXTENSION)g_pDeviceObject->DeviceExtension;
    if (!pExt) {
        *InputDataConsumed = (ULONG)((ULONG_PTR)InputDataEnd - (ULONG_PTR)InputDataStart) / sizeof(MOUSE_INPUT_DATA);
        return;
    }
    
    PFILE_OBJECT fileObject = IoGetCurrentIrpStackLocation(IoGetCurrentIrp(DeviceObject))->FileObject;
    HANDLE deviceHandle = NULL;
    if (fileObject) {
        deviceHandle = (HANDLE)fileObject;
    }
    
    // ============================================================
    // ADD: DUAL MOUSE TRACKING
    // ============================================================
    if (pExt->DualMouse.Enabled) {
        // Track mouse movements for dual cursor
        PMOUSE_INPUT_DATA current = InputDataStart;
        ULONG processed = 0;
        
        while (current < InputDataEnd) {
            if (deviceHandle == pExt->DualMouse.Device1Handle) {
                pExt->DualMouse.Cursor1X += current->LastX;
                pExt->DualMouse.Cursor1Y += current->LastY;
                pExt->DualMouse.Device1Assigned = TRUE;
                pExt->DualMouse.TotalPackets++;
            } else if (deviceHandle == pExt->DualMouse.Device2Handle) {
                pExt->DualMouse.Cursor2X += current->LastX;
                pExt->DualMouse.Cursor2Y += current->LastY;
                pExt->DualMouse.Device2Assigned = TRUE;
                pExt->DualMouse.TotalPackets++;
            }
            current++;
            processed++;
        }
        *InputDataConsumed = processed;
        return;
    }
    
    // ============================================================
    // EXISTING CODE CONTINUES
    // ============================================================
    ULONG workspaceId = WORKSPACE_UNASSIGNED;
    KeAcquireSpinLock(&pExt->lock, (PKIRQL)NULL);
    
    for (ULONG i = 0; i < pExt->deviceCount; i++) {
        if (pExt->deviceHandles[i] == deviceHandle && pExt->deviceTypes[i] == DEVICE_TYPE_MOUSE) {
            workspaceId = pExt->workspaceIds[i];
            break;
        }
    }
    
    KeReleaseSpinLock(&pExt->lock, (PKIRQL)NULL);
    
    PMOUSE_INPUT_DATA current = InputDataStart;
    ULONG processedCount = 0;
    ULONG blockedCount = 0;
    ULONG routedCount = 0;
    
    while (current < InputDataEnd) {
        if (pExt->routeModeEnabled) {
            if (workspaceId != WORKSPACE_UNASSIGNED) {
                routedCount++;
            } else {
                blockedCount++;
            }
        } else {
            routedCount++;
        }
        
        InterlockedIncrement(&pExt->totalInputs);
        current++;
        processedCount++;
    }
    
    InterlockedAdd(&pExt->routedInputs, routedCount);
    InterlockedAdd(&pExt->blockedInputs, blockedCount);
    
    *InputDataConsumed = processedCount;
}

// ============================================================
// EXISTING REGISTER FUNCTIONS (Keep as is)
// ============================================================
NTSTATUS RegisterKeyboardFilter(PDEVICE_EXTENSION pExt) {
    // ... EXISTING CODE - Keep unchanged ...
    NTSTATUS status;
    UNICODE_STRING filterName;
    WCHAR filterNameBuffer[64];
    
    DbgPrint("DualDesk: Registering keyboard filter...\n");
    
    RtlStringCbPrintfW(filterNameBuffer, sizeof(filterNameBuffer), 
                       L"\\Device\\DualDeskKeyboardFilter");
    RtlInitUnicodeString(&filterName, filterNameBuffer);
    
    status = IoCreateDevice(
        g_pDeviceObject->DriverObject,
        0,
        &filterName,
        FILE_DEVICE_KEYBOARD,
        0,
        FALSE,
        &pExt->keyboardFilterDevice
    );
    
    if (!NT_SUCCESS(status)) {
        DbgPrint("DualDesk: Failed to create keyboard filter device: 0x%X\n", status);
        return status;
    }
    
    pExt->keyboardFilterDevice->Flags |= DO_BUFFERED_IO;
    pExt->keyboardFilterDevice->StackSize = 1;
    
    UNICODE_STRING kbdClassName;
    RtlInitUnicodeString(&kbdClassName, L"\\Device\\KeyboardClass0");
    
    status = IoGetDeviceObjectPointer(&kbdClassName, FILE_READ_ATTRIBUTES, 
                                      &pExt->keyboardTargetDevice, NULL);
    
    if (!NT_SUCCESS(status)) {
        DbgPrint("DualDesk: Failed to get keyboard target device: 0x%X\n", status);
        IoDeleteDevice(pExt->keyboardFilterDevice);
        pExt->keyboardFilterDevice = NULL;
        return status;
    }
    
    PDEVICE_OBJECT attachedDevice = IoAttachDeviceToDeviceStack(
        pExt->keyboardFilterDevice,
        pExt->keyboardTargetDevice
    );
    
    if (!attachedDevice) {
        DbgPrint("DualDesk: Failed to attach keyboard filter\n");
        ObDereferenceObject(pExt->keyboardTargetDevice);
        IoDeleteDevice(pExt->keyboardFilterDevice);
        pExt->keyboardFilterDevice = NULL;
        return STATUS_UNSUCCESSFUL;
    }
    
    DbgPrint("DualDesk: Keyboard filter registered successfully\n");
    return STATUS_SUCCESS;
}

NTSTATUS RegisterMouseFilter(PDEVICE_EXTENSION pExt) {
    // ... EXISTING CODE - Keep unchanged ...
    NTSTATUS status;
    UNICODE_STRING filterName;
    WCHAR filterNameBuffer[64];
    
    DbgPrint("DualDesk: Registering mouse filter...\n");
    
    RtlStringCbPrintfW(filterNameBuffer, sizeof(filterNameBuffer), 
                       L"\\Device\\DualDeskMouseFilter");
    RtlInitUnicodeString(&filterName, filterNameBuffer);
    
    status = IoCreateDevice(
        g_pDeviceObject->DriverObject,
        0,
        &filterName,
        FILE_DEVICE_MOUSE,
        0,
        FALSE,
        &pExt->mouseFilterDevice
    );
    
    if (!NT_SUCCESS(status)) {
        DbgPrint("DualDesk: Failed to create mouse filter device: 0x%X\n", status);
        return status;
    }
    
    pExt->mouseFilterDevice->Flags |= DO_BUFFERED_IO;
    pExt->mouseFilterDevice->StackSize = 1;
    
    UNICODE_STRING mouClassName;
    RtlInitUnicodeString(&mouClassName, L"\\Device\\MouseClass0");
    
    status = IoGetDeviceObjectPointer(&mouClassName, FILE_READ_ATTRIBUTES, 
                                      &pExt->mouseTargetDevice, NULL);
    
    if (!NT_SUCCESS(status)) {
        DbgPrint("DualDesk: Failed to get mouse target device: 0x%X\n", status);
        IoDeleteDevice(pExt->mouseFilterDevice);
        pExt->mouseFilterDevice = NULL;
        return status;
    }
    
    PDEVICE_OBJECT attachedDevice = IoAttachDeviceToDeviceStack(
        pExt->mouseFilterDevice,
        pExt->mouseTargetDevice
    );
    
    if (!attachedDevice) {
        DbgPrint("DualDesk: Failed to attach mouse filter\n");
        ObDereferenceObject(pExt->mouseTargetDevice);
        IoDeleteDevice(pExt->mouseFilterDevice);
        pExt->mouseFilterDevice = NULL;
        return STATUS_UNSUCCESSFUL;
    }
    
    DbgPrint("DualDesk: Mouse filter registered successfully\n");
    return STATUS_SUCCESS;
}

// ============================================================
// EXISTING HELPER FUNCTIONS (Keep as is)
// ============================================================
ULONG FindDeviceWorkspace(PDEVICE_EXTENSION pExt, HANDLE deviceHandle) {
    if (!pExt) return WORKSPACE_UNASSIGNED;
    
    KeAcquireSpinLock(&pExt->lock, (PKIRQL)NULL);
    for (ULONG i = 0; i < pExt->deviceCount; i++) {
        if (pExt->deviceHandles[i] == deviceHandle) {
            ULONG workspaceId = pExt->workspaceIds[i];
            KeReleaseSpinLock(&pExt->lock, (PKIRQL)NULL);
            return workspaceId;
        }
    }
    KeReleaseSpinLock(&pExt->lock, (PKIRQL)NULL);
    return WORKSPACE_UNASSIGNED;
}

NTSTATUS AddDeviceMapping(PDEVICE_EXTENSION pExt, HANDLE deviceHandle, ULONG workspaceId, ULONG deviceType) {
    if (!pExt) return STATUS_INVALID_PARAMETER;
    
    KeAcquireSpinLock(&pExt->lock, (PKIRQL)NULL);
    
    for (ULONG i = 0; i < pExt->deviceCount; i++) {
        if (pExt->deviceHandles[i] == deviceHandle) {
            pExt->workspaceIds[i] = workspaceId;
            pExt->deviceTypes[i] = deviceType;
            KeReleaseSpinLock(&pExt->lock, (PKIRQL)NULL);
            return STATUS_SUCCESS;
        }
    }
    
    if (pExt->deviceCount >= MAX_DEVICES) {
        KeReleaseSpinLock(&pExt->lock, (PKIRQL)NULL);
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    
    pExt->deviceHandles[pExt->deviceCount] = deviceHandle;
    pExt->workspaceIds[pExt->deviceCount] = workspaceId;
    pExt->deviceTypes[pExt->deviceCount] = deviceType;
    pExt->deviceCount++;
    
    KeReleaseSpinLock(&pExt->lock, (PKIRQL)NULL);
    
    DbgPrint("DualDesk: Added device %p (type %lu) to workspace %lu\n", 
             deviceHandle, deviceType, workspaceId);
    
    return STATUS_SUCCESS;
}

NTSTATUS RemoveDeviceMapping(PDEVICE_EXTENSION pExt, HANDLE deviceHandle) {
    if (!pExt) return STATUS_INVALID_PARAMETER;
    
    KeAcquireSpinLock(&pExt->lock, (PKIRQL)NULL);
    
    for (ULONG i = 0; i < pExt->deviceCount; i++) {
        if (pExt->deviceHandles[i] == deviceHandle) {
            for (ULONG j = i; j < pExt->deviceCount - 1; j++) {
                pExt->deviceHandles[j] = pExt->deviceHandles[j + 1];
                pExt->workspaceIds[j] = pExt->workspaceIds[j + 1];
                pExt->deviceTypes[j] = pExt->deviceTypes[j + 1];
            }
            pExt->deviceCount--;
            KeReleaseSpinLock(&pExt->lock, (PKIRQL)NULL);
            return STATUS_SUCCESS;
        }
    }
    
    KeReleaseSpinLock(&pExt->lock, (PKIRQL)NULL);
    return STATUS_NOT_FOUND;
}

// ============================================================
// DISPATCH FUNCTIONS
// ============================================================
NTSTATUS DispatchCreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
    UNREFERENCED_PARAMETER(DeviceObject);
    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}

// ============================================================
// DISPATCH DEVICE CONTROL - ADD DUAL MOUSE IOCTLS
// ============================================================
NTSTATUS DispatchDeviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
    PIO_STACK_LOCATION pStack = IoGetCurrentIrpStackLocation(Irp);
    ULONG controlCode = pStack->Parameters.DeviceIoControl.IoControlCode;
    PDEVICE_EXTENSION pExt = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;
    NTSTATUS status = STATUS_SUCCESS;
    ULONG information = 0;
    
    DbgPrint("DualDesk: IOCTL 0x%X received\n", controlCode);
    
    switch (controlCode) {
        // ============================================================
        // EXISTING IOCTLS (Keep as is)
        // ============================================================
        case IOCTL_DUALDESK_ASSIGN_DEVICE: {
            if (pStack->Parameters.DeviceIoControl.InputBufferLength < sizeof(ULONG) * 3) {
                status = STATUS_INVALID_PARAMETER;
                break;
            }
            
            ULONG* buffer = (ULONG*)Irp->AssociatedIrp.SystemBuffer;
            HANDLE deviceHandle = (HANDLE)buffer[0];
            ULONG workspaceId = buffer[1];
            ULONG deviceType = buffer[2];
            
            status = AddDeviceMapping(pExt, deviceHandle, workspaceId, deviceType);
            DbgPrint("DualDesk: ASSIGN device %p -> workspace %lu (type %lu, status: %X)\n", 
                     deviceHandle, workspaceId, deviceType, status);
            break;
        }
        
        case IOCTL_DUALDESK_UNASSIGN_DEVICE: {
            if (pStack->Parameters.DeviceIoControl.InputBufferLength < sizeof(ULONG)) {
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
            if (pStack->Parameters.DeviceIoControl.InputBufferLength < sizeof(ULONG) ||
                pStack->Parameters.DeviceIoControl.OutputBufferLength < sizeof(ULONG)) {
                status = STATUS_INVALID_PARAMETER;
                break;
            }
            
            ULONG deviceHandleUlong = *(ULONG*)Irp->AssociatedIrp.SystemBuffer;
            HANDLE deviceHandle = (HANDLE)(ULONG_PTR)deviceHandleUlong;
            
            ULONG workspaceId = FindDeviceWorkspace(pExt, deviceHandle);
            *(ULONG*)Irp->AssociatedIrp.SystemBuffer = workspaceId;
            information = sizeof(ULONG);
            
            DbgPrint("DualDesk: GET_WORKSPACE device %p -> workspace %lu\n", deviceHandle, workspaceId);
            break;
        }
        
        case IOCTL_DUALDESK_SET_ROUTE_MODE: {
            if (pStack->Parameters.DeviceIoControl.InputBufferLength < sizeof(BOOLEAN)) {
                status = STATUS_INVALID_PARAMETER;
                break;
            }
            
            BOOLEAN enable = *(BOOLEAN*)Irp->AssociatedIrp.SystemBuffer;
            pExt->routeModeEnabled = enable;
            DbgPrint("DualDesk: ROUTE MODE %s\n", enable ? "ENABLED" : "DISABLED");
            break;
        }
        
        case IOCTL_DUALDESK_RESET: {
            KeAcquireSpinLock(&pExt->lock, (PKIRQL)NULL);
            pExt->deviceCount = 0;
            pExt->routeModeEnabled = FALSE;
            pExt->blockedInputs = 0;
            pExt->routedInputs = 0;
            pExt->totalInputs = 0;
            for (int i = 0; i < MAX_DEVICES; i++) {
                pExt->deviceHandles[i] = NULL;
                pExt->workspaceIds[i] = WORKSPACE_UNASSIGNED;
                pExt->deviceTypes[i] = 0;
            }
            KeReleaseSpinLock(&pExt->lock, (PKIRQL)NULL);
            
            DbgPrint("DualDesk: RESET all mappings\n");
            break;
        }
        
        case IOCTL_DUALDESK_GET_STATUS: {
            if (pStack->Parameters.DeviceIoControl.OutputBufferLength < sizeof(ULONG) * 5) {
                status = STATUS_BUFFER_TOO_SMALL;
                break;
            }
            
            ULONG* output = (ULONG*)Irp->AssociatedIrp.SystemBuffer;
            output[0] = pExt->deviceCount;
            output[1] = pExt->routedInputs;
            output[2] = pExt->blockedInputs;
            output[3] = pExt->totalInputs;
            output[4] = pExt->filtersRegistered ? 1 : 0;
            
            information = sizeof(ULONG) * 5;
            break;
        }
        
        case IOCTL_DUALDESK_REGISTER_FILTER: {
            if (pExt->filtersRegistered) {
                DbgPrint("DualDesk: Filters already registered\n");
                break;
            }
            
            status = RegisterKeyboardFilter(pExt);
            if (NT_SUCCESS(status)) {
                status = RegisterMouseFilter(pExt);
                if (NT_SUCCESS(status)) {
                    pExt->filtersRegistered = TRUE;
                    DbgPrint("DualDesk: Filters registered successfully!\n");
                }
            }
            
            if (!NT_SUCCESS(status)) {
                DbgPrint("DualDesk: Filter registration failed: 0x%X\n", status);
            }
            break;
        }
        
        // ============================================================
        // ADD: DUAL MOUSE IOCTL HANDLERS
        // ============================================================
        case IOCTL_DUALMOUSE_ENABLE: {
            pExt->DualMouse.Enabled = TRUE;
            DbgPrint("DualDesk: Dual Mouse ENABLED\n");
            status = STATUS_SUCCESS;
            break;
        }
        
        case IOCTL_DUALMOUSE_DISABLE: {
            pExt->DualMouse.Enabled = FALSE;
            DbgPrint("DualDesk: Dual Mouse DISABLED\n");
            status = STATUS_SUCCESS;
            break;
        }
        
        case IOCTL_DUALMOUSE_SET_COLORS: {
            if (pStack->Parameters.DeviceIoControl.InputBufferLength < sizeof(DUALMOUSE_COLORS)) {
                status = STATUS_INVALID_PARAMETER;
                break;
            }
            
            PDUALMOUSE_COLORS colors = (PDUALMOUSE_COLORS)Irp->AssociatedIrp.SystemBuffer;
            pExt->DualMouse.Cursor1Color = colors->Color1;
            pExt->DualMouse.Cursor2Color = colors->Color2;
            DbgPrint("DualDesk: Dual Mouse colors set: 1=0x%X, 2=0x%X\n", 
                     colors->Color1, colors->Color2);
            status = STATUS_SUCCESS;
            break;
        }
        
        case IOCTL_DUALMOUSE_GET_POSITIONS: {
            if (pStack->Parameters.DeviceIoControl.OutputBufferLength < sizeof(DUALMOUSE_POSITIONS)) {
                status = STATUS_BUFFER_TOO_SMALL;
                break;
            }
            
            PDUALMOUSE_POSITIONS pos = (PDUALMOUSE_POSITIONS)Irp->AssociatedIrp.SystemBuffer;
            pos->X1 = pExt->DualMouse.Cursor1X;
            pos->Y1 = pExt->DualMouse.Cursor1Y;
            pos->X2 = pExt->DualMouse.Cursor2X;
            pos->Y2 = pExt->DualMouse.Cursor2Y;
            information = sizeof(DUALMOUSE_POSITIONS);
            DbgPrint("DualDesk: Dual Mouse positions: (%d,%d) (%d,%d)\n", 
                     pos->X1, pos->Y1, pos->X2, pos->Y2);
            status = STATUS_SUCCESS;
            break;
        }
        
        case IOCTL_DUALMOUSE_SET_DEVICE: {
            if (pStack->Parameters.DeviceIoControl.InputBufferLength < sizeof(DUALMOUSE_DEVICE_ASSIGN)) {
                status = STATUS_INVALID_PARAMETER;
                break;
            }
            
            PDUALMOUSE_DEVICE_ASSIGN assign = (PDUALMOUSE_DEVICE_ASSIGN)Irp->AssociatedIrp.SystemBuffer;
            if (assign->DeviceNumber == 1) {
                pExt->DualMouse.Device1Handle = assign->DeviceHandle;
                pExt->DualMouse.Device1Assigned = TRUE;
                DbgPrint("DualDesk: Device 1 assigned: %p\n", assign->DeviceHandle);
            } else if (assign->DeviceNumber == 2) {
                pExt->DualMouse.Device2Handle = assign->DeviceHandle;
                pExt->DualMouse.Device2Assigned = TRUE;
                DbgPrint("DualDesk: Device 2 assigned: %p\n", assign->DeviceHandle);
            } else {
                status = STATUS_INVALID_PARAMETER;
            }
            break;
        }
        
        case IOCTL_DUALMOUSE_GET_STATUS: {
            if (pStack->Parameters.DeviceIoControl.OutputBufferLength < sizeof(DUALMOUSE_STATUS)) {
                status = STATUS_BUFFER_TOO_SMALL;
                break;
            }
            
            PDUALMOUSE_STATUS dmStatus = (PDUALMOUSE_STATUS)Irp->AssociatedIrp.SystemBuffer;
            dmStatus->Enabled = pExt->DualMouse.Enabled;
            dmStatus->TotalPackets = pExt->DualMouse.TotalPackets;
            dmStatus->RoutedPackets = pExt->DualMouse.RoutedPackets;
            dmStatus->Device1Active = pExt->DualMouse.Device1Assigned;
            dmStatus->Device2Active = pExt->DualMouse.Device2Assigned;
            information = sizeof(DUALMOUSE_STATUS);
            status = STATUS_SUCCESS;
            break;
        }
        
        default: {
            DbgPrint("DualDesk: Unknown IOCTL 0x%X\n", controlCode);
            status = STATUS_INVALID_DEVICE_REQUEST;
            break;
        }
    }
    
    Irp->IoStatus.Status = status;
    Irp->IoStatus.Information = information;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return status;
}

// ============================================================
// DRIVER UNLOAD
// ============================================================
VOID DualDeskDriverUnload(PDRIVER_OBJECT DriverObject) {
    UNICODE_STRING dosDeviceName;
    RtlInitUnicodeString(&dosDeviceName, DOS_DEVICE_NAME);
    
    DbgPrint("DualDesk: Driver unloading\n");
    
    if (g_pDeviceObject) {
        PDEVICE_EXTENSION pExt = (PDEVICE_EXTENSION)g_pDeviceObject->DeviceExtension;
        
        if (pExt->keyboardFilterDevice) {
            if (pExt->keyboardTargetDevice) {
                IoDetachDevice(pExt->keyboardFilterDevice);
                ObDereferenceObject(pExt->keyboardTargetDevice);
            }
            IoDeleteDevice(pExt->keyboardFilterDevice);
        }
        
        if (pExt->mouseFilterDevice) {
            if (pExt->mouseTargetDevice) {
                IoDetachDevice(pExt->mouseFilterDevice);
                ObDereferenceObject(pExt->mouseTargetDevice);
            }
            IoDeleteDevice(pExt->mouseFilterDevice);
        }
        
        IoDeleteSymbolicLink(&dosDeviceName);
        IoDeleteDevice(g_pDeviceObject);
        g_pDeviceObject = NULL;
    }
    
    DbgPrint("DualDesk: Driver unloaded\n");
}

// ============================================================
// DRIVER ENTRY
// ============================================================
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
    RtlZeroMemory(pExt, sizeof(DEVICE_EXTENSION));
    
    pExt->pDeviceObject = deviceObject;
    pExt->routeModeEnabled = FALSE;
    pExt->filtersRegistered = FALSE;
    pExt->deviceCount = 0;
    pExt->blockedInputs = 0;
    pExt->routedInputs = 0;
    pExt->totalInputs = 0;
    pExt->keyboardFilterDevice = NULL;
    pExt->mouseFilterDevice = NULL;
    pExt->keyboardTargetDevice = NULL;
    pExt->mouseTargetDevice = NULL;
    pExt->keyboardCallback = NULL;
    pExt->mouseCallback = NULL;
    
    // ============================================================
    // ADD: Initialize Dual Mouse context
    // ============================================================
    pExt->DualMouse.Enabled = FALSE;
    pExt->DualMouse.Cursor1X = 500;
    pExt->DualMouse.Cursor1Y = 300;
    pExt->DualMouse.Cursor2X = 600;
    pExt->DualMouse.Cursor2Y = 300;
    pExt->DualMouse.Cursor1Color = RGB(255, 50, 50);
    pExt->DualMouse.Cursor2Color = RGB(50, 150, 255);
    pExt->DualMouse.Device1Handle = NULL;
    pExt->DualMouse.Device2Handle = NULL;
    pExt->DualMouse.Device1Assigned = FALSE;
    pExt->DualMouse.Device2Assigned = FALSE;
    pExt->DualMouse.TotalPackets = 0;
    pExt->DualMouse.RoutedPackets = 0;
    
    KeInitializeSpinLock(&pExt->lock);
    
    for (int i = 0; i < MAX_DEVICES; i++) {
        pExt->deviceHandles[i] = NULL;
        pExt->workspaceIds[i] = WORKSPACE_UNASSIGNED;
        pExt->deviceTypes[i] = 0;
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
    
    // Register filters
    status = RegisterKeyboardFilter(pExt);
    if (NT_SUCCESS(status)) {
        status = RegisterMouseFilter(pExt);
        if (NT_SUCCESS(status)) {
            pExt->filtersRegistered = TRUE;
            DbgPrint("DualDesk: Filters registered on load!\n");
        }
    }
    
    DbgPrint("DualDesk: Driver loaded SUCCESSFULLY! (filters: %s)\n", 
             pExt->filtersRegistered ? "REGISTERED" : "NOT REGISTERED");
    
    return STATUS_SUCCESS;
}