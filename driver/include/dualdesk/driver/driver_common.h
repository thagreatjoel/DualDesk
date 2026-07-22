#pragma once

#include <ntddk.h>
#include <wdm.h>
#include <kbdclass.h>
#include <mouclass.h>
#include <ntstrsafe.h>

// ============================================================
// SUPPRESS INTELLISENSE ERRORS IN VSCODE
// ============================================================
#ifndef __INTELLISENSE__
    // Normal compilation
#else
    // IntelliSense workaround - these types are already defined
    // but we include them to make IntelliSense happy
    #include <windows.h>
#endif


// ============================================================
// EXISTING DEFINITIONS (Keep as is)
// ============================================================
#define DUALDESK_DEVICE_NAME L"\\Device\\DualDesk"
#define DUALDESK_SYMLINK L"\\DosDevices\\DualDesk"
#define DUALDESK_POOL_TAG 'ksaD'

// Existing IOCTL Codes
#define IOCTL_GET_DRIVER_STATUS    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_ROUTE_INPUT          CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_LOCK_WINDOW          CTL_CODE(FILE_DEVICE_UNKNOWN, 0x802, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_CONFINE_MOUSE        CTL_CODE(FILE_DEVICE_UNKNOWN, 0x803, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_SET_MONITOR_BOUNDS   CTL_CODE(FILE_DEVICE_UNKNOWN, 0x804, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_GET_MONITOR_BOUNDS   CTL_CODE(FILE_DEVICE_UNKNOWN, 0x805, METHOD_BUFFERED, FILE_ANY_ACCESS)

// ============================================================
// EXISTING IOCTL CODES FROM driver_main.cpp (Keep as is)
// ============================================================
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
// ADD: DUAL MOUSE IOCTL CODES (NEW - Non-breaking)
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

// ============================================================
// EXISTING CONSTANTS (Keep as is)
// ============================================================
#define WORKSPACE_UNASSIGNED 0xFFFFFFFF
#define MAX_DEVICES 64
#define MAX_FILTERS 32
#define DEVICE_TYPE_KEYBOARD 1
#define DEVICE_TYPE_MOUSE 2

// ============================================================
// EXISTING STRUCTURES (Keep as is)
// ============================================================
typedef struct _DRIVER_STATUS {
    BOOLEAN IsRunning;
    BOOLEAN IsIsolatingInput;
    ULONG ActiveMonitors;
    ULONG ConnectedKeyboards;
    ULONG ConnectedMice;
} DRIVER_STATUS;

typedef struct _MONITOR_BOUNDS {
    ULONG MonitorId;
    LONG Left;
    LONG Top;
    LONG Right;
    LONG Bottom;
} MONITOR_BOUNDS;

typedef struct _INPUT_ROUTE {
    ULONG DeviceId;
    ULONG MonitorId;
    BOOLEAN IsKeyboard;
    BOOLEAN IsMouse;
} INPUT_ROUTE;

// ============================================================
// ADD: DUAL MOUSE DATA STRUCTURES (NEW - Non-breaking)
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
    ULONG DeviceNumber;  // 1 or 2
} DUALMOUSE_DEVICE_ASSIGN, *PDUALMOUSE_DEVICE_ASSIGN;

typedef struct _DUALMOUSE_STATUS {
    BOOLEAN Enabled;
    ULONG TotalPackets;
    ULONG RoutedPackets;
    BOOLEAN Device1Active;
    BOOLEAN Device2Active;
    LONG Cursor1X;
    LONG Cursor1Y;
    LONG Cursor2X;
    LONG Cursor2Y;
} DUALMOUSE_STATUS, *PDUALMOUSE_STATUS;

// ============================================================
// ADD: DUAL MOUSE CONTEXT (NEW - Non-breaking)
// ============================================================
typedef struct _DUALMOUSE_CONTEXT {
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
} DUALMOUSE_CONTEXT, *PDUALMOUSE_CONTEXT;

// ============================================================
// COMPLETE DEVICE EXTENSION (Merge both versions)
// ============================================================
typedef struct _DEVICE_EXTENSION {
    // ============================================================
    // EXISTING FIELDS (from your original driver_common.h)
    // ============================================================
    PDEVICE_OBJECT DeviceObject;
    UNICODE_STRING DeviceName;
    UNICODE_STRING SymlinkName;
    ULONG DeviceId;
    BOOLEAN IsInputIsolationActive;
    ULONG MonitorCount;
    
    // ============================================================
    // EXISTING FIELDS (from driver_main.h DEVICE_EXTENSION)
    // ============================================================
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
    
    // Filter devices
    PDEVICE_OBJECT keyboardFilterDevice;
    PDEVICE_OBJECT mouseFilterDevice;
    PDEVICE_OBJECT keyboardTargetDevice;
    PDEVICE_OBJECT mouseTargetDevice;
    
    // Original callbacks
    PKEYBOARD_CALLBACK keyboardCallback;
    PMOUSE_CALLBACK mouseCallback;
    
    // ============================================================
    // ADD: DUAL MOUSE CONTEXT (NEW - Non-breaking)
    // ============================================================
    DUALMOUSE_CONTEXT DualMouse;
    
} DEVICE_EXTENSION, *PDEVICE_EXTENSION;

// ============================================================
// EXISTING DRIVER FUNCTIONS (Keep as is)
// ============================================================
#ifdef __cplusplus
extern "C" {
#endif

NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath);
VOID DriverUnload(PDRIVER_OBJECT DriverObject);

// Dispatch functions
NTSTATUS OnCreate(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS OnClose(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS OnDeviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp);

// ============================================================
// ADD: DUAL MOUSE FUNCTION DECLARATIONS (NEW)
// ============================================================
NTSTATUS DualMouseEnable(PDEVICE_EXTENSION pExt);
NTSTATUS DualMouseDisable(PDEVICE_EXTENSION pExt);
NTSTATUS DualMouseSetColors(PDEVICE_EXTENSION pExt, ULONG color1, ULONG color2);
NTSTATUS DualMouseGetPositions(PDEVICE_EXTENSION pExt, PDUALMOUSE_POSITIONS positions);
NTSTATUS DualMouseSetDevice(PDEVICE_EXTENSION pExt, HANDLE deviceHandle, ULONG deviceNumber);
NTSTATUS DualMouseGetStatus(PDEVICE_EXTENSION pExt, PDUALMOUSE_STATUS status);

#ifdef __cplusplus
}
#endif