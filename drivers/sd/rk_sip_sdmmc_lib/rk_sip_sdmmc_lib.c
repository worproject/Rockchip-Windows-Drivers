/*++

Copyright (c) 2024 Mario Bălănică <mariobalanica02@gmail.com>

SPDX-License-Identifier: MIT

Module Name:

    rk_sip_sdmmc.c

Abstract:

    Rockchip SDMMC Control Service Library implementation

Environment:

    Kernel mode only.

--*/

#include <ntddk.h>
#include <ArmSmcLib.h>

#ifdef _ARM64_
#define ARCH_SMC_ID(_Id) ((_Id) | (1 << 30))
#else
#define ARCH_SMC_ID(_Id) ((_Id) & ~(1 << 30))
#endif

/* Rockchip SD/MMC Control Service (for Windows on Arm) */
#define RK_SIP_SDMMC_CLOCK_RATE_GET     	0x82000020
#define RK_SIP_SDMMC_CLOCK_RATE_SET     	0x82000021
#define RK_SIP_SDMMC_CLOCK_PHASE_GET    	0x82000022
#define RK_SIP_SDMMC_CLOCK_PHASE_SET    	0x82000023
#define RK_SIP_SDMMC_REGULATOR_VOLTAGE_GET	0x82000024
#define RK_SIP_SDMMC_REGULATOR_VOLTAGE_SET	0x82000025
#define RK_SIP_SDMMC_REGULATOR_ENABLE_GET	0x82000026
#define RK_SIP_SDMMC_REGULATOR_ENABLE_SET	0x82000027

enum {
    RK_SIP_E_SUCCESS = 0,
    RK_SIP_E_NOT_SUPPORTED = -1, // SMC_UNK
    RK_SIP_E_INVALID_PARAM = -2,
    RK_SIP_E_NOT_IMPLEMENTED = -3,
};

__forceinline
NTSTATUS
SmcReturnToStatus(
    _In_ ARM_SMC_ARGS *SmcArgs
    )
{
    switch (SmcArgs->Arg0) {
    case RK_SIP_E_SUCCESS:
        return STATUS_SUCCESS;
    case RK_SIP_E_NOT_SUPPORTED:
        return STATUS_NOT_SUPPORTED;
    case RK_SIP_E_INVALID_PARAM:
        return STATUS_INVALID_PARAMETER;
    case RK_SIP_E_NOT_IMPLEMENTED:
        return STATUS_NOT_IMPLEMENTED;
    default:
        return STATUS_UNSUCCESSFUL;
    }
}

NTSTATUS
RkSipSdmmcClockRateGet(
    _In_ ULONG_PTR ControllerAddress,
    _In_ UCHAR Id,
    _Out_ ULONG* RateHz
    )
{
    ARM_SMC_ARGS SmcArgs = { 0 };

    SmcArgs.Arg0 = ARCH_SMC_ID(RK_SIP_SDMMC_CLOCK_RATE_GET);
    SmcArgs.Arg1 = ControllerAddress;
    SmcArgs.Arg2 = Id;

    ArmCallSmc(&SmcArgs);

    if (RateHz != NULL) {
        *RateHz = (ULONG)SmcArgs.Arg1;
    }

    return SmcReturnToStatus(&SmcArgs);
}

NTSTATUS
RkSipSdmmcClockRateSet(
    _In_ ULONG_PTR ControllerAddress,
    _In_ UCHAR Id,
    _In_ ULONG RateHz
    )
{
    ARM_SMC_ARGS SmcArgs = { 0 };

    SmcArgs.Arg0 = ARCH_SMC_ID(RK_SIP_SDMMC_CLOCK_RATE_SET);
    SmcArgs.Arg1 = ControllerAddress;
    SmcArgs.Arg2 = Id;
    SmcArgs.Arg3 = RateHz;

    ArmCallSmc(&SmcArgs);

    return SmcReturnToStatus(&SmcArgs);
}

NTSTATUS
RkSipSdmmcClockPhaseGet(
    _In_ ULONG_PTR ControllerAddress,
    _In_ UCHAR Id,
    _Out_ ULONG* PhaseDegrees
    )
{
    ARM_SMC_ARGS SmcArgs = { 0 };

    SmcArgs.Arg0 = ARCH_SMC_ID(RK_SIP_SDMMC_CLOCK_PHASE_GET);
    SmcArgs.Arg1 = ControllerAddress;
    SmcArgs.Arg2 = Id;

    ArmCallSmc(&SmcArgs);

    if (PhaseDegrees != NULL) {
        *PhaseDegrees = (ULONG)SmcArgs.Arg1;
    }

    return SmcReturnToStatus(&SmcArgs);
}

NTSTATUS
RkSipSdmmcClockPhaseSet(
    _In_ ULONG_PTR ControllerAddress,
    _In_ UCHAR Id,
    _In_ ULONG PhaseDegrees
    )
{
    ARM_SMC_ARGS SmcArgs = { 0 };

    SmcArgs.Arg0 = ARCH_SMC_ID(RK_SIP_SDMMC_CLOCK_PHASE_SET);
    SmcArgs.Arg1 = ControllerAddress;
    SmcArgs.Arg2 = Id;
    SmcArgs.Arg3 = PhaseDegrees;

    ArmCallSmc(&SmcArgs);

    return SmcReturnToStatus(&SmcArgs);
}

NTSTATUS
RkSipSdmmcRegulatorVoltageGet(
    _In_ ULONG_PTR ControllerAddress,
    _In_ UCHAR Id,
    _Out_ ULONG* Microvolts
    )
{
    ARM_SMC_ARGS SmcArgs = { 0 };

    SmcArgs.Arg0 = ARCH_SMC_ID(RK_SIP_SDMMC_REGULATOR_VOLTAGE_GET);
    SmcArgs.Arg1 = ControllerAddress;
    SmcArgs.Arg2 = Id;

    ArmCallSmc(&SmcArgs);

    if (Microvolts != NULL) {
        *Microvolts = (ULONG)SmcArgs.Arg1;
    }

    return SmcReturnToStatus(&SmcArgs);
}

NTSTATUS
RkSipSdmmcRegulatorVoltageSet(
    _In_ ULONG_PTR ControllerAddress,
    _In_ UCHAR Id,
    _In_ ULONG Microvolts
    )
{
    ARM_SMC_ARGS SmcArgs = { 0 };

    SmcArgs.Arg0 = ARCH_SMC_ID(RK_SIP_SDMMC_REGULATOR_VOLTAGE_SET);
    SmcArgs.Arg1 = ControllerAddress;
    SmcArgs.Arg2 = Id;
    SmcArgs.Arg3 = Microvolts;

    ArmCallSmc(&SmcArgs);

    return SmcReturnToStatus(&SmcArgs);
}

NTSTATUS
RkSipSdmmcRegulatorEnableGet(
    _In_ ULONG_PTR ControllerAddress,
    _In_ UCHAR Id,
    _Out_ BOOLEAN* Enable
    )
{
    ARM_SMC_ARGS SmcArgs = { 0 };

    SmcArgs.Arg0 = ARCH_SMC_ID(RK_SIP_SDMMC_REGULATOR_ENABLE_GET);
    SmcArgs.Arg1 = ControllerAddress;
    SmcArgs.Arg2 = Id;

    ArmCallSmc(&SmcArgs);

    if (Enable != NULL) {
        *Enable = (BOOLEAN)SmcArgs.Arg1;
    }

    return SmcReturnToStatus(&SmcArgs);
}

NTSTATUS
RkSipSdmmcRegulatorEnableSet(
    _In_ ULONG_PTR ControllerAddress,
    _In_ UCHAR Id,
    _In_ BOOLEAN Enable
    )
{
    ARM_SMC_ARGS SmcArgs = { 0 };

    SmcArgs.Arg0 = ARCH_SMC_ID(RK_SIP_SDMMC_REGULATOR_ENABLE_SET);
    SmcArgs.Arg1 = ControllerAddress;
    SmcArgs.Arg2 = Id;
    SmcArgs.Arg3 = Enable;

    ArmCallSmc(&SmcArgs);

    return SmcReturnToStatus(&SmcArgs);
}
