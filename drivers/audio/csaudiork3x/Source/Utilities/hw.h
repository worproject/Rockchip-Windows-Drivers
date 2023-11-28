/*++

Copyright (c) Microsoft Corporation All Rights Reserved

Module Name:

    hw.h

Abstract:

    Declaration of Simple Audio Sample HW class. 
    Simple Audio Sample HW has an array for storing mixer and volume settings
    for the topology.
--*/

#ifndef _CSAUDIORK3X_HW_H_
#define _CSAUDIORK3X_HW_H_

typedef enum {
    CSAudioEndpointTypeDSP,
    CSAudioEndpointTypeSpeaker,
    CSAudioEndpointTypeHeadphone,
    CSAudioEndpointTypeMicArray,
    CSAudioEndpointTypeMicJack
} CSAudioEndpointType;

typedef enum {
    CSAudioEndpointRegister,
    CSAudioEndpointStart,
    CSAudioEndpointStop,
    CSAudioEndpointOverrideFormat
} CSAudioEndpointRequest;

typedef struct CSAUDIOFORMATOVERRIDE {
    UINT16 channels;
    UINT16 frequency;
    UINT16 bitsPerSample;
    UINT16 validBitsPerSample;
    BOOL force32BitOutputContainer;
} CsAudioFormatOverride;

typedef struct CSAUDIOARG {
    UINT32 argSz;
    CSAudioEndpointType endpointType;
    CSAudioEndpointRequest endpointRequest;
    union {
        CsAudioFormatOverride formatOverride;
    };
} CsAudioArg, * PCsAudioArg;

#define USERKHW 1

#if USERKHW
#include "rk3x.h"
#include <pl330dma.h>

union baseaddr {
    PVOID Base;
    UINT8* baseptr;
};

typedef struct _PCI_BAR {
    union baseaddr Base;
    PHYSICAL_ADDRESS PhysAddr;
    ULONG Len;
} PCI_BAR, * PPCI_BAR;

struct rk_stream {
    UINT32 bufferBytes;
    BOOLEAN isActive;

    PHYSICAL_ADDRESS bufferBaseAddress;
    BOOLEAN recording;
    HANDLE dmaThread;
};

#include "adsp.h"

#define JACK_TPLG "i2s-jack"
#define HDMI_TPLG "hdmi"
#define DP_TPLG "dp"

#endif

//=============================================================================
// Defines
//=============================================================================
// BUGBUG we should dynamically allocate this...
#define MAX_TOPOLOGY_NODES      20

//=============================================================================
// Classes
//=============================================================================
///////////////////////////////////////////////////////////////////////////////
// CCsAudioRk3xHW
// This class represents virtual Simple Audio Sample HW. An array representing volume
// registers and mute registers.

class CCsAudioRk3xHW
{
public:
    int CsAudioArg2;
    void CSAudioAPICalled(CsAudioArg arg);

private:
    PCALLBACK_OBJECT CSAudioAPICallback;
    PVOID CSAudioAPICallbackObj;

    NTSTATUS CSAudioAPIInit();
    NTSTATUS CSAudioAPIDeinit();
    CSAudioEndpointType GetCSAudioEndpoint(eDeviceType deviceType);
    eDeviceType GetDeviceType(CSAudioEndpointType endpointType);

protected:
    LONG                        m_PeakMeterControls[MAX_TOPOLOGY_NODES];
    ULONG                       m_ulMux;            // Mux selection
    BOOL                        m_bDevSpecific;
    INT                         m_iDevSpecific;
    UINT                        m_uiDevSpecific;
#if USERKHW
    WDFDEVICE m_wdfDevice;
    RKDSP_BUS_INTERFACE m_rkdspInterface;
    PCI_BAR m_MMIO;
    PL330DMA_INTERFACE_STANDARD m_DMAInterface;

public:
    RK_TPLG rkTPLG;
    BOOLEAN isJack;

protected:
    struct rk_stream outputStream;
    struct rk_stream inputStream;

    UINT32 i2s_read32(UINT32 reg);
    void i2s_write32(UINT32 reg, UINT32 val);
    void i2s_update32(UINT32 reg, UINT32 mask, UINT32 val);

    NTSTATUS connectDMA();
#endif

public:
    CCsAudioRk3xHW(_In_  PRKDSP_BUS_INTERFACE           AdspInterface, _In_ WDFDEVICE wdfDevice);
    ~CCsAudioRk3xHW();

    bool                        ResourcesValidated();

    NTSTATUS rk3x_init();
    NTSTATUS rk3x_deinit();

    BOOLEAN rk3x_irq();

    struct rk_stream* rk_get_stream(eDeviceType deviceType);

    NTSTATUS rk3x_program_dma(eDeviceType deviceType, PMDL mdl, IPortWaveRTStream* stream, UINT32 byteCount);
    NTSTATUS rk3x_play(eDeviceType deviceType);
    NTSTATUS rk3x_stop(eDeviceType deviceType);
    NTSTATUS rk3x_current_position(eDeviceType deviceType, UINT32* linkPos, UINT64* linearPos);
    
    void                        MixerReset();
    BOOL                        bGetDevSpecific();
    void                        bSetDevSpecific
    (
        _In_  BOOL                bDevSpecific
    );
    INT                         iGetDevSpecific();
    void                        iSetDevSpecific
    (
        _In_  INT                 iDevSpecific
    );
    UINT                        uiGetDevSpecific();
    void                        uiSetDevSpecific
    (
        _In_  UINT                uiDevSpecific
    );
    ULONG                       GetMixerMux();
    void                        SetMixerMux
    (
        _In_  ULONG               ulNode
    );
    
    LONG                        GetMixerPeakMeter
    (   
        _In_  ULONG               ulNode,
        _In_  ULONG               ulChannel
    );

protected:
private:
};
typedef CCsAudioRk3xHW    *PCCsAudioRk3xHW;

#endif  // _CSAUDIORK3X_HW_H_
