#if !defined(_RKI2SBUS_H_)
#define _RKI2SBUS_H_

#define POOL_ZERO_DOWN_LEVEL_SUPPORT

#pragma warning(disable:4200)  // suppress nameless struct/union warning
#pragma warning(disable:4201)  // suppress nameless struct/union warning
#pragma warning(disable:4214)  // suppress bit field types other than int warning
#include <ntddk.h>
#include <initguid.h>

#include <wdm.h>
#include <wdmguid.h>
#include <wdf.h>
#include <ntintsafe.h>
#include <ntstrsafe.h>
#include <portcls.h>

#include "fdo.h"
#include "buspdo.h"

#define DRIVERNAME "acpbus.sys: "
#define RKI2SBUS_POOL_TAG 'SIKR'

//
// Helper macros
//

#define DEBUG_LEVEL_ERROR   1
#define DEBUG_LEVEL_INFO    2
#define DEBUG_LEVEL_VERBOSE 3

#define DBG_INIT  1
#define DBG_PNP   2
#define DBG_IOCTL 4

static ULONG RKI2SBusDebugLevel = 100;
static ULONG RKI2SBusDebugCatagories = DBG_INIT || DBG_PNP || DBG_IOCTL;

#if 0
#define RKI2SBusPrint(dbglevel, dbgcatagory, fmt, ...) {          \
    if (RKI2SBusDebugLevel >= dbglevel &&                         \
        (RKI2SBusDebugCatagories && dbgcatagory))                 \
		    {                                                           \
        DbgPrint(DRIVERNAME);                                   \
        DbgPrint(fmt, __VA_ARGS__);                             \
		    }                                                           \
}
#else
#define RKI2SBusPrint(dbglevel, fmt, ...) {                       \
}
#endif
#endif