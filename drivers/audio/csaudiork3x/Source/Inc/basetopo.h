
/*++

Copyright (c) Microsoft Corporation All Rights Reserved

Module Name:

    basetopo.h

Abstract:

    Declaration of topology miniport.
--*/

#ifndef _CSAUDIORK3X_BASETOPO_H_
#define _CSAUDIORK3X_BASETOPO_H_

//=============================================================================
// Classes
//=============================================================================

///////////////////////////////////////////////////////////////////////////////
// CMiniportTopologyCsAudioRk3x
//

class CMiniportTopologyCsAudioRk3x
{
  protected:
    PADAPTERCOMMON              m_AdapterCommon;        // Adapter common object.
    PPCFILTER_DESCRIPTOR        m_FilterDescriptor;     // Filter descriptor.
    PPORTEVENTS                 m_PortEvents;           // Event interface.
    USHORT                      m_DeviceMaxChannels;    // Max device channels.

  public:
    CMiniportTopologyCsAudioRk3x(
        _In_        PCFILTER_DESCRIPTOR    *FilterDesc,
        _In_        USHORT                  DeviceMaxChannels
        );
    
    ~CMiniportTopologyCsAudioRk3x();

    NTSTATUS                    GetDescription
    (   
        _Out_ PPCFILTER_DESCRIPTOR *  Description
    );

    NTSTATUS                    DataRangeIntersection
    (   
        _In_  ULONG             PinId,
        _In_  PKSDATARANGE      ClientDataRange,
        _In_  PKSDATARANGE      MyDataRange,
        _In_  ULONG             OutputBufferLength,
        _Out_writes_bytes_to_opt_(OutputBufferLength, *ResultantFormatLength)
              PVOID             ResultantFormat OPTIONAL,
        _Out_ PULONG            ResultantFormatLength
    );

    NTSTATUS                    Init
    ( 
        _In_  PUNKNOWN          UnknownAdapter,
        _In_  PPORTTOPOLOGY     Port_ 
    );

    // PropertyHandlers.
    NTSTATUS                    PropertyHandlerGeneric
    (
        _In_  PPCPROPERTY_REQUEST PropertyRequest
    );

    NTSTATUS                    PropertyHandlerMuxSource
    (
        _In_  PPCPROPERTY_REQUEST PropertyRequest
    );

    NTSTATUS                    PropertyHandlerDevSpecific
    (
        _In_  PPCPROPERTY_REQUEST PropertyRequest
    );

    VOID                        AddEventToEventList
    (
        _In_  PKSEVENT_ENTRY    EventEntry 
    );
    
    VOID                        GenerateEventList
    (
        _In_opt_    GUID       *Set,
        _In_        ULONG       EventId,
        _In_        BOOL        PinEvent,
        _In_        ULONG       PinId,
        _In_        BOOL        NodeEvent,
        _In_        ULONG       NodeId
    );
};

#endif
