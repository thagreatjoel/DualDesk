#include "dualdesk/driver/driver_common.h"
#include "dualdesk/driver/ioctl_codes.h"
#include <ntddk.h>
#include <wdm.h>

// Device assignments (from user mode)
typedef struct _DEVICE_ASSIGNMENT {
    ULONG DeviceId;
    ULONG WorkspaceId;
    ULONG MonitorIndex;
    BOOLEAN IsKeyboard;
    BOOLEAN Active;
} DEVICE_ASSIGNMENT;

// Global assignment table
#define MAX_DEVICES 32
DEVICE_ASSIGNMENT g_DeviceAssignments[MAX_DEVICES];
ULONG g_NumAssignments = 0;

// Cursor lock state
BOOLEAN g_CursorLocked = FALSE;
ULONG g_LockedMonitor = 0;

// Function to assign device to workspace (used by HandleAssignDevice)
NTSTATUS AssignDeviceToWorkspace(ULONG deviceId, ULONG workspaceId, BOOLEAN isKeyboard) {
    DbgPrint("[DualDesk] Assigning Device %lu to Workspace %lu\n", deviceId, workspaceId);
    
    // Check if device already assigned
    for (ULONG i = 0; i < g_NumAssignments; i++) {
        if (g_DeviceAssignments[i].DeviceId == deviceId) {
            g_DeviceAssignments[i].WorkspaceId = workspaceId;
            g_DeviceAssignments[i].Active = TRUE;
            return STATUS_SUCCESS;
        }
    }
    
    // Add new assignment
    if (g_NumAssignments >= MAX_DEVICES) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    
    g_DeviceAssignments[g_NumAssignments].DeviceId = deviceId;
    g_DeviceAssignments[g_NumAssignments].WorkspaceId = workspaceId;
    g_DeviceAssignments[g_NumAssignments].IsKeyboard = isKeyboard;
    g_DeviceAssignments[g_NumAssignments].Active = TRUE;
    g_NumAssignments++;
    
    return STATUS_SUCCESS;
}

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
        g_CursorLocked = TRUE;
        g_LockedMonitor = lockInfo->MonitorIndex;
        DbgPrint("[DualDesk] Cursor locked to Monitor %lu\n", g_LockedMonitor);
    } else {
        g_CursorLocked = FALSE;
        g_LockedMonitor = 0;
        DbgPrint("[DualDesk] Cursor unlocked\n");
    }
    
    return STATUS_SUCCESS;
}