/*++

Copyright (c) 2024 Mario Bălănică <mariobalanica02@gmail.com>
Copyright (c) Microsoft Corporation. All rights reserved.

SPDX-License-Identifier: MIT

Module Name:

    precomp.h

Abstract:

    Precompiled header

Environment:

    Kernel mode only.

--*/

#pragma once

#define POOL_ZERO_DOWN_LEVEL_SUPPORT

#include <ntddk.h>

extern "C" {
    #include <sdport.h>
    #include <acpiioct.h>
    #include <initguid.h>
} // extern "C"

#define PAGED_CODE_SEG __declspec(code_seg("PAGE"))
#define INIT_CODE_SEG __declspec(code_seg("INIT"))
