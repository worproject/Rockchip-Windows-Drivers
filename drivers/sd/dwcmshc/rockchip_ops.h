/*++

Copyright (c) 2024 Mario Bălănică <mariobalanica02@gmail.com>

SPDX-License-Identifier: MIT

Module Name:

    rockchip_ops.h

Abstract:

    Rockchip specific abstractions

Environment:

    Kernel mode only.

--*/

#pragma once

//
// Platform callbacks
//

MSHC_PLATFORM_SET_CLOCK MshcRockchipSetClock;
MSHC_PLATFORM_SET_VOLTAGE MshcRockchipSetVoltage;
MSHC_PLATFORM_SET_SIGNALING_VOLTAGE MshcRockchipSetSignalingVoltage;
MSHC_PLATFORM_EXECUTE_TUNING MshcRockchipExecuteTuning;
