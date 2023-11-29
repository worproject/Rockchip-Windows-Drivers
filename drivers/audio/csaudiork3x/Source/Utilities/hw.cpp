/*++

Copyright (c)  Microsoft Corporation All Rights Reserved

Module Name:

    hw.cpp

Abstract:

    Implementation of Simple Audio Sample HW class. 
    Simple Audio Sample HW has an array for storing mixer and volume settings
    for the topology.
--*/
#include "definitions.h"
#include "hw.h"

BOOL I2SInterrupt(PVOID pHW) {
    CCsAudioRk3xHW* hw = (CCsAudioRk3xHW*)pHW;

    return hw->rk3x_irq();
}

//=============================================================================
// CCsAudioRk3xHW
//=============================================================================

//=============================================================================
#pragma code_seg("PAGE")
CCsAudioRk3xHW::CCsAudioRk3xHW(
    _In_ PRKDSP_BUS_INTERFACE     RKdspInterface,
    _In_ WDFDEVICE                wdfDevice
)
: m_ulMux(0),
    m_bDevSpecific(FALSE),
    m_iDevSpecific(0),
    m_uiDevSpecific(0)
/*++

Routine Description:

    Constructor for CsAudioRk3xHW. 

Arguments:

Return Value:

    void

--*/
{
    PAGED_CODE();

#if USERKHW
    TPLG_INFO tplgInfo = { 0 };

    RKdspInterface->GetResources(RKdspInterface->Context, &m_MMIO, &tplgInfo);

    RtlZeroMemory(&this->outputStream, sizeof(this->outputStream));
    RtlZeroMemory(&this->inputStream, sizeof(this->inputStream));

    m_rkdspInterface = *RKdspInterface;
    m_wdfDevice = wdfDevice;

    this->m_rkdspInterface.RegisterInterrupt(this->m_rkdspInterface.Context, &I2SInterrupt, NULL, this);

    if (tplgInfo.rkTplg && tplgInfo.rkTplgSz > offsetof(RK_TPLG, length)) {
        PRK_TPLG rawSofTplg = (PRK_TPLG)tplgInfo.rkTplg;
        if (rawSofTplg->magic == RKTPLG_MAGIC) {
            RtlCopyMemory(&this->rkTPLG, tplgInfo.rkTplg, min(tplgInfo.rkTplgSz, sizeof(RK_TPLG)));
        }
        this->rkTPLG.magic = RKTPLG_MAGIC;
        this->rkTPLG.length = sizeof(RK_TPLG);
    }
#endif
    
    MixerReset();
} // CsAudioRk3xHW
#pragma code_seg()

bool CCsAudioRk3xHW::ResourcesValidated() {
    if (!m_MMIO.Base.Base)
        return false;

    if (!NT_SUCCESS(connectDMA()))
        return false;

    this->isJack = (strcmp(this->rkTPLG.audio_tplg, JACK_TPLG) == 0);

    if (this->isJack) {
        if (!NT_SUCCESS(this->CSAudioAPIInit()))
            return false;
    }

    return true;
}

CCsAudioRk3xHW::~CCsAudioRk3xHW() {
    this->m_rkdspInterface.UnregisterInterrupt(this->m_rkdspInterface.Context);

    this->CSAudioAPIDeinit();
}

#if USERKHW
static UINT32 read32(PVOID addr) {
    UINT32 ret = READ_REGISTER_NOFENCE_ULONG((ULONG *)addr);
    //DbgPrint("Read from %p: 0x%x\n", addr, ret);
    return ret;
}

static void write32(PVOID addr, UINT32 data) {
    WRITE_REGISTER_NOFENCE_ULONG((ULONG *)addr, data);
    //DbgPrint("Write to %p: 0x%x\n", addr, data);
}

UINT32 CCsAudioRk3xHW::i2s_read32(UINT32 reg)
{
    return read32(m_MMIO.Base.baseptr + reg);
}

void CCsAudioRk3xHW::i2s_write32(UINT32 reg, UINT32 val)
{
    write32(m_MMIO.Base.baseptr + reg, val);
}

void CCsAudioRk3xHW::i2s_update32(UINT32 reg, UINT32 mask, UINT32 val)
{
    UINT32 tmp = i2s_read32(reg);
    tmp &= ~mask;
    tmp |= val;
    i2s_write32(reg, tmp);
}
#endif

struct rk_stream* CCsAudioRk3xHW::rk_get_stream(eDeviceType deviceType) {
    switch (deviceType) {
    case eOutputDevice:
        return &this->outputStream;
    case eInputDevice:
        return &this->inputStream;
    default:
        return NULL;
    }
}

NTSTATUS CCsAudioRk3xHW::rk3x_init() {
#if USERKHW

    this->m_rkdspInterface.SetDSPPowerState(this->m_rkdspInterface.Context, PowerDeviceD0);
    
    //write defaults
    i2s_write32(I2S_TXCR, 0x7200000f);
    i2s_write32(I2S_RXCR, 0x01c8000f);
    i2s_write32(I2S_CKR, 0x00001f1f);
    i2s_write32(I2S_DMACR, 0x001f0000);
    i2s_write32(I2S_INTCR, 0x01f00000);
    i2s_write32(I2S_TDM_TXCR, 0x00003eff);
    i2s_write32(I2S_TDM_RXCR, 0x00003eff);
    i2s_write32(I2S_CLKDIV, 0x00000707);

    UINT32 clk_trcm = 0 << I2S_CKR_TRCM_SHIFT;
    i2s_update32(I2S_DMACR, I2S_DMACR_TDL_MASK, I2S_DMACR_TDL(16));
    i2s_update32(I2S_DMACR, I2S_DMACR_RDL_MASK, I2S_DMACR_RDL(16));
    i2s_update32(I2S_CKR, I2S_CKR_TRCM_MASK, clk_trcm);

    //Set fmt
    i2s_update32(I2S_CKR, I2S_CKR_MSS_MASK, I2S_CKR_MSS_MASTER); //cbs cfs
    i2s_update32(I2S_CKR,
        I2S_CKR_CKP_MASK | I2S_CKR_TLP_MASK | I2S_CKR_RLP_MASK,
        I2S_CKR_CKP_NORMAL | I2S_CKR_TLP_NORMAL | I2S_CKR_RLP_NORMAL); //nb nf
    i2s_update32(I2S_TXCR,
        I2S_TXCR_IBM_MASK | I2S_TXCR_TFS_MASK | I2S_TXCR_PBM_MASK,
        I2S_TXCR_IBM_NORMAL); //daifmt i2s
    i2s_update32(I2S_RXCR,
        I2S_RXCR_IBM_MASK | I2S_RXCR_TFS_MASK | I2S_RXCR_PBM_MASK,
        I2S_RXCR_IBM_NORMAL); //daifmt i2s

    //Set bclk (bclk = 4, lrclk = 64, fmt = 0xf)
    i2s_update32(I2S_CLKDIV,
        I2S_CLKDIV_TXM_MASK | I2S_CLKDIV_RXM_MASK,
        I2S_CLKDIV_TXM(4) | I2S_CLKDIV_RXM(4));
    i2s_update32(I2S_CKR,
        I2S_CKR_TSD_MASK | I2S_CKR_RSD_MASK,
        I2S_CKR_TSD(64) | I2S_CKR_RSD(64));

    i2s_update32(I2S_TXCR,
        I2S_TXCR_VDW_MASK | I2S_TXCR_CSR_MASK,
        I2S_TXCR_VDW(16));
    i2s_update32(I2S_RXCR,
        I2S_RXCR_VDW_MASK | I2S_RXCR_CSR_MASK,
        I2S_RXCR_VDW(16));

    //set path
    i2s_update32(I2S_TXCR,
        I2S_TXCR_PATH_MASK(0),
        I2S_TXCR_PATH(0, 3));
    i2s_update32(I2S_TXCR,
        I2S_TXCR_PATH_MASK(1),
        I2S_TXCR_PATH(1, 2));
    i2s_update32(I2S_TXCR,
        I2S_TXCR_PATH_MASK(2),
        I2S_TXCR_PATH(2, 1));
    i2s_update32(I2S_TXCR,
        I2S_TXCR_PATH_MASK(3),
        I2S_TXCR_PATH(3, 0));

    i2s_update32(I2S_RXCR,
        I2S_RXCR_PATH_MASK(0),
        I2S_RXCR_PATH(0, 1));
    i2s_update32(I2S_RXCR,
        I2S_RXCR_PATH_MASK(1),
        I2S_RXCR_PATH(1, 3));
    i2s_update32(I2S_RXCR,
        I2S_RXCR_PATH_MASK(2),
        I2S_RXCR_PATH(2, 2));
    i2s_update32(I2S_RXCR,
        I2S_RXCR_PATH_MASK(3),
        I2S_RXCR_PATH(3, 0));

    return STATUS_SUCCESS;
#else
    return STATUS_SUCCESS;
#endif
}

NTSTATUS CCsAudioRk3xHW::rk3x_deinit() {
#if USERKHW
    this->m_rkdspInterface.SetDSPPowerState(this->m_rkdspInterface.Context, PowerDeviceD3);

    return STATUS_SUCCESS;
#else
    return STATUS_SUCCESS;
#endif
}

BOOLEAN CCsAudioRk3xHW::rk3x_irq() {
    UINT32 val = i2s_read32(I2S_INTSR);

    if (val & I2S_INTSR_TXUI_ACT) {
        DbgPrint("TX FIFO Underrun\n");
        i2s_update32(I2S_INTCR,
            I2S_INTCR_TXUIC, I2S_INTCR_TXUIC);
        i2s_update32(I2S_INTCR,
            I2S_INTCR_TXUIE_MASK,
            I2S_INTCR_TXUIE(0));
    }

    if (val & I2S_INTSR_RXOI_ACT) {
        DbgPrint("RX FIFO Overrun\n");
        i2s_update32(I2S_INTCR,
            I2S_INTCR_RXOIC, I2S_INTCR_RXOIC);
        i2s_update32(I2S_INTCR,
            I2S_INTCR_RXOIE_MASK,
            I2S_INTCR_RXOIE(0));
    }

    return TRUE;
}

NTSTATUS CCsAudioRk3xHW::rk3x_program_dma(eDeviceType deviceType, PMDL mdl, IPortWaveRTStream *stream, UINT32 byteCount) {
#if USERKHW
    struct rk_stream *i2sStream = rk_get_stream(deviceType);
    if (!i2sStream) {
        return STATUS_INVALID_PARAMETER;
    }

    if (i2sStream->dmaThread) {
        return STATUS_SUCCESS; //already allocated. Return
    }

    int pageCount = stream->GetPhysicalPagesCount(mdl);
    if (pageCount < 1) {
        return STATUS_NO_MEMORY;
    }

    int dmaIndex = 0;
    UINT32 deviceReg = 0;
    switch (deviceType) {
    case eOutputDevice:
        dmaIndex = this->rkTPLG.tx;
        i2sStream->recording = FALSE;
        deviceReg = I2S_TXDR;
        break;
    case eInputDevice:
        dmaIndex = this->rkTPLG.rx;
        i2sStream->recording = TRUE;
        deviceReg = I2S_RXDR;
        break;
    default:
        return STATUS_INVALID_PARAMETER;
    }

    PVOID dmaContext = this->m_DMAInterface.InterfaceHeader.Context;

    i2sStream->bufferBaseAddress = stream->GetPhysicalPageAddress(mdl, 0);
    i2sStream->bufferBytes = byteCount;
    i2sStream->dmaThread = this->m_DMAInterface.GetChannel(dmaContext, dmaIndex);

    if (!i2sStream->dmaThread) {
        return STATUS_UNSUCCESSFUL;
    }

    UINT32 srcAddr;
    UINT32 dstAddr;
    if (i2sStream->recording) {
        srcAddr = this->m_MMIO.PhysAddr.LowPart + deviceReg;
        dstAddr = i2sStream->bufferBaseAddress.LowPart;
    } else {
        srcAddr = i2sStream->bufferBaseAddress.LowPart;
        dstAddr = this->m_MMIO.PhysAddr.LowPart + deviceReg;
    }

    if (!i2sStream->recording) {
        return this->m_DMAInterface.SubmitAudioDMA(
            dmaContext,
            i2sStream->dmaThread,
            i2sStream->recording,
            i2sStream->bufferBaseAddress.LowPart,
            this->m_MMIO.PhysAddr.LowPart + deviceReg,
            byteCount,
            byteCount / 2
        );
    }
    else {
        return this->m_DMAInterface.SubmitAudioDMA(
            dmaContext,
            i2sStream->dmaThread,
            i2sStream->recording,
            this->m_MMIO.PhysAddr.LowPart + deviceReg,
            i2sStream->bufferBaseAddress.LowPart,
            byteCount,
            byteCount / 2
        );
    }
#else
    return STATUS_SUCCESS;
#endif
}

NTSTATUS CCsAudioRk3xHW::rk3x_play(eDeviceType deviceType) {
#if USERKHW
    struct rk_stream* i2sStream = rk_get_stream(deviceType);
    if (!i2sStream) {
        return STATUS_INVALID_PARAMETER;
    }

    if (!i2sStream->dmaThread) {
        return STATUS_INVALID_DEVICE_STATE;
    }

    if (i2sStream->isActive) {
        return STATUS_SUCCESS;
    }

    switch (deviceType) {
    case eOutputDevice:
        i2s_update32(I2S_DMACR, I2S_DMACR_TDE_MASK, I2S_DMACR_TDE(1));
        i2s_update32(I2S_XFER, I2S_XFER_TXS_MASK, I2S_XFER_TXS_START);
        break;
    case eInputDevice:
        i2s_update32(I2S_DMACR, I2S_DMACR_RDE_MASK, I2S_DMACR_RDE(1));
        i2s_update32(I2S_XFER, I2S_XFER_RXS_MASK, I2S_XFER_RXS_START);
        break;
    default:
        return STATUS_INVALID_PARAMETER;
    }
#endif

    i2sStream->isActive = TRUE;

    if (this->isJack) {
        CsAudioArg arg;
        RtlZeroMemory(&arg, sizeof(CsAudioArg));
        arg.argSz = sizeof(CsAudioArg);
        arg.endpointType = GetCSAudioEndpoint(deviceType);
        arg.endpointRequest = CSAudioEndpointStart;
        ExNotifyCallback(this->CSAudioAPICallback, &arg, &CsAudioArg2);
    }
    return STATUS_SUCCESS;
}

NTSTATUS CCsAudioRk3xHW::rk3x_stop(eDeviceType deviceType) {
#if USERKHW
    struct rk_stream* i2sStream = rk_get_stream(deviceType);
    if (!i2sStream) {
        return STATUS_INVALID_PARAMETER;
    }

    if (!i2sStream->isActive) {
        return STATUS_SUCCESS;
    }

    if (!i2sStream->dmaThread) {
        return STATUS_INVALID_DEVICE_STATE;
    }

    if (this->isJack) {
        CsAudioArg arg;
        RtlZeroMemory(&arg, sizeof(CsAudioArg));
        arg.argSz = sizeof(CsAudioArg);
        arg.endpointType = GetCSAudioEndpoint(deviceType);
        arg.endpointRequest = CSAudioEndpointStop;
        ExNotifyCallback(this->CSAudioAPICallback, &arg, &CsAudioArg2);
    }

    switch (deviceType) {
    case eOutputDevice:
        i2s_update32(I2S_DMACR, I2S_DMACR_TDE_MASK, I2S_DMACR_TDE(0));
        i2s_update32(I2S_XFER, I2S_XFER_TXS_MASK, I2S_XFER_TXS_STOP);
        break;
    case eInputDevice:
        i2s_update32(I2S_DMACR, I2S_DMACR_RDE_MASK, I2S_DMACR_RDE(0));
        i2s_update32(I2S_XFER, I2S_XFER_RXS_MASK, I2S_XFER_RXS_STOP);
        break;
    default:
        return STATUS_INVALID_PARAMETER;
    }

    PVOID dmaContext = m_DMAInterface.InterfaceHeader.Context;
    HANDLE dmaThread = i2sStream->dmaThread;

    this->m_DMAInterface.StopDMA(dmaContext, dmaThread);
    this->m_DMAInterface.FreeChannel(dmaContext, dmaThread);

    UINT32 clr = 0;

    switch (deviceType) {
    case eOutputDevice:
        clr = I2S_CLR_TXC;
        break;
    case eInputDevice:
        clr = I2S_CLR_RXC;
        break;
    default:
        return STATUS_INVALID_PARAMETER;
    }

    i2s_update32(I2S_CLR, clr, clr);

    for (int i = 0; i < 10; i++) {
        UINT32 reg = i2s_read32(I2S_CLR);
        if ((reg & clr) == 0){
            break;
        }
        LARGE_INTEGER Interval;
        Interval.QuadPart = -100;
        KeDelayExecutionThread(KernelMode, FALSE, &Interval);
    }

    i2s_update32(I2S_CLR, clr, 0);

    i2sStream->dmaThread = NULL;
    i2sStream->bufferBytes = 0;
    i2sStream->isActive = FALSE;
#endif
    return STATUS_SUCCESS;
}

NTSTATUS CCsAudioRk3xHW::rk3x_current_position(eDeviceType deviceType, UINT32 *linkPos, UINT64 *linearPos) {
#if USERKHW
    struct rk_stream* i2sStream = rk_get_stream(deviceType);
    if (!i2sStream) {
        return STATUS_INVALID_PARAMETER;
    }

    if (!i2sStream->isActive) {
        return STATUS_SUCCESS;
    }

    if (!i2sStream->dmaThread) {
        return STATUS_INVALID_DEVICE_STATE;
    }

    PVOID dmaContext = this->m_DMAInterface.InterfaceHeader.Context;
    UINT32 sourceAddr;
    UINT32 dstAddr;

    this->m_DMAInterface.GetThreadRegisters(
        dmaContext,
        i2sStream->dmaThread,
        NULL,
        &sourceAddr,
        &dstAddr
    );

    UINT32 positionOffset;
    if (i2sStream->recording) {
        positionOffset = dstAddr - i2sStream->bufferBaseAddress.LowPart;
    }
    else {
        positionOffset = sourceAddr - i2sStream->bufferBaseAddress.LowPart;
    }

    if (linkPos)
        *linkPos = positionOffset;
    if (linearPos)
        *linearPos = positionOffset;
#endif
    return STATUS_SUCCESS;
}

//=============================================================================
BOOL
CCsAudioRk3xHW::bGetDevSpecific()
/*++

Routine Description:

  Gets the HW (!) Device Specific info

Arguments:

  N/A

Return Value:

  True or False (in this example).

--*/
{
    return m_bDevSpecific;
} // bGetDevSpecific

//=============================================================================
void
CCsAudioRk3xHW::bSetDevSpecific
(
    _In_  BOOL                bDevSpecific
)
/*++

Routine Description:

  Sets the HW (!) Device Specific info

Arguments:

  fDevSpecific - true or false for this example.

Return Value:

    void

--*/
{
    m_bDevSpecific = bDevSpecific;
} // bSetDevSpecific

//=============================================================================
INT
CCsAudioRk3xHW::iGetDevSpecific()
/*++

Routine Description:

  Gets the HW (!) Device Specific info

Arguments:

  N/A

Return Value:

  int (in this example).

--*/
{
    return m_iDevSpecific;
} // iGetDevSpecific

//=============================================================================
void
CCsAudioRk3xHW::iSetDevSpecific
(
    _In_  INT                 iDevSpecific
)
/*++

Routine Description:

  Sets the HW (!) Device Specific info

Arguments:

  fDevSpecific - true or false for this example.

Return Value:

    void

--*/
{
    m_iDevSpecific = iDevSpecific;
} // iSetDevSpecific

//=============================================================================
UINT
CCsAudioRk3xHW::uiGetDevSpecific()
/*++

Routine Description:

  Gets the HW (!) Device Specific info

Arguments:

  N/A

Return Value:

  UINT (in this example).

--*/
{
    return m_uiDevSpecific;
} // uiGetDevSpecific

//=============================================================================
void
CCsAudioRk3xHW::uiSetDevSpecific
(
    _In_  UINT                uiDevSpecific
)
/*++

Routine Description:

  Sets the HW (!) Device Specific info

Arguments:

  uiDevSpecific - int for this example.

Return Value:

    void

--*/
{
    m_uiDevSpecific = uiDevSpecific;
} // uiSetDevSpecific

//=============================================================================
ULONG                       
CCsAudioRk3xHW::GetMixerMux()
/*++

Routine Description:

  Return the current mux selection

Arguments:

Return Value:

  ULONG

--*/
{
    return m_ulMux;
} // GetMixerMux

//=============================================================================
LONG
CCsAudioRk3xHW::GetMixerPeakMeter
(   
    _In_  ULONG                   ulNode,
    _In_  ULONG                   ulChannel
)
/*++

Routine Description:

  Gets the HW (!) peak meter for Simple Audio Sample.

Arguments:

  ulNode - topology node id

  ulChannel - which channel are we reading?

Return Value:

  LONG - sample peak meter level

--*/
{
    UNREFERENCED_PARAMETER(ulChannel);

    if (ulNode < MAX_TOPOLOGY_NODES)
    {
        return m_PeakMeterControls[ulNode];
    }

    return 0;
} // GetMixerVolume

//=============================================================================
#pragma code_seg("PAGE")
void 
CCsAudioRk3xHW::MixerReset()
/*++

Routine Description:

  Resets the mixer registers.

Arguments:

Return Value:

    void

--*/
{
    PAGED_CODE();

    for (ULONG i=0; i<MAX_TOPOLOGY_NODES; ++i)
    {
        m_PeakMeterControls[i] = PEAKMETER_SIGNED_MAXIMUM/2;
    }
    
    // BUGBUG change this depending on the topology
    m_ulMux = 2;
} // MixerReset
#pragma code_seg()

//=============================================================================
void                        
CCsAudioRk3xHW::SetMixerMux
(
    _In_  ULONG                   ulNode
)
/*++

Routine Description:

  Sets the HW (!) mux selection

Arguments:

  ulNode - topology node id

Return Value:

    void

--*/
{
    m_ulMux = ulNode;
} // SetMixMux
