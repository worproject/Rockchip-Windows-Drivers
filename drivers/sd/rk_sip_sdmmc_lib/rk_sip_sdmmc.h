/*++

Copyright (c) 2024 Mario Bălănică <mariobalanica02@gmail.com>

SPDX-License-Identifier: MIT

Module Name:

    rk_sip_sdmmc.h

Abstract:

    Rockchip SDMMC Control Service Library header

Environment:

    Kernel mode only.

--*/

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	RK_SIP_SDMMC_CLOCK_ID_MSHC_CIU = 0,
	RK_SIP_SDMMC_CLOCK_ID_MSHC_CIU_DRIVE,
	RK_SIP_SDMMC_CLOCK_ID_MSHC_CIU_SAMPLE
} RK_SIP_SDMMC_CLOCK_ID_MSHC;

typedef enum {
	RK_SIP_SDMMC_CLOCK_ID_EMMC_CCLK = 0
} RK_SIP_SDMMC_CLOCK_ID_EMMC;

typedef enum {
	RK_SIP_SDMMC_REGULATOR_ID_SUPPLY = 0,
	RK_SIP_SDMMC_REGULATOR_ID_SIGNAL
} RK_SIP_SDMMC_REGULATOR_ID;

NTSTATUS
RkSipSdmmcClockRateGet(
    _In_ ULONG_PTR ControllerAddress,
    _In_ UCHAR Id,
    _Out_ ULONG* RateHz
    );

NTSTATUS
RkSipSdmmcClockRateSet(
    _In_ ULONG_PTR ControllerAddress,
    _In_ UCHAR Id,
    _In_ ULONG RateHz
    );

NTSTATUS
RkSipSdmmcClockPhaseGet(
    _In_ ULONG_PTR ControllerAddress,
    _In_ UCHAR Id,
    _Out_ ULONG* PhaseDegrees
    );

NTSTATUS
RkSipSdmmcClockPhaseSet(
    _In_ ULONG_PTR ControllerAddress,
    _In_ UCHAR Id,
    _In_ ULONG PhaseDegrees
    );

NTSTATUS
RkSipSdmmcRegulatorVoltageGet(
    _In_ ULONG_PTR ControllerAddress,
    _In_ UCHAR Id,
    _Out_ ULONG* Microvolts
    );

NTSTATUS
RkSipSdmmcRegulatorVoltageSet(
    _In_ ULONG_PTR ControllerAddress,
    _In_ UCHAR Id,
    _In_ ULONG Microvolts
    );

NTSTATUS
RkSipSdmmcRegulatorEnableGet(
    _In_ ULONG_PTR ControllerAddress,
    _In_ UCHAR Id,
    _Out_ BOOLEAN* Enable
    );

NTSTATUS
RkSipSdmmcRegulatorEnableSet(
    _In_ ULONG_PTR ControllerAddress,
    _In_ UCHAR Id,
    _In_ BOOLEAN Enable
    );

#ifdef __cplusplus
}
#endif
