#ifndef __ADSP_INTERFACE
#define __ADSP_INTERFACE
//
// The GUID_RKDSP_BUS_INTERFACE interface GUID
//
// {863DD2AC-5B54-4C57-8487-782F9ADFE8D7}
DEFINE_GUID(GUID_RKDSP_BUS_INTERFACE,
    0x863dd2ac, 0x5b54, 0x4c57, 0x84, 0x87, 0x78, 0x2f, 0x9a, 0xdf, 0xe8, 0xd7);

typedef struct _TPLG_INFO {
    PVOID rkTplg;
    UINT64 rkTplgSz;
} TPLG_INFO, * PTPLG_INFO;

typedef _Must_inspect_result_ NTSTATUS(*PGET_ADSP_RESOURCES) (_In_ PVOID _context, _Out_ _PCI_BAR* acpBar, PTPLG_INFO tplgInfo);
typedef _Must_inspect_result_ NTSTATUS(*PDSP_SET_POWER_STATE) (_In_ PVOID _context, _In_ DEVICE_POWER_STATE newPowerState);
typedef _Must_inspect_result_ BOOL(*PADSP_INTERRUPT_CALLBACK)(PVOID context);
typedef _Must_inspect_result_ VOID(*PADSP_DPC_CALLBACK)(PVOID context);
typedef _Must_inspect_result_ NTSTATUS (*PADSP_QUEUE_DPC)(PVOID context);
typedef _Must_inspect_result_ NTSTATUS(*PREGISTER_ADSP_INTERRUPT) (_In_ PVOID _context, _In_ PADSP_INTERRUPT_CALLBACK callback, _In_ PADSP_DPC_CALLBACK dpcCallback, _In_ PVOID callbackContext);
typedef _Must_inspect_result_ NTSTATUS(*PUNREGISTER_ADSP_INTERRUPT) (_In_ PVOID _context);

typedef struct _RKDSP_BUS_INTERFACE
{
    //
    // First we define the standard INTERFACE structure ...
    //
    USHORT                    Size;
    USHORT                    Version;
    PVOID                     Context;
    PINTERFACE_REFERENCE      InterfaceReference;
    PINTERFACE_DEREFERENCE    InterfaceDereference;

    //
    // Then we expand the structure with the ADSP_BUS_INTERFACE stuff.

    PGET_ADSP_RESOURCES           GetResources;
    PDSP_SET_POWER_STATE          SetDSPPowerState;
    PREGISTER_ADSP_INTERRUPT      RegisterInterrupt;
    PUNREGISTER_ADSP_INTERRUPT    UnregisterInterrupt;
    PADSP_QUEUE_DPC               QueueDPCForInterrupt;
} RKDSP_BUS_INTERFACE, * PRKDSP_BUS_INTERFACE;

#ifndef ADSP_DECL
RKDSP_BUS_INTERFACE RKDSP_BusInterface(PVOID Context);
#endif
#endif