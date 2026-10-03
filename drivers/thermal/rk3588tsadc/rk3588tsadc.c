/*
 * rk3588tsadc: temperature sensor driver for the RK3588 TSADC.
 *
 * Binds to ACPI\RKCP3004 (TSADC registers in _CRS). A thermal zone that names this device in
 * its _DSM (function 2) reads its temperature from here with IOCTL_THERMAL_READ_TEMPERATURE.
 * The firmware starts the TSADC (clock, auto mode); this driver only reads channel 0, the
 * package sensor, polled once a second while a read is waiting (like Linux's polling-delay).
 * Based on the Windows-driver-samples thermal/simsensor sample.
 *
 * Requests are completed without holding the lock: the caller may send the next read from
 * its completion routine, on the same thread.
 *
 * Copyright (c) Microsoft Corporation.
 * Copyright (c) 2026, Meari <275577454+Meari-Prototype@users.noreply.github.com>
 *
 * SPDX-License-Identifier: MIT
 */

#include <ntddk.h>
#include <wdf.h>
#include <poclass.h>

#define TSADC_AUTO_CON      0x04
#define TSADC_AUTO_EN       0x1
#define TSADC_DATA(Ch)      (0x2c + 4 * (Ch))
#define TSADC_DATA_MASK     0x1ff
#define TSADC_REGS_MIN      0x30

#define TSADC_CHANNEL       0               // package (top) sensor
#define TSADC_POLL_100NS    (1000 * 10000LL)

typedef struct {
    volatile ULONG *Regs;
    SIZE_T RegsLength;
    WDFQUEUE PendingQueue;                  // reads waiting for a threshold or a timeout
    WDFSPINLOCK Lock;                       // serializes scans of PendingQueue, protects Running
    WDFTIMER Timer;
    BOOLEAN Running;                        // in D0, the timer may be armed
} DEVICE_CONTEXT, *PDEVICE_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DEVICE_CONTEXT, GetDeviceContext);

typedef struct {
    LONGLONG Expiry;                        // system time, -1 = no timeout
    ULONG Low;
    ULONG High;
} READ_CONTEXT, *PREAD_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(READ_CONTEXT, GetReadContext);

DRIVER_INITIALIZE DriverEntry;
EVT_WDF_DRIVER_DEVICE_ADD TsadcEvtDeviceAdd;
EVT_WDF_DEVICE_PREPARE_HARDWARE TsadcEvtPrepareHardware;
EVT_WDF_DEVICE_RELEASE_HARDWARE TsadcEvtReleaseHardware;
EVT_WDF_DEVICE_D0_ENTRY TsadcEvtD0Entry;
EVT_WDF_DEVICE_D0_EXIT TsadcEvtD0Exit;
EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL TsadcEvtIoDeviceControl;
EVT_WDF_TIMER TsadcEvtTimer;

//
// RK3588 code table (as in Linux rockchip_thermal.c), linear in between.
//
static const struct {
    ULONG Code;
    LONG MilliC;
} TsadcTable[] = {
    { 215, -40000 },
    { 285, 25000 },
    { 350, 85000 },
    { 395, 125000 },
};

static ULONG
TsadcCodeToDeciKelvin (
    _In_ ULONG Code
    )
{
    LONG MilliC;
    ULONG i;

    if (Code <= TsadcTable[0].Code) {
        MilliC = TsadcTable[0].MilliC;
    } else if (Code >= TsadcTable[ARRAYSIZE(TsadcTable) - 1].Code) {
        MilliC = TsadcTable[ARRAYSIZE(TsadcTable) - 1].MilliC;
    } else {
        for (i = 1; Code > TsadcTable[i].Code; i++) {
        }

        MilliC = TsadcTable[i - 1].MilliC +
                 (LONG)(Code - TsadcTable[i - 1].Code) *
                 (TsadcTable[i].MilliC - TsadcTable[i - 1].MilliC) /
                 (LONG)(TsadcTable[i].Code - TsadcTable[i - 1].Code);
    }

    return (ULONG)((MilliC + 273150 + 50) / 100);
}

static ULONG
TsadcRead (
    _In_ PDEVICE_CONTEXT Ctx
    )
{
    return TsadcCodeToDeciKelvin(READ_REGISTER_ULONG(&Ctx->Regs[TSADC_DATA(TSADC_CHANNEL) / 4]) &
                                 TSADC_DATA_MASK);
}

static BOOLEAN
TsadcReadIsDone (
    _In_ PREAD_CONTEXT Rc,
    _In_ ULONG Temperature,
    _In_ LONGLONG Now
    )
{
    return Temperature < Rc->Low || Temperature > Rc->High ||
           (Rc->Expiry >= 0 && Now >= Rc->Expiry);
}

static VOID
TsadcCompleteRead (
    _In_ WDFREQUEST Request,
    _In_ ULONG Temperature
    )
{
    PULONG Out;
    NTSTATUS Status;

    Status = WdfRequestRetrieveOutputBuffer(Request, sizeof(ULONG), (PVOID *)&Out, NULL);
    if (!NT_SUCCESS(Status)) {
        WdfRequestComplete(Request, Status);
        return;
    }

    *Out = Temperature;
    WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, sizeof(ULONG));
}

//
// Completes the waiting reads that are done and arms the timer for the rest.
//
static VOID
TsadcScan (
    _In_ PDEVICE_CONTEXT Ctx
    )
{
    ULONG Count;
    WDFREQUEST Done[8];
    WDFREQUEST Found;
    ULONG i;
    LONGLONG Next;
    LARGE_INTEGER Now;
    WDFREQUEST Prev;
    PREAD_CONTEXT Rc;
    NTSTATUS Status;
    ULONG Temperature;
    BOOLEAN Waiting;

    do {
        Temperature = TsadcRead(Ctx);
        KeQuerySystemTime(&Now);
        Count = 0;
        Prev = NULL;
        Next = TSADC_POLL_100NS;
        Waiting = FALSE;

        WdfSpinLockAcquire(Ctx->Lock);
        while (Count < ARRAYSIZE(Done)) {
            Status = WdfIoQueueFindRequest(Ctx->PendingQueue, Prev, NULL, NULL, &Found);
            if (Prev != NULL) {
                WdfObjectDereference(Prev);
                Prev = NULL;
            }

            if (Status == STATUS_NOT_FOUND) {
                //
                // The previous request left the queue (cancelled): start over.
                //
                Next = TSADC_POLL_100NS;
                Waiting = FALSE;
                continue;
            }

            if (!NT_SUCCESS(Status)) {
                break;
            }

            Rc = GetReadContext(Found);
            if (TsadcReadIsDone(Rc, Temperature, Now.QuadPart) &&
                NT_SUCCESS(WdfIoQueueRetrieveFoundRequest(Ctx->PendingQueue, Found, &Done[Count]))) {

                WdfObjectDereference(Found);
                Count++;
                Next = TSADC_POLL_100NS;
                Waiting = FALSE;
                continue;
            }

            Waiting = TRUE;
            if (Rc->Expiry >= 0 && Rc->Expiry - Now.QuadPart < Next) {
                Next = Rc->Expiry - Now.QuadPart;
            }

            Prev = Found;
        }

        if (Prev != NULL) {
            WdfObjectDereference(Prev);
        }

        if (Waiting && Ctx->Running) {
            WdfTimerStart(Ctx->Timer, -max(Next, 10000LL));
        }

        WdfSpinLockRelease(Ctx->Lock);

        for (i = 0; i < Count; i++) {
            TsadcCompleteRead(Done[i], Temperature);
        }

    } while (Count == ARRAYSIZE(Done));
}

VOID
TsadcEvtTimer (
    _In_ WDFTIMER Timer
    )
{
    TsadcScan(GetDeviceContext(WdfTimerGetParentObject(Timer)));
}

VOID
TsadcEvtIoDeviceControl (
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t OutputBufferLength,
    _In_ size_t InputBufferLength,
    _In_ ULONG IoControlCode
    )
{
    WDF_OBJECT_ATTRIBUTES Attributes;
    PDEVICE_CONTEXT Ctx;
    LARGE_INTEGER Now;
    PREAD_CONTEXT Rc;
    NTSTATUS Status;
    ULONG Temperature;
    PTHERMAL_WAIT_READ Wait;

    UNREFERENCED_PARAMETER(InputBufferLength);

    if (IoControlCode != IOCTL_THERMAL_READ_TEMPERATURE) {
        WdfRequestComplete(Request, STATUS_INVALID_DEVICE_REQUEST);
        return;
    }

    if (OutputBufferLength < sizeof(ULONG)) {
        WdfRequestComplete(Request, STATUS_BUFFER_TOO_SMALL);
        return;
    }

    Status = WdfRequestRetrieveInputBuffer(Request, sizeof(*Wait), (PVOID *)&Wait, NULL);
    if (!NT_SUCCESS(Status)) {
        WdfRequestComplete(Request, Status);
        return;
    }

    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&Attributes, READ_CONTEXT);
    Status = WdfObjectAllocateContext(Request, &Attributes, (PVOID *)&Rc);
    if (!NT_SUCCESS(Status)) {
        WdfRequestComplete(Request, Status);
        return;
    }

    //
    // Input and output share the buffer: copy the input before anything is written.
    //
    Ctx = GetDeviceContext(WdfIoQueueGetDevice(Queue));
    Temperature = TsadcRead(Ctx);
    KeQuerySystemTime(&Now);
    Rc->Low = Wait->LowTemperature;
    Rc->High = Wait->HighTemperature;
    Rc->Expiry = (Wait->Timeout == MAXULONG) ? -1 : Now.QuadPart + (LONGLONG)Wait->Timeout * 10000;

    if (TsadcReadIsDone(Rc, Temperature, Now.QuadPart)) {
        TsadcCompleteRead(Request, Temperature);
        return;
    }

    Status = WdfRequestForwardToIoQueue(Request, Ctx->PendingQueue);
    if (!NT_SUCCESS(Status)) {
        WdfRequestComplete(Request, Status);
        return;
    }

    TsadcScan(Ctx);
}

NTSTATUS
TsadcEvtPrepareHardware (
    _In_ WDFDEVICE Device,
    _In_ WDFCMRESLIST ResourcesRaw,
    _In_ WDFCMRESLIST ResourcesTranslated
    )
{
    PDEVICE_CONTEXT Ctx;
    PCM_PARTIAL_RESOURCE_DESCRIPTOR Desc;
    ULONG i;

    UNREFERENCED_PARAMETER(ResourcesRaw);

    Ctx = GetDeviceContext(Device);
    for (i = 0; i < WdfCmResourceListGetCount(ResourcesTranslated); i++) {
        Desc = WdfCmResourceListGetDescriptor(ResourcesTranslated, i);
        if (Desc->Type == CmResourceTypeMemory && Desc->u.Memory.Length >= TSADC_REGS_MIN) {
            Ctx->Regs = MmMapIoSpaceEx(Desc->u.Memory.Start,
                                       Desc->u.Memory.Length,
                                       PAGE_READWRITE | PAGE_NOCACHE);

            Ctx->RegsLength = Desc->u.Memory.Length;
            break;
        }
    }

    if (Ctx->Regs == NULL) {
        return STATUS_DEVICE_CONFIGURATION_ERROR;
    }

    //
    // Without the firmware's setup the data registers hold no measurement.
    //
    if ((READ_REGISTER_ULONG(&Ctx->Regs[TSADC_AUTO_CON / 4]) & TSADC_AUTO_EN) == 0) {
        MmUnmapIoSpace((PVOID)Ctx->Regs, Ctx->RegsLength);
        Ctx->Regs = NULL;
        return STATUS_DEVICE_NOT_READY;
    }

    return STATUS_SUCCESS;
}

NTSTATUS
TsadcEvtReleaseHardware (
    _In_ WDFDEVICE Device,
    _In_ WDFCMRESLIST ResourcesTranslated
    )
{
    PDEVICE_CONTEXT Ctx;

    UNREFERENCED_PARAMETER(ResourcesTranslated);

    Ctx = GetDeviceContext(Device);
    if (Ctx->Regs != NULL) {
        MmUnmapIoSpace((PVOID)Ctx->Regs, Ctx->RegsLength);
        Ctx->Regs = NULL;
    }

    return STATUS_SUCCESS;
}

NTSTATUS
TsadcEvtD0Entry (
    _In_ WDFDEVICE Device,
    _In_ WDF_POWER_DEVICE_STATE PreviousState
    )
{
    PDEVICE_CONTEXT Ctx;

    UNREFERENCED_PARAMETER(PreviousState);

    Ctx = GetDeviceContext(Device);
    WdfSpinLockAcquire(Ctx->Lock);
    Ctx->Running = TRUE;
    WdfSpinLockRelease(Ctx->Lock);
    TsadcScan(Ctx);
    return STATUS_SUCCESS;
}

NTSTATUS
TsadcEvtD0Exit (
    _In_ WDFDEVICE Device,
    _In_ WDF_POWER_DEVICE_STATE TargetState
    )
{
    PDEVICE_CONTEXT Ctx;

    UNREFERENCED_PARAMETER(TargetState);

    Ctx = GetDeviceContext(Device);
    WdfSpinLockAcquire(Ctx->Lock);
    Ctx->Running = FALSE;
    WdfSpinLockRelease(Ctx->Lock);
    WdfTimerStop(Ctx->Timer, TRUE);
    return STATUS_SUCCESS;
}

NTSTATUS
TsadcEvtDeviceAdd (
    _In_ WDFDRIVER Driver,
    _Inout_ PWDFDEVICE_INIT DeviceInit
    )
{
    WDF_OBJECT_ATTRIBUTES Attributes;
    PDEVICE_CONTEXT Ctx;
    WDFDEVICE Device;
    WDF_PNPPOWER_EVENT_CALLBACKS PnpPower;
    WDF_IO_QUEUE_CONFIG QueueConfig;
    NTSTATUS Status;
    WDF_TIMER_CONFIG TimerConfig;

    UNREFERENCED_PARAMETER(Driver);

    WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&PnpPower);
    PnpPower.EvtDevicePrepareHardware = TsadcEvtPrepareHardware;
    PnpPower.EvtDeviceReleaseHardware = TsadcEvtReleaseHardware;
    PnpPower.EvtDeviceD0Entry = TsadcEvtD0Entry;
    PnpPower.EvtDeviceD0Exit = TsadcEvtD0Exit;
    WdfDeviceInitSetPnpPowerEventCallbacks(DeviceInit, &PnpPower);

    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&Attributes, DEVICE_CONTEXT);
    Status = WdfDeviceCreate(&DeviceInit, &Attributes, &Device);
    if (!NT_SUCCESS(Status)) {
        return Status;
    }

    Ctx = GetDeviceContext(Device);

    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&QueueConfig, WdfIoQueueDispatchParallel);
    QueueConfig.EvtIoDeviceControl = TsadcEvtIoDeviceControl;
    Status = WdfIoQueueCreate(Device, &QueueConfig, WDF_NO_OBJECT_ATTRIBUTES, WDF_NO_HANDLE);
    if (!NT_SUCCESS(Status)) {
        return Status;
    }

    //
    // Not power managed: waiting reads stay queued across a power transition.
    // The framework completes them if they are cancelled.
    //
    WDF_IO_QUEUE_CONFIG_INIT(&QueueConfig, WdfIoQueueDispatchManual);
    QueueConfig.PowerManaged = WdfFalse;
    Status = WdfIoQueueCreate(Device, &QueueConfig, WDF_NO_OBJECT_ATTRIBUTES, &Ctx->PendingQueue);
    if (!NT_SUCCESS(Status)) {
        return Status;
    }

    WDF_OBJECT_ATTRIBUTES_INIT(&Attributes);
    Attributes.ParentObject = Device;
    Status = WdfSpinLockCreate(&Attributes, &Ctx->Lock);
    if (!NT_SUCCESS(Status)) {
        return Status;
    }

    WDF_TIMER_CONFIG_INIT(&TimerConfig, TsadcEvtTimer);
    TimerConfig.AutomaticSerialization = FALSE;
    WDF_OBJECT_ATTRIBUTES_INIT(&Attributes);
    Attributes.ParentObject = Device;
    Attributes.ExecutionLevel = WdfExecutionLevelPassive;
    return WdfTimerCreate(&TimerConfig, &Attributes, &Ctx->Timer);
}

NTSTATUS
DriverEntry (
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PUNICODE_STRING RegistryPath
    )
{
    WDF_DRIVER_CONFIG Config;

    WDF_DRIVER_CONFIG_INIT(&Config, TsadcEvtDeviceAdd);
    return WdfDriverCreate(DriverObject, RegistryPath, WDF_NO_OBJECT_ATTRIBUTES, &Config, WDF_NO_HANDLE);
}
