#pragma once

#include <ntddk.h>
#include <wdm.h>
#include <mouclass.h>
#include <ntstrsafe.h>

#define DUALMOUSE_POOL_TAG 'smD'  // 'Dms'

// IOCTL Codes for user-mode communication
#define IOCTL_DUALMOUSE_ENABLE \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_DUALMOUSE_DISABLE \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_DUALMOUSE_SET_COLOR \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x802, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_DUALMOUSE_GET_STATUS \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x803, METHOD_BUFFERED, FILE_ANY_ACCESS)

// Device structure
typedef struct _DUALMOUSE_DEVICE_EXTENSION {
    PDEVICE_OBJECT DeviceObject;
    PDEVICE_OBJECT MouseDeviceObject;
    PDEVICE_OBJECT AttachedDeviceObject;
    
    // Cursor tracking
    LONG Mouse1X;
    LONG Mouse1Y;
    LONG Mouse2X;
    LONG Mouse2Y;
    
    // Colors (in BGR format for GDI)
    ULONG Mouse1Color;
    ULONG Mouse2Color;
    
    // State
    BOOLEAN Enabled;
    BOOLEAN Mouse1Active;
    BOOLEAN Mouse2Active;
    HANDLE Mouse1Handle;
    HANDLE Mouse2Handle;
    
    // Stats
    ULONG TotalPackets;
    ULONG RoutedPackets;
    
    KSPIN_LOCK Lock;
} DUALMOUSE_DEVICE_EXTENSION, *PDUALMOUSE_DEVICE_EXTENSION;

// Mouse filter callback
NTSTATUS DualMouseServiceCallback(
    PDEVICE_OBJECT DeviceObject,
    PMOUSE_INPUT_DATA InputDataStart,
    PMOUSE_INPUT_DATA InputDataEnd,
    PULONG InputDataConsumed
);

// Driver functions
NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath);
NTSTATUS DualMouseCreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS DualMouseDeviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp);
VOID DualMouseUnload(PDRIVER_OBJECT DriverObject);
NTSTATUS DualMouseAddDevice(PDRIVER_OBJECT DriverObject, PDEVICE_OBJECT PhysicalDeviceObject);