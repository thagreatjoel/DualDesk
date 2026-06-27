// driver/src/driver_filter.cpp
// Kernel-level input filter for DualDesk

#include "dualdesk/driver/driver_common.h"
#include "dualdesk/driver/ioctl_codes.h"
#include <ntddk.h>
#include <wdm.h>
#include <kbdclass.h>
#include <mouclass.h>

// ==================== GLOBALS ====================
// Device assignments (from user mode)
typedef struct _DEVICE_ASSIGNMENT {
    ULONG DeviceId;         // HID device ID
    ULONG WorkspaceId;      // 1 = Laptop, 2 = HDMI TV
    ULONG MonitorIndex;     // 0 = Primary, 1 = Secondary
    BOOLEAN IsKeyboard;     // TRUE = keyboard, FALSE = mouse
    BOOLEAN Active;         // Is this assignment active?
} DEVICE_ASSIGNMENT;

// Global assignment table
#define MAX_DEVICES 32
DEVICE_ASSIGNMENT g_DeviceAssignments[MAX_DEVICES];
ULONG g_NumAssignments = 0;

// Cursor lock state
BOOLEAN g_CursorLocked = FALSE;
ULONG g_LockedMonitor = 0;  // 0 = Primary, 1 = Secondary

// ==================== INPUT FILTER FUNCTIONS ====================

// Called for every keyboard input
NTSTATUS FilterKeyboardInput(PDEVICE_OBJECT DeviceObject, PKEYBOARD_INPUT_DATA InputData) {
    UNREFERENCED_PARAMETER(DeviceObject);
    
    if (!InputData) {
        return STATUS_SUCCESS;
    }
    
    // Get the device handle
    ULONG deviceId = (ULONG)DeviceObject->DeviceExtension;
    
    // Find the assignment for this device
    ULONG workspaceId = 0;
    for (ULONG i = 0; i < g_NumAssignments; i++) {
        if (g_DeviceAssignments[i].DeviceId == deviceId && 
            g_DeviceAssignments[i].IsKeyboard &&
            g_DeviceAssignments[i].Active) {
            workspaceId = g_DeviceAssignments[i].WorkspaceId;
            break;
        }
    }
    
    // If no assignment, use default (Workspace 1 = Primary)
    if (workspaceId == 0) {
        KdPrint(("[DualDesk] Unassigned keyboard input, using Workspace 1\n"));
        workspaceId = 1;
    }
    
    // Route to correct workspace
    KdPrint(("[DualDesk] Keyboard input from Device %lu -> Workspace %lu\n", 
             deviceId, workspaceId));
    
    // In a real implementation, we would:
    // 1. Check if the input should be sent to this workspace
    // 2. If not, drop the input
    // 3. If yes, pass it through
    
    return STATUS_SUCCESS;
}

// Called for every mouse input
NTSTATUS FilterMouseInput(PDEVICE_OBJECT DeviceObject, PMOUSE_INPUT_DATA InputData) {
    UNREFERENCED_PARAMETER(DeviceObject);
    
    if (!InputData) {
        return STATUS_SUCCESS;
    }
    
    // Get the device handle
    ULONG deviceId = (ULONG)DeviceObject->DeviceExtension;
    
    // Find the assignment for this device
    ULONG workspaceId = 0;
    for (ULONG i = 0; i < g_NumAssignments; i++) {
        if (g_DeviceAssignments[i].DeviceId == deviceId && 
            !g_DeviceAssignments[i].IsKeyboard &&
            g_DeviceAssignments[i].Active) {
            workspaceId = g_DeviceAssignments[i].WorkspaceId;
            break;
        }
    }
    
    // If no assignment, use default (Workspace 1 = Primary)
    if (workspaceId == 0) {
        KdPrint(("[DualDesk] Unassigned mouse input, using Workspace 1\n"));
        workspaceId = 1;
    }
    
    // ============ CURSOR LOCKING ============
    // This is where we prevent cursor from crossing screens!
    if (g_CursorLocked) {
        // Get current cursor position
        POINT cursorPos;
        cursorPos.x = InputData->LastX;
        cursorPos.y = InputData->LastY;
        
        // Get monitor bounds
        // In a real implementation, we would:
        // 1. Get the monitor bounds for the locked monitor
        // 2. Check if cursor is within bounds
        // 3. If outside, clamp the cursor position
        
        // For now, we just log it
        KdPrint(("[DualDesk] Cursor locked to Monitor %lu\n", g_LockedMonitor));
    }
    
    // Route to correct workspace
    KdPrint(("[DualDesk] Mouse input from Device %lu -> Workspace %lu\n", 
             deviceId, workspaceId));
    
    // In a real implementation, we would:
    // 1. Check if the input should be sent to this workspace
    // 2. If not, drop the input
    // 3. If yes, pass it through
    
    return STATUS_SUCCESS;
}

// ==================== DEVICE ASSIGNMENT FUNCTIONS ====================

// Assign a device to a workspace
NTSTATUS AssignDeviceToWorkspace(ULONG deviceId, ULONG workspaceId, BOOLEAN isKeyboard) {
    KdPrint(("[DualDesk] Assigning Device %lu to Workspace %lu\n", deviceId, workspaceId));
    
    // Check if device already assigned
    for (ULONG i = 0; i < g_NumAssignments; i++) {
        if (g_DeviceAssignments[i].DeviceId == deviceId) {
            // Update existing assignment
            g_DeviceAssignments[i].WorkspaceId = workspaceId;
            g_DeviceAssignments[i].Active = TRUE;
            KdPrint(("[DualDesk] Updated existing assignment\n"));
            return STATUS_SUCCESS;
        }
    }
    
    // Add new assignment
    if (g_NumAssignments >= MAX_DEVICES) {
        KdPrint(("[DualDesk] Too many devices!\n"));
        return STATUS_TOO_MANY_DEVICES;
    }
    
    g_DeviceAssignments[g_NumAssignments].DeviceId = deviceId;
    g_DeviceAssignments[g_NumAssignments].WorkspaceId = workspaceId;
    g_DeviceAssignments[g_NumAssignments].IsKeyboard = isKeyboard;
    g_DeviceAssignments[g_NumAssignments].Active = TRUE;
    g_NumAssignments++;
    
    KdPrint(("[DualDesk] Device assigned successfully\n"));
    return STATUS_SUCCESS;
}

// Get assignment for a device
ULONG GetDeviceWorkspace(ULONG deviceId) {
    for (ULONG i = 0; i < g_NumAssignments; i++) {
        if (g_DeviceAssignments[i].DeviceId == deviceId && 
            g_DeviceAssignments[i].Active) {
            return g_DeviceAssignments[i].WorkspaceId;
        }
    }
    return 0;  // Not assigned
}

// ==================== CURSOR LOCK FUNCTIONS ====================

// Lock cursor to a specific monitor
NTSTATUS LockCursorToMonitor(ULONG monitorIndex) {
    KdPrint(("[DualDesk] Locking cursor to Monitor %lu\n", monitorIndex));
    
    g_CursorLocked = TRUE;
    g_LockedMonitor = monitorIndex;
    
    return STATUS_SUCCESS;
}

// Unlock cursor
NTSTATUS UnlockCursor() {
    KdPrint(("[DualDesk] Unlocking cursor\n"));
    
    g_CursorLocked = FALSE;
    g_LockedMonitor = 0;
    
    return STATUS_SUCCESS;
}

// ==================== IOCTL HANDLERS ====================

// Handle device assignment from user mode
NTSTATUS HandleAssignDevice(PIRP Irp) {
    PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation(Irp);
    PDUALDESK_DEVICE_INFO deviceInfo = 
        (PDUALDESK_DEVICE_INFO)Irp->AssociatedIrp.SystemBuffer;
    
    if (!deviceInfo) {
        return STATUS_INVALID_PARAMETER;
    }
    
    ULONG deviceId = deviceInfo->DeviceId;
    ULONG workspaceId = deviceInfo->WorkspaceId;
    BOOLEAN isKeyboard = (deviceInfo->DeviceType == 0);
    
    return AssignDeviceToWorkspace(deviceId, workspaceId, isKeyboard);
}

// Handle cursor lock from user mode
NTSTATUS HandleLockCursor(PIRP Irp) {
    PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation(Irp);
    PDUALDESK_LOCK_INFO lockInfo = 
        (PDUALDESK_LOCK_INFO)Irp->AssociatedIrp.SystemBuffer;
    
    if (!lockInfo) {
        return STATUS_INVALID_PARAMETER;
    }
    
    if (lockInfo->LockCursor) {
        return LockCursorToMonitor(lockInfo->MonitorIndex);
    } else {
        return UnlockCursor();
    }
}