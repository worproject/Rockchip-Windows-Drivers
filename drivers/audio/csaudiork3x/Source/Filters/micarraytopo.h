
/*++

Copyright (c) Microsoft Corporation All Rights Reserved

Module Name:

    micarraytopo.h

Abstract:

    Declaration of mic array topology miniport.

--*/

#ifndef _CSAUDIORK3X_MICARRAYTOPO_H_
#define _CSAUDIORK3X_MICARRAYTOPO_H_

#include "basetopo.h"

//=============================================================================
// Classes
//=============================================================================

///////////////////////////////////////////////////////////////////////////////
// CMicArrayMiniportTopology 
//   

#pragma code_seg()
class CMicArrayMiniportTopology :
    public CMiniportTopologyCsAudioRk3x,
    public IMiniportTopology,
    public CUnknown
{
public:
    DECLARE_STD_UNKNOWN();
    CMicArrayMiniportTopology
    (
        _In_opt_    PUNKNOWN                UnknownOuter,
        _In_        PCFILTER_DESCRIPTOR* FilterDesc,
        _In_        USHORT                  DeviceMaxChannels,
        _In_        eDeviceType             DeviceType
    )
        : CUnknown(UnknownOuter),
        CMiniportTopologyCsAudioRk3x(FilterDesc, DeviceMaxChannels),
        m_DeviceType(DeviceType)
    {
        ASSERT(m_DeviceType == eInputDevice);
    }

    ~CMicArrayMiniportTopology();

    IMP_IMiniportTopology;

    NTSTATUS            PropertyHandlerMicArrayGeometry
    (
        _In_ PPCPROPERTY_REQUEST  PropertyRequest
    );

    NTSTATUS            PropertyHandlerMicProperties
    (
        _In_ PPCPROPERTY_REQUEST  PropertyRequest
    );

    NTSTATUS            PropertyHandlerJackDescription
    (
        _In_ PPCPROPERTY_REQUEST  PropertyRequest
    );

    NTSTATUS            PropertyHandlerJackDescription2
    (
        _In_ PPCPROPERTY_REQUEST  PropertyRequest
    );

protected:
    eDeviceType     m_DeviceType;

};

typedef CMicArrayMiniportTopology* PCMicArrayMiniportTopology;


NTSTATUS
CreateMicArrayMiniportTopology(
    _Out_           PUNKNOWN* Unknown,
    _In_            REFCLSID,
    _In_opt_        PUNKNOWN                                UnknownOuter,
    _In_            POOL_TYPE                               PoolType,
    _In_            PUNKNOWN                                UnknownAdapter,
    _In_opt_        PVOID                                   DeviceContext,
    _In_            PENDPOINT_MINIPAIR                      MiniportPair
);

NTSTATUS PropertyHandler_MicArrayTopoFilter(_In_ PPCPROPERTY_REQUEST PropertyRequest);
NTSTATUS PropertyHandler_MicArrayTopology(_In_ PPCPROPERTY_REQUEST PropertyRequest);

#endif // _CSAUDIORK3X_MICARRAYTOPO_H_
