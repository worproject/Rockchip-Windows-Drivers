#if !defined(_PL330DMA_H_)
#define _PL330DMA_H_

#pragma warning(disable:4200)  // suppress nameless struct/union warning
#pragma warning(disable:4201)  // suppress nameless struct/union warning
#pragma warning(disable:4214)  // suppress bit field types other than int warning
#include <ntddk.h>
#include <ntstrsafe.h>
#include <initguid.h>
#include <wdm.h>

#pragma warning(default:4200)
#pragma warning(default:4201)
#pragma warning(default:4214)
#include <wdf.h>
#include <acpiioct.h>

#include "pl330.h"
#include <pl330dma.h>

//
// String definitions
//

#define DRIVERNAME                 "pl330dma.sys: "

#define PL330DMA_POOL_TAG            (ULONG) '033P'
#define PL330DMA_HARDWARE_IDS        L"CoolStar\\PL330DMA\0\0"
#define PL330DMA_HARDWARE_IDS_LENGTH sizeof(PL330DMA_HARDWARE_IDS)

#define SYMBOLIC_NAME_PREFIX       L"\\DosDevices\\%hs"

typedef struct _PL330DMA_CONFIG {
#define DMAC_MODE_NS	(1 << 0)
    UINT32 Mode;
    UINT32 DataBusWidth : 10;
    UINT32 DataBufDepth : 11;
    UINT32 NumChan : 4;
    UINT32 NumPeripherals : 6;
    UINT32 PeripheralNS;
    UINT32 NumEvents : 6;
    UINT32 IrqNS;
} PL330DMA_CONFIG, *PPL330DMA_CONFIG;

typedef struct _XFER_SPEC {
    UINT32 ccr;

    BOOLEAN involvesDevice;
    BOOLEAN toDevice;
    UINT8 Peripheral;

    int numPeriods;

    UINT32 srcAddr;
    UINT32 dstAddr;
    UINT32 periodBytes;
} XFER_SPEC, *PXFER_SPEC;

typedef struct _PL330DMA_REQ {
    PHYSICAL_ADDRESS MCbus;
    UINT8* MCcpu;
    XFER_SPEC Xfer;
} PL330DMA_REQ, *PPL330DMA_REQ;

struct _PL330DMA_CONTEXT;

typedef struct _PL330DMA_THREAD_CALLBACK {
    BOOLEAN InUse;
    PDEVICE_OBJECT Fdo;
    PDMA_NOTIFICATION_CALLBACK NotificationCallback;
    PVOID CallbackContext;
} PL330DMA_THREAD_CALLBACK, *PPL330DMA_THREAD_CALLBACK;

#define MAX_NOTIF_EVENTS 16

typedef struct _PL330DMA_THREAD {
    UINT8 Id;
    int ev;
    /* If the channel isn't in use yet */
    BOOLEAN Free;
    /* Parent Device */
    struct _PL330DMA_CONTEXT *Device;
    /* Only 1 at a time */
    PL330DMA_REQ Req[1];

    PL330DMA_THREAD_CALLBACK registeredCallbacks[MAX_NOTIF_EVENTS];

    /* Index of last submitted request (or -1 if stopped) */
    BOOLEAN ReqRunning;
} PL330DMA_THREAD, *PPL330DMA_THREAD;

typedef struct _PL330DMA_CONTEXT
{
	//
	// Handle back to the WDFDEVICE
	//

	WDFDEVICE FxDevice;

    PVOID MMIOAddress;
    SIZE_T MMIOSize;
    WDFINTERRUPT Interrupt;
    WDFINTERRUPT Interrupt2;

    PL330DMA_CONFIG Config;

    //Size of MicroCode buffers for each channel
    unsigned MCBufSz;

    //CPU address of MicroCode buffer
    UINT8 *MCodeCPU;

    //List of all Channel Threads
    PPL330DMA_THREAD Channels;
    //Pointer to the Manager thread
    PPL330DMA_THREAD Manager;

    //Interrupt Context
    UINT32 irqLastEvents;
} PL330DMA_CONTEXT, *PPL330DMA_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(PL330DMA_CONTEXT, GetDeviceContext)

//
// Function definitions
//

DRIVER_INITIALIZE DriverEntry;

EVT_WDF_DRIVER_UNLOAD Pl330DmaDriverUnload;

EVT_WDF_DRIVER_DEVICE_ADD Pl330DmaEvtDeviceAdd;

EVT_WDF_IO_QUEUE_IO_INTERNAL_DEVICE_CONTROL Pl330DmaEvtInternalDeviceControl;

EVT_WDF_INTERRUPT_ISR pl330_dma_irq;
EVT_WDF_INTERRUPT_DPC pl330_dma_dpc;

void udelay(ULONG usec);
UINT32 read32(PPL330DMA_CONTEXT pDevice, UINT32 reg);
void write32(PPL330DMA_CONTEXT pDevice, UINT32 reg, UINT32 val);

void ReadDmacConfiguration(PPL330DMA_CONTEXT pDevice);
NTSTATUS AllocResources(PPL330DMA_CONTEXT pDevice);
void FreeResources(PPL330DMA_CONTEXT pDevice);

void _stop(PPL330DMA_THREAD Thread);
BOOLEAN _start(PPL330DMA_THREAD Thread);

PPL330DMA_THREAD GetHandle(PPL330DMA_CONTEXT pDevice, int Idx);
BOOLEAN FreeHandle(PPL330DMA_CONTEXT pDevice, PPL330DMA_THREAD Thread);
void StopThread(PPL330DMA_CONTEXT pDevice, PPL330DMA_THREAD Thread);
void GetThreadRegisters(PPL330DMA_CONTEXT pDevice, PPL330DMA_THREAD Thread, UINT32* cpc, UINT32* sa, UINT32* da);

NTSTATUS SubmitAudioDMA(
    PPL330DMA_CONTEXT pDevice,
    PPL330DMA_THREAD Thread,
    BOOLEAN fromDevice,
    UINT32 srcAddr, UINT32 dstAddr,
    UINT32 len, UINT32 periodLen
);

NTSTATUS RegisterNotificationCallback(
    PPL330DMA_CONTEXT pDevice,
    PPL330DMA_THREAD Thread,
    PDEVICE_OBJECT Fdo,
    PDMA_NOTIFICATION_CALLBACK NotificationCallback,
    PVOID CallbackContext);

NTSTATUS UnregisterNotificationCallback(
    PPL330DMA_CONTEXT pDevice,
    PPL330DMA_THREAD Thread,
    PDMA_NOTIFICATION_CALLBACK NotificationCallback,
    PVOID CallbackContext);

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
#define Pl330DmaPrint(dbglevel, dbgcatagory, fmt, ...) {          \
    if (Pl330DmaDebugLevel >= dbglevel &&                         \
        (Pl330DmaDebugCatagories && dbgcatagory))                 \
		    {                                                           \
        DbgPrint(DRIVERNAME);                                   \
        DbgPrint(fmt, __VA_ARGS__);                             \
		    }                                                           \
}
#else
#define Pl330DmaPrint(dbglevel, fmt, ...) {                       \
}
#endif
#endif