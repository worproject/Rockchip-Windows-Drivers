#if !defined(_ES8323_H_)
#define _ES8323_H_

#pragma warning(disable:4200)  // suppress nameless struct/union warning
#pragma warning(disable:4201)  // suppress nameless struct/union warning
#pragma warning(disable:4214)  // suppress bit field types other than int warning
#include <initguid.h>
#include <wdm.h>

#pragma warning(default:4200)
#pragma warning(default:4201)
#pragma warning(default:4214)
#include <wdf.h>

#pragma warning(disable:4201)  // suppress nameless struct/union warning
#pragma warning(disable:4214)  // suppress bit field types other than int warning
#include <hidport.h>

#include "es8323.h"
#include "spb.h"

//
// String definitions
//

#define DRIVERNAME                 "es8323.sys: "

#define ES8323_POOL_TAG            (ULONG) '8323'
#define ES8323_HARDWARE_IDS        L"CoolStar\\ES8323\0\0"
#define ES8323_HARDWARE_IDS_LENGTH sizeof(ES8323_HARDWARE_IDS)

#define NTDEVICE_NAME_STRING       L"\\Device\\ES8323"
#define SYMBOLIC_NAME_STRING       L"\\DosDevices\\ES8323"

#define true 1
#define false 0

typedef struct _ES8323_CONTEXT
{

	//
	// Handle back to the WDFDEVICE
	//

	WDFDEVICE FxDevice;

	WDFQUEUE ReportQueue;

	SPB_CONTEXT I2CContext;

	BOOLEAN ConnectInterrupt;

	WDFINTERRUPT Interrupt;

} ES8323_CONTEXT, *PES8323_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(ES8323_CONTEXT, GetDeviceContext)

//
// Function definitions
//

DRIVER_INITIALIZE DriverEntry;

EVT_WDF_DRIVER_UNLOAD Es8323DriverUnload;

EVT_WDF_DRIVER_DEVICE_ADD Es8323EvtDeviceAdd;

EVT_WDFDEVICE_WDM_IRP_PREPROCESS Es8323EvtWdmPreprocessMnQueryId;

EVT_WDF_IO_QUEUE_IO_INTERNAL_DEVICE_CONTROL Es8323EvtInternalDeviceControl;

//
// Helper macros
//

#define DEBUG_LEVEL_ERROR   1
#define DEBUG_LEVEL_INFO    2
#define DEBUG_LEVEL_VERBOSE 3

#define DBG_INIT  1
#define DBG_PNP   2
#define DBG_IOCTL 4

#if 0
#define Es8323Print(dbglevel, dbgcatagory, fmt, ...) {          \
    if (Es8323DebugLevel >= dbglevel &&                         \
        (Es8323DebugCatagories && dbgcatagory))                 \
		    {                                                           \
        DbgPrint(DRIVERNAME);                                   \
        DbgPrint(fmt, __VA_ARGS__);                             \
		    }                                                           \
}
#else
#define Es8323Print(dbglevel, fmt, ...) {                       \
}
#endif
#endif