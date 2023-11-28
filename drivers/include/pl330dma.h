#ifndef _PL330DMA_INTERFACE_H
#define _PL330DMA_INTERFACE_H

typedef
void
(*PDMA_NOTIFICATION_CALLBACK)(
    PVOID context
    );

typedef
HANDLE
(*PGET_HANDLE)(
    IN      PVOID Context,
    IN      int Idx
    );

typedef
BOOLEAN
(*PFREE_HANDLE)(
    IN PVOID Context,
    IN HANDLE Handle
    );

typedef
VOID
(*PSTOP_DMA)(
    IN PVOID Context,
    IN HANDLE Handle
    );

typedef
VOID
(*PGET_THREAD_REGISTERS)(
    IN PVOID Context,
    IN HANDLE Handle,
    OUT UINT32* cpc,
    OUT UINT32* sa,
    OUT UINT32* da
    );

typedef
NTSTATUS
(*PSUBMIT_AUDIO_DMA) (
    IN PVOID Context,
    IN HANDLE Handle,
    IN BOOLEAN fromDevice,
    IN UINT32 srcAddr,
    IN UINT32 dstAddr,
    IN UINT32 len,
    IN UINT32 periodLen
    );

typedef
NTSTATUS
(*PSUBMIT_DMA) (
    IN PVOID Context,
    IN HANDLE Handle,
    IN BOOLEAN fromDevice,
    IN PMDL pMDL,
    IN UINT32 dstAddr
    );

typedef
NTSTATUS
(*PREGISTER_NOTIFICATION_CALLBACK)(
    IN PVOID Context,
    IN HANDLE Handle,
    IN PDEVICE_OBJECT Fdo,
    IN PDMA_NOTIFICATION_CALLBACK NotificationCallback,
    IN PVOID CallbackContext
    );

typedef
NTSTATUS
(*PUNREGISTER_NOTIFICATION_CALLBACK)(
    IN PVOID Context,
    IN HANDLE Handle,
    IN PDMA_NOTIFICATION_CALLBACK NotificationCallback,
    IN PVOID CallbackContext
    );

DEFINE_GUID(GUID_PL330DMA_INTERFACE_STANDARD,
    0xdb4b9e2c, 0x7fc6, 0x11ee, 0x95, 0x37, 0x00, 0x15, 0x5d, 0x45, 0x35, 0x74);

typedef struct _PL330DMA_INTERFACE_STANDARD {
    INTERFACE                           InterfaceHeader;
    PGET_HANDLE                         GetChannel;
    PFREE_HANDLE                        FreeChannel;
    PSTOP_DMA                           StopDMA;
    PGET_THREAD_REGISTERS               GetThreadRegisters;
    PSUBMIT_AUDIO_DMA                   SubmitAudioDMA;
    PSUBMIT_DMA                         SubmitDMA; //reserved for future use
    PREGISTER_NOTIFICATION_CALLBACK     RegisterNotificationCallback;
    PUNREGISTER_NOTIFICATION_CALLBACK   UnregisterNotificationCallback;
} PL330DMA_INTERFACE_STANDARD, * PPL330DMA_INTERFACE_STANDARD;
#endif