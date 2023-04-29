#pragma once

#include <Ntddk.h>

extern "C" {
    #include <sdport.h>
    #include <acpiioct.h>
    #include <initguid.h>
} // extern "C"

#define NONPAGED_SEGMENT_BEGIN \
    __pragma(code_seg(push)) \
    //__pragma(code_seg(.text))

#define NONPAGED_SEGMENT_END \
    __pragma(code_seg(pop))

#define INIT_SEGMENT_BEGIN \
    __pragma(code_seg(push)) \
    __pragma(code_seg("INIT"))

#define INIT_SEGMENT_END \
    __pragma(code_seg(pop))

#define POOL_ZERO_DOWN_LEVEL_SUPPORT
