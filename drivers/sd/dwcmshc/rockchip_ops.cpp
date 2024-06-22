/*++

Copyright (c) 2024 Mario Bălănică <mariobalanica02@gmail.com>
Copyright (c) 2014, Fuzhou Rockchip Electronics Co., Ltd

SPDX-License-Identifier: MIT

Module Name:

    rockchip_ops.cpp

Abstract:

    Rockchip specific abstractions

Environment:

    Kernel mode only.

--*/

#include "precomp.h"
#pragma hdrstop

#include <rk_sip_sdmmc.h>

#include "trace.h"
#include "rockchip_ops.tmh"

#include "dwcmshc.h"
#include "rockchip_ops.h"

#define NUM_PHASES              360
#define BAD_PHASES_TO_SKIP      20

#define SAMPLE_PHASE_DEFAULT    0

#define RANGE_INDEX_TO_PHASE(_Index, _NumPhases) \
        DIV_CEIL((_Index) * 360, (_NumPhases))

#define PHASE_TO_RANGE_INDEX(_Phase, _NumPhases) \
        DIV_CEIL((_Phase) * (_NumPhases), 360)

#define CRU_SD_CLKGEN_DIV   2

NTSTATUS
MshcRockchipSetClock(
    _In_ PMSHC_EXTENSION MshcExtension,
    _In_ ULONG FrequencyKhz
    )
{
    NTSTATUS Status;

    Status = RkSipSdmmcClockRateSet(
        (ULONG_PTR)MshcExtension->PhysicalAddress,
        RK_SIP_SDMMC_CLOCK_ID_MSHC_CIU,
        FrequencyKhz * 1000 * CRU_SD_CLKGEN_DIV);

    if (!NT_SUCCESS(Status)) {
        MSHC_LOG_WARN(
            MshcExtension->LogHandle,
            MshcExtension,
            "Failed to set clock rate. Status=%!STATUS!",
            Status);
    } else {
        MshcExtension->Capabilities.BaseClockFrequencyKhz = FrequencyKhz;
    }

    if (FrequencyKhz <= 400) {
        Status = RkSipSdmmcClockPhaseSet(
            (ULONG_PTR)MshcExtension->PhysicalAddress,
            RK_SIP_SDMMC_CLOCK_ID_MSHC_CIU_SAMPLE,
            SAMPLE_PHASE_DEFAULT);

        if (!NT_SUCCESS(Status)) {
            MSHC_LOG_WARN(
                MshcExtension->LogHandle,
                MshcExtension,
                "Failed to set sample phase to %d. Status=%!STATUS!",
                SAMPLE_PHASE_DEFAULT,
                Status);
        }
    }

    ULONG DrivePhase = 90;

    if (FrequencyKhz >= 100000) {
        DrivePhase = 180;
    }

    Status = RkSipSdmmcClockPhaseSet(
        (ULONG_PTR)MshcExtension->PhysicalAddress,
        RK_SIP_SDMMC_CLOCK_ID_MSHC_CIU_DRIVE,
        DrivePhase);

    if (!NT_SUCCESS(Status)) {
        MSHC_LOG_WARN(
            MshcExtension->LogHandle,
            MshcExtension,
            "Failed to set drive phase to %d. Status=%!STATUS!",
            DrivePhase,
            Status);
    }

    return STATUS_SUCCESS;
}

NTSTATUS
MshcRockchipSetVoltage(
    _In_ PMSHC_EXTENSION MshcExtension,
    _In_ SDPORT_BUS_VOLTAGE Voltage
    )
{
    NTSTATUS Status;

    Status = RkSipSdmmcRegulatorEnableSet(
        (ULONG_PTR)MshcExtension->PhysicalAddress,
        RK_SIP_SDMMC_REGULATOR_ID_SUPPLY,
        Voltage == SdBusVoltage33);

    if (!NT_SUCCESS(Status)) {
        MSHC_LOG_WARN(
            MshcExtension->LogHandle,
            MshcExtension,
            "Failed to set external regulator. Status=%!STATUS!",
            Status);
    }

    return Status;
}

NTSTATUS
MshcRockchipSetSignalingVoltage(
    _In_ PMSHC_EXTENSION MshcExtension,
    _In_ SDPORT_SIGNALING_VOLTAGE Voltage
    )
{
    NTSTATUS Status;

    Status = RkSipSdmmcRegulatorVoltageSet(
        (ULONG_PTR)MshcExtension->PhysicalAddress,
        RK_SIP_SDMMC_REGULATOR_ID_SIGNAL,
        (Voltage == SdSignalingVoltage18) ? 1800000 : 3300000);

    if (!NT_SUCCESS(Status)) {
        MSHC_LOG_WARN(
            MshcExtension->LogHandle,
            MshcExtension,
            "Failed to set external regulator. Status=%!STATUS!",
            Status);
    }

    return Status;
}

NTSTATUS
MshcRockchipExecuteTuning(
    _In_ PMSHC_EXTENSION MshcExtension
    )
{
    NTSTATUS Status;
    ULONG Index;
    BOOLEAN CurrentTuneCmdOk;
    BOOLEAN PreviousTuneCmdOk;
    BOOLEAN FirstTuneCmdOk;
    ULONG RangeCount;
    LONG LongestRangeLength;
    LONG LongestRange;
    LONG MiddlePhase;

    struct {
        LONG Start;
        LONG End;
    } Ranges[NUM_PHASES / 2 + 1] = { 0 };

    PreviousTuneCmdOk = FALSE;
    FirstTuneCmdOk = FALSE;
    RangeCount = 0;
    LongestRangeLength = -1;
    LongestRange = -1;

    //
    // Try each phase and extract good ranges.
    //
    for (Index = 0; Index < NUM_PHASES;) {
        Status = RkSipSdmmcClockPhaseSet(
            (ULONG_PTR)MshcExtension->PhysicalAddress,
            RK_SIP_SDMMC_CLOCK_ID_MSHC_CIU_SAMPLE,
            RANGE_INDEX_TO_PHASE(Index, NUM_PHASES));

        if (!NT_SUCCESS(Status)) {
            MSHC_LOG_WARN(
                MshcExtension->LogHandle,
                MshcExtension,
                "Failed to set sample phase to %d. Status=%!STATUS!",
                RANGE_INDEX_TO_PHASE(Index, NUM_PHASES),
                Status);
        } else {
            Status = MshcSendTuningCommand(MshcExtension);
        }
        CurrentTuneCmdOk = NT_SUCCESS(Status);

        if (Index == 0) {
            FirstTuneCmdOk = CurrentTuneCmdOk;
        }

        if (CurrentTuneCmdOk) {
            if (!PreviousTuneCmdOk) {
                RangeCount++;
                Ranges[RangeCount - 1].Start = Index;
            }
            Ranges[RangeCount - 1].End = Index;
            Index++;
        } else if (Index == NUM_PHASES - 1) {
            // No extra skipping rules if we're at the end.
            Index++;
        } else {
            //
            // No need to check too close to an invalid one since testing
            // bad phases is slow. Skip a few degrees.
            //
            Index += PHASE_TO_RANGE_INDEX(BAD_PHASES_TO_SKIP, NUM_PHASES);

            // Don't skip too far.
            if (Index >= NUM_PHASES) {
                Index = NUM_PHASES - 1;
            }
        }

        PreviousTuneCmdOk = CurrentTuneCmdOk;
    }

    if (RangeCount == 0) {
        MSHC_LOG_ERROR(
            MshcExtension->LogHandle,
            MshcExtension,
            "All phases bad!");

        return STATUS_IO_DEVICE_ERROR;
    }

    // Wrap around case (e.g. 340 - 12 degrees), merge the end points.
    if ((RangeCount > 1) && FirstTuneCmdOk && CurrentTuneCmdOk) {
        Ranges[0].Start = Ranges[RangeCount - 1].Start;
        RangeCount--;
    }

    if ((Ranges[0].Start == 0) && (Ranges[0].End == NUM_PHASES - 1)) {
        MSHC_LOG_INFO(
            MshcExtension->LogHandle,
            MshcExtension,
            "All phases good - using default phase %d",
            SAMPLE_PHASE_DEFAULT);

        Status = RkSipSdmmcClockPhaseSet(
            (ULONG_PTR)MshcExtension->PhysicalAddress,
            RK_SIP_SDMMC_CLOCK_ID_MSHC_CIU_SAMPLE,
            SAMPLE_PHASE_DEFAULT);

        if (!NT_SUCCESS(Status)) {
            MSHC_LOG_WARN(
                MshcExtension->LogHandle,
                MshcExtension,
                "Failed to set sample phase to %d. Status=%!STATUS!",
                SAMPLE_PHASE_DEFAULT,
                Status);
        }

        return STATUS_SUCCESS;
    }

    //
    // Find the longest range.
    //
    for (Index = 0; Index < RangeCount; Index++) {
        LONG Length = (Ranges[Index].End - Ranges[Index].Start + 1);

        if (Length < 0) {
            Length += NUM_PHASES;
        }

        if (LongestRangeLength < Length) {
            LongestRangeLength = Length;
            LongestRange = Index;
        }

        MSHC_LOG_INFO(
            MshcExtension->LogHandle,
            MshcExtension,
            "Good phase range %d-%d (Length=%d)",
            RANGE_INDEX_TO_PHASE(Ranges[Index].Start, NUM_PHASES),
            RANGE_INDEX_TO_PHASE(Ranges[Index].End, NUM_PHASES),
            Length);
    }

    MSHC_LOG_INFO(
        MshcExtension->LogHandle,
        MshcExtension,
        "Best phase range %d-%d (Length=%d)",
        RANGE_INDEX_TO_PHASE(Ranges[LongestRange].Start, NUM_PHASES),
        RANGE_INDEX_TO_PHASE(Ranges[LongestRange].End, NUM_PHASES),
        LongestRangeLength);

    // Sampling point should be in the middle of a good range.
    MiddlePhase = Ranges[LongestRange].Start + LongestRangeLength / 2;
    MiddlePhase %= NUM_PHASES;

    MSHC_LOG_INFO(
        MshcExtension->LogHandle,
        MshcExtension,
        "Successfully tuned phase to %d",
        RANGE_INDEX_TO_PHASE(MiddlePhase, NUM_PHASES));

    Status = RkSipSdmmcClockPhaseSet(
        (ULONG_PTR)MshcExtension->PhysicalAddress,
        RK_SIP_SDMMC_CLOCK_ID_MSHC_CIU_SAMPLE,
        RANGE_INDEX_TO_PHASE(MiddlePhase, NUM_PHASES));

    if (!NT_SUCCESS(Status)) {
        MSHC_LOG_WARN(
            MshcExtension->LogHandle,
            MshcExtension,
            "Failed to set sample phase to %d. Status=%!STATUS!",
            RANGE_INDEX_TO_PHASE(MiddlePhase, NUM_PHASES),
            Status);
    }

    return STATUS_SUCCESS;
}
