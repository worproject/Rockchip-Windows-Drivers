#pragma once

// *dwc_eqos = {5d8331d3-70b3-5620-5664-db28f48a4b79}
TRACELOGGING_DECLARE_PROVIDER(TraceProvider);

enum : UINT8
{
    LEVEL_CRITICAL = WINEVENT_LEVEL_CRITICAL,
    LEVEL_ERROR    = WINEVENT_LEVEL_ERROR,
    LEVEL_WARNING  = WINEVENT_LEVEL_WARNING,
    LEVEL_INFO     = WINEVENT_LEVEL_INFO,
    LEVEL_VERBOSE  = WINEVENT_LEVEL_VERBOSE,
};

#define KW_GENERAL    TraceLoggingKeyword(0x1) // All events should have at least one keyword.
#define KW_ENTRY_EXIT TraceLoggingKeyword(0x2)

// TraceLoggingWrite(eventName, KW_GENERAL, ...).
#define TraceWrite(eventName, level, ...) \
    TraceLoggingWrite(TraceProvider, eventName, TraceLoggingLevel(level), KW_GENERAL, __VA_ARGS__)

// TraceLoggingWrite("funcName", KW_ENTRY_EXIT, ...).
#define TraceEntryExit(funcName, level, ...) \
    TraceWrite(#funcName, level, KW_ENTRY_EXIT, __VA_ARGS__) \

// TraceLoggingWrite("funcName", KW_ENTRY_EXIT, status, ...)
#define TraceEntryExitWithStatus(funcName, successLevel, status, ...) \
    if (NTSTATUS const _status = (status); NT_SUCCESS(_status)) { \
        TraceEntryExit(funcName, successLevel, TraceLoggingNTStatus(_status, "status"), __VA_ARGS__); \
    } else { \
        TraceEntryExit(funcName, LEVEL_ERROR, TraceLoggingNTStatus(_status, "status"), __VA_ARGS__); \
    } \

// TraceLoggingWrite("funcName-Entry", KW_ENTRY_EXIT, ...).
#define TraceEntry(funcName, level, ...) \
    TraceWrite(#funcName "-Entry", level, KW_ENTRY_EXIT, __VA_ARGS__) \

// TraceLoggingWrite("funcName-Exit", KW_ENTRY_EXIT, ...).
#define TraceExit(funcName, level, ...) \
    TraceWrite(#funcName "-Exit", level, KW_ENTRY_EXIT, __VA_ARGS__) \

// TraceLoggingWrite("funcName-Exit", KW_ENTRY_EXIT, status, ...)
#define TraceExitWithStatus(funcName, successLevel, status, ...) \
    if (NTSTATUS const _status = (status); NT_SUCCESS(_status)) { \
        TraceExit(funcName, successLevel, TraceLoggingNTStatus(_status, "status"), __VA_ARGS__); \
    } else { \
        TraceExit(funcName, LEVEL_ERROR, TraceLoggingNTStatus(_status, "status"), __VA_ARGS__); \
    } \
