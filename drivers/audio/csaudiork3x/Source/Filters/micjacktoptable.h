/*++

Copyright (c) Microsoft Corporation All Rights Reserved

Module Name:

    usbhsmictopotable.h

Abstract:

    Declaration of topology table for the USB Headset mic (external)

--*/

#ifndef _CSAUDIORK3X_MICJACKTOPTABLE_H_
#define _CSAUDIORK3X_MICJACKTOPTABLE_H_

//=============================================================================
static
KSDATARANGE MicJackTopoPinDataRangesBridge[] =
{
 {
   sizeof(KSDATARANGE),
   0,
   0,
   0,
   STATICGUIDOF(KSDATAFORMAT_TYPE_AUDIO),
   STATICGUIDOF(KSDATAFORMAT_SUBTYPE_ANALOG),
   STATICGUIDOF(KSDATAFORMAT_SPECIFIER_NONE)
 }
};

//=============================================================================
static
PKSDATARANGE MicJackTopoPinDataRangePointersBridge[] =
{
  &MicJackTopoPinDataRangesBridge[0]
};

//=============================================================================
static
PCPIN_DESCRIPTOR MicJackTopoMiniportPins[] =
{
    // KSPIN_TOPO_MIC_ELEMENTS
    {
      0,
      0,
      0,                                                  // InstanceCount
      NULL,                                               // AutomationTable
      {                                                   // KsPinDescriptor
        0,                                                // InterfacesCount
        NULL,                                             // Interfaces
        0,                                                // MediumsCount
        NULL,                                             // Mediums
        SIZEOF_ARRAY(MicJackTopoPinDataRangePointersBridge),// DataRangesCount
        MicJackTopoPinDataRangePointersBridge,              // DataRanges
        KSPIN_DATAFLOW_IN,                                // DataFlow
        KSPIN_COMMUNICATION_NONE,                         // Communication
        &KSNODETYPE_MICROPHONE,                   // Category
        NULL,                                             // Name
        0                                                 // Reserved
      }
    },

    // KSPIN_TOPO_BRIDGE
    {
      0,
      0,
      0,                                                  // InstanceCount
      NULL,                                               // AutomationTable
      {                                                   // KsPinDescriptor
        0,                                                // InterfacesCount
        NULL,                                             // Interfaces
        0,                                                // MediumsCount
        NULL,                                             // Mediums
        SIZEOF_ARRAY(MicJackTopoPinDataRangePointersBridge),// DataRangesCount
        MicJackTopoPinDataRangePointersBridge,              // DataRanges
        KSPIN_DATAFLOW_OUT,                               // DataFlow
        KSPIN_COMMUNICATION_NONE,                         // Communication
        &KSCATEGORY_AUDIO,                                // Category
        NULL,                                             // Name
        0                                                 // Reserved
      }
    }
};

//=============================================================================
static
KSJACK_DESCRIPTION MicJackDesc =
{
    KSAUDIO_SPEAKER_MONO,
    JACKDESC_RGB(0, 0, 0),
    eConnType3Point5mm,
    eGeoLocRight,
    eGenLocPrimaryBox,
    ePortConnJack,
    TRUE               // NOTE: For convenience, wired headset jacks will be "unplugged" at boot.
};

//=============================================================================
// Only return a KSJACK_DESCRIPTION for the physical bridge pin.
static
PKSJACK_DESCRIPTION MicJackDescriptions[] =
{
    &MicJackDesc,
    NULL
};

//=============================================================================
static
PCCONNECTION_DESCRIPTOR MicJackMiniportConnections[] =
{
    //  FromNode,                 FromPin,                    ToNode,                 ToPin
    {   PCFILTER_NODE,            KSPIN_TOPO_MIC_ELEMENTS,    PCFILTER_NODE,     KSPIN_TOPO_BRIDGE }
};


//=============================================================================
static
PCPROPERTY_ITEM MicJackPropertiesTopoFilter[] =
{
    {
        &KSPROPSETID_Jack,
        KSPROPERTY_JACK_DESCRIPTION,
        KSPROPERTY_TYPE_GET | KSPROPERTY_TYPE_BASICSUPPORT,
        PropertyHandler_MicJackTopoFilter
    },
    {
        &KSPROPSETID_Jack,
        KSPROPERTY_JACK_DESCRIPTION2,
        KSPROPERTY_TYPE_GET | KSPROPERTY_TYPE_BASICSUPPORT,
        PropertyHandler_MicJackTopoFilter
    }
};


DEFINE_PCAUTOMATION_TABLE_PROP(AutomationMicJackTopoFilter,
    MicJackPropertiesTopoFilter);


//=============================================================================
static
PCFILTER_DESCRIPTOR MicJackTopoMiniportFilterDescriptor =
{
  0,                                        // Version
  &AutomationMicJackTopoFilter,  // AutomationTable
  sizeof(PCPIN_DESCRIPTOR),                 // PinSize
  SIZEOF_ARRAY(MicJackTopoMiniportPins),  // PinCount
  MicJackTopoMiniportPins,                // Pins
  sizeof(PCNODE_DESCRIPTOR),                // NodeSize
  0,     // NodeCount
  NULL,                   // Nodes
  SIZEOF_ARRAY(MicJackMiniportConnections),// ConnectionCount
  MicJackMiniportConnections,             // Connections
  0,                                        // CategoryCount
  NULL                                      // Categories
};

#endif // _CSAUDIORK3X_MICJACKTOPTABLE_H_
