/*++

Copyright (c) 2024 Mario Bălănică <mariobalanica02@gmail.com>
Copyright (c) Microsoft Corporation. All rights reserved.

SPDX-License-Identifier: MIT

Module Name:

    dwcmshc.cpp

Abstract:

    This file contains the code that interfaces with host controller.

Environment:

    Kernel mode only.

--*/

#include "precomp.h"
#pragma hdrstop

#include <acpiutil.hpp>
#include <sddef.h>

#include "trace.h"
#include "dwcmshc.tmh"

#include "dwcmshc.h"

#include "rockchip_ops.h"

//
// Configuration variables used by tracing macros.
//
BOOLEAN gCrashdumpMode = FALSE;
BOOLEAN gBreakOnError = FALSE;

//
// A driver wide log to track events not associated with a specific
// slot extension.
//
RECORDER_LOG DriverLogHandle = NULL;

//
// Platform data read from ACPI for each miniport instance.
//
static PLIST_ENTRY gPlatformInfoList;
static FAST_MUTEX gPlatformInfoListLock;

//
// Supported controllers
//
static MSHC_PLATFORM_DRIVER_DATA SupportedPlatforms[] = {
    {
        {                                           // Info
            MshcPlatformRockchip,                       // Type
            { },                                        // Capabilities
        },
        {                                           // Operations
            MshcRockchipSetClock,                       // SetClock
            MshcRockchipSetVoltage,                     // SetVoltage
            MshcRockchipSetSignalingVoltage,            // SetSignalingVoltage
            MshcRockchipExecuteTuning                   // ExecuteTuning
        }
    }
};

#define MSHC_PLATFORM_INFO_LIST_DRIVER_EXT_ID (PVOID)1

_Use_decl_annotations_
INIT_CODE_SEG
NTSTATUS
DriverEntry(
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PUNICODE_STRING RegistryPath
    )
{
    NTSTATUS Status;

    //
    // Crashdump stack will call DriverEntry at IRQL >= DISPATCH_LEVEL,
    // which is not possible to initialize WPP at by design, among other things.
    //
    if (KeGetCurrentIrql() >= DISPATCH_LEVEL) {
        gCrashdumpMode = TRUE;
    } else {
        ExInitializeDriverRuntime(0);

        //
        // Allocate a driver extension for the list of platform infos, so it can be
        // conveniently accessed from crashdump mode as well.
        //
        Status = IoAllocateDriverObjectExtension(
            DriverObject,
            MSHC_PLATFORM_INFO_LIST_DRIVER_EXT_ID,
            sizeof(LIST_ENTRY),
            (PVOID *)&gPlatformInfoList);

        if (!NT_SUCCESS(Status)) {
            MSHC_LOG_ERROR(
                DriverLogHandle,
                NULL,
                "Failed to allocate driver extension. Status: %!STATUS!",
                Status);
            return Status;
        }

        InitializeListHead(gPlatformInfoList);

        ExInitializeFastMutex(&gPlatformInfoListLock);

        //
        // After WPP is enabled for the driver project, WPP creates a default log.
        // The buffer size of the default log to which WPP prints is 4096 bytes.
        // For a debug build, the buffer is 24576 bytes.
        //
        WPP_INIT_TRACING(DriverObject, RegistryPath);

        //
        // If the default log buffer fails to allocate, trace messages are
        // sent to the WPP but can't be recorded through IFR.
        //
        if (WppRecorderIsDefaultLogAvailable()) {
            RECORDER_CONFIGURE_PARAMS recorderConfig;
            RECORDER_CONFIGURE_PARAMS_INIT(&recorderConfig);
            WppRecorderConfigure(&recorderConfig);
            DriverLogHandle = WppRecorderLogGetDefault();
        }

#if DBG
        //
        // Enable verbose/trace messages only for debug builds.
        //
        WPP_CONTROL(WPP_BIT_DEFAULT).AutoLogVerboseEnabled = TRUE;
        WPP_CONTROL(WPP_BIT_ENTEREXIT).AutoLogVerboseEnabled = TRUE;
#endif
    }

    MSHC_LOG_TRACE(
        DriverLogHandle,
        NULL,
        "DriverObject:0x%p",
        DriverObject);

    SDPORT_INITIALIZATION_DATA InitializationData;

    RtlZeroMemory(&InitializationData, sizeof(InitializationData));
    InitializationData.StructureSize = sizeof(InitializationData);

    //
    // Initialize the entry points/callbacks for the miniport interface.
    //
    InitializationData.GetSlotCount = MshcGetSlotCount;
    InitializationData.GetSlotCapabilities = MshcGetSlotCapabilities;
    InitializationData.Initialize = MshcSlotInitialize;
    InitializationData.IssueBusOperation = MshcSlotIssueBusOperation;
    InitializationData.GetCardDetectState = MshcSlotGetCardDetectState;
    InitializationData.GetWriteProtectState = MshcSlotGetWriteProtectState;
    InitializationData.Interrupt = MshcSlotInterrupt;
    InitializationData.IssueRequest = MshcSlotIssueRequest;
    InitializationData.GetResponse = MshcSlotGetResponse;
    InitializationData.ToggleEvents = MshcSlotToggleEvents;
    InitializationData.ClearEvents = MshcSlotClearEvents;
    InitializationData.RequestDpc = MshcRequestDpc;
    InitializationData.SaveContext = MshcSaveContext;
    InitializationData.RestoreContext = MshcRestoreContext;
    InitializationData.PowerControlCallback = MshcPoFxPowerControlCallback;
    InitializationData.Cleanup = MshcCleanup;

    InitializationData.PrivateExtensionSize = sizeof(MSHC_EXTENSION);
    InitializationData.CrashdumpSupported = TRUE;

    //
    // Hook up the IRP dispatch routines.
    //
    Status = SdPortInitialize(DriverObject, RegistryPath, &InitializationData);
    if (!NT_SUCCESS(Status)) {
        MSHC_LOG_ERROR(
            DriverLogHandle,
            NULL,
            "SdPortInitialize() failed. Status: %!STATUS!",
            Status);

        if (!gCrashdumpMode) {
            WPP_CLEANUP(DriverObject);
        }
    }

    return Status;
}

//
// sdport DDI callbacks
//

_Use_decl_annotations_
NTSTATUS
MshcGetSlotCount(
    _In_ PSD_MINIPORT Miniport,
    _Out_ PUCHAR SlotCount
    )
{
    NTSTATUS Status;

    MSHC_LOG_ENTER(DriverLogHandle, NULL, "()");

    // Don't expect this to be called in crashdump mode!
    NT_ASSERT(!gCrashdumpMode);

    switch (Miniport->ConfigurationInfo.BusType) {
    case SdBusTypeAcpi:
        //
        // Only support a single slot.
        // Oh, also, sdport crashes with any other value.
        //
        *SlotCount = 1;

        Status = MshcMiniportInitialize(Miniport);
        break;

    default:
        *SlotCount = 0;
        Status = STATUS_INVALID_PARAMETER;
        break;
    }

    MSHC_LOG_EXIT(DriverLogHandle, NULL, "()");
    return Status;
}

_Use_decl_annotations_
VOID
MshcGetSlotCapabilities(
    _In_ PVOID PrivateExtension,
    _Out_ PSDPORT_CAPABILITIES Capabilities
    )
{
    PMSHC_EXTENSION MshcExtension;

    MshcExtension = (PMSHC_EXTENSION)PrivateExtension;
    RtlCopyMemory(Capabilities,
                  &MshcExtension->Capabilities,
                  sizeof(MshcExtension->Capabilities));
}

_Use_decl_annotations_
NTSTATUS
MshcSlotInitialize(
    _In_ PVOID PrivateExtension,
    _In_ PHYSICAL_ADDRESS PhysicalBase,
    _In_ PVOID VirtualBase,
    _In_ ULONG Length,
    _In_ BOOLEAN CrashdumpMode
    )
{
    NTSTATUS Status;
    PMSHC_EXTENSION MshcExtension;
    PSD_MINIPORT Miniport;
    PDEVICE_OBJECT MiniportFdo;
    PMSHC_PLATFORM_CAPABILITIES PlatformCapabilities;
    PMSHC_PLATFORM_DRIVER_DATA PlatformDriverData;
    PSDPORT_CAPABILITIES Capabilities;
    ULONG HconReg;

    MSHC_LOG_ENTER(DriverLogHandle, NULL, "()");

    MSHC_LOG_INFO(
        DriverLogHandle,
        NULL,
        "PhysicalBase:0x%p, VirtualBase:0x%p, Length:%lu, CrashdumpMode:%!bool!",
        PVOID(PhysicalBase.LowPart),
        VirtualBase,
        Length,
        CrashdumpMode);

    Status = STATUS_SUCCESS;

    MshcExtension = (PMSHC_EXTENSION)PrivateExtension;
    Miniport = MINIPORT_FROM_SLOTEXT(PrivateExtension);
    MiniportFdo = (PDEVICE_OBJECT)Miniport->ConfigurationInfo.DeviceObject;
    PlatformCapabilities = NULL;

    //
    // Initialize the MSHC_EXTENSION register space.
    //
    MshcExtension->PhysicalAddress = (PVOID)PhysicalBase.QuadPart;
    MshcExtension->BaseAddress = VirtualBase;

    //
    // Check whether the driver is in crashdump mode.
    //
    if (!CrashdumpMode) {
        // Initialize IFR log for this slot.
        MshcLogInit(MshcExtension);

        // Allocate a work item for commands with busy signaling.
        MshcExtension->CompleteRequestBusyWorkItem = IoAllocateWorkItem(MiniportFdo);

        // Initialize the DPC that handles local request completions.
        KeInitializeDpc(&MshcExtension->LocalRequestDpc, MshcLocalRequestDpc, MshcExtension);

    } else {
        //
        // Recover platform data list from normal driver image still in memory.
        //
        if (gPlatformInfoList == NULL) {
            //
            // sdport saves most of the previous miniport configuration data,
            // including a pointer to its FDO.
            //
            if (MiniportFdo != NULL) {
                gPlatformInfoList = (PLIST_ENTRY)IoGetDriverObjectExtension(
                    MiniportFdo->DriverObject,
                    MSHC_PLATFORM_INFO_LIST_DRIVER_EXT_ID);
            }

            if (gPlatformInfoList == NULL) {
                Status = STATUS_DEVICE_CONFIGURATION_ERROR;
                NT_ASSERTMSG("Normal driver context lost!", FALSE);
                goto Exit;
            }
        }
    }

    //
    // Process platform info.
    //
    if (!CrashdumpMode) {
        ExAcquireFastMutex(&gPlatformInfoListLock);
    }

    for (PLIST_ENTRY Entry = gPlatformInfoList->Flink;
         Entry != gPlatformInfoList;
         Entry = Entry->Flink) {
        PMSHC_PLATFORM_INFO_LIST_ENTRY PlatformInfoEntry =
            CONTAINING_RECORD(Entry, MSHC_PLATFORM_INFO_LIST_ENTRY, ListEntry);

        if (PlatformInfoEntry->MiniportFdo != MiniportFdo) {
            continue;
        }

        MshcExtension->PlatformInfo = &PlatformInfoEntry->PlatformInfo;
        PlatformCapabilities = &MshcExtension->PlatformInfo->Capabilities;

        //
        // If FIFO depth was not supplied, try to determine it from hardware.
        // RX_WMark is FIFO_DEPTH - 1 at power-on, though it may have been
        // clobbered by early firmware.
        //
        if (PlatformCapabilities->FifoDepth == 0) {
            ULONG Fifoth = MshcReadRegister(MshcExtension, MSHC_FIFOTH);

            PlatformCapabilities->FifoDepth =
                ((Fifoth & MSHC_FIFOTH_RX_WMARK_MASK) >> MSHC_FIFOTH_RX_WMARK_SHIFT) + 1;
        }
    }

    if (!CrashdumpMode) {
        ExReleaseFastMutex(&gPlatformInfoListLock);
    }

    if (PlatformCapabilities == NULL) {
        Status = STATUS_DEVICE_CONFIGURATION_ERROR;
        MSHC_LOG_ERROR(
            MshcExtension->LogHandle,
            MshcExtension,
            "Platform info not found!");
        goto Exit;
    }

    //
    // Get supported platform operations.
    // This has to be part of the slot extension rather than the global
    // platform info, because that is shared between the normal/crashdump
    // driver images while function pointers are specific to the current image.
    //
    PlatformDriverData = MshcGetPlatformDriverData(MshcExtension->PlatformInfo->Type);
    if (PlatformDriverData == NULL) {
        Status = STATUS_NOT_SUPPORTED;
        MSHC_LOG_ERROR(DriverLogHandle, NULL, "Failed to get platform driver data!");
        goto Exit;
    }
    MshcExtension->PlatformOperations = &PlatformDriverData->Operations;

    //
    // Get IP core version.
    //
    MshcExtension->CoreVersion = MshcReadRegister(MshcExtension, MSHC_VERID);

    MSHC_LOG_INFO(
        MshcExtension->LogHandle,
        MshcExtension,
        "MSHC Core Version: %lx",
        MshcExtension->CoreVersion);

    //
    // Initialize host capabilities.
    //
    Capabilities = (PSDPORT_CAPABILITIES) &MshcExtension->Capabilities;
    HconReg = MshcReadRegister(MshcExtension, MSHC_HCON);

    Capabilities->SpecVersion = 3;
    Capabilities->MaximumOutstandingRequests = 1;

    Capabilities->MaximumBlockSize = 0xFFFF;
    Capabilities->MaximumBlockCount = 0xFFFF;

    ULONG DmaInterface = (HconReg & MSHC_HCON_DMA_INTERFACE_MASK) >> MSHC_HCON_DMA_INTERFACE_SHIFT;
    switch (DmaInterface) {
    case MSHC_HCON_DMA_INTERFACE_IDMAC:
        if ((HconReg & MSHC_HCON_ADDR_CONFIG_IDMAC64) == 0) {
            Capabilities->DmaDescriptorSize = sizeof(MSHC_IDMAC_DESCRIPTOR);

            Capabilities->Supported.ScatterGatherDma = 1;
            Capabilities->Supported.Address64Bit = 0;
        } else {
            MSHC_LOG_WARN(
                MshcExtension->LogHandle,
                MshcExtension,
                "Unsupported 64-bit IDMAC");
        }
        break;

    default:
        MSHC_LOG_WARN(
            MshcExtension->LogHandle,
            MshcExtension,
            "Unsupported DMA interface: %d",
            DmaInterface);
        break;
    }

    Capabilities->AlignmentRequirement = sizeof(ULONG) - 1;

    //
    // Use PIO for small transfers.
    // TODO: Determine the optimal value here?
    //
    Capabilities->PioTransferMaxThreshold = 64;
    Capabilities->Flags.UsePioForRead = TRUE;
    Capabilities->Flags.UsePioForWrite = TRUE;

    Capabilities->Supported.AutoCmd12 = 1;
    Capabilities->Supported.AutoCmd23 = 0;

    Capabilities->BaseClockFrequencyKhz = PlatformCapabilities->ClockFrequency / 1000;

    if (PlatformCapabilities->BusWidth == 8) {
        Capabilities->Supported.BusWidth8Bit = 1;
    }

    if (PlatformCapabilities->SupportHighSpeed) {
        Capabilities->Supported.HighSpeed = 1;
    }

    if (PlatformCapabilities->SupportDdr50) {
        // XXX: DDR50 is broken in sdport.
        // Capabilities->Supported.DDR50 = 1;
    }

    if (PlatformCapabilities->SupportSdr50) {
        Capabilities->Supported.SDR50 = 1;
    }

    if (PlatformCapabilities->SupportSdr104) {
        Capabilities->Supported.SDR104 = 1;
    }

    if (!PlatformCapabilities->SupportHs200) {
        Capabilities->Supported.HS200 = 1;
    }

    //
    // When set, this flag has no effect (i.e. no tuning is actually
    // requested) other than forcing the crashdump stack to use HS mode.
    //
    // From my (limited) testing, SDR50 works fine with any phase shift
    // value on Rockchip hardware, therefore tuning is not required.
    //
    Capabilities->Supported.TuningForSDR50 = 0;

    if (!PlatformCapabilities->No18vRegulator) {
        Capabilities->Supported.SignalingVoltage18V = 1;
    }

    Capabilities->Supported.Voltage18V = 0;
    Capabilities->Supported.Voltage30V = 0;
    Capabilities->Supported.Voltage33V = 1;

    Capabilities->Supported.DriverTypeA = 1;
    Capabilities->Supported.DriverTypeB = 1;
    Capabilities->Supported.DriverTypeC = 1;
    Capabilities->Supported.DriverTypeD = 1;

    Capabilities->Supported.Limit800mA = 1;
    Capabilities->Supported.Limit600mA = 1;
    Capabilities->Supported.Limit400mA = 1;
    Capabilities->Supported.Limit200mA = 1;

    //
    // Only support slot #0.
    //
    MshcExtension->SlotId = 0;

    //
    // The single active request.
    //
    MshcExtension->OutstandingRequest = NULL;

    //
    // Make sure to start from a fresh state.
    //
    MshcResetHost(MshcExtension, SdResetTypeAll);

Exit:
    //
    // WORKAROUND:
    // We can't return failure here (sdport will crash...), so keep track
    // of the extension state and do it in MshcSlotIssueBusOperation().
    //
    MshcExtension->Initialized = NT_SUCCESS(Status);

    MSHC_LOG_EXIT(DriverLogHandle, NULL, "()");

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS
MshcSlotIssueBusOperation(
    _In_ PVOID PrivateExtension,
    _In_ PSDPORT_BUS_OPERATION BusOperation
    )
{
    PMSHC_EXTENSION MshcExtension;
    NTSTATUS Status;

    MshcExtension = (PMSHC_EXTENSION)PrivateExtension;

    // If SlotInitialize() failed, it's not safe to continue...
    if (!MshcExtension->Initialized) {
        return STATUS_DEVICE_CONFIGURATION_ERROR;
    }

    MSHC_LOG_ENTER(MshcExtension->LogHandle, MshcExtension, "()");

    MSHC_LOG_INFO(
        MshcExtension->LogHandle,
        MshcExtension,
        "Type:%!BUSOPERATIONTYPE!",
        BusOperation->Type);

    Status = STATUS_NOT_SUPPORTED;

    switch (BusOperation->Type) {
    case SdResetHw:
        break;

    case SdResetHost:
        MSHC_LOG_INFO(
            MshcExtension->LogHandle,
            MshcExtension,
            "ResetType:%!RESETTYPE!",
            BusOperation->Parameters.ResetType);

        Status = MshcResetHost(
                    MshcExtension,
                    BusOperation->Parameters.ResetType);
        break;

    case SdSetClock:
        MSHC_LOG_INFO(
            MshcExtension->LogHandle,
            MshcExtension,
            "FrequencyKhz:%lu",
            BusOperation->Parameters.FrequencyKhz);

        Status = MshcSetClock(
                    MshcExtension,
                    BusOperation->Parameters.FrequencyKhz);
        break;

    case SdSetVoltage:
        MSHC_LOG_INFO(
            MshcExtension->LogHandle,
            MshcExtension,
            "Voltage:%!BUSVOLTAGE!",
            BusOperation->Parameters.Voltage);

        Status = MshcSetVoltage(
                    MshcExtension,
                    BusOperation->Parameters.Voltage);
        break;

    case SdSetPower:
        break;

    case SdSetBusWidth:
        MSHC_LOG_INFO(
            MshcExtension->LogHandle,
            MshcExtension,
            "BusWidth:%!BUSWIDTH!",
            BusOperation->Parameters.BusWidth);

        Status = MshcSetBusWidth(
                    MshcExtension,
                    BusOperation->Parameters.BusWidth);
        break;

    case SdSetBusSpeed:
        MSHC_LOG_INFO(
            MshcExtension->LogHandle,
            MshcExtension,
            "BusSpeed:%!BUSSPEED!",
            BusOperation->Parameters.BusSpeed);

        Status = MshcSetBusSpeed(MshcExtension, BusOperation->Parameters.BusSpeed);
        break;

    case SdSetSignalingVoltage:
        MSHC_LOG_INFO(
            MshcExtension->LogHandle,
            MshcExtension,
            "SignalingVoltage:%!SIGNALINGVOLTAGE!",
            BusOperation->Parameters.SignalingVoltage);

        Status = MshcSetSignalingVoltage(
                    MshcExtension,
                    BusOperation->Parameters.SignalingVoltage);
        break;

    case SdSetDriveStrength:
        break;

    case SdSetDriverType:
        break;

    case SdSetPresetValue:
        break;

    case SdSetBlockGapInterrupt:
        break;

    case SdExecuteTuning:
        Status = MshcExecuteTuning(MshcExtension);
        break;

    default:
        Status = STATUS_INVALID_PARAMETER;
        break;
    }

    MSHC_LOG_EXIT(MshcExtension->LogHandle, MshcExtension, "%!STATUS!", Status);
    return Status;
}

_Use_decl_annotations_
BOOLEAN
MshcSlotGetCardDetectState(
    _In_ PVOID PrivateExtension
    )
{
    PMSHC_EXTENSION MshcExtension;
    BOOLEAN Present;

    MshcExtension = (PMSHC_EXTENSION)PrivateExtension;

    MSHC_LOG_ENTER(MshcExtension->LogHandle, MshcExtension, "()");

    Present = MshcIsCardInserted(MshcExtension);

    MSHC_LOG_EXIT(MshcExtension->LogHandle, MshcExtension, "%!bool!", Present);

    return Present;
}

_Use_decl_annotations_
BOOLEAN
MshcSlotGetWriteProtectState(
    _In_ PVOID PrivateExtension
    )
{
    PMSHC_EXTENSION MshcExtension;
    BOOLEAN WriteProtected;

    MshcExtension = (PMSHC_EXTENSION)PrivateExtension;

    MSHC_LOG_ENTER(MshcExtension->LogHandle, MshcExtension, "()");

    WriteProtected = MshcIsCardWriteProtected(MshcExtension);

    MSHC_LOG_EXIT(MshcExtension->LogHandle, MshcExtension, "%!bool!", WriteProtected);

    return WriteProtected;
}

_Use_decl_annotations_
BOOLEAN
MshcSlotInterrupt(
    _In_ PVOID PrivateExtension,
    _Out_ PULONG Events,
    _Out_ PULONG Errors,
    _Out_ PBOOLEAN CardChange,
    _Out_ PBOOLEAN SdioInterrupt,
    _Out_ PBOOLEAN Tuning
    )
{
    PMSHC_EXTENSION MshcExtension;
    PSDPORT_REQUEST Request;
    ULONG Rintsts;
    ULONG Idsts;
    ULONG Ctrl;
    BOOLEAN Handled;

    MshcExtension = (PMSHC_EXTENSION)PrivateExtension;

    MSHC_LOG_ENTER(MshcExtension->LogHandle, MshcExtension, "()");

    //
    // Ideally we'd read MINTSTS here to ignore masked bits,
    // but that doesn't work well with RXDR/TXDR during PIO
    // transfers.
    //
    Rintsts = MshcReadRegister(MshcExtension, MSHC_RINTSTS);
    Idsts = MshcReadRegister(MshcExtension, MSHC_IDSTS);
    Ctrl = MshcReadRegister(MshcExtension, MSHC_CTRL);

    if (Rintsts & MSHC_INT_ERROR) {
        MSHC_LOG_ERROR(
            MshcExtension->LogHandle,
            MshcExtension,
            "RINTSTS:0x%lx"
            " (EBE:%lu"
            "  ACD:%lu"
            "  SBE:%lu"
            "  HLE:%lu"
            "  FRUN:%lu"
            "  HTO:%lu"
            "  DRTO:%lu"
            "  RTO:%lu"
            "  DCRC:%lu"
            "  RCRC:%lu"
            "  RXDR:%lu"
            "  TXDR:%lu"
            "  DTO:%lu"
            "  CMD:%lu"
            "  RE:%lu"
            "  CD:%lu)",
            Rintsts,
            (Rintsts & MSHC_INT_EBE) != 0,
            (Rintsts & MSHC_INT_ACD) != 0,
            (Rintsts & MSHC_INT_SBE) != 0,
            (Rintsts & MSHC_INT_HLE) != 0,
            (Rintsts & MSHC_INT_FRUN) != 0,
            (Rintsts & MSHC_INT_HTO) != 0,
            (Rintsts & MSHC_INT_DRTO) != 0,
            (Rintsts & MSHC_INT_RTO) != 0,
            (Rintsts & MSHC_INT_DCRC) != 0,
            (Rintsts & MSHC_INT_RCRC) != 0,
            (Rintsts & MSHC_INT_RXDR) != 0,
            (Rintsts & MSHC_INT_TXDR) != 0,
            (Rintsts & MSHC_INT_DTO) != 0,
            (Rintsts & MSHC_INT_CMD) != 0,
            (Rintsts & MSHC_INT_RE) != 0,
            (Rintsts & MSHC_INT_CD) != 0);
    } else {
        MSHC_LOG_TRACE(
            MshcExtension->LogHandle,
            MshcExtension,
            "RINTSTS:0x%lx"
            " (ACD:%lu"
            "  RXDR:%lu"
            "  TXDR:%lu"
            "  DTO:%lu"
            "  CMD:%lu"
            "  CD:%lu)",
            Rintsts,
            (Rintsts & MSHC_INT_ACD) != 0,
            (Rintsts & MSHC_INT_RXDR) != 0,
            (Rintsts & MSHC_INT_TXDR) != 0,
            (Rintsts & MSHC_INT_DTO) != 0,
            (Rintsts & MSHC_INT_CMD) != 0,
            (Rintsts & MSHC_INT_CD) != 0);
    }

    if (Idsts & MSHC_IDINT_AIS) {
        MSHC_LOG_ERROR(
            MshcExtension->LogHandle,
            MshcExtension,
            "IDSTS:0x%lx"
            " (AIS:%lu"
            "  NIS:%lu"
            "  CES:%lu"
            "  DUI:%lu"
            "  FBE:%lu"
            "  RI:%lu"
            "  TI:%lu)",
            Idsts,
            (Idsts & MSHC_IDINT_AIS) != 0,
            (Idsts & MSHC_IDINT_NIS) != 0,
            (Idsts & MSHC_IDINT_CES) != 0,
            (Idsts & MSHC_IDINT_DUI) != 0,
            (Idsts & MSHC_IDINT_FBE) != 0,
            (Idsts & MSHC_IDINT_RI) != 0,
            (Idsts & MSHC_IDINT_TI) != 0);
    } else {
        MSHC_LOG_TRACE(
            MshcExtension->LogHandle,
            MshcExtension,
            "IDSTS:0x%lx"
            " (NIS:%lu"
            "  RI:%lu"
            "  TI:%lu)",
            Idsts,
            (Idsts & MSHC_IDINT_NIS) != 0,
            (Idsts & MSHC_IDINT_RI) != 0,
            (Idsts & MSHC_IDINT_TI) != 0);
    }

    *Events = 0;
    *Errors = 0;
    *CardChange = FALSE;
    *SdioInterrupt = FALSE;
    *Tuning = FALSE;

    //
    // If there aren't any events to handle or interrupt signals
    // are disabled, then we don't need to process anything.
    //
    if (!Rintsts && !Idsts || ((Ctrl & MSHC_CTRL_INT_ENABLE) == 0)) {
        MSHC_LOG_TRACE(MshcExtension->LogHandle, MshcExtension, "Spurious interrupt");
        return FALSE;
    }

    //
    // Acknowledge/clear interrupt status. Request completions will occur in
    // the port driver's slot completion DPC.
    //
    MshcAcknowledgeInterrupts(MshcExtension, Rintsts, Idsts);

    Request = (PSDPORT_REQUEST)InterlockedCompareExchangePointer(
                (PVOID volatile *)&MshcExtension->OutstandingRequest, NULL, NULL);
    if (Request != NULL) {
        if (Request->Command.Index == SDCMD_VOLTAGE_SWITCH) {
            //
            // HTO after CMD11 means the command has completed.
            //
            if (Rintsts & MSHC_INT_HTO) {
                Rintsts &= ~MSHC_INT_HTO;
                Rintsts |= MSHC_INT_CMD;
            }
        }
    }

    //
    // RXDR/TXDR can only be cleared after the data is transferred.
    // Mask them here to prevent an interrupt storm.
    //
    if (Rintsts & (MSHC_INT_RXDR | MSHC_INT_TXDR)) {
        MshcDisableInterrupts(MshcExtension, MSHC_INT_RXDR | MSHC_INT_TXDR, 0);
    }

    if (Rintsts & MSHC_INT_CD) {
        *CardChange = TRUE;
        Rintsts &= ~MSHC_INT_CD;
    }

    MshcConvertIntStatusToStandardEvents(Rintsts, Idsts, Events, Errors);

    Handled = (*Events != 0) ||
              (*Errors != 0) ||
              (*CardChange == TRUE) ||
              (*SdioInterrupt == TRUE) ||
              (*Tuning == TRUE);

    if (Handled && !gCrashdumpMode) {
        if (InterlockedCompareExchange(&MshcExtension->LocalRequestPending, 0, 0)) {
            //
            // We may not get a DPC from sdport when issuing local requests.
            // Queue a custom DPC instead.
            //
            InterlockedOr((PLONG)&MshcExtension->CurrentEvents, *Events);
            InterlockedOr((PLONG)&MshcExtension->CurrentErrors, *Errors);

            KeInsertQueueDpc(&MshcExtension->LocalRequestDpc, NULL, NULL);

            //
            // Actually ensure we don't get a DpcForIsr at the same time.
            // Let sdport sense card changes but not anything else.
            //
            Handled = (*CardChange == TRUE);
            *Events = 0;
            *Errors = 0;
        }
    }

    MSHC_LOG_EXIT(MshcExtension->LogHandle, MshcExtension, "%!bool!", Handled);
    return Handled;
}

_Use_decl_annotations_
NTSTATUS
MshcSlotIssueRequest(
    _In_ PVOID PrivateExtension,
    _In_ PSDPORT_REQUEST Request
    )
{
    PMSHC_EXTENSION MshcExtension;
    NTSTATUS Status;

    MshcExtension = (PMSHC_EXTENSION)PrivateExtension;

    MSHC_LOG_ENTER(MshcExtension->LogHandle, MshcExtension, "()");

    if (Request->Type == SdRequestTypeCommandNoTransfer) {
        MSHC_LOG_INFO(
            MshcExtension->LogHandle,
            MshcExtension,
            "Type:%!REQUESTTYPE! %s%d(0x%08X)",
            Request->Type,
            (Request->Command.Class == SdCommandClassApp ? "ACMD" : "CMD"),
            Request->Command.Index,
            Request->Command.Argument);
    } else {
        MSHC_LOG_INFO(
            MshcExtension->LogHandle,
            MshcExtension,
            "Type:%!REQUESTTYPE! %s%d(0x%08X) %s %s(Blocks:%lu, Length:%lu)",
            Request->Type,
            (Request->Command.Class == SdCommandClassApp ? "ACMD" : "CMD"),
            Request->Command.Index,
            Request->Command.Argument,
            Request->Command.TransferMethod == SdTransferMethodSgDma ? "DMA" : "PIO",
            (Request->Command.TransferDirection == SdTransferDirectionRead ? "Read" : "Write"),
            Request->Command.BlockCount,
            Request->Command.Length);
    }

    //
    // Acquire the outstanding request.
    //
    if (InterlockedExchangePointer(
        (PVOID volatile *)&MshcExtension->OutstandingRequest, Request) != NULL) {
        MSHC_LOG_WARN(MshcExtension->LogHandle, MshcExtension, "Aborted previous request.");
    }

    //
    // Dispatch the request based off of the request type.
    //
    switch (Request->Type) {
    case SdRequestTypeCommandNoTransfer:
    case SdRequestTypeCommandWithTransfer:
        Status = MshcSendCommand(MshcExtension, Request);
        break;

    case SdRequestTypeStartTransfer:
        Status = MshcStartTransfer(MshcExtension, Request);
        break;

    default:
        Status = STATUS_NOT_SUPPORTED;
        break;
    }

    MSHC_LOG_EXIT(MshcExtension->LogHandle, MshcExtension, "%!STATUS!", Status);
    return Status;
}

_Use_decl_annotations_
VOID
MshcSlotGetResponse(
    _In_ PVOID PrivateExtension,
    _In_ PSDPORT_COMMAND Command,
    _Out_ PVOID ResponseBuffer
    )
{
    PMSHC_EXTENSION MshcExtension;
    ULONG LongResponseBuffer[4];
    PULONG Response;

    MshcExtension = (PMSHC_EXTENSION)PrivateExtension;

    MSHC_LOG_ENTER(MshcExtension->LogHandle, MshcExtension, "()");

    MSHC_LOG_INFO(
        MshcExtension->LogHandle,
        MshcExtension,
        "%s%d(0x%08X)",
        (Command->Class == SdCommandClassApp ? "ACMD" : "CMD"),
        Command->Index,
        Command->Argument);

    Response = (PULONG)ResponseBuffer;
    *(PULONG)(ResponseBuffer) = 0;

    switch (Command->ResponseType) {
    case SdResponseTypeR1:
    case SdResponseTypeR3:
    case SdResponseTypeR4:
    case SdResponseTypeR5:
    case SdResponseTypeR6:
    case SdResponseTypeR1B:
    case SdResponseTypeR5B:
        Response[0] = MshcReadRegister(MshcExtension, MSHC_RESP0);

        MSHC_LOG_INFO(
            MshcExtension->LogHandle,
            MshcExtension,
            "RESP[0]: 0x%08X",
            Response[0]);
        break;

    case SdResponseTypeR2:
        LongResponseBuffer[0] = MshcReadRegister(MshcExtension, MSHC_RESP0);
        LongResponseBuffer[1] = MshcReadRegister(MshcExtension, MSHC_RESP1);
        LongResponseBuffer[2] = MshcReadRegister(MshcExtension, MSHC_RESP2);
        LongResponseBuffer[3] = MshcReadRegister(MshcExtension, MSHC_RESP3);

        //
        // Right shift the response one byte to discard the CRC + end bit,
        // since sdport does not expect them.
        //
        RtlCopyMemory(
            ResponseBuffer,
            (UCHAR*)(LongResponseBuffer) + 1,
            sizeof(LongResponseBuffer) - 1);

        MSHC_LOG_INFO(
            MshcExtension->LogHandle,
            MshcExtension,
            "RESP[0-3]: 0x%08X, 0x%08X, 0x%08X, 0x%08X",
            LongResponseBuffer[0],
            LongResponseBuffer[1],
            LongResponseBuffer[2],
            LongResponseBuffer[3]);
        break;

    case SdResponseTypeNone:
        break;

    default:
        NT_ASSERTMSG("Invalid response type!", FALSE);
    }

    if (Command->Index == SDCMD_SEND_RELATIVE_ADDR) {
        MshcExtension->CardRca = Response[0] >> 16;
    }

    MSHC_LOG_EXIT(MshcExtension->LogHandle, MshcExtension, "()");
}

_Use_decl_annotations_
VOID
MshcSlotToggleEvents(
    _In_ PVOID PrivateExtension,
    _In_ ULONG EventMask,
    _In_ BOOLEAN Enable
    )
{
    ULONG Intsts;
    ULONG Idsts;

    PMSHC_EXTENSION MshcExtension;

    MshcExtension = (PMSHC_EXTENSION)PrivateExtension;

    MSHC_LOG_ENTER(
        MshcExtension->LogHandle,
        MshcExtension,
        "EventMask:%08X, Enable:%!bool!",
        EventMask,
        Enable);

    MshcConvertStandardEventsToIntStatus(EventMask, 0xFFFF, &Intsts, &Idsts);

    //
    // We only care about HTO in the context of CMD11.
    // During PIO transfers, it's merely an indicator of the driver not
    // handling the FIFO in time (CPU busy doing something else?), i.e.
    // we should continue normal operation and not treat it as an error.
    //
    Intsts |= MSHC_INT_HTO;

    if (Enable) {
        MshcEnableInterrupts(MshcExtension, Intsts, Idsts);
    } else {
        MshcDisableInterrupts(MshcExtension, Intsts, Idsts);
    }

    MSHC_LOG_EXIT(MshcExtension->LogHandle, MshcExtension, "()");
}

_Use_decl_annotations_
VOID
MshcSlotClearEvents(
    _In_ PVOID PrivateExtension,
    _In_ ULONG EventMask
    )
{
    ULONG Intsts;
    ULONG Idsts;
    PMSHC_EXTENSION MshcExtension;

    MshcExtension = (PMSHC_EXTENSION)PrivateExtension;

    MSHC_LOG_ENTER(
        MshcExtension->LogHandle,
        MshcExtension,
        "EventMask:0x%08X",
        EventMask);

    MshcConvertStandardEventsToIntStatus(EventMask, 0xFFFF, &Intsts, &Idsts);

    MshcAcknowledgeInterrupts(MshcExtension, Intsts, Idsts);

    MSHC_LOG_EXIT(MshcExtension->LogHandle, MshcExtension, "()");
}

_Use_decl_annotations_
VOID
MshcRequestDpc(
    _In_ PVOID PrivateExtension,
    _Inout_ PSDPORT_REQUEST Request,
    _In_ ULONG Events,
    _In_ ULONG Errors
    )
{
    PMSHC_EXTENSION MshcExtension;

    MshcExtension = (PMSHC_EXTENSION)PrivateExtension;

    //
    // WORKAROUND:
    // This can occur when we're delaying request completion in a thread.
    //
    if (((Events == 0) && (Errors == 0)) || (Request->RequiredEvents == 0)) {
        return;
    }

    MSHC_LOG_ENTER(MshcExtension->LogHandle, MshcExtension, "()");

    MSHC_LOG_INFO(
        MshcExtension->LogHandle,
        MshcExtension,
        "RequiredEvents:0x%08X Events:0x%08X Errors:0x%08X",
        Request->RequiredEvents,
        Events,
        Errors);

    //
    // Save current events, since we may not be waiting for them
    // at this stage, but we may be on the next phase of the command processing.
    // For instance with short data read requests, the transfer is probably
    // completed by the time the command is done.
    // In that case if we wait for those events after they've already arrived,
    // we will fail with timeout.
    //
    InterlockedOr((PLONG)&MshcExtension->CurrentEvents, Events);
    InterlockedOr((PLONG)&MshcExtension->CurrentErrors, Errors);

    //
    // Check for out of sequence call?
    // SDPORT does not maintain a request state, so we may get a request that
    // has not been issued yet!
    //
    if (InterlockedExchangePointer(
        (PVOID volatile *)&MshcExtension->OutstandingRequest,
        MshcExtension->OutstandingRequest) == NULL) {
        MSHC_LOG_WARN(MshcExtension->LogHandle, MshcExtension, "Out of sequence request! Ignoring.");
        goto Exit;
    }

    //
    // Clear the request's required events if they have completed.
    //
    Request->RequiredEvents &= ~Events;

    //
    // If there are errors, we need to fail whatever outstanding request
    // was on the bus. Otherwise, the request succeeded.
    //
    Errors = InterlockedCompareExchange((PLONG)&MshcExtension->CurrentErrors, 0, 0);
    if (Errors) {
        //
        // Wait for CMD/DTO interrupt after timeout before completing the request.
        //
        if (((Errors & SDPORT_ERROR_CMD_TIMEOUT) && !(Events & SDPORT_EVENT_CARD_RESPONSE)) ||
            ((Errors & SDPORT_ERROR_DATA_TIMEOUT) && !(Events & SDPORT_EVENT_CARD_RW_END))) {
            goto Exit;
        }

        if (MshcExtension->TuningPerformed &&
            (Errors & SDPORT_ERROR_DATA_CRC_ERROR)) {
            MshcExtension->DataCrcErrorsSinceLastTuning++;
        }

        Request->RequiredEvents = 0;
        InterlockedAnd((PLONG)&MshcExtension->CurrentEvents, 0);

        Request->Status = MshcConvertStandardErrorToStatus(Errors);
        InterlockedAnd((PLONG)&MshcExtension->CurrentErrors, 0);

        MshcCompleteRequest(MshcExtension, Request, Request->Status);

    } else if (Request->RequiredEvents == 0) {
        if (Request->Status != STATUS_MORE_PROCESSING_REQUIRED) {
            Request->Status = STATUS_SUCCESS;
        }

        MshcCompleteRequest(MshcExtension, Request, Request->Status);
    }

Exit:
    MSHC_LOG_EXIT(MshcExtension->LogHandle, MshcExtension, "()");
}

_Use_decl_annotations_
VOID
MshcSaveContext(
    _In_ PVOID PrivateExtension
    )
{
    PMSHC_EXTENSION MshcExtension;

    MshcExtension = (PMSHC_EXTENSION)PrivateExtension;

    MSHC_LOG_ENTER(MshcExtension->LogHandle, MshcExtension, "()");
    MSHC_LOG_EXIT(MshcExtension->LogHandle, MshcExtension, "()");
}

_Use_decl_annotations_
VOID
MshcRestoreContext(
    _In_ PVOID PrivateExtension
    )
{
    PMSHC_EXTENSION MshcExtension;

    MshcExtension = (PMSHC_EXTENSION)PrivateExtension;

    MSHC_LOG_ENTER(MshcExtension->LogHandle, MshcExtension, "()");
    MSHC_LOG_EXIT(MshcExtension->LogHandle, MshcExtension, "()");
}

_Use_decl_annotations_
NTSTATUS
MshcPoFxPowerControlCallback(
    _In_ PSD_MINIPORT Miniport,
    _In_ LPCGUID PowerControlCode,
    _In_reads_bytes_opt_(InputBufferSize) PVOID InputBuffer,
    _In_ SIZE_T InputBufferSize,
    _Out_writes_bytes_opt_(OutputBufferSize) PVOID OutputBuffer,
    _In_ SIZE_T OutputBufferSize,
    _Out_opt_ PSIZE_T BytesReturned
    )
{
    UNREFERENCED_PARAMETER(Miniport);
    UNREFERENCED_PARAMETER(PowerControlCode);
    UNREFERENCED_PARAMETER(InputBuffer);
    UNREFERENCED_PARAMETER(InputBufferSize);
    UNREFERENCED_PARAMETER(OutputBuffer);
    UNREFERENCED_PARAMETER(OutputBufferSize);
    UNREFERENCED_PARAMETER(BytesReturned);

    MSHC_LOG_ENTER(DriverLogHandle, NULL, "()");
    MSHC_LOG_EXIT(DriverLogHandle, NULL, "()");

    return STATUS_NOT_IMPLEMENTED;
}

_Use_decl_annotations_
VOID
MshcCleanup(
    _In_ PSD_MINIPORT Miniport
    )
{
    MSHC_LOG_ENTER(DriverLogHandle, NULL, "()");

    if (!gCrashdumpMode) {
        ExAcquireFastMutex(&gPlatformInfoListLock);
    }

    if (!IsListEmpty(gPlatformInfoList)) {
        for (PLIST_ENTRY Entry = gPlatformInfoList->Flink;
             Entry != gPlatformInfoList;
             Entry = Entry->Flink) {

            PMSHC_PLATFORM_INFO_LIST_ENTRY PlatformInfoEntry =
                CONTAINING_RECORD(Entry, MSHC_PLATFORM_INFO_LIST_ENTRY, ListEntry);

            if (PlatformInfoEntry->MiniportFdo != Miniport->ConfigurationInfo.DeviceObject) {
                continue;
            }

            RemoveEntryList(Entry);
            ExFreePoolWithTag(PlatformInfoEntry, MSHC_ALLOC_TAG);
        }
    }

    if (!gCrashdumpMode) {
        ExReleaseFastMutex(&gPlatformInfoListLock);
    }

    for (ULONG Index = 0; Index < Miniport->SlotCount; Index++) {
        PMSHC_EXTENSION MshcExtension =
            (PMSHC_EXTENSION)Miniport->SlotExtensionList[Index]->PrivateExtension;

        if (MshcExtension->CompleteRequestBusyWorkItem) {
            IoFreeWorkItem(MshcExtension->CompleteRequestBusyWorkItem);
        }

        MshcLogCleanup(MshcExtension);
    }

    if (IsListEmpty(gPlatformInfoList)) {
        PDEVICE_OBJECT Fdo = (PDEVICE_OBJECT)Miniport->ConfigurationInfo.DeviceObject;
        WPP_CLEANUP(Fdo->DriverObject);
    }
}

//
// Bus operation routines
//

_Use_decl_annotations_
NTSTATUS
MshcResetHost(
    _In_ PMSHC_EXTENSION MshcExtension,
    _In_ SDPORT_RESET_TYPE ResetType
    )
{
    NTSTATUS Status;
    PSDPORT_REQUEST Request;
    ULONG ResetMask;

    Request = (PSDPORT_REQUEST)InterlockedExchangePointer(
        (PVOID volatile *)&MshcExtension->OutstandingRequest, NULL);
    if (Request != NULL) {
        MSHC_LOG_WARN(MshcExtension->LogHandle, MshcExtension, "Aborted outstanding request.");
    }

    //
    // Better reset everything no matter the requested type, just in case.
    //
    ResetMask = MSHC_CTRL_CONTROLLER_RESET | MSHC_CTRL_FIFO_RESET;
    if (MshcExtension->Capabilities.Supported.ScatterGatherDma) {
        ResetMask |= MSHC_CTRL_DMA_RESET;
    }

    switch (ResetType) {
    case SdResetTypeAll:
        break;

    case SdResetTypeCmd:
        //
        // Return early in case of a failed data transfer.
        // SdResetTypeCmd is requested with interrupts masked off, which
        // means we can't issue any local commands, such as stop transmission
        // or retuning.
        //
        // sdport will call again with SdResetTypeDat (after unmasking interrupts)
        // and we will do the entire reset sequence there.
        //
        if ((Request != NULL) &&
            (Request->Command.TransferType != SdTransferTypeNone) &&
            (Request->Command.TransferType != SdTransferTypeUndefined)) {
            return STATUS_SUCCESS;
        }
        break;

    case SdResetTypeDat:
        //
        // End any pending transfer so the controller and card FSM
        // can gracefully return to idle before a reset.
        // This is sometimes needed even for single-block transfers
        // (e.g. during tuning).
        //
        MshcSendStopCommand(MshcExtension);
        break;

    default:
        NT_ASSERTMSG("Invalid reset type!", FALSE);
        return STATUS_INVALID_PARAMETER;
    }

    if (ResetMask) {
        Status = MshcResetWait(MshcExtension, ResetMask);
        if (!NT_SUCCESS(Status)) {
            return Status;
        }

        // Clock registers need to be synced after a controller reset.
        if (ResetMask & MSHC_CTRL_CONTROLLER_RESET) {
            MshcCiuUpdateClock(MshcExtension, 0);
        }

        if (ResetMask & MSHC_CTRL_DMA_RESET) {
            MshcWriteRegister(MshcExtension, MSHC_BMOD, MSHC_BMOD_SWR);

            ULONG Ctrl = MshcReadRegister(MshcExtension, MSHC_CTRL);
            Ctrl &= ~MSHC_CTRL_USE_INTERNAL_DMAC;
            MshcWriteRegister(MshcExtension, MSHC_CTRL, Ctrl);
        }
    }

    if (ResetType == SdResetTypeAll) {
        //
        // WORKAROUND:
        // In crashdump mode, sdport will switch to an UHS-I mode
        // without issuing CMD11 or touching the signaling voltage.
        // Don't attempt to reset the voltage here either and hope
        // for the best...
        //
        if (!gCrashdumpMode) {
            //
            // Reset voltages.
            // We don't trust sdport to always do it when needed.
            //
            MshcSetVoltage(MshcExtension, SdBusVoltageOff);
            MshcSetSignalingVoltage(MshcExtension, SdSignalingVoltage33);
        }

        // Disable clock
        MshcSetClock(MshcExtension, 0);

        // Reset bus width
        MshcSetBusWidth(MshcExtension, SdBusWidth1Bit);

        // Mask all interrupts, sdport will toggle them as needed.
        MshcDisableInterrupts(MshcExtension, MSHC_INT_ALL, MSHC_IDINT_ALL);

        // Global interrupt enable
        MshcWriteRegister(MshcExtension, MSHC_CTRL, MSHC_CTRL_INT_ENABLE);

        // Set the max HW timeout for bus operations.
        MshcWriteRegister(MshcExtension, MSHC_TMOUT, 0xffffffff);
    }

    // Acknowledge any pending interrupts.
    MshcAcknowledgeInterrupts(MshcExtension, MSHC_INT_ALL, MSHC_IDINT_ALL);

    //
    // Perform retuning if the data CRC error threshold has been reached.
    // This might happen due to changes in temperature over time.
    //
    if ((ResetType == SdResetTypeDat) &&
        MshcExtension->TuningPerformed &&
        (MshcExtension->DataCrcErrorsSinceLastTuning >= MSHC_DCRC_ERROR_RETUNING_THRESHOLD)) {
        MSHC_LOG_WARN(
            MshcExtension->LogHandle,
            MshcExtension,
            "Too many data CRC errors! Retuning...");

        MshcExecuteTuning(MshcExtension);
    }

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS
MshcSetVoltage(
    _In_ PMSHC_EXTENSION MshcExtension,
    _In_ SDPORT_BUS_VOLTAGE Voltage
    )
{
    NTSTATUS Status;
    PMSHC_PLATFORM_OPERATIONS PlatformOperations;
    ULONG Pwren;

    Status = STATUS_SUCCESS;
    PlatformOperations = MshcExtension->PlatformOperations;

    //
    // Set the internal regulator first.
    //
    Pwren = MshcReadRegister(MshcExtension, MSHC_PWREN);

    switch (Voltage) {
    case SdBusVoltage33:
        Pwren |= MSHC_PWREN_POWER_ENABLE(MshcExtension->SlotId);
        MshcExtension->CardInitialized = FALSE;
        break;

    case SdBusVoltageOff:
        Pwren &= ~MSHC_PWREN_POWER_ENABLE(MshcExtension->SlotId);
        break;

    default:
        NT_ASSERTMSG("Voltage profile invalid!", FALSE);
        return STATUS_INVALID_PARAMETER;
    }

    //
    // Some controllers have an external regulator.
    //
    if (PlatformOperations->SetVoltage != NULL) {
        Status = PlatformOperations->SetVoltage(MshcExtension, Voltage);
    }

    MshcExtension->CardPowerControlSupported = NT_SUCCESS(Status);

    //
    // Wait 10ms for regulator to stabilize.
    //
    SdPortWait(10000);

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS
MshcSetClock(
    _In_ PMSHC_EXTENSION MshcExtension,
    _In_ ULONG FrequencyKhz
    )
{
    NTSTATUS Status;
    PMSHC_PLATFORM_OPERATIONS PlatformOperations;
    PMSHC_PLATFORM_CAPABILITIES PlatformCapabilities;
    ULONG Clkena;
    ULONG Divisor = 0;

    PlatformOperations = MshcExtension->PlatformOperations;
    PlatformCapabilities = &MshcExtension->PlatformInfo->Capabilities;

    Clkena = MshcReadRegister(MshcExtension, MSHC_CLKENA);
    Clkena &= ~MSHC_CLKENA_CCLK_ENABLE(MshcExtension->SlotId);
    Clkena &= ~MSHC_CLKENA_CCLK_LOW_POWER(MshcExtension->SlotId);
    MshcWriteRegister(MshcExtension, MSHC_CLKENA, Clkena);

    //
    // These are also slot specific but we don't know how
    // they may be wired, so assume there's only one divider source.
    //
    MshcWriteRegister(MshcExtension, MSHC_CLKSRC, 0);
    MshcWriteRegister(MshcExtension, MSHC_CLKDIV, 0);

    Status = MshcCiuUpdateClock(MshcExtension, 0);
    if (!NT_SUCCESS(Status)) {
        return Status;
    }

    if (FrequencyKhz > 0) {
        //
        // WORKAROUND:
        // sdport, go home, you're drunk...
        //
        if (gCrashdumpMode) {
            ULONG CorrectFrequencyKhz = FrequencyKhz;

            switch (MshcExtension->BusSpeed) {
            case SdBusSpeedNormal:
                CorrectFrequencyKhz = 25000;
                break;
            case SdBusSpeedHigh:
            case SdBusSpeedDDR50:
                CorrectFrequencyKhz = 50000;
                break;
            case SdBusSpeedSDR50:
                CorrectFrequencyKhz = 100000;
                break;
            }

            if (CorrectFrequencyKhz != FrequencyKhz) {
                MSHC_LOG_WARN(
                    MshcExtension->LogHandle,
                    MshcExtension,
                    "CorrectFrequencyKhz: %u",
                    CorrectFrequencyKhz);

                FrequencyKhz = CorrectFrequencyKhz;
            }
        }

        if ((PlatformCapabilities->MaxFrequency != 0) &&
            (PlatformCapabilities->MaxFrequency / 1000 < FrequencyKhz)) {
            FrequencyKhz = PlatformCapabilities->MaxFrequency / 1000;

            MSHC_LOG_INFO(
                MshcExtension->LogHandle,
                MshcExtension,
                "Maximum frequency: %u",
                FrequencyKhz);
        }

        if (PlatformOperations->SetClock != NULL) {
            Status = PlatformOperations->SetClock(MshcExtension, FrequencyKhz);
            if (!NT_SUCCESS(Status)) {
                return Status;
            }
        }

        if (FrequencyKhz != MshcExtension->Capabilities.BaseClockFrequencyKhz) {
            Divisor = DIV_CEIL(
                MshcExtension->Capabilities.BaseClockFrequencyKhz,
                FrequencyKhz * 2);
        }

        MshcWriteRegister(MshcExtension, MSHC_CLKDIV, Divisor);

        Status = MshcCiuUpdateClock(MshcExtension, 0);
        if (!NT_SUCCESS(Status)) {
            return Status;
        }

        Clkena |= MSHC_CLKENA_CCLK_ENABLE(MshcExtension->SlotId);
        Clkena |= MSHC_CLKENA_CCLK_LOW_POWER(MshcExtension->SlotId);
        MshcWriteRegister(MshcExtension, MSHC_CLKENA, Clkena);

        Status = MshcCiuUpdateClock(MshcExtension, 0);
        if (!NT_SUCCESS(Status)) {
            return Status;
        }
    }

    MshcExtension->BusFrequencyKhz = FrequencyKhz;

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS
MshcSetBusWidth(
    _In_ PMSHC_EXTENSION MshcExtension,
    _In_ SDPORT_BUS_WIDTH Width
    )
{
    ULONG CtypeWidth;
    ULONG Ctype;

    switch (Width) {
    case SdBusWidth1Bit:
        CtypeWidth = MSHC_CTYPE_CARD_WIDTH_1;
        break;

    case SdBusWidth4Bit:
        CtypeWidth = MSHC_CTYPE_CARD_WIDTH_4;
        break;

    case SdBusWidth8Bit:
        CtypeWidth = MSHC_CTYPE_CARD_WIDTH_8;
        break;

    default:
        NT_ASSERTMSG("Provided bus width is invalid!", FALSE);
        return STATUS_INVALID_PARAMETER;
    }

    Ctype = MshcReadRegister(MshcExtension, MSHC_CTYPE);
    Ctype &= ~(MSHC_CTYPE_CARD_WIDTH_MASK << MshcExtension->SlotId);
    Ctype |= CtypeWidth << MshcExtension->SlotId;
    MshcWriteRegister(MshcExtension, MSHC_CTYPE, Ctype);

    MshcExtension->BusWidth = Width;

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS
MshcSetBusSpeed(
    _In_ PMSHC_EXTENSION MshcExtension,
    _In_ SDPORT_BUS_SPEED Speed
    )
{
    ULONG UhsReg;

    UhsReg = MshcReadRegister(MshcExtension, MSHC_UHS_REG);

    switch (Speed) {
    case SdBusSpeedNormal:
    case SdBusSpeedHigh:
    case SdBusSpeedSDR12:
    case SdBusSpeedSDR25:
    case SdBusSpeedSDR50:
    case SdBusSpeedSDR104:
    case SdBusSpeedHS200:
        UhsReg &= ~MSHC_UHS_REG_DDR_REG(MshcExtension->SlotId);
        break;

    case SdBusSpeedDDR50:
    case SdBusSpeedHS400:
        UhsReg |= MSHC_UHS_REG_DDR_REG(MshcExtension->SlotId);
        break;

    default:
        NT_ASSERTMSG("Invalid speed mode selected!", FALSE);
        return STATUS_INVALID_PARAMETER;
        break;
    }

    MshcWriteRegister(MshcExtension, MSHC_UHS_REG, UhsReg);

    MshcExtension->BusSpeed = Speed;

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS
MshcSetSignalingVoltage(
    _In_ PMSHC_EXTENSION MshcExtension,
    _In_ SDPORT_SIGNALING_VOLTAGE Voltage
    )
{
    NTSTATUS Status;
    PMSHC_PLATFORM_OPERATIONS PlatformOperations;
    BOOLEAN Switch18v;
    ULONG Clkena;
    ULONG UhsReg;
    ULONG Rintsts;

    switch (Voltage) {
    case SdSignalingVoltage33:
        //
        // The card should've been power cycled for its signaling level to
        // return to 3.3V. If the platform does not support power control and the
        // card was already initialized at 1.8V, it's unsafe to change voltage here.
        // Stay at the current level, since sdport will not switch to 1.8V again
        // if the card responds with S18A = 0 to ACMD41.
        //
        if ((MshcExtension->SignalingVoltage == SdSignalingVoltage18) &&
            !MshcExtension->CardPowerControlSupported) {
            //
            // Issue SEND_STATUS. If it fails, the card was either removed
            // or isn't actually initialized yet (RCA won't match -> timeout).
            // We will allow the voltage switch only in this case.
            //
            ULONG StatusRegister = 0;
            Status = MshcSendStatusCommand(MshcExtension, &StatusRegister);
            if (NT_SUCCESS(Status)) {
                MSHC_LOG_WARN(
                    MshcExtension->LogHandle,
                    MshcExtension,
                    "Not changing signaling for current card - power control unsupported!");
                return STATUS_SUCCESS;
            }
        }
        Switch18v = FALSE;
        break;

    case SdSignalingVoltage18:
        Switch18v = TRUE;
        break;

    default:
        NT_ASSERTMSG("Invalid signaling voltage!", FALSE);
        return STATUS_INVALID_PARAMETER;
    }

    Status = STATUS_SUCCESS;
    PlatformOperations = MshcExtension->PlatformOperations;

    if (Switch18v) {
        //
        // Temporarily disable all interrupts, we're checking
        // the status below.
        //
        MshcEnableGlobalInterrupt(MshcExtension, FALSE);

        //
        // Disable clock.
        //
        Clkena = MshcReadRegister(MshcExtension, MSHC_CLKENA);
        Clkena &= ~MSHC_CLKENA_CCLK_ENABLE(MshcExtension->SlotId);
        MshcWriteRegister(MshcExtension, MSHC_CLKENA, Clkena);
        MshcCiuUpdateClock(MshcExtension, MSHC_CMD_VOLT_SWITCH);
    }

    //
    // Set the internal regulator first.
    //
    UhsReg = MshcReadRegister(MshcExtension, MSHC_UHS_REG);
    if (Switch18v) {
        UhsReg |= MSHC_UHS_REG_VOLT_REG(MshcExtension->SlotId);
    } else {
        UhsReg &= ~MSHC_UHS_REG_VOLT_REG(MshcExtension->SlotId);
    }
    MshcWriteRegister(MshcExtension, MSHC_UHS_REG, UhsReg);

    //
    // Some controllers have an external regulator.
    //
    if (PlatformOperations->SetSignalingVoltage != NULL) {
        Status = PlatformOperations->SetSignalingVoltage(MshcExtension, Voltage);
    }

    if (Switch18v) {
        if (NT_SUCCESS(Status)) {
            //
            // Wait 10ms for regulator to stabilize.
            // Databook recommends 5ms.
            //
            SdPortWait(10000);

            //
            // Enable clock.
            //
            Clkena = MshcReadRegister(MshcExtension, MSHC_CLKENA);
            Clkena |= MSHC_CLKENA_CCLK_ENABLE(MshcExtension->SlotId);
            MshcWriteRegister(MshcExtension, MSHC_CLKENA, Clkena);
            MshcCiuUpdateClock(MshcExtension, MSHC_CMD_VOLT_SWITCH);

            //
            // Wait 2ms for card to toggle CMD & DAT lines.
            // Databook recommends 1 ms.
            //
            SdPortWait(2000);

            //
            // Check if switching was successful.
            //
            Rintsts = MshcReadRegister(MshcExtension, MSHC_RINTSTS);

            if ((Rintsts & (MSHC_INT_HTO | MSHC_INT_CMD)) !=
                (MSHC_INT_HTO | MSHC_INT_CMD)) {
                Status = STATUS_IO_DEVICE_ERROR;
            }

            //
            // Acknowledge interrupts.
            //
            MshcWriteRegister(MshcExtension, MSHC_RINTSTS, Rintsts);
        }

        //
        // Enable interrupts back.
        //
        MshcEnableGlobalInterrupt(MshcExtension, TRUE);
    }

    if (NT_SUCCESS(Status)) {
        MshcExtension->SignalingVoltage = Voltage;
    }

    return Status;
}

_Use_decl_annotations_
NTSTATUS
MshcExecuteTuning(
    _In_ PMSHC_EXTENSION MshcExtension
    )
{
    NTSTATUS Status;
    PMSHC_PLATFORM_OPERATIONS PlatformOperations;

    Status = STATUS_NOT_IMPLEMENTED;
    PlatformOperations = MshcExtension->PlatformOperations;

    MshcExtension->TuningPerformed = FALSE;
    MshcExtension->DataCrcErrorsSinceLastTuning = 0;

    if (PlatformOperations->ExecuteTuning != NULL) {
        Status = PlatformOperations->ExecuteTuning(MshcExtension);
    }

    MshcExtension->TuningPerformed = NT_SUCCESS(Status);

    return Status;
}

//
// Interrupt helper routines
//

_Use_decl_annotations_
VOID
MshcEnableGlobalInterrupt(
    _In_ PMSHC_EXTENSION MshcExtension,
    _In_ BOOLEAN Enable
    )
{
    ULONG Ctrl;

    Ctrl = MshcReadRegister(MshcExtension, MSHC_CTRL);
    if (Enable) {
        Ctrl |= MSHC_CTRL_INT_ENABLE;
    } else {
        Ctrl &= ~MSHC_CTRL_INT_ENABLE;
    }
    MshcWriteRegister(MshcExtension, MSHC_CTRL, Ctrl);
}

_Use_decl_annotations_
VOID
MshcEnableInterrupts(
    _In_ PMSHC_EXTENSION MshcExtension,
    _In_ ULONG Intsts,
    _In_ ULONG Idsts
    )
{
    ULONG Intmask;

    Intmask = MshcReadRegister(MshcExtension, MSHC_INTMASK);
    Intmask |= Intsts;
    MshcWriteRegister(MshcExtension, MSHC_INTMASK, Intmask);

    if (Idsts) {
        Intmask = MshcReadRegister(MshcExtension, MSHC_IDINTEN);
        Intmask |= Idsts;
        MshcWriteRegister(MshcExtension, MSHC_IDINTEN, Intmask);
    }
}

_Use_decl_annotations_
VOID
MshcDisableInterrupts(
    _In_ PMSHC_EXTENSION MshcExtension,
    _In_ ULONG Intsts,
    _In_ ULONG Idsts
    )
{
    ULONG Intmask;

    Intmask = MshcReadRegister(MshcExtension, MSHC_INTMASK);
    Intmask &= ~Intsts;
    MshcWriteRegister(MshcExtension, MSHC_INTMASK, Intmask);

    if (Idsts) {
        Intmask = MshcReadRegister(MshcExtension, MSHC_IDINTEN);
        Intmask &= ~Idsts;
        MshcWriteRegister(MshcExtension, MSHC_IDINTEN, Intmask);
    }
}

_Use_decl_annotations_
VOID
MshcAcknowledgeInterrupts(
    _In_ PMSHC_EXTENSION MshcExtension,
    _In_ ULONG Intsts,
    _In_ ULONG Idsts
    )
{
    MshcWriteRegister(MshcExtension, MSHC_RINTSTS, Intsts);

    if (Idsts) {
        MshcWriteRegister(MshcExtension, MSHC_IDSTS, Idsts);
    }
}

_Use_decl_annotations_
VOID
MshcConvertIntStatusToStandardEvents(
    _In_ ULONG Intsts,
    _In_ ULONG Idsts,
    _Out_ PULONG Events,
    _Out_ PULONG Errors
    )
{
    *Events = 0;
    *Errors = 0;

    if (Intsts & (MSHC_INT_CMD)) {
        *Events |= SDPORT_EVENT_CARD_RESPONSE;
    }
    if (Intsts & MSHC_INT_DTO) {
        *Events |= SDPORT_EVENT_CARD_RW_END;
    }
    if (Intsts & MSHC_INT_TXDR) {
        *Events |= SDPORT_EVENT_BUFFER_EMPTY;
    }
    if (Intsts & MSHC_INT_RXDR) {
        *Events |= SDPORT_EVENT_BUFFER_FULL;
    }
    if (Intsts & MSHC_INT_CD) {
        *Events |= SDPORT_EVENT_CARD_CHANGE;
    }

    if (Idsts & (MSHC_IDINT_NIS | MSHC_IDINT_TI | MSHC_IDINT_RI)) {
        *Events |= SDPORT_EVENT_DMA_COMPLETE;
    }

    if ((Intsts & MSHC_INT_ERROR) || (Idsts & MSHC_IDINT_AIS)) {
        *Events |= SDPORT_EVENT_ERROR;

        if (Intsts & MSHC_INT_RTO) {
            *Errors |= SDPORT_ERROR_CMD_TIMEOUT;
        }
        if (Intsts & MSHC_INT_RCRC) {
            *Errors |= SDPORT_ERROR_CMD_CRC_ERROR;
        }
        if (Intsts & MSHC_INT_RE) {
            *Errors |= SDPORT_ERROR_CMD_END_BIT_ERROR;
            *Errors |= SDPORT_ERROR_CMD_INDEX_ERROR;
        }
        if (Intsts & MSHC_INT_DRTO) {
            *Errors |= SDPORT_ERROR_DATA_TIMEOUT;
        }
        if (Intsts & MSHC_INT_DCRC) {
            *Errors |= SDPORT_ERROR_DATA_CRC_ERROR;
        }
        if (Intsts & MSHC_INT_EBE) {
            *Errors |= SDPORT_ERROR_DATA_END_BIT_ERROR;
        }
        if (Intsts & (MSHC_INT_SBE |
                      MSHC_INT_HLE |
                      MSHC_INT_FRUN)) {
            *Errors |= SDPORT_GENERIC_IO_ERROR;
        }

        if (Idsts & (MSHC_IDINT_AIS | MSHC_IDINT_CES | MSHC_IDINT_DUI | MSHC_IDINT_FBE)) {
            *Errors |= SDPORT_ERROR_ADMA_ERROR;
        }
    }
}

_Use_decl_annotations_
VOID
MshcConvertStandardEventsToIntStatus(
    _In_ ULONG Events,
    _In_ ULONG Errors,
    _Out_ PULONG Intsts,
    _Out_ PULONG Idsts
    )
{
    *Intsts = 0;
    *Idsts = 0;

    if (Events & SDPORT_EVENT_CARD_RESPONSE) {
        *Intsts |= MSHC_INT_CMD;
    }
    if (Events & SDPORT_EVENT_CARD_RW_END) {
        *Intsts |= MSHC_INT_DTO;
    }
    if (Events & SDPORT_EVENT_BUFFER_EMPTY) {
        *Intsts |= MSHC_INT_TXDR;
    }
    if (Events & SDPORT_EVENT_BUFFER_FULL) {
        *Intsts |= MSHC_INT_RXDR;
    }
    if (Events & SDPORT_EVENT_CARD_CHANGE) {
        *Intsts |= MSHC_INT_CD;
    }

    if (Events & SDPORT_EVENT_DMA_COMPLETE) {
        *Idsts |= (MSHC_IDINT_NIS | MSHC_IDINT_TI | MSHC_IDINT_RI);
    }

    if (Events & SDPORT_EVENT_ERROR) {
        *Intsts |= MSHC_INT_ERROR;

        if (Errors & SDPORT_ERROR_CMD_TIMEOUT) {
            *Intsts |= MSHC_INT_RTO;
        }
        if (Errors & SDPORT_ERROR_CMD_CRC_ERROR) {
            *Intsts |= MSHC_INT_RCRC;
        }
        if (Errors & (SDPORT_ERROR_CMD_END_BIT_ERROR | SDPORT_ERROR_CMD_INDEX_ERROR)) {
            *Intsts |= MSHC_INT_RE;
        }
        if (Errors & SDPORT_ERROR_DATA_TIMEOUT) {
            *Intsts |= MSHC_INT_DRTO;
        }
        if (Errors & SDPORT_ERROR_DATA_CRC_ERROR) {
            *Intsts |= MSHC_INT_DCRC;
        }
        if (Errors & SDPORT_ERROR_DATA_END_BIT_ERROR) {
            *Intsts |= MSHC_INT_EBE;
        }
        if (Errors & SDPORT_GENERIC_IO_ERROR) {
            *Intsts |= (MSHC_INT_SBE |
                        MSHC_INT_HLE |
                        MSHC_INT_FRUN);
        }

        if (Errors & SDPORT_ERROR_ADMA_ERROR) {
            *Intsts |= (MSHC_IDINT_AIS | MSHC_IDINT_CES | MSHC_IDINT_DUI | MSHC_IDINT_FBE);
        }
    }
}

_Use_decl_annotations_
NTSTATUS
MshcConvertStandardErrorToStatus(
    _In_ ULONG Error
    )
{
    if (Error == 0) {
        return STATUS_SUCCESS;
    }

    if (Error & (SDPORT_ERROR_CMD_TIMEOUT | SDPORT_ERROR_DATA_TIMEOUT)) {
        return STATUS_IO_TIMEOUT;
    }

    if (Error & (SDPORT_ERROR_CMD_CRC_ERROR | SDPORT_ERROR_DATA_CRC_ERROR)) {
        return STATUS_CRC_ERROR;
    }

    if (Error & (SDPORT_ERROR_CMD_END_BIT_ERROR | SDPORT_ERROR_DATA_END_BIT_ERROR)) {
        return STATUS_DEVICE_DATA_ERROR;
    }

    if (Error & SDPORT_ERROR_CMD_INDEX_ERROR) {
        return STATUS_DEVICE_PROTOCOL_ERROR;
    }

    if (Error & SDPORT_ERROR_BUS_POWER_ERROR) {
        return STATUS_DEVICE_POWER_FAILURE;
    }

    return STATUS_IO_DEVICE_ERROR;
}

//
// Request helper routines
//

_Use_decl_annotations_
NTSTATUS
MshcSendCommand(
    _In_ PMSHC_EXTENSION MshcExtension,
    _In_ PSDPORT_REQUEST Request
    )
{
    PSDPORT_COMMAND Command;
    ULONG Cmd;
    NTSTATUS Status;

    //
    // Initialize transfer parameters if this command is a data command.
    //
    Command = &Request->Command;
    if ((Command->TransferType != SdTransferTypeNone) &&
        (Command->TransferType != SdTransferTypeUndefined)) {

        Status = MshcBuildTransfer(MshcExtension, Request);
        if (!NT_SUCCESS(Status)) {
            return Status;
        }
    }

    //
    // Set the command parameters based off the given request.
    //
    Cmd = (Command->Index << MSHC_CMD_INDEX_SHIFT) & MSHC_CMD_INDEX_MASK;
    Cmd |= MSHC_CMD_START_CMD;

    switch (Command->ResponseType) {
    case SdResponseTypeNone:
        break;

    case SdResponseTypeR1:
    case SdResponseTypeR5:
    case SdResponseTypeR6:
    case SdResponseTypeR1B:
    case SdResponseTypeR5B:
        Cmd |= MSHC_CMD_RESPONSE_EXPECT |
               MSHC_CMD_CHECK_RESPONSE_CRC;
        break;

    case SdResponseTypeR2:
        Cmd |= MSHC_CMD_RESPONSE_EXPECT |
               MSHC_CMD_CHECK_RESPONSE_CRC |
               MSHC_CMD_RESPONSE_LENGTH;
        break;

    case SdResponseTypeR3:
    case SdResponseTypeR4:
        Cmd |= MSHC_CMD_RESPONSE_EXPECT;
        break;

    default:
        NT_ASSERTMSG("Invalid response type!", FALSE);
        return STATUS_INVALID_PARAMETER;
    }

    if (Command->Index == SDCMD_GO_IDLE_STATE ||
        Command->Index == SDCMD_STOP_TRANSMISSION ||
        Command->Index == SDCMD_GO_INACTIVE_STATE ||
        Command->Index == SDCMD_IO_RW_DIRECT) {
        Cmd |= MSHC_CMD_STOP_ABORT_CMD;
    }

    if (Command->Index == SDCMD_VOLTAGE_SWITCH) {
        Cmd |= MSHC_CMD_VOLT_SWITCH;

        //
        // Disable clock auto-gating since it can interfere with
        // the voltage switch. It will be enabled back in a later
        // call to MshcSetClock().
        //
        ULONG Clkena = MshcReadRegister(MshcExtension, MSHC_CLKENA);
        Clkena &= ~MSHC_CLKENA_CCLK_LOW_POWER(MshcExtension->SlotId);
        MshcWriteRegister(MshcExtension, MSHC_CLKENA, Clkena);
        MshcCiuUpdateClock(MshcExtension, 0);
    }

    if (Request->Command.TransferType != SdTransferTypeNone &&
        Request->Command.TransferType != SdTransferTypeUndefined) {
        Cmd |= MSHC_CMD_DATA_EXPECTED;
        Cmd |= MSHC_CMD_WAIT_PRVDATA_COMPLETE;

        if (Request->Command.TransferDirection == SdTransferDirectionWrite) {
            Cmd |= MSHC_CMD_WR;
        }

        if (Request->Command.UseAutoCmd12 &&
            Request->Command.TransferType == SdTransferTypeMultiBlock) {
            Cmd |= MSHC_CMD_SEND_AUTO_STOP;
        }
    }

    Cmd |= MSHC_CMD_USE_HOLD_REG;

    if (!MshcExtension->CardInitialized) {
        Cmd |= MSHC_CMD_SEND_INITIALIZATION;
        MshcExtension->CardInitialized = TRUE;
    }

    //
    // Set the bitmask for the required events that will fire after
    // writing to the command register. Depending on the response
    // type or whether the command involves data transfer, we will need
    // to wait on a number of different events.
    //
    InterlockedAnd((PLONG)&MshcExtension->CurrentEvents, 0);
    InterlockedAnd((PLONG)&MshcExtension->CurrentErrors, 0);

    Request->RequiredEvents = SDPORT_EVENT_CARD_RESPONSE;

    if (Request->Command.TransferMethod == SdTransferMethodSgDma) {
        Request->RequiredEvents |= SDPORT_EVENT_CARD_RW_END;

    } else if (Request->Command.TransferMethod == SdTransferMethodPio) {
        if (Request->Command.TransferDirection == SdTransferDirectionRead) {
            Request->RequiredEvents |= SDPORT_EVENT_BUFFER_FULL;
        } else {
            Request->RequiredEvents |= SDPORT_EVENT_BUFFER_EMPTY;
        }

        MshcEnableInterrupts(MshcExtension, MSHC_INT_RXDR | MSHC_INT_TXDR, 0);
    }

    //
    // Issue the command.
    //
    MshcWriteRegister(MshcExtension, MSHC_CMDARG, Command->Argument);
    MshcWriteRegister(MshcExtension, MSHC_CMD, Cmd);

    return STATUS_PENDING;
}

_Use_decl_annotations_
NTSTATUS
MshcSetTransferMode(
    _In_ PMSHC_EXTENSION MshcExtension,
    _In_ PSDPORT_REQUEST Request
    )
{
    USHORT BlockCount;
    USHORT BlockSize;
    ULONG Fifoth;

    NT_ASSERT(Request->Command.TransferMethod != SdTransferMethodUndefined);

    NT_ASSERT(
        (Request->Command.TransferDirection == SdTransferDirectionRead) ||
        (Request->Command.TransferDirection == SdTransferDirectionWrite));

    // Transfer length must be FIFO word aligned for now.
    NT_ASSERT(Request->Command.Length % sizeof(ULONG) == 0);

    BlockCount = Request->Command.BlockCount;
    BlockSize = Request->Command.BlockSize;

    MshcWriteRegister(MshcExtension, MSHC_BLKSIZ, BlockSize);
    MshcWriteRegister(MshcExtension, MSHC_BYTCNT, BlockCount * BlockSize);

    //
    // Write the desired FIFO watermarks.
    //
    Fifoth = (MshcExtension->FifoDmaBurstSize << MSHC_FIFOTH_DMA_MULTIPLE_TRANSACTION_SIZE_SHIFT) & MSHC_FIFOTH_DMA_MULTIPLE_TRANSACTION_SIZE_MASK;
    Fifoth |= ((MshcExtension->FifoThreshold - 1) << MSHC_FIFOTH_RX_WMARK_SHIFT) & MSHC_FIFOTH_RX_WMARK_MASK;
    Fifoth |= (MshcExtension->FifoThreshold << MSHC_FIFOTH_TX_WMARK_SHIFT) & MSHC_FIFOTH_TX_WMARK_MASK;
    MshcWriteRegister(MshcExtension, MSHC_FIFOTH, Fifoth);

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS
MshcBuildTransfer(
    _In_ PMSHC_EXTENSION MshcExtension,
    _In_ PSDPORT_REQUEST Request
    )
{
    NTSTATUS Status;

    NT_ASSERT(Request->Command.TransferType != SdTransferTypeNone);

    NT_ASSERT(Request->Command.TransferMethod != SdTransferMethodUndefined);

    MshcExtension->CurrentTransferRemainingLength = Request->Command.Length;

    switch (Request->Command.TransferMethod) {
    case SdTransferMethodPio:
        Status = MshcBuildPioTransfer(MshcExtension, Request);
        break;

    case SdTransferMethodSgDma:
        Status = MshcBuildDmaTransfer(MshcExtension, Request);
        break;

    default:
        Status = STATUS_NOT_SUPPORTED;
        break;
    }

    return Status;
}

_Use_decl_annotations_
NTSTATUS
MshcStartTransfer(
    _In_ PMSHC_EXTENSION MshcExtension,
    _In_ PSDPORT_REQUEST Request
    )
{
    NTSTATUS Status;

    NT_ASSERT(Request->Command.TransferType != SdTransferTypeNone);

    switch (Request->Command.TransferMethod) {
    case SdTransferMethodPio:
        Status = MshcStartPioTransfer(MshcExtension, Request);
        break;

    case SdTransferMethodSgDma:
        Status = MshcStartDmaTransfer(MshcExtension, Request);
        break;

    default:
        Status = STATUS_NOT_SUPPORTED;
        break;
    }

    return Status;
}

_Use_decl_annotations_
NTSTATUS
MshcBuildPioTransfer(
    _In_ PMSHC_EXTENSION MshcExtension,
    _In_ PSDPORT_REQUEST Request
    )
{
    //
    // If the transfer length is less than the optimal FIFO threshold, then
    // use that as the threshold so we still get a Data Request interrupt.
    //
    MshcExtension->FifoThreshold = MIN(
        Request->Command.Length / sizeof(ULONG),
        MSHC_OPTIMAL_FIFO_THRESHOLD(MshcExtension->PlatformInfo->Capabilities.FifoDepth));

    MshcExtension->FifoDmaBurstSize = 0; // don't care

    //
    // Disable IDMAC
    //
    ULONG Ctrl = MshcReadRegister(MshcExtension, MSHC_CTRL);
    if (Ctrl & MSHC_CTRL_USE_INTERNAL_DMAC) {
        Ctrl &= ~MSHC_CTRL_USE_INTERNAL_DMAC;
        MshcWriteRegister(MshcExtension, MSHC_CTRL, Ctrl);
        MshcWriteRegister(MshcExtension, MSHC_BMOD, 0);
    }

    return MshcSetTransferMode(MshcExtension, Request);
}

_Use_decl_annotations_
NTSTATUS
MshcBuildDmaTransfer(
    _In_ PMSHC_EXTENSION MshcExtension,
    _In_ PSDPORT_REQUEST Request
    )
{
    NTSTATUS Status;

    MshcExtension->FifoThreshold =
        MSHC_OPTIMAL_FIFO_THRESHOLD(MshcExtension->PlatformInfo->Capabilities.FifoDepth);
    MshcExtension->FifoDmaBurstSize = 2; // 8 transfers

    Status = MshcSetTransferMode(MshcExtension, Request);
    if (!NT_SUCCESS(Status)) {
        return Status;
    }

    NT_ASSERT(Request->Command.ScatterGatherList != NULL);

    //
    // Create the IDMAC descriptor table in the host's DMA buffer.
    //
    Status = MshcCreateIdmacDescriptorTable(MshcExtension, Request);
    if (!NT_SUCCESS(Status)) {
        return Status;
    }

    NT_ASSERT(Request->Command.DmaPhysicalAddress.HighPart == 0);

    MshcWriteRegister(
        MshcExtension,
        MSHC_DBADDR,
        Request->Command.DmaPhysicalAddress.LowPart);

    //
    // Enable IDMAC
    //
    ULONG Ctrl = MshcReadRegister(MshcExtension, MSHC_CTRL);
    Ctrl |= MSHC_CTRL_USE_INTERNAL_DMAC;
    MshcWriteRegister(MshcExtension, MSHC_CTRL, Ctrl);
    MshcWriteRegister(MshcExtension, MSHC_BMOD, MSHC_BMOD_DE | MSHC_BMOD_FB);

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS
MshcStartPioTransfer(
    _In_ PMSHC_EXTENSION MshcExtension,
    _In_ PSDPORT_REQUEST Request
    )
{
    ULONG CurrentEvents;
    ULONG SubTransferLength;

    NT_ASSERT((Request->Command.TransferDirection == SdTransferDirectionRead) ||
              (Request->Command.TransferDirection == SdTransferDirectionWrite));

    CurrentEvents = InterlockedAnd((PLONG)&MshcExtension->CurrentEvents, 0);

    //
    // PIO transfers are paced by RX/TX Data Request interrupts, which signal when
    // there are a minimum of FifoThreshold words to read or free space to write
    // respectively.
    //
    SubTransferLength = MshcExtension->FifoThreshold * sizeof(ULONG);

    if (Request->Command.TransferDirection == SdTransferDirectionRead) {
        MshcReadDataPort(
            MshcExtension,
            (PULONG)Request->Command.DataBuffer,
            MshcExtension->FifoThreshold);
    } else  {
        MshcWriteDataPort(
            MshcExtension,
            (PULONG)Request->Command.DataBuffer,
            MshcExtension->FifoThreshold);
    }

    //
    // We have to acknowledge Data Request interrupts right after the transfer,
    // even though this should also happen in the ISR. Otherwise the FIFO may start
    // misbehaving.
    //
    if (!gCrashdumpMode) {
        MshcAcknowledgeInterrupts(MshcExtension, MSHC_INT_RXDR | MSHC_INT_TXDR, 0);
    }

    MshcExtension->CurrentTransferRemainingLength -= SubTransferLength;

    if (MshcExtension->CurrentTransferRemainingLength > 0) {
        Request->Command.DataBuffer += SubTransferLength;
        if (Request->Command.TransferDirection == SdTransferDirectionRead) {
            Request->RequiredEvents |= SDPORT_EVENT_BUFFER_FULL;

        } else {
            Request->RequiredEvents |= SDPORT_EVENT_BUFFER_EMPTY;
        }

        Request->Status = STATUS_MORE_PROCESSING_REQUIRED;

        //
        // We've disabled these in the ISR. Enable them back to chain the transfer.
        //
        MshcEnableInterrupts(MshcExtension, MSHC_INT_RXDR | MSHC_INT_TXDR, 0);

    } else {
        Request->Status = STATUS_SUCCESS;

        if ((CurrentEvents & SDPORT_EVENT_CARD_RW_END) != 0) {
            MshcCompleteRequest(MshcExtension, Request, Request->Status);
        } else {
            Request->RequiredEvents |= SDPORT_EVENT_CARD_RW_END;
        }
    }

    return STATUS_PENDING;
}

_Use_decl_annotations_
NTSTATUS
MshcStartDmaTransfer(
    _In_ PMSHC_EXTENSION MshcExtension,
    _In_ PSDPORT_REQUEST Request
    )
{
    MshcExtension->CurrentTransferRemainingLength = 0;

    Request->Status = STATUS_SUCCESS;
    MshcCompleteRequest(MshcExtension, Request, Request->Status);

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
VOID
MshcCompleteRequest(
    _In_ PMSHC_EXTENSION MshcExtension,
    _In_ PSDPORT_REQUEST Request,
    _In_ NTSTATUS Status
    )
{
    Request->Status = Status;

    //
    // Busy signal may still be asserted after R1b and R5b commands
    // or write transfers. We have to wait for it to be deasserted before
    // being able to issue a new command.
    //
    // In case of writes, it consistently takes over 500 us for deassertion.
    // Queue a work item so we don't block this long at DISPATCH_LEVEL.
    //
    if ((Status == STATUS_SUCCESS) && (Request->RequiredEvents == 0) &&
        ((Request->Command.ResponseType == SdResponseTypeR1B) ||
         (Request->Command.ResponseType == SdResponseTypeR5B) ||
         (Request->Command.TransferDirection == SdTransferDirectionWrite))) {
        if (MshcIsDataBusy(MshcExtension)) {
            if ((MshcExtension->CompleteRequestBusyWorkItem != NULL) && (KeGetCurrentIrql() == DISPATCH_LEVEL)) {
                IoQueueWorkItem(
                    MshcExtension->CompleteRequestBusyWorkItem,
                    MshcCompleteRequestBusyWorker,
                    CriticalWorkQueue,
                    MshcExtension);
            } else {
                MshcCompleteRequestBusyWorker(NULL, MshcExtension);
            }
            return;
        }
    }

    if (Request->Type == SdRequestTypeCommandNoTransfer) {
        MSHC_LOG_INFO(
            MshcExtension->LogHandle,
            MshcExtension,
            "Type:%!REQUESTTYPE! %s%d(0x%08X) %!STATUS!",
            Request->Type,
            (Request->Command.Class == SdCommandClassApp ? "ACMD" : "CMD"),
            Request->Command.Index,
            Request->Command.Argument,
            Status);
    } else {
        MSHC_LOG_INFO(
            MshcExtension->LogHandle,
            MshcExtension,
            "Type:%!REQUESTTYPE! %s%d(0x%08X) %s %s(Blocks:%lu, Length:%lu) %!STATUS!",
            Request->Type,
            (Request->Command.Class == SdCommandClassApp ? "ACMD" : "CMD"),
            Request->Command.Index,
            Request->Command.Argument,
            Request->Command.TransferMethod == SdTransferMethodSgDma ? "DMA" : "PIO",
            (Request->Command.TransferDirection == SdTransferDirectionRead ? "Read" : "Write"),
            Request->Command.BlockCount,
            Request->Command.Length,
            Status);
    }

    //
    // Release the outstanding request if it completed successfully.
    // Otherwise keep it around, because sdport will call ResetHost
    // and we need to know what kind of request failed there.
    //
    if ((Request->Status == STATUS_SUCCESS) ||
        (Request->Status == STATUS_MORE_PROCESSING_REQUIRED))
        if ((PSDPORT_REQUEST)InterlockedExchangePointer(
            (PVOID volatile *)&MshcExtension->OutstandingRequest, NULL) != Request) {
            NT_ASSERTMSG("Request mismatch", FALSE);
        }

    //
    // WORKAROUND:
    // If the card was removed, complete the request with failure
    // to avoid a crash in sdport.
    //
    if (!MshcIsCardInserted(MshcExtension)) {
        Request->Status = STATUS_IO_DEVICE_ERROR;

        MSHC_LOG_WARN(
            MshcExtension->LogHandle,
            MshcExtension,
            "Card removed - failing outstanding request!");
    }

    LONG LocalRequestPending =
        InterlockedCompareExchange(&MshcExtension->LocalRequestPending, 0, 1);

    if (!LocalRequestPending) {
        SdPortCompleteRequest(Request, Request->Status);
    }
}

_Use_decl_annotations_
VOID
MshcCompleteRequestBusyWorker(
    _In_opt_ PDEVICE_OBJECT DeviceObject,
    _In_ PVOID Context
    )
{
    UNREFERENCED_PARAMETER(DeviceObject);

    PMSHC_EXTENSION MshcExtension;
    NTSTATUS Status;

    MshcExtension = (PMSHC_EXTENSION)Context;

    Status = MshcWaitDataIdle(MshcExtension);

    MshcCompleteRequest(
        MshcExtension,
        MshcExtension->OutstandingRequest,
        Status);
}

_Use_decl_annotations_
NTSTATUS
MshcCreateIdmacDescriptorTable(
    _In_ PMSHC_EXTENSION MshcExtension,
    _In_ PSDPORT_REQUEST Request
    )
{
    UNREFERENCED_PARAMETER(MshcExtension);

    PMSHC_IDMAC_DESCRIPTOR Descriptor;
    ULONG Count;
    ULONG NumberOfElements;
    PHYSICAL_ADDRESS NextAddress;
    ULONG NextLength;
    ULONG RemainingLength;
    PSCATTER_GATHER_ELEMENT SglistElement;

    Descriptor = (PMSHC_IDMAC_DESCRIPTOR)Request->Command.DmaVirtualAddress;
    NumberOfElements = Request->Command.ScatterGatherList->NumberOfElements;
    SglistElement = &Request->Command.ScatterGatherList->Elements[0];
    Count = 0;

    NT_ASSERT(NumberOfElements > 0);

    //
    // Iterate through each element in the SG list and convert it into the
    // descriptor table required by the controller.
    //
    while (NumberOfElements > 0) {
        RemainingLength = SglistElement->Length;
        NextAddress.QuadPart = SglistElement->Address.QuadPart;

        NT_ASSERT(RemainingLength > 0);

        while (RemainingLength > 0) {
            NextLength = MIN(MSHC_IDMAC_MAX_LENGTH_PER_ENTRY, RemainingLength);
            RemainingLength -= NextLength;

            //
            // Set initial attributes.
            //
            Descriptor[Count].Des0 = MSHC_IDMAC_DES0_OWN | MSHC_IDMAC_DES0_DIC;

            //
            // Set the buffer length.
            //
            NT_ASSERT(IS_ALIGNED(NextLength, sizeof(ULONG)));
            Descriptor[Count].Des1 = (NextLength << MSHC_IDMAC_DES1_BS1_SHIFT) & MSHC_IDMAC_DES1_BS1_MASK;

            //
            // Set the address field.
            //
            // The HighPart should not be a non-zero value, since in the
            // DMA adapter object, we declared that this device only
            // supports 32-bit addressing.
            //
            NT_ASSERT(NextAddress.HighPart == 0);
            NT_ASSERT(IS_ALIGNED(NextAddress.HighPart, sizeof(ULONG)));
            Descriptor[Count].Des2 = NextAddress.LowPart;

            //
            // Not using chained or dual-buffer modes.
            //
            Descriptor[Count].Des3 = 0;

            NextAddress.QuadPart += NextLength;
            Count++;
        }

        SglistElement += 1;
        NumberOfElements -= 1;
    }

    //
    // Mark the first descriptor.
    //
    Descriptor[0].Des0 |= MSHC_IDMAC_DES0_FS;

    //
    // Mark the last descriptor.
    //
    Descriptor[Count - 1].Des0 |= (MSHC_IDMAC_DES0_ER | MSHC_IDMAC_DES0_LD);
    Descriptor[Count - 1].Des0 &= ~MSHC_IDMAC_DES0_DIC;

    MSHC_LOG_TRACE(
        MshcExtension->LogHandle,
        MshcExtension,
        "PhysicalAddress:0x%p, VirtualAddress:0x%p, Count:%lu",
        (PVOID)(Request->Command.DmaPhysicalAddress.LowPart),
        Request->Command.DmaVirtualAddress,
        Count);

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS
MshcIssueRequestSynchronously(
    _In_ PMSHC_EXTENSION MshcExtension,
    _In_ PSDPORT_REQUEST Request,
    _In_ ULONG TimeoutUs,
    _In_ BOOLEAN ErrorRecovery
    )
{
    NTSTATUS Status;
    ULONG Retry;

    const ULONG PollWaitUs = 10;

    InterlockedExchange(&MshcExtension->LocalRequestPending, 1);

    Status = MshcSlotIssueRequest(MshcExtension, Request);
    if (Status != STATUS_PENDING) {
        return Status;
    }

    for (Retry = DIV_CEIL(TimeoutUs, PollWaitUs);
         (Retry > 0) || (TimeoutUs == 0);
         Retry--) {
        if (gCrashdumpMode) {
            //
            // Interrupts are disabled in crashdump mode.
            //
            ULONG Events = 0;
            ULONG Errors = 0;
            BOOLEAN CardChange = FALSE;
            BOOLEAN SdioInterrupt = FALSE;
            BOOLEAN Tuning = FALSE;
            BOOLEAN Handled = FALSE;

            Handled = MshcSlotInterrupt(
                        MshcExtension,
                        &Events,
                        &Errors,
                        &CardChange,
                        &SdioInterrupt,
                        &Tuning);
            if (Handled) {
                MshcRequestDpc(MshcExtension, Request, Events, Errors);
            }
        }

        if (!MshcExtension->LocalRequestPending) {
            break;
        }

        SdPortWait(PollWaitUs);
    }

    InterlockedExchange(&MshcExtension->LocalRequestPending, 0);

    if ((TimeoutUs != 0) && (Retry == 0)) {
        MSHC_LOG_ERROR(MshcExtension->LogHandle, MshcExtension, "Timeout");
        Request->Status = STATUS_IO_TIMEOUT;
    }

    if (ErrorRecovery && !NT_SUCCESS(Request->Status)) {
        SDPORT_BUS_OPERATION BusOperation;
        BusOperation.Type = SdResetHost;

        BusOperation.Parameters.ResetType = SdResetTypeCmd;
        MshcSlotIssueBusOperation(MshcExtension, &BusOperation);

        if ((Request->Command.TransferType != SdTransferTypeNone) &&
            (Request->Command.TransferType != SdTransferTypeUndefined)) {
            BusOperation.Parameters.ResetType = SdResetTypeDat;
            MshcSlotIssueBusOperation(MshcExtension, &BusOperation);
        }
    }

    return Request->Status;
}

_Use_decl_annotations_
VOID
MshcLocalRequestDpc(
    _In_ PKDPC Dpc,
    _In_ PVOID DeferredContext,
    _In_ PVOID SystemArgument1,
    _In_ PVOID SystemArgument2
    )
{
    UNREFERENCED_PARAMETER(Dpc);
    UNREFERENCED_PARAMETER(SystemArgument1);
    UNREFERENCED_PARAMETER(SystemArgument2);

    PSDPORT_REQUEST Request;
    ULONG Events;
    ULONG Errors;

    PMSHC_EXTENSION MshcExtension = (PMSHC_EXTENSION)DeferredContext;

    Request = (PSDPORT_REQUEST)InterlockedCompareExchangePointer(
        (PVOID volatile*)&MshcExtension->OutstandingRequest, NULL, NULL);

    Events = InterlockedCompareExchange((PLONG)&MshcExtension->CurrentEvents, 0, 0);
    Errors = InterlockedCompareExchange((PLONG)&MshcExtension->CurrentErrors, 0, 0);

    if (Request != NULL) {
        MshcRequestDpc(DeferredContext, Request, Events, Errors);
    }
}

//
// Local command routines
//

_Use_decl_annotations_
NTSTATUS
MshcSendStopCommand(
    _In_ PMSHC_EXTENSION MshcExtension
    )
{
    SDPORT_REQUEST Request;

    RtlZeroMemory(&Request, sizeof(Request));

    Request.Command.Index = SDCMD_STOP_TRANSMISSION;
    Request.Command.Class = SdCommandClassStandard;
    Request.Command.ResponseType = SdResponseTypeR1B;
    Request.Command.TransferType = SdTransferTypeNone;
    Request.Command.Argument = 0;

    //
    // Send the command.
    // Don't request error recovery since this function
    // is already called as part of the reset sequence.
    //
    Request.Type = SdRequestTypeCommandNoTransfer;

    return MshcIssueRequestSynchronously(MshcExtension, &Request, 1000000, FALSE);
}

_Use_decl_annotations_
NTSTATUS
MshcSendStatusCommand(
    _In_ PMSHC_EXTENSION MshcExtension,
    _Out_ PULONG StatusRegister
    )
{
    NTSTATUS Status;
    SDPORT_REQUEST Request;

    RtlZeroMemory(&Request, sizeof(Request));

    Request.Command.Index = SDCMD_SEND_STATUS;
    Request.Command.Class = SdCommandClassStandard;
    Request.Command.ResponseType = SdResponseTypeR1;
    Request.Command.TransferType = SdTransferTypeNone;
    Request.Command.Argument = MshcExtension->CardRca << 16;

    //
    // Send the command.
    //
    Request.Type = SdRequestTypeCommandNoTransfer;

    Status = MshcIssueRequestSynchronously(MshcExtension, &Request, 50000, TRUE);
    if (!NT_SUCCESS(Status)) {
        return Status;
    }

    MshcSlotGetResponse(MshcExtension, &Request.Command, StatusRegister);

    return Status;
}

_Use_decl_annotations_
NTSTATUS
MshcSendTuningCommand(
    _In_ PMSHC_EXTENSION MshcExtension
    )
{
    static const UCHAR TuningBlock4BitPattern[] = {
        0xff, 0x0f, 0xff, 0x00, 0xff, 0xcc, 0xc3, 0xcc,
        0xc3, 0x3c, 0xcc, 0xff, 0xfe, 0xff, 0xfe, 0xef,
        0xff, 0xdf, 0xff, 0xdd, 0xff, 0xfb, 0xff, 0xfb,
        0xbf, 0xff, 0x7f, 0xff, 0x77, 0xf7, 0xbd, 0xef,
        0xff, 0xf0, 0xff, 0xf0, 0x0f, 0xfc, 0xcc, 0x3c,
        0xcc, 0x33, 0xcc, 0xcf, 0xff, 0xef, 0xff, 0xee,
        0xff, 0xfd, 0xff, 0xfd, 0xdf, 0xff, 0xbf, 0xff,
        0xbb, 0xff, 0xf7, 0xff, 0xf7, 0x7f, 0x7b, 0xde
    };

    static const UCHAR TuningBlock8BitPattern[] = {
        0xff, 0xff, 0x00, 0xff, 0xff, 0xff, 0x00, 0x00,
        0xff, 0xff, 0xcc, 0xcc, 0xcc, 0x33, 0xcc, 0xcc,
        0xcc, 0x33, 0x33, 0xcc, 0xcc, 0xcc, 0xff, 0xff,
        0xff, 0xee, 0xff, 0xff, 0xff, 0xee, 0xee, 0xff,
        0xff, 0xff, 0xdd, 0xff, 0xff, 0xff, 0xdd, 0xdd,
        0xff, 0xff, 0xff, 0xbb, 0xff, 0xff, 0xff, 0xbb,
        0xbb, 0xff, 0xff, 0xff, 0x77, 0xff, 0xff, 0xff,
        0x77, 0x77, 0xff, 0x77, 0xbb, 0xdd, 0xee, 0xff,
        0xff, 0xff, 0xff, 0x00, 0xff, 0xff, 0xff, 0x00,
        0x00, 0xff, 0xff, 0xcc, 0xcc, 0xcc, 0x33, 0xcc,
        0xcc, 0xcc, 0x33, 0x33, 0xcc, 0xcc, 0xcc, 0xff,
        0xff, 0xff, 0xee, 0xff, 0xff, 0xff, 0xee, 0xee,
        0xff, 0xff, 0xff, 0xdd, 0xff, 0xff, 0xff, 0xdd,
        0xdd, 0xff, 0xff, 0xff, 0xbb, 0xff, 0xff, 0xff,
        0xbb, 0xbb, 0xff, 0xff, 0xff, 0x77, 0xff, 0xff,
        0xff, 0x77, 0x77, 0xff, 0x77, 0xbb, 0xdd, 0xee
    };

    NTSTATUS Status;
    SDPORT_REQUEST Request;
    UCHAR DataBuffer[128];
    PUCHAR TuningBlockPattern;

    RtlZeroMemory(&Request, sizeof(Request));
    RtlZeroMemory(&DataBuffer, sizeof(DataBuffer));

    switch (MshcExtension->BusSpeed) {
    case SdBusSpeedSDR50:
    case SdBusSpeedSDR104:
        Request.Command.Index = SDCMD_SEND_TUNING_BLOCK;
        break;
    case SdBusSpeedHS200:
    case SdBusSpeedHS400:
        Request.Command.Index = SDCMD_EMMC_SEND_TUNING_BLOCK;
        break;
    default:
        NT_ASSERTMSG("Unexpected bus speed!", FALSE);
        return STATUS_INVALID_PARAMETER;
    }
    Request.Command.Class = SdCommandClassStandard;
    Request.Command.ResponseType = SdResponseTypeR1;
    Request.Command.TransferType = SdTransferTypeSingleBlock;
    Request.Command.TransferDirection = SdTransferDirectionRead;
    Request.Command.TransferMethod = SdTransferMethodPio;
    Request.Command.Argument = 0;

    if (MshcExtension->BusWidth == SdBusWidth8Bit) {
        TuningBlockPattern = (PUCHAR)TuningBlock8BitPattern;
        Request.Command.BlockSize = 128;
    }
    else {
        TuningBlockPattern = (PUCHAR)TuningBlock4BitPattern;
        Request.Command.BlockSize = 64;
    }
    Request.Command.BlockCount = 1;
    Request.Command.Length = Request.Command.BlockSize * Request.Command.BlockCount;
    Request.Command.DataBuffer = DataBuffer;

    //
    // Send the command
    //
    Request.Type = SdRequestTypeCommandWithTransfer;

    Status = MshcIssueRequestSynchronously(MshcExtension, &Request, 1000000, TRUE);
    if (!NT_SUCCESS(Status)) {
        return Status;
    }

    //
    // Perform the block transfer
    //
    Request.Type = SdRequestTypeStartTransfer;

    do {
        Status = MshcIssueRequestSynchronously(MshcExtension, &Request, 1000000, TRUE);
    } while (Status == STATUS_MORE_PROCESSING_REQUIRED);

    if (!NT_SUCCESS(Status)) {
        return Status;
    }

    //
    // Ensure the tuning block matches the pattern
    //
    if (!RtlEqualMemory(Request.Command.DataBuffer, TuningBlockPattern, Request.Command.Length)) {
        return STATUS_IO_DEVICE_ERROR;
    }

    return STATUS_SUCCESS;
}

//
// General utility routines
//

_Use_decl_annotations_
VOID
MshcReadDataPort(
    _In_ PMSHC_EXTENSION MshcExtension,
    _Out_writes_all_(Length) PULONG Buffer,
    _In_ ULONG Length
    )
{
    MshcReadRegisterBufferUlong(MshcExtension, MSHC_FIFO_BASE, Buffer, Length);
}

_Use_decl_annotations_
VOID
MshcWriteDataPort(
    _In_ PMSHC_EXTENSION MshcExtension,
    _In_reads_(Length) PULONG Buffer,
    _In_ ULONG Length
    )
{
    MshcWriteRegisterBufferUlong(MshcExtension, MSHC_FIFO_BASE, Buffer, Length);
}

_Use_decl_annotations_
NTSTATUS
MshcResetWait(
    _In_ PMSHC_EXTENSION MshcExtension,
    _In_ ULONG Mask
    )
{
    ULONG Ctrl;
    ULONG Retry;

    Ctrl = MshcReadRegister(MshcExtension, MSHC_CTRL);
    Ctrl |= Mask;
    MshcWriteRegister(MshcExtension, MSHC_CTRL, Ctrl);

    for (Retry = 300; Retry > 0; Retry--) {
        Ctrl = MshcReadRegister(MshcExtension, MSHC_CTRL);
        if ((Ctrl & Mask) == 0) {
            return STATUS_SUCCESS;
        }
        SdPortWait(1);
    }

    MSHC_LOG_ERROR(MshcExtension->LogHandle, MshcExtension, "Timeout (CTRL:%lx)", Ctrl);
    return STATUS_IO_TIMEOUT;
}

_Use_decl_annotations_
BOOLEAN
MshcIsDataBusy(
    _In_ PMSHC_EXTENSION MshcExtension
    )
{
    return (MshcReadRegister(MshcExtension, MSHC_STATUS) & MSHC_STATUS_DATA_BUSY) != 0;
}

_Use_decl_annotations_
NTSTATUS
MshcWaitDataIdle(
    _In_ PMSHC_EXTENSION MshcExtension
    )
{
    ULONG Status;
    ULONG Retry;

    for (Retry = 100000; Retry > 0; Retry--) {
        Status = MshcReadRegister(MshcExtension, MSHC_STATUS);
        if ((Status & MSHC_STATUS_DATA_BUSY) == 0) {
            return STATUS_SUCCESS;
        }
        SdPortWait(1);
    }

    MSHC_LOG_ERROR(MshcExtension->LogHandle, MshcExtension, "Timeout (STATUS:%lx)", Status);
    return STATUS_IO_TIMEOUT;
}

_Use_decl_annotations_
NTSTATUS
MshcCiuUpdateClock(
    _In_ PMSHC_EXTENSION MshcExtension,
    _In_ ULONG Flags
    )
{
    ULONG Cmd;
    ULONG Retry;

    // Busy is expected during voltage switch.
    if ((Flags & MSHC_CMD_VOLT_SWITCH) == 0) {
        MshcWaitDataIdle(MshcExtension);
    }

    MshcWriteRegister(MshcExtension, MSHC_CMDARG, 0);

    Cmd = MSHC_CMD_WAIT_PRVDATA_COMPLETE |
          MSHC_CMD_UPDATE_CLOCK_REGS_ONLY |
          MSHC_CMD_START_CMD |
          Flags;

    MshcWriteRegister(MshcExtension, MSHC_CMD, Cmd);

    for (Retry = 200000; Retry > 0; Retry--) {
        Cmd = MshcReadRegister(MshcExtension, MSHC_CMD);
        if ((Cmd & MSHC_CMD_START_CMD) == 0) {
            return STATUS_SUCCESS;
        }
        SdPortWait(1);
    }

    MSHC_LOG_ERROR(MshcExtension->LogHandle, MshcExtension, "Timeout (CMD:%lx)", Cmd);
    return STATUS_IO_TIMEOUT;
}

_Use_decl_annotations_
BOOLEAN
MshcIsCardInserted(
    _In_ PMSHC_EXTENSION MshcExtension
    )
{
    ULONG Cdetect;

    Cdetect = MshcReadRegister(MshcExtension, MSHC_CDETECT);

    return (Cdetect & MSHC_CDETECT_CARD_DETECT_N(MshcExtension->SlotId)) == 0;
}

_Use_decl_annotations_
BOOLEAN
MshcIsCardWriteProtected(
    _In_ PMSHC_EXTENSION MshcExtension
    )
{
    ULONG Wrtprt;

    Wrtprt = MshcReadRegister(MshcExtension, MSHC_WRTPRT);

    return (Wrtprt & MSHC_WRTPRT_WRITE_PROTECT(MshcExtension->SlotId)) != 0;
}

_Use_decl_annotations_
NTSTATUS
MshcQueryAcpiPlatformCapabilities(
    _In_ PDEVICE_OBJECT Pdo,
    _Out_ PMSHC_PLATFORM_CAPABILITIES Capabilities
    )
{
    NTSTATUS Status;
    PACPI_EVAL_OUTPUT_BUFFER OutputBuffer = nullptr;
    const ACPI_METHOD_ARGUMENT UNALIGNED* Properties = nullptr;

    Status = AcpiQueryDsd(Pdo, &OutputBuffer);
    if (!NT_SUCCESS(Status)) {
        MSHC_LOG_ERROR(
            DriverLogHandle,
            NULL,
            "AcpiQueryDsd() failed. Status: %!STATUS!",
            Status);
        goto Cleanup;
    }

    Status = AcpiParseDsdAsDeviceProperties(OutputBuffer, &Properties);
    if (!NT_SUCCESS(Status)) {
        MSHC_LOG_ERROR(
            DriverLogHandle,
            NULL,
            "AcpiParseDsdAsDeviceProperties() failed. Status: %!STATUS!",
            Status);
        goto Cleanup;
    }

    AcpiDevicePropertiesQueryIntegerValue(Properties, "clock-frequency", &Capabilities->ClockFrequency);
    AcpiDevicePropertiesQueryIntegerValue(Properties, "max-frequency", &Capabilities->MaxFrequency);
    AcpiDevicePropertiesQueryIntegerValue(Properties, "fifo-depth", &Capabilities->FifoDepth);
    AcpiDevicePropertiesQueryIntegerValue(Properties, "bus-width", &Capabilities->BusWidth);
    Status = AcpiDevicePropertiesQueryIntegerValue(Properties, "cap-sd-highspeed", &Capabilities->SupportHighSpeed);
    if (!NT_SUCCESS(Status)) {
        AcpiDevicePropertiesQueryIntegerValue(Properties, "cap-mmc-highspeed", &Capabilities->SupportHighSpeed);
    }
    AcpiDevicePropertiesQueryIntegerValue(Properties, "sd-uhs-ddr50", &Capabilities->SupportDdr50);
    AcpiDevicePropertiesQueryIntegerValue(Properties, "sd-uhs-sdr50", &Capabilities->SupportSdr50);
    AcpiDevicePropertiesQueryIntegerValue(Properties, "sd-uhs-sdr104", &Capabilities->SupportSdr104);
    AcpiDevicePropertiesQueryIntegerValue(Properties, "mmc-hs200-1_8v", &Capabilities->SupportHs200);
    AcpiDevicePropertiesQueryIntegerValue(Properties, "no-1-8-v", &Capabilities->No18vRegulator);

    Status = STATUS_SUCCESS;

Cleanup:

    if (OutputBuffer != nullptr) {
        ExFreePoolWithTag(OutputBuffer, ACPI_TAG_EVAL_OUTPUT_BUFFER);
    }

    return Status;
}

_Use_decl_annotations_
MSHC_PLATFORM_TYPE
MshcGetPlatformType(
    _In_ PDEVICE_OBJECT Pdo
    )
{
    NTSTATUS Status;
    HANDLE Key = NULL;
    UNICODE_STRING ValueName = RTL_CONSTANT_STRING(L"PlatformType");
    ULONG Value;
    MSHC_PLATFORM_TYPE PlatformType = MshcPlatformUnknown;

    Status = IoOpenDeviceRegistryKey(Pdo, PLUGPLAY_REGKEY_DEVICE, KEY_READ, &Key);
    if (!NT_SUCCESS(Status)) {
        goto Cleanup;
    }

    Status = RegKeyQueryUlongValue(Key, &ValueName, &Value);
    if (!NT_SUCCESS(Status)) {
        goto Cleanup;
    }

    PlatformType = (MSHC_PLATFORM_TYPE)Value;

Cleanup:
    if (Key != NULL) {
        ZwClose(Key);
    }

    return PlatformType;
}

_Use_decl_annotations_
PMSHC_PLATFORM_DRIVER_DATA
MshcGetPlatformDriverData(
    _In_ MSHC_PLATFORM_TYPE PlatformType
    )
{
    for (ULONG Index = 0; Index < ARRAYSIZE(SupportedPlatforms); Index++) {
        if (PlatformType == SupportedPlatforms[Index].Info.Type) {
            return &SupportedPlatforms[Index];
        }
    }

    return NULL;
}

_Use_decl_annotations_
NTSTATUS
MshcMiniportInitialize(
    _In_ PSD_MINIPORT Miniport
    )
{
    NTSTATUS Status;
    PMSHC_PLATFORM_INFO_LIST_ENTRY Entry = NULL;
    PDEVICE_OBJECT Pdo;
    MSHC_PLATFORM_TYPE PlatformType;
    PMSHC_PLATFORM_DRIVER_DATA PlatformDriverData;

    Entry = (PMSHC_PLATFORM_INFO_LIST_ENTRY)ExAllocatePoolZero(
        NonPagedPoolNx,
        sizeof(MSHC_PLATFORM_INFO_LIST_ENTRY),
        MSHC_ALLOC_TAG);

    if (Entry == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        MSHC_LOG_ERROR(DriverLogHandle, NULL, "Failed to allocate list entry!");
        goto Exit;
    }

    Entry->MiniportFdo = (PDEVICE_OBJECT)Miniport->ConfigurationInfo.DeviceObject;

    Pdo = Entry->MiniportFdo->DeviceObjectExtension->AttachedTo;

    PlatformType = MshcGetPlatformType(Pdo);
    MSHC_LOG_INFO(
        DriverLogHandle,
        NULL,
        "Platform type: %!PLATFORMTYPE!",
        PlatformType);

    PlatformDriverData = MshcGetPlatformDriverData(PlatformType);
    if (PlatformDriverData == NULL) {
        Status = STATUS_NOT_SUPPORTED;
        MSHC_LOG_ERROR(DriverLogHandle, NULL, "Failed to get platform driver data!");
        goto Exit;
    }

    RtlCopyMemory(&Entry->PlatformInfo, &PlatformDriverData->Info, sizeof(MSHC_PLATFORM_INFO));

    Status = MshcQueryAcpiPlatformCapabilities(Pdo, &Entry->PlatformInfo.Capabilities);
    if (!NT_SUCCESS(Status)) {
        Status = STATUS_ACPI_INVALID_DATA;
        goto Exit;
    }

    if ((Entry->PlatformInfo.Capabilities.ClockFrequency == 0) &&
        (PlatformDriverData->Operations.SetClock == NULL)) {
        Status = STATUS_ACPI_INVALID_DATA;
        MSHC_LOG_ERROR(DriverLogHandle, NULL, "Missing platform clock info!");
        goto Exit;
    }

    ExAcquireFastMutex(&gPlatformInfoListLock);
    InsertTailList(gPlatformInfoList, &Entry->ListEntry);
    ExReleaseFastMutex(&gPlatformInfoListLock);

Exit:
    if (!NT_SUCCESS(Status) && (Entry != NULL)) {
        ExFreePoolWithTag(Entry, MSHC_ALLOC_TAG);
    }

    return Status;
}

_Use_decl_annotations_
VOID
MshcLogInit(
    PMSHC_EXTENSION MshcExtension
    )
{
    //
    // After WPP is enabled for the driver, WPP creates a default IFR log.
    // The buffer size of the default log to which WPP prints is 4096 bytes.
    // For a debug build, the buffer is 24576 bytes.
    //
    RECORDER_LOG_CREATE_PARAMS recorderLogCreateParams;
    RECORDER_LOG_CREATE_PARAMS_INIT(&recorderLogCreateParams, nullptr);

    //
    // NOTE: Actual log size may be adjusted down by WPP.
    //
    recorderLogCreateParams.TotalBufferSize = MSHC_RECORDER_TOTAL_BUFFER_SIZE;
    recorderLogCreateParams.ErrorPartitionSize = MSHC_RECORDER_ERROR_PARTITION_SIZE;
    RtlStringCchPrintfA(recorderLogCreateParams.LogIdentifier,
        sizeof(recorderLogCreateParams.LogIdentifier),
        MSHC_RECORDER_LOG_ID,
        (ULONG)(ULONG_PTR)MshcExtension->PhysicalAddress);

    NTSTATUS status = WppRecorderLogCreate(
        &recorderLogCreateParams,
        &MshcExtension->LogHandle);
    if (!NT_SUCCESS(status)) {
        NT_ASSERTMSG("Unable to create trace log recorder - default log will be used instead", FALSE);
        MshcExtension->LogHandle = WppRecorderLogGetDefault();
    }
}

_Use_decl_annotations_
VOID
MshcLogCleanup(
    PMSHC_EXTENSION MshcExtension
    )
{
    WppRecorderLogDelete(MshcExtension->LogHandle);
}

_Use_decl_annotations_
NTSTATUS
RegKeyQueryUlongValue(
    _In_ HANDLE Key,
    _In_ PUNICODE_STRING ValueName,
    _Out_ PULONG Value
    )
{
    NTSTATUS Status;
    UCHAR Buffer[sizeof(KEY_VALUE_PARTIAL_INFORMATION) + sizeof(ULONG)];
    ULONG ResultLength;

    Status = ZwQueryValueKey(
        Key,
        ValueName,
        KeyValuePartialInformation,
        Buffer,
        sizeof(Buffer),
        &ResultLength);
    if (!NT_SUCCESS(Status)) {
        return Status;
    }

    *Value = *((PULONG)&((PKEY_VALUE_PARTIAL_INFORMATION)Buffer)->Data);

    return Status;
}
