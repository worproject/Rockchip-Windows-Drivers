/*++

Copyright (c) Microsoft Corporation All Rights Reserved

Module Name:

    speakertoptable.h

Abstract:

    Declaration of topology tables.
--*/

#ifndef _CSAUDIORK3X_HEADPHONETOPTABLE_H_
#define _CSAUDIORK3X_HEADPHONETOPTABLE_H_

//=============================================================================
static
KSDATARANGE HeadphoneTopoPinDataRangesBridge[] =
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
PKSDATARANGE HeadphoneTopoPinDataRangePointersBridge[] =
{
  &HeadphoneTopoPinDataRangesBridge[0]
};

//=============================================================================
static
PCPIN_DESCRIPTOR HeadphoneTopoMiniportPins[] =
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
      SIZEOF_ARRAY(HeadphoneTopoPinDataRangePointersBridge),// DataRangesCount
      HeadphoneTopoPinDataRangePointersBridge,            // DataRanges
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
      SIZEOF_ARRAY(HeadphoneTopoPinDataRangePointersBridge),// DataRangesCount
      HeadphoneTopoPinDataRangePointersBridge,            // DataRanges
      KSPIN_DATAFLOW_OUT,                               // DataFlow
      KSPIN_COMMUNICATION_NONE,                         // Communication
      &KSNODETYPE_HEADPHONES,                              // Category
      NULL,                                             // Name
      0                                                 // Reserved
    }
  }
};

//=============================================================================
static
KSJACK_DESCRIPTION HeadphoneJackDescBridge =
{
    KSAUDIO_SPEAKER_STEREO,
    0xB3C98C,               // Color spec for green
    eConnType3Point5mm,
    eGeoLocRight,
    eGenLocPrimaryBox,
    ePortConnJack,
    TRUE
};

// Only return a KSJACK_DESCRIPTION for the physical bridge pin.
static 
PKSJACK_DESCRIPTION HeadphoneJackDescriptions[] =
{
    NULL,
    &HeadphoneJackDescBridge
};

static
PCCONNECTION_DESCRIPTOR HeadphoneTopoMiniportConnections[] =
{
    {PCFILTER_NODE,            KSPIN_TOPO_WAVEOUT_SOURCE,    PCFILTER_NODE,     KSPIN_TOPO_LINEOUT_DEST} //no volume controls
};

//=============================================================================
static
PCPROPERTY_ITEM PropertiesHeadphoneTopoFilter[] =
{
    {
        &KSPROPSETID_Jack,
        KSPROPERTY_JACK_DESCRIPTION,
        KSPROPERTY_TYPE_GET |
        KSPROPERTY_TYPE_BASICSUPPORT,
        PropertyHandler_HeadphoneTopoFilter
    },
    {
        &KSPROPSETID_Jack,
        KSPROPERTY_JACK_DESCRIPTION2,
        KSPROPERTY_TYPE_GET |
        KSPROPERTY_TYPE_BASICSUPPORT,
        PropertyHandler_HeadphoneTopoFilter
    }
};

DEFINE_PCAUTOMATION_TABLE_PROP(AutomationHeadphoneTopoFilter, PropertiesHeadphoneTopoFilter);

//=============================================================================
static
PCFILTER_DESCRIPTOR HeadphoneTopoMiniportFilterDescriptor =
{
  0,                                            // Version
  &AutomationHeadphoneTopoFilter,                 // AutomationTable
  sizeof(PCPIN_DESCRIPTOR),                     // PinSize
  SIZEOF_ARRAY(HeadphoneTopoMiniportPins),        // PinCount
  HeadphoneTopoMiniportPins,                      // Pins
  sizeof(PCNODE_DESCRIPTOR),                    // NodeSize
  0,           // NodeCount
  NULL,                         // Nodes
  SIZEOF_ARRAY(HeadphoneTopoMiniportConnections), // ConnectionCount
  HeadphoneTopoMiniportConnections,               // Connections
  0,                                            // CategoryCount
  NULL                                          // Categories
};

#endif // _CSAUDIORK3X_HEADPHONETOPTABLE_H_
