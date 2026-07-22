// ============================================================
// DUALDESK DRIVER FILTER - Placeholder
// Filter callbacks are in driver_main.cpp
// ============================================================

#include <ntddk.h>

// This file is a placeholder.
// The actual keyboard and mouse filter callbacks are
// implemented in driver_main.cpp as:
// - KeyboardFilterCallback()
// - MouseFilterCallback()
// Add dual mouse handling to existing filter

NTSTATUS DualDeskFilterMouse(
    PDEVICE_OBJECT DeviceObject,
    PMOUSE_INPUT_DATA InputDataStart,
    PMOUSE_INPUT_DATA InputDataEnd,
    PULONG InputDataConsumed
) {
    PDEVICE_EXTENSION extension = GetDeviceExtension(DeviceObject);
    PDUALMOUSE_CONTEXT dm = &extension->DualMouse;
    
    if (!dm->Enabled) {
        // Pass through normally
        *InputDataConsumed = (ULONG)(InputDataEnd - InputDataStart);
        return STATUS_SUCCESS;
    }
    
    HANDLE deviceHandle = GetDeviceHandle(DeviceObject);
    PMOUSE_INPUT_DATA current = InputDataStart;
    ULONG processed = 0;
    
    while (current < InputDataEnd) {
        if (deviceHandle == dm->Device1Handle) {
            // Mouse 1 - track position
            dm->Cursor1X += current->LastX;
            dm->Cursor1Y += current->LastY;
            
            // Clamp to screen
            if (dm->Cursor1X < 0) dm->Cursor1X = 0;
            if (dm->Cursor1Y < 0) dm->Cursor1Y = 0;
            if (dm->Cursor1X > GetSystemMetrics(SM_CXSCREEN)) 
                dm->Cursor1X = GetSystemMetrics(SM_CXSCREEN);
            if (dm->Cursor1Y > GetSystemMetrics(SM_CYSCREEN)) 
                dm->Cursor1Y = GetSystemMetrics(SM_CYSCREEN);
            
            // Route to overlay window
            SendDualMouseEvent(1, dm->Cursor1X, dm->Cursor1Y);
            
        } else if (deviceHandle == dm->Device2Handle) {
            // Mouse 2 - track position
            dm->Cursor2X += current->LastX;
            dm->Cursor2Y += current->LastY;
            
            if (dm->Cursor2X < 0) dm->Cursor2X = 0;
            if (dm->Cursor2Y < 0) dm->Cursor2Y = 0;
            if (dm->Cursor2X > GetSystemMetrics(SM_CXSCREEN)) 
                dm->Cursor2X = GetSystemMetrics(SM_CXSCREEN);
            if (dm->Cursor2Y > GetSystemMetrics(SM_CYSCREEN)) 
                dm->Cursor2Y = GetSystemMetrics(SM_CYSCREEN);
            
            SendDualMouseEvent(2, dm->Cursor2X, dm->Cursor2Y);
        }
        
        current++;
        processed++;
    }
    
    // Don't let the system cursor move - we're controlling it
    *InputDataConsumed = processed;
    return STATUS_SUCCESS;
}