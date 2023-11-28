/*++

Copyright (c) Microsoft Corporation All Rights Reserved

Module Name:

    minwavertstream.h

Abstract:

    Definition of wavert miniport class.
--*/

#ifndef _CSAUDIOACP3X_MINWAVERTSTREAM_H_
#define _CSAUDIOACP3X_MINWAVERTSTREAM_H_

//
// Structure to store notifications events in a protected list
//
typedef struct _NotificationListEntry
{
    LIST_ENTRY  ListEntry;
    PKEVENT     NotificationEvent;
} NotificationListEntry;

EXT_CALLBACK   TimerNotifyRT;

//=============================================================================
// Referenced Forward
//=============================================================================
class CMiniportWaveRT;
typedef CMiniportWaveRT *PCMiniportWaveRT;

//=============================================================================
// Classes
//=============================================================================
///////////////////////////////////////////////////////////////////////////////
// CMiniportWaveRTStream 
// 
class CMiniportWaveRTStream : 
    public IMiniportWaveRTStream,
    public IDrmAudioStream,
    public CUnknown
{
public:
    PPORTWAVERTSTREAM           m_pPortStream;
    PMDL m_pMDL;

    DECLARE_STD_UNKNOWN();
    DEFINE_STD_CONSTRUCTOR(CMiniportWaveRTStream);
    ~CMiniportWaveRTStream();

    IMP_IMiniportWaveRTStream;
    IMP_IMiniportWaveRT;
    IMP_IDrmAudioStream;

    NTSTATUS                    Init
    ( 
        _In_  PCMiniportWaveRT    Miniport,
        _In_  PPORTWAVERTSTREAM   Stream,
        _In_  ULONG               Channel,
        _In_  BOOLEAN             Capture,
        _In_  PKSDATAFORMAT       DataFormat,
        _In_  GUID                SignalProcessingMode
    );

    // Friends
    friend class                CMiniportWaveRT;
protected:
    CMiniportWaveRT*            m_pMiniport;
    ULONG                       m_ulPin;
    BOOLEAN                     m_bCapture;
    BOOLEAN                     m_bUnregisterStream;
    ULONG                       m_ulDmaBufferSize;
    KSSTATE                     m_KsState;
    ULONG                       m_ulDmaMovementRate;
    PWAVEFORMATEXTENSIBLE       m_pWfExt;
    ULONG                       m_ulContentId;
    KSPIN_LOCK                  m_PositionSpinLock;
    UINT32                      m_lastLinkPos;
    UINT64                      m_lastLinearPos;
    
};
typedef CMiniportWaveRTStream *PCMiniportWaveRTStream;
#endif // _CSAUDIOACP3X_MINWAVERTSTREAM_H_

