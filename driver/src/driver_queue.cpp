#include "dualdesk/driver/driver_common.h"
#include <ntddk.h>
#include <wdm.h>
#include <wdmguid.h>

// Queue management for pending requests
typedef struct _REQUEST_QUEUE {
    LIST_ENTRY ListHead;
    KSPIN_LOCK Lock;
    ULONG Count;
} REQUEST_QUEUE;

static REQUEST_QUEUE g_PendingRequests;

// Initialize the request queue
VOID InitializeRequestQueue() {
    InitializeListHead(&g_PendingRequests.ListHead);
    KeInitializeSpinLock(&g_PendingRequests.Lock);
    g_PendingRequests.Count = 0;
}

// Add request to queue
NTSTATUS QueueRequest(PIRP Irp) {
    KIRQL oldIrql;
    KeAcquireSpinLock(&g_PendingRequests.Lock, &oldIrql);
    
    InsertTailList(&g_PendingRequests.ListHead, &Irp->Tail.Overlay.ListEntry);
    g_PendingRequests.Count++;
    
    KeReleaseSpinLock(&g_PendingRequests.Lock, oldIrql);
    
    DbgPrint("[DualDesk] Request queued. Total: %d\n", g_PendingRequests.Count);
    return STATUS_SUCCESS;
}

// Process next request from queue
PIRP DequeueRequest() {
    KIRQL oldIrql;
    PIRP irp = NULL;
    
    KeAcquireSpinLock(&g_PendingRequests.Lock, &oldIrql);
    
    if (!IsListEmpty(&g_PendingRequests.ListHead)) {
        PLIST_ENTRY entry = RemoveHeadList(&g_PendingRequests.ListHead);
        irp = CONTAINING_RECORD(entry, IRP, Tail.Overlay.ListEntry);
        g_PendingRequests.Count--;
    }
    
    KeReleaseSpinLock(&g_PendingRequests.Lock, oldIrql);
    
    if (irp) {
        DbgPrint("[DualDesk] Request dequeued. Remaining: %d\n", g_PendingRequests.Count);
    }
    
    return irp;
}

// Get queue count
ULONG GetQueueCount() {
    return g_PendingRequests.Count;
}

// Clear all pending requests
VOID ClearRequestQueue() {
    KIRQL oldIrql;
    KeAcquireSpinLock(&g_PendingRequests.Lock, &oldIrql);
    
    while (!IsListEmpty(&g_PendingRequests.ListHead)) {
        PLIST_ENTRY entry = RemoveHeadList(&g_PendingRequests.ListHead);
        PIRP irp = CONTAINING_RECORD(entry, IRP, Tail.Overlay.ListEntry);
        Irp->IoStatus.Status = STATUS_CANCELLED;
        IoCompleteRequest(irp, IO_NO_INCREMENT);
    }
    g_PendingRequests.Count = 0;
    
    KeReleaseSpinLock(&g_PendingRequests.Lock, oldIrql);
    
    DbgPrint("[DualDesk] Request queue cleared\n");
}