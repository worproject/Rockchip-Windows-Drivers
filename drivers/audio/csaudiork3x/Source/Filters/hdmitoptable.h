/*++

Copyright (c) Microsoft Corporation All Rights Reserved

Module Name:

    speakertoptable.h

Abstract:

    Declaration of topology tables.
--*/

#ifndef _CSAUDIORK3X_HDMITOPTABLE_H_
#define _CSAUDIORK3X_HDMITOPTABLE_H_

//=============================================================================
static
KSDATARANGE HdmiTopoPinDataRangesBridge[] =
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
PKSDATARANGE HdmiTopoPinDataRangePointersBridge[] =
{
  &HdmiTopoPinDataRangesBridge[0]
};

//=============================================================================
static
PCPIN_DESCRIPTOR HdmiTopoMiniportPins[] =
{
    // KSPIN_TOPO_WAVEOUT_SOURCE
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
        SIZEOF_ARRAY(HdmiTopoPinDataRangePointersBridge),// DataRangesCount
        HdmiTopoPinDataRangePointersBridge,            // DataRanges
        KSPIN_DATAFLOW_IN,                                // DataFlow
        KSPIN_COMMUNICATION_NONE,                         // Communication
        &KSCATEGORY_AUDIO,                                // Category
        NULL,                                             // Name
        0                                                 // Reserved
      }
    },
    // KSPIN_TOPO_LINEOUT_DEST
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
        SIZEOF_ARRAY(HdmiTopoPinDataRangePointersBridge),// DataRangesCount
        HdmiTopoPinDataRangePointersBridge,            // DataRanges
        KSPIN_DATAFLOW_OUT,                               // DataFlow
        KSPIN_COMMUNICATION_NONE,                         // Communication
        &KSNODETYPE_HDMI_INTERFACE,                       // Category
        NULL,                                             // Name
        0                                                 // Reserved
      }
    }
};

//=============================================================================
static
KSJACK_DESCRIPTION HdmiJackDescBridge =
{
    KSAUDIO_SPEAKER_STEREO,
    0x0000,               // Color spec for green
    eConnTypeOtherDigital,
    eGeoLocHDMI,
    eGenLocPrimaryBox,
    ePortConnJack,
    TRUE
};

// Only return a KSJACK_DESCRIPTION for the physical bridge pin.
static
PKSJACK_DESCRIPTION HdmiJackDescriptions[] =
{
    NULL,
    &HdmiJackDescBridge
};

static
PCCONNECTION_DESCRIPTOR HdmiTopoMiniportConnections[] =
{
    {PCFILTER_NODE,            KSPIN_TOPO_WAVEOUT_SOURCE,    PCFILTER_NODE,     KSPIN_TOPO_LINEOUT_DEST} //no volume controls
};

//=============================================================================
static
PCPROPERTY_ITEM PropertiesHdmiTopoFilter[] =
{
    {
        &KSPROPSETID_Jack,
        KSPROPERTY_JACK_DESCRIPTION,
        KSPROPERTY_TYPE_GET |
        KSPROPERTY_TYPE_BASICSUPPORT,
        PropertyHandler_HdmiTopoFilter
    },
    {
        &KSPROPSETID_Jack,
        KSPROPERTY_JACK_DESCRIPTION2,
        KSPROPERTY_TYPE_GET |
        KSPROPERTY_TYPE_BASICSUPPORT,
        PropertyHandler_HdmiTopoFilter
    }
};

DEFINE_PCAUTOMATION_TABLE_PROP(AutomationHdmiTopoFilter, PropertiesHdmiTopoFilter);

//=============================================================================
static
PCFILTER_DESCRIPTOR HdmiTopoMiniportFilterDescriptor =
{
  0,                                            // Version
  &AutomationHdmiTopoFilter,                 // AutomationTable
  sizeof(PCPIN_DESCRIPTOR),                     // PinSize
  SIZEOF_ARRAY(HdmiTopoMiniportPins),        // PinCount
  HdmiTopoMiniportPins,                      // Pins
  sizeof(PCNODE_DESCRIPTOR),                    // NodeSize
  0,           // NodeCount
  NULL,                         // Nodes
  SIZEOF_ARRAY(HdmiTopoMiniportConnections), // ConnectionCount
  HdmiTopoMiniportConnections,               // Connections
  0,                                            // CategoryCount
  NULL                                          // Categories
};

#endif // _CSAUDIORK3X_HDMITOPTABLE_H_
