#include <ntddk.h>
#include <wdf.h>
#include <wdm.h>

#pragma warning(disable: 4200)

// Driver context structure
typedef struct _DUALDESK_DEVICE_CONTEXT {
    WDFDEVICE Device;
    WDFQUEUE IoQueue;
    KSPIN_LOCK DeviceListLock;
    LIST_ENTRY DeviceListHead;
    ULONG RouteMode;
    ULONG TargetProcessId;
    ULONG TotalEventsRouted;
    ULONG TotalEventsBlocked;
} DUALDESK_DEVICE_CONTEXT, *PDEVICE_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DUALDESK_DEVICE_CONTEXT, DualDeskGetDeviceContext)

// IOCTL definitions
#define DUALDESK_IOCTL_BASE 0x800

#define IOCTL_DUALDESK_GET_STATS \
    CTL_CODE(FILE_DEVICE_UNKNOWN, DUALDESK_IOCTL_BASE + 5, METHOD_BUFFERED, FILE_ANY_ACCESS)

typedef struct _DUALDESK_STATS_OUTPUT {
    ULONG TotalDevices;
    ULONG TotalEventsRouted;
    ULONG TotalEventsBlocked;
    LARGE_INTEGER DriverStartTime;
} DUALDESK_STATS_OUTPUT, *PDUALDESK_STATS_OUTPUT;

// Function prototypes
NTSTATUS DualDeskEvtDeviceAdd(WDFDRIVER Driver, PWDFDEVICE_INIT DeviceInit);
VOID DualDeskEvtIoDeviceControl(WDFQUEUE Queue, WDFREQUEST Request, size_t OutputBufferLength,
                                size_t InputBufferLength, ULONG IoControlCode);
VOID DualDeskEvtDriverUnload(WDFDRIVER Driver);

// Driver entry point
extern "C" NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath) {
    NTSTATUS status;
    WDF_DRIVER_CONFIG config;
    WDFDRIVER driver;

    KdPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL,
        "DualDesk Driver: DriverEntry called\n");

    WDF_DRIVER_CONFIG_INIT(&config, DualDeskEvtDeviceAdd);
    config.EvtDriverUnload = DualDeskEvtDriverUnload;

    status = WdfDriverCreate(DriverObject, RegistryPath, WDF_NO_OBJECT_ATTRIBUTES, &config, &driver);

    if (!NT_SUCCESS(status)) {
        KdPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL,
            "DualDesk Driver: WdfDriverCreate failed with 0x%08X\n", status);
        return status;
    }

    KdPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL,
        "DualDesk Driver: DriverEntry completed successfully\n");

    return STATUS_SUCCESS;
}

// Device add callback
NTSTATUS DualDeskEvtDeviceAdd(WDFDRIVER Driver, PWDFDEVICE_INIT DeviceInit) {
    UNREFERENCED_PARAMETER(Driver);
    NTSTATUS status;
    WDFDEVICE device;
    WDF_OBJECT_ATTRIBUTES attributes;
    PDEVICE_CONTEXT context;
    WDF_IO_QUEUE_CONFIG queueConfig;
    UNICODE_STRING symbolicLink;

    KdPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL,
        "DualDesk Driver: EvtDeviceAdd called\n");

    // Set device type
    WdfDeviceInitSetDeviceType(DeviceInit, FILE_DEVICE_UNKNOWN);
    WdfDeviceInitSetExclusive(DeviceInit, FALSE);

    // Create device object
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, DUALDESK_DEVICE_CONTEXT);

    status = WdfDeviceCreate(&DeviceInit, &attributes, &device);
    if (!NT_SUCCESS(status)) {
        KdPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL,
            "DualDesk Driver: WdfDeviceCreate failed with 0x%08X\n", status);
        return status;
    }

    // Initialize device context
    context = DualDeskGetDeviceContext(device);
    context->Device = device;
    context->RouteMode = 0;
    context->TargetProcessId = 0;
    context->TotalEventsRouted = 0;
    context->TotalEventsBlocked = 0;
    KeInitializeSpinLock(&context->DeviceListLock);
    InitializeListHead(&context->DeviceListHead);

    // Create symbolic link for user-mode communication
    RtlInitUnicodeString(&symbolicLink, L"\\??\\DualDeskDriver");
    status = WdfDeviceCreateSymbolicLink(device, &symbolicLink);

    if (!NT_SUCCESS(status)) {
        KdPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_WARNING_LEVEL,
            "DualDesk Driver: WdfDeviceCreateSymbolicLink failed with 0x%08X\n", status);
        // Continue anyway - not fatal
    } else {
        KdPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL,
            "DualDesk Driver: Symbolic link created: \\??\\DualDeskDriver\n");
    }

    // Create default IO queue
    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&queueConfig, WdfIoQueueDispatchSequential);
    queueConfig.EvtIoDeviceControl = DualDeskEvtIoDeviceControl;

    status = WdfIoQueueCreate(device, &queueConfig, WDF_NO_OBJECT_ATTRIBUTES, &context->IoQueue);
    if (!NT_SUCCESS(status)) {
        KdPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL,
            "DualDesk Driver: WdfIoQueueCreate failed with 0x%08X\n", status);
        return status;
    }

    KdPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL,
        "DualDesk Driver: EvtDeviceAdd completed successfully\n");

    return STATUS_SUCCESS;
}

// IOCTL handler
VOID DualDeskEvtIoDeviceControl(WDFQUEUE Queue, WDFREQUEST Request,
                                size_t OutputBufferLength, size_t InputBufferLength,
                                ULONG IoControlCode) {
    WDFDEVICE device = WdfIoQueueGetDevice(Queue);
    PDEVICE_CONTEXT context = DualDeskGetDeviceContext(device);
    NTSTATUS status = STATUS_INVALID_DEVICE_REQUEST;

    KdPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL,
        "DualDesk Driver: IOCTL 0x%08X received\n", IoControlCode);

    switch (IoControlCode) {
        case IOCTL_DUALDESK_GET_STATS: {
            if (OutputBufferLength < sizeof(DUALDESK_STATS_OUTPUT)) {
                status = STATUS_BUFFER_TOO_SMALL;
                break;
            }

            PDUALDESK_STATS_OUTPUT output = NULL;
            status = WdfRequestRetrieveOutputBuffer(Request, sizeof(DUALDESK_STATS_OUTPUT),
                                                    (PVOID*)&output, NULL);
            if (!NT_SUCCESS(status)) break;

            // Fill statistics
            output->TotalDevices = 0; // Count devices from list
            output->TotalEventsRouted = context->TotalEventsRouted;
            output->TotalEventsBlocked = context->TotalEventsBlocked;
            KeQuerySystemTime(&output->DriverStartTime);

            status = STATUS_SUCCESS;
            break;
        }

        default:
            status = STATUS_INVALID_DEVICE_REQUEST;
            break;
    }

    WdfRequestComplete(Request, status);
}

// Driver unload
VOID DualDeskEvtDriverUnload(WDFDRIVER Driver) {
    UNREFERENCED_PARAMETER(Driver);

    KdPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL,
        "DualDesk Driver: Unloading\n");
}