/*++

Copyright (c) Microsoft Corporation. All rights reserved.
Copyright (c) 2024 Mario Bălănică <mariobalanica02@gmail.com>

Module Name:

    trace.h

Abstract:

    Defines all WPP logging macros, definitions and logging customizations.

Environment:

    Kernel mode only.

--*/

#pragma once

extern "C" {
#include <WppRecorder.h>
} // extern "C"

//
// Extended DbgPrint support
//
#define _DCS_KERNEL_
#include <TraceDbg.h>

extern "C" {
#define WPP_CHECK_FOR_NULL_STRING  // to prevent exceptions due to NULL strings

//
// Have a larger WPP Log Buffer in case of a debug build
//
#if DBG
#define MSHC_RECORDER_TOTAL_BUFFER_SIZE       (PAGE_SIZE * 4)
#define MSHC_RECORDER_ERROR_PARTITION_SIZE    (MSHC_RECORDER_TOTAL_BUFFER_SIZE / 4)
#else
#define MSHC_RECORDER_TOTAL_BUFFER_SIZE       (PAGE_SIZE)
#define MSHC_RECORDER_ERROR_PARTITION_SIZE    (MSHC_RECORDER_TOTAL_BUFFER_SIZE / 4)
#endif

//
// Enable crashdump path debugging via DbgPrint
//
#ifndef MSHC_DEBUG_CRASHDUMP
#define MSHC_DEBUG_CRASHDUMP DBG
#endif
//
// ComponentId used to filter debug messages sent to kernel debugger
// Use IHVBUS in the registry to enable seeing debug messages
// from kernel debugger. Example:
// reg add "HKLM\SYSTEM\ControlSet001\Control\Session Manager\Debug Print Filter" /v "IHVBUS" /t REG_DWORD /d 0xF /f
//
#define MSHC_COMPONENT_ID          DPFLTR_IHVBUS_ID

//
// An identifier string to prefix debug print outputs
//
#define MSHC_DBG_PRINT_PREFIX      "dwcmshc"

//
// The identifier for each MSHC controller log where %x is the controller
// physical address as supplied by Sdport
//
#define MSHC_RECORDER_LOG_ID       "MSHC#%x"

//
// Helpers used by tracing macros.
//
extern BOOLEAN gCrashdumpMode;
extern BOOLEAN gBreakOnError;

//
// Tracing GUID - fc48551a-5d5a-4609-8b04-b4165f0d25b0
// Define 2 logging flags, default flag and another one to
// control the logging of entry and exit
//
#define WPP_CONTROL_GUIDS \
    WPP_DEFINE_CONTROL_GUID(DWCMSHC, (fc48551a,5d5a,4609,8b04,b4165f0d25b0), \
        WPP_DEFINE_BIT(DEFAULT)     /* 0x00000001 */ \
        WPP_DEFINE_BIT(ENTEREXIT)   /* 0x00000002 */ \
    )

//
// Utility macro to extract MSHC physical address from private extension pointer
//
#define SLOTEXT_PA(SLOTEXT)   ((SLOTEXT) ? ((PMSHC_EXTENSION)SLOTEXT)->PhysicalAddress : NULL)

//
// sdport enums and types to use in logging macros
// Example: Using %!BUSSPEED! with value of 2 will get translated to
// SdBusSpeedHigh string by WPP trace preprocessor
//
// begin_wpp config
// CUSTOM_TYPE(REQUESTTYPE, ItemEnum(_SDPORT_REQUEST_TYPE));
// CUSTOM_TYPE(RESETTYPE, ItemEnum(_SDPORT_RESET_TYPE));
// CUSTOM_TYPE(BUSVOLTAGE, ItemEnum(_SDPORT_BUS_VOLTAGE));
// CUSTOM_TYPE(BUSWIDTH, ItemEnum(_SDPORT_BUS_WIDTH));
// CUSTOM_TYPE(BUSSPEED, ItemEnum(_SDPORT_BUS_SPEED));
// CUSTOM_TYPE(SIGNALINGVOLTAGE, ItemEnum(_SDPORT_SIGNALING_VOLTAGE));
// CUSTOM_TYPE(BUSOPERATIONTYPE, ItemEnum(_SDPORT_BUS_OPERATION_TYPE));
// CUSTOM_TYPE(PLATFORMTYPE, ItemEnum(_MSHC_PLATFORM_TYPE));
// end_wpp
//

#if defined(WPP_DEFINE_TYPE)
__inline const char* CDbgPrintTrace::ExpandCustom(const char* arg, va_list & ap)
{
    switch ((arg[0] << 8) | arg[1]) {
    case 'RE':
        WPP_DEFINE_LIST(REQUESTTYPE, unsigned char, %hhu,
            WPP_DEFINE_ENUM_VALUE(SdRequestTypeUnspecified)
            WPP_DEFINE_ENUM_VALUE(SdRequestTypeCommandNoTransfer)
            WPP_DEFINE_ENUM_VALUE(SdRequestTypeCommandWithTransfer)
            WPP_DEFINE_ENUM_VALUE(SdRequestTypeStartTransfer)
        )
        WPP_DEFINE_LIST(RESETTYPE, unsigned char, %hhu,
            WPP_DEFINE_ENUM_VALUE(SdResetTypeUndefined)
            WPP_DEFINE_ENUM_VALUE(SdResetTypeAll)
            WPP_DEFINE_ENUM_VALUE(SdResetTypeCmd)
            WPP_DEFINE_ENUM_VALUE(SdResetTypeDat)
        )
        break;
    case 'BU':
        WPP_DEFINE_LIST(BUSVOLTAGE, unsigned char, %hhu,
            WPP_DEFINE_ENUM_VALUE(SdBusVoltageUndefined)
            WPP_DEFINE_ENUM_VALUE(SdBusVoltage33)
            WPP_DEFINE_ENUM_VALUE(SdBusVoltage30)
            WPP_DEFINE_ENUM_VALUE(SdBusVoltage18)
            WPP_DEFINE_ENUM_VALUE(SdBusVoltageOff)
        )
        WPP_DEFINE_LIST(BUSWIDTH, unsigned char, %hhu,
            WPP_DEFINE_ENUM_VALUE(SdBusWidthUndefined)
            WPP_DEFINE_ENUM_VALUE(SdBusWidth1Bit)
            WPP_DEFINE_ENUM_VALUE(SdBusWidth4Bit)
            WPP_DEFINE_ENUM_VALUE(SdBusWidth8Bit)
        )
        WPP_DEFINE_LIST(BUSSPEED, unsigned char, %hhu,
            WPP_DEFINE_ENUM_VALUE(SdBusSpeedUndefined)
            WPP_DEFINE_ENUM_VALUE(SdBusSpeedNormal)
            WPP_DEFINE_ENUM_VALUE(SdBusSpeedHigh)
            WPP_DEFINE_ENUM_VALUE(SdBusSpeedSDR12)
            WPP_DEFINE_ENUM_VALUE(SdBusSpeedSDR25)
            WPP_DEFINE_ENUM_VALUE(SdBusSpeedSDR50)
            WPP_DEFINE_ENUM_VALUE(SdBusSpeedDDR50)
            WPP_DEFINE_ENUM_VALUE(SdBusSpeedSDR104)
            WPP_DEFINE_ENUM_VALUE(SdBusSpeedHS200)
            WPP_DEFINE_ENUM_VALUE(SdBusSpeedHS400)
        )
        WPP_DEFINE_LIST(BUSOPERATIONTYPE, unsigned char, %hhu,
            WPP_DEFINE_ENUM_VALUE(SdBusOperationUndefined)
            WPP_DEFINE_ENUM_VALUE(SdResetHw)
            WPP_DEFINE_ENUM_VALUE(SdResetHost)
            WPP_DEFINE_ENUM_VALUE(SdSetClock)
            WPP_DEFINE_ENUM_VALUE(SdSetVoltage)
            WPP_DEFINE_ENUM_VALUE(SdSetPower)
            WPP_DEFINE_ENUM_VALUE(SdSetBusWidth)
            WPP_DEFINE_ENUM_VALUE(SdSetBusSpeed)
            WPP_DEFINE_ENUM_VALUE(SdSetSignalingVoltage)
            WPP_DEFINE_ENUM_VALUE(SdSetDriveStrength)
            WPP_DEFINE_ENUM_VALUE(SdSetDriverType)
            WPP_DEFINE_ENUM_VALUE(SdSetPresetValue)
            WPP_DEFINE_ENUM_VALUE(SdSetBlockGapInterrupt)
            WPP_DEFINE_ENUM_VALUE(SdExecuteTuning)
        )
        break;
    case 'SI':
        WPP_DEFINE_LIST(SIGNALINGVOLTAGE, unsigned char, %hhu,
            WPP_DEFINE_ENUM_VALUE(SdignalingVoltageUndefined)
            WPP_DEFINE_ENUM_VALUE(SdSignalingVoltage33)
            WPP_DEFINE_ENUM_VALUE(SdSignalingVoltage18)
        )
        break;
    }

    return NULL;
}
#endif // WPP_DEFINE_TYPE

#define WPP_LOG_ALWAYS_TRACE_CALL(LEVEL_STR, LEVEL, SLOTEXT, MSG, ...) \
    if (gCrashdumpMode) { \
        DbgPrintTraceMsg(MSHC_COMPONENT_ID, LEVEL, NULL, \
            MSHC_DBG_PRINT_PREFIX ": [0x%p] " LEVEL_STR " " __FUNCTION__ ":" MSG "\n", \
            SLOTEXT_PA(SLOTEXT), __VA_ARGS__); \
    }

#define WPP_LOG_ALWAYS_TRACE(x)	WPP_LOG_ALWAYS_TRACE_CALL x

#if MSHC_DEBUG_CRASHDUMP
#define WPP_LOG_ALWAYS(EX, ...)	WPP_LOG_ALWAYS_TRACE((EX, __VA_ARGS__))
#endif

//
// MACRO: MSHC_LOG_INFO
//
// begin_wpp config
// USEPREFIX (MSHC_LOG_INFO, "%!STDPREFIX![0x%p] INFO %!FUNC!: ", SLOTEXT_PA(LOG_INFO_SLOTEXT));
// FUNC MSHC_LOG_INFO{LEVEL=TRACE_LEVEL_INFORMATION, FLAGS=DEFAULT}(IFRLOG, LOG_INFO_SLOTEXT, MSG, ...);
// end_wpp
//
#define WPP_RECORDER_LEVEL_FLAGS_IFRLOG_LOG_INFO_SLOTEXT_FILTER(LEVEL, FLAGS, IFRLOG, IFRLOG_INFO_SLOTEXT) \
    WPP_RECORDER_IFRLOG_LEVEL_FLAGS_FILTER(IFRLOG, LEVEL, FLAGS)

#define WPP_RECORDER_LEVEL_FLAGS_IFRLOG_LOG_INFO_SLOTEXT_ARGS(LEVEL, FLAGS, IFRLOG, LOG_INFO_SLOTEXT) \
    WPP_RECORDER_IFRLOG_LEVEL_FLAGS_ARGS(IFRLOG, LEVEL, FLAGS)

#define WPP_EX_LEVEL_FLAGS_IFRLOG_LOG_INFO_SLOTEXT(LEVEL, FLAGS, IFRLOG, LOG_INFO_SLOTEXT) \
    "INFO", DPFLTR_INFO_LEVEL, LOG_INFO_SLOTEXT

//
// MACRO: MSHC_LOG_TRACE
//
// begin_wpp config
// USEPREFIX (MSHC_LOG_TRACE, "%!STDPREFIX![0x%p] TRACE %!FUNC!: ", SLOTEXT_PA(LOG_TRACE_SLOTEXT));
// FUNC MSHC_LOG_TRACE{LEVEL=TRACE_LEVEL_VERBOSE, FLAGS=DEFAULT}(IFRLOG, LOG_TRACE_SLOTEXT, MSG, ...);
// end_wpp
//
#define WPP_RECORDER_LEVEL_FLAGS_IFRLOG_LOG_TRACE_SLOTEXT_FILTER(LEVEL, FLAGS, IFRLOG, LOG_TRACE_SLOTEXT) \
    WPP_RECORDER_IFRLOG_LEVEL_FLAGS_FILTER(IFRLOG, LEVEL, FLAGS)

#define WPP_RECORDER_LEVEL_FLAGS_IFRLOG_LOG_TRACE_SLOTEXT_ARGS(LEVEL, FLAGS, IFRLOG, LOG_TRACE_SLOTEXT) \
    WPP_RECORDER_IFRLOG_LEVEL_FLAGS_ARGS(IFRLOG, LEVEL, FLAGS)

#define WPP_EX_LEVEL_FLAGS_IFRLOG_LOG_TRACE_SLOTEXT(LEVEL, FLAGS, IFRLOG, LOG_TRACE_SLOTEXT) \
    "TRACE", DPFLTR_TRACE_LEVEL, LOG_TRACE_SLOTEXT

//
// MACRO: MSHC_LOG_WARN
//
// begin_wpp config
// USEPREFIX (MSHC_LOG_WARN, "%!STDPREFIX![0x%p] WARN %!FUNC!: ", SLOTEXT_PA(LOG_WARN_SLOTEXT));
// FUNC MSHC_LOG_WARN{LEVEL=TRACE_LEVEL_WARNING, FLAGS=DEFAULT}(IFRLOG, LOG_WARN_SLOTEXT, MSG, ...);
// end_wpp
//
#define WPP_RECORDER_LEVEL_FLAGS_IFRLOG_LOG_WARN_SLOTEXT_FILTER(LEVEL, FLAGS, IFRLOG, LOG_WARN_SLOTEXT) \
    WPP_RECORDER_IFRLOG_LEVEL_FLAGS_FILTER(IFRLOG, LEVEL, FLAGS)

#define WPP_RECORDER_LEVEL_FLAGS_IFRLOG_LOG_WARN_SLOTEXT_ARGS(LEVEL, FLAGS, IFRLOG, LOG_WARN_SLOTEXT) \
    WPP_RECORDER_IFRLOG_LEVEL_FLAGS_ARGS(IFRLOG, LEVEL, FLAGS)

#define WPP_EX_LEVEL_FLAGS_IFRLOG_LOG_WARN_SLOTEXT(LEVEL, FLAGS, IFRLOG, LOG_WARN_SLOTEXT) \
    "WARN", DPFLTR_WARNING_LEVEL, LOG_WARN_SLOTEXT

//
// MACRO: MSHC_LOG_ERROR
//
// begin_wpp config
// USEPREFIX (MSHC_LOG_ERROR, "%!STDPREFIX![0x%p] ERROR %!FUNC!:", SLOTEXT_PA(LOG_ERROR_SLOTEXT));
// FUNC MSHC_LOG_ERROR{LEVEL=TRACE_LEVEL_ERROR, FLAGS=DEFAULT}(IFRLOG, LOG_ERROR_SLOTEXT, MSG, ...);
// end_wpp
//
#define WPP_RECORDER_LEVEL_FLAGS_IFRLOG_LOG_ERROR_SLOTEXT_ARGS(LEVEL, FLAGS, IFRLOG, LOG_ERROR_SLOTEXT) \
    WPP_RECORDER_IFRLOG_LEVEL_FLAGS_ARGS(IFRLOG, LEVEL, FLAGS)

#define WPP_RECORDER_LEVEL_FLAGS_IFRLOG_LOG_ERROR_SLOTEXT_FILTER(LEVEL, FLAGS, IFRLOG, LOG_ERROR_SLOTEXT) \
    WPP_RECORDER_IFRLOG_LEVEL_FLAGS_FILTER(IFRLOG, LEVEL, FLAGS)

#define WPP_EX_LEVEL_FLAGS_IFRLOG_LOG_ERROR_SLOTEXT(LEVEL, FLAGS, IFRLOG, LOG_ERROR_SLOTEXT) \
    "ERROR", DPFLTR_ERROR_LEVEL, LOG_ERROR_SLOTEXT

#if DBG
#define WPP_LEVEL_FLAGS_IFRLOG_LOG_ERROR_SLOTEXT_PRE(LEVEL, FLAGS, IFRLOG, LOG_ERROR_SLOTEXT) \
    { const PMSHC_EXTENSION _MshcExtension = (PMSHC_EXTENSION)LOG_ERROR_SLOTEXT;

#define WPP_LEVEL_FLAGS_IFRLOG_LOG_ERROR_SLOTEXT_POST(LEVEL, FLAGS, IFRLOG, LOG_ERROR_SLOTEXT) ; \
    (((_MshcExtension && _MshcExtension->BreakOnError) || (!_MshcExtension && gBreakOnError)) ? DbgBreakPoint() : 1); }
#endif

//
// MACRO: MSHC_LOG_ENTER
//
// begin_wpp config
// USEPREFIX (MSHC_LOG_ENTER, "%!STDPREFIX![0x%p] ENTER ==> %!FUNC!:", SLOTEXT_PA(LOG_ENTER_SLOTEXT));
// FUNC MSHC_LOG_ENTER{LEVEL=TRACE_LEVEL_VERBOSE, FLAGS=ENTEREXIT}(IFRLOG, LOG_ENTER_SLOTEXT, MSG, ...);
// end_wpp
//
#define WPP_RECORDER_LEVEL_FLAGS_IFRLOG_LOG_ENTER_SLOTEXT_ARGS(LEVEL, FLAGS, IFRLOG, LOG_ENTER_SLOTEXT) \
    WPP_RECORDER_IFRLOG_LEVEL_FLAGS_ARGS(IFRLOG, LEVEL, FLAGS)

#define WPP_RECORDER_LEVEL_FLAGS_IFRLOG_LOG_ENTER_SLOTEXT_FILTER(LEVEL, FLAGS, IFRLOG, LOG_ENTER_SLOTEXT) \
    WPP_RECORDER_IFRLOG_LEVEL_FLAGS_FILTER(IFRLOG, LEVEL, FLAGS)

#define WPP_EX_LEVEL_FLAGS_IFRLOG_LOG_ENTER_SLOTEXT(LEVEL, FLAGS, IFRLOG, LOG_ENTER_SLOTEXT) \
    "ENTER ==>", DPFLTR_TRACE_LEVEL, LOG_ENTER_SLOTEXT

//
// MACRO: MSHC_LOG_EXIT
//
// begin_wpp config
// USEPREFIX (MSHC_LOG_EXIT, "%!STDPREFIX![0x%p] EXIT <== %!FUNC!:", SLOTEXT_PA(LOG_EXIT_SLOTEXT));
// FUNC MSHC_LOG_EXIT{LEVEL=TRACE_LEVEL_VERBOSE, FLAGS=ENTEREXIT}(IFRLOG, LOG_EXIT_SLOTEXT, MSG, ...);
// end_wpp
//
#define WPP_RECORDER_LEVEL_FLAGS_IFRLOG_LOG_EXIT_SLOTEXT_ARGS(LEVEL, FLAGS, IFRLOG, LOG_EXIT_SLOTEXT) \
    WPP_RECORDER_IFRLOG_LEVEL_FLAGS_ARGS(IFRLOG, LEVEL, FLAGS)

#define WPP_RECORDER_LEVEL_FLAGS_IFRLOG_LOG_EXIT_SLOTEXT_FILTER(LEVEL, FLAGS, IFRLOG, LOG_EXIT_SLOTEXT) \
    WPP_RECORDER_IFRLOG_LEVEL_FLAGS_FILTER(IFRLOG, LEVEL, FLAGS)

#define WPP_EX_LEVEL_FLAGS_IFRLOG_LOG_EXIT_SLOTEXT(LEVEL, FLAGS, IFRLOG, LOG_EXIT_SLOTEXT) \
    "EXIT <==", DPFLTR_TRACE_LEVEL, LOG_EXIT_SLOTEXT

} // extern "C"
