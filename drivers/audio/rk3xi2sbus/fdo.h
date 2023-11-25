#if !defined(_RKI2SBUS_FDO_H_)
#define _RKI2SBUS_FDO_H_

union baseaddr {
    PVOID Base;
    UINT8* baseptr;
};

typedef struct _PCI_BAR {
    union baseaddr Base;
    PHYSICAL_ADDRESS PhysAddr;
    ULONG Len;
} PCI_BAR, * PPCI_BAR;

#include "adsp.h"

struct _FDO_CONTEXT;
struct _PDO_DEVICE_DATA;

typedef struct _FDO_CONTEXT
{
    WDFDEVICE WdfDevice;

    PCI_BAR m_MMIO; //required
    WDFINTERRUPT Interrupt;

    BOOLEAN ControllerEnabled;

    PADSP_INTERRUPT_CALLBACK dspInterruptCallback;
    PADSP_DPC_CALLBACK dspDPCCallback;
    PVOID dspInterruptContext;
    PVOID rkTplg;
    UINT64 rkTplgSz;
} FDO_CONTEXT, * PFDO_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(FDO_CONTEXT, Fdo_GetContext)

NTSTATUS
Fdo_Create(
	_Inout_ PWDFDEVICE_INIT DeviceInit
);

#endif