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

#define IOCTL_DUALDESK_ASSIGN_DEVICE_TO_WORKSPACE \
    CTL_CODE(FILE_DEVICE_UNKNOWN, DUALDESK_IOCTL_BASE + 6, METHOD_BUFFERED, FILE_ANY_ACCESS)

typedef struct _DUALDESK_STATS_OUTPUT {
    ULONG TotalDevices;
    ULONG TotalEventsRouted;
    ULONG TotalEventsBlocked;
    LARGE_INTEGER DriverStartTime;
} DUALDESK_STATS_OUTPUT, *PDUALDESK_STATS_OUTPUT;

typedef struct _DUALDESK_ASSIGN_DEVICE_INPUT {
    HANDLE DeviceHandle;
    ULONG WorkspaceId;
} DUALDESK_ASSIGN_DEVICE_INPUT, *PDUALDESK_ASSIGN_DEVICE_INPUT;

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

    DbgPrint("DualDesk: DriverEntry called\n");

    WDF_DRIVER_CONFIG_INIT(&config, DualDeskEvtDeviceAdd);
    config.EvtDriverUnload = DualDeskEvtDriverUnload;

    status = WdfDriverCreate(DriverObject, RegistryPath, WDF_NO_OBJECT_ATTRIBUTES, &config, &driver);

    if (!NT_SUCCESS(status)) {
        DbgPrint("DualDesk: WdfDriverCreate failed with 0x%08X\n", status);
        return status;
    }

    DbgPrint("DualDesk: DriverEntry completed successfully\n");
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

    DbgPrint("DualDesk: EvtDeviceAdd called\n");

    // Set device type
    WdfDeviceInitSetDeviceType(DeviceInit, FILE_DEVICE_UNKNOWN);
    WdfDeviceInitSetExclusive(DeviceInit, FALSE);

    // Create device object
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, DUALDESK_DEVICE_CONTEXT);

    status = WdfDeviceCreate(&DeviceInit, &attributes, &device);
    if (!NT_SUCCESS(status)) {
        DbgPrint("DualDesk: WdfDeviceCreate failed with 0x%08X\n", status);
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

    // CREATE SYMBOLIC LINK FOR USER-MODE COMMUNICATION
    RtlInitUnicodeString(&symbolicLink, L"\\??\\DualDesk");
    status = WdfDeviceCreateSymbolicLink(device, &symbolicLink);

    if (!NT_SUCCESS(status)) {
        DbgPrint("DualDesk: WdfDeviceCreateSymbolicLink failed with 0x%08X\n", status);
    } else {
        DbgPrint("DualDesk: Symbolic link created: \\??\\DualDesk\n");
    }

    // CREATE IO QUEUE WITH IOCTL HANDLER
    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&queueConfig, WdfIoQueueDispatchSequential);
    queueConfig.EvtIoDeviceControl = DualDeskEvtIoDeviceControl;

    status = WdfIoQueueCreate(device, &queueConfig, WDF_NO_OBJECT_ATTRIBUTES, &context->IoQueue);
    if (!NT_SUCCESS(status)) {
        DbgPrint("DualDesk: WdfIoQueueCreate failed with 0x%08X\n", status);
        return status;
    }

    DbgPrint("DualDesk: EvtDeviceAdd completed successfully\n");
    return STATUS_SUCCESS;
}

// IOCTL handler
VOID DualDeskEvtIoDeviceControl(WDFQUEUE Queue, WDFREQUEST Request,
                                size_t OutputBufferLength, size_t InputBufferLength,
                                ULONG IoControlCode) {
    WDFDEVICE device = WdfIoQueueGetDevice(Queue);
    PDEVICE_CONTEXT context = DualDeskGetDeviceContext(device);
    NTSTATUS status = STATUS_INVALID_DEVICE_REQUEST;

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

            output->TotalDevices = 0;
            output->TotalEventsRouted = context->TotalEventsRouted;
            output->TotalEventsBlocked = context->TotalEventsBlocked;
            KeQuerySystemTime(&output->DriverStartTime);

            status = STATUS_SUCCESS;
            break;
        }

        case IOCTL_DUALDESK_ASSIGN_DEVICE_TO_WORKSPACE: {
            if (InputBufferLength < sizeof(DUALDESK_ASSIGN_DEVICE_INPUT)) {
                status = STATUS_BUFFER_TOO_SMALL;
                break;
            }

            PDUALDESK_ASSIGN_DEVICE_INPUT input = NULL;
            status = WdfRequestRetrieveInputBuffer(Request, sizeof(DUALDESK_ASSIGN_DEVICE_INPUT),
                                                   (PVOID*)&input, NULL);
            if (!NT_SUCCESS(status)) break;

            DbgPrint("DualDesk: Device assigned to workspace %lu\n", input->WorkspaceId);
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
    DbgPrint("DualDesk: Unloading\n");
}

case IOCTL_DUALDESK_SET_ROUTE_MODE: {
    if (InputBufferLength < sizeof(ULONG)) {
        status = STATUS_BUFFER_TOO_SMALL;
        break;
    }

    PULONG mode = NULL;
    status = WdfRequestRetrieveInputBuffer(Request, sizeof(ULONG), (PVOID*)&mode, NULL);
    if (!NT_SUCCESS(status)) break;

    context->RouteMode = *mode;
    DbgPrint("DualDesk: Route mode set to %lu\n", *mode);
    status = STATUS_SUCCESS;
    break;
}