/*++

Copyright (c) Microsoft Corporation All Rights Reserved

Module Name:

    minipairs.h

Abstract:

    Local audio endpoint filter definitions. 
--*/

#ifndef _CSAUDIORK3X_MINIPAIRS_H_
#define _CSAUDIORK3X_MINIPAIRS_H_

#include "headphonetopo.h"
#include "hdmitopo.h"
#include "speakertopo.h"
#include "headphonetoptable.h"
#include "hdmitoptable.h"
#include "speakertoptable.h"
#include "speakerwavtable.h"

#include "micjacktopo.h"
#include "micarraytopo.h"
#include "micjacktoptable.h"
#include "micarray1toptable.h"
#include "micarraywavtable.h"


NTSTATUS
CreateMiniportWaveRTCsAudioRk3x
( 
    _Out_       PUNKNOWN *,
    _In_        REFCLSID,
    _In_opt_    PUNKNOWN,
    _In_        POOL_TYPE,
    _In_        PUNKNOWN,
    _In_opt_    PVOID,
    _In_        PENDPOINT_MINIPAIR
);

NTSTATUS
CreateMiniportTopologyCsAudioRk3x
( 
    _Out_       PUNKNOWN *,
    _In_        REFCLSID,
    _In_opt_    PUNKNOWN,
    _In_        POOL_TYPE,
    _In_        PUNKNOWN,
    _In_opt_    PVOID,
    _In_        PENDPOINT_MINIPAIR
);

//
// Render miniports.
//

/*********************************************************************
* Topology/Wave bridge connection for speaker (internal)             *
*                                                                    *
*              +------+                +------+                      *
*              | Wave |                | Topo |                      *
*              |      |                |      |                      *
* System   --->|0    1|--------------->|0    1|---> Line Out         *
*              |      |                |      |                      *
*              +------+                +------+                      *
*********************************************************************/
static
PHYSICALCONNECTIONTABLE SpeakerTopologyPhysicalConnections[] =
{
    {
        KSPIN_TOPO_WAVEOUT_SOURCE,  // TopologyIn
        KSPIN_WAVE_RENDER3_SOURCE,   // WaveOut
        CONNECTIONTYPE_WAVE_OUTPUT
    }
};

static
ENDPOINT_MINIPAIR SpeakerMiniports =
{
    eOutputDevice,
    L"TopologySpeaker",                                     // make sure this or the template name matches with KSNAME_TopologySpeaker in the inf's [Strings] section 
    NULL,                                                   // optional template name
    CreateMiniportTopologyCsAudioRk3x,
    &SpeakerTopoMiniportFilterDescriptor,
    0, NULL,                                                // Interface properties
    L"WaveSpeaker",                                         // make sure this or the template name matches with KSNAME_WaveSpeaker in the inf's [Strings] section
    NULL,                                                   // optional template name
    CreateMiniportWaveRTCsAudioRk3x,
    &SpeakerWaveMiniportFilterDescriptor,
    0,                                                      // Interface properties
    NULL,
    SPEAKER_DEVICE_MAX_CHANNELS,
    SpeakerPinDeviceFormatsAndModes,
    SIZEOF_ARRAY(SpeakerPinDeviceFormatsAndModes),
    SpeakerTopologyPhysicalConnections,
    SIZEOF_ARRAY(SpeakerTopologyPhysicalConnections),
    ENDPOINT_NO_FLAGS,
};

static
ENDPOINT_MINIPAIR HeadphoneMiniports =
{
    eOutputDevice,
    L"TopologyHeadphones",                                     // make sure this or the template name matches with KSNAME_TopologySpeaker in the inf's [Strings] section 
    NULL,                                                   // optional template name
    CreateMiniportTopologyCsAudioRk3x,
    &HeadphoneTopoMiniportFilterDescriptor,
    0, NULL,                                                // Interface properties
    L"WaveHeadphones",                                         // make sure this or the template name matches with KSNAME_WaveSpeaker in the inf's [Strings] section
    NULL,                                                   // optional template name
    CreateMiniportWaveRTCsAudioRk3x,
    &SpeakerWaveMiniportFilterDescriptor,
    0,                                                      // Interface properties
    NULL,
    SPEAKER_DEVICE_MAX_CHANNELS,
    SpeakerPinDeviceFormatsAndModes,
    SIZEOF_ARRAY(SpeakerPinDeviceFormatsAndModes),
    SpeakerTopologyPhysicalConnections,
    SIZEOF_ARRAY(SpeakerTopologyPhysicalConnections),
    ENDPOINT_NO_FLAGS,
};

static
ENDPOINT_MINIPAIR HdmiMiniports =
{
    eOutputDevice,
    L"TopologyHdmi",                                     // make sure this or the template name matches with KSNAME_TopologySpeaker in the inf's [Strings] section 
    NULL,                                                   // optional template name
    CreateMiniportTopologyCsAudioRk3x,
    &HdmiTopoMiniportFilterDescriptor,
    0, NULL,                                                // Interface properties
    L"WaveHdmi",                                         // make sure this or the template name matches with KSNAME_WaveSpeaker in the inf's [Strings] section
    NULL,                                                   // optional template name
    CreateMiniportWaveRTCsAudioRk3x,
    &SpeakerWaveMiniportFilterDescriptor,
    0,                                                      // Interface properties
    NULL,
    SPEAKER_DEVICE_MAX_CHANNELS,
    SpeakerPinDeviceFormatsAndModes,
    SIZEOF_ARRAY(SpeakerPinDeviceFormatsAndModes),
    SpeakerTopologyPhysicalConnections,
    SIZEOF_ARRAY(SpeakerTopologyPhysicalConnections),
    ENDPOINT_NO_FLAGS,
};

//
// Capture miniports.
//

/*********************************************************************
* Topology/Wave bridge connection for mic array  1 (front)           *
*                                                                    *
*              +------+    +------+                                  *
*              | Topo |    | Wave |                                  *
*              |      |    |      |                                  *
*  Mic in  --->|0    1|===>|0    1|---> Capture Host Pin             *
*              |      |    |      |                                  *
*              +------+    +------+                                  *
*********************************************************************/
static
PHYSICALCONNECTIONTABLE MicArray1TopologyPhysicalConnections[] =
{
    {
        KSPIN_TOPO_BRIDGE,          // TopologyOut
        KSPIN_WAVE_BRIDGE,          // WaveIn
        CONNECTIONTYPE_TOPOLOGY_OUTPUT
    }
};

static
ENDPOINT_MINIPAIR MicArray1Miniports =
{
    eInputDevice,
    L"TopologyMicArray1",                   // make sure this or the template name matches with KSNAME_TopologyMicArray1 in the inf's [Strings] section 
    NULL,                                   // optional template name
    CreateMicArrayMiniportTopology,
    &MicArray1TopoMiniportFilterDescriptor,
    0, NULL,                                // Interface properties
    L"WaveMicArray1",                       // make sure this or the tempalte name matches with KSNAME_WaveMicArray1 in the inf's [Strings] section
    NULL,                                   // optional template name
    CreateMiniportWaveRTCsAudioRk3x,
    &MicArrayWaveMiniportFilterDescriptor,
    0,                                      // Interface properties
    NULL,
    MICARRAY_DEVICE_MAX_CHANNELS,
    MicArrayPinDeviceFormatsAndModes,
    SIZEOF_ARRAY(MicArrayPinDeviceFormatsAndModes),
    MicArray1TopologyPhysicalConnections,
    SIZEOF_ARRAY(MicArray1TopologyPhysicalConnections),
    ENDPOINT_NO_FLAGS,
};

static
ENDPOINT_MINIPAIR MicJackMiniports =
{
    eInputDevice,
    L"TopologyMicJack",                   // make sure this or the template name matches with KSNAME_TopologyMicArray1 in the inf's [Strings] section 
    NULL,                                   // optional template name
    CreateMiniportTopologyCsAudioRk3x,
    &MicJackTopoMiniportFilterDescriptor,
    0, NULL,                                // Interface properties
    L"WaveMicJack",                       // make sure this or the tempalte name matches with KSNAME_WaveMicArray1 in the inf's [Strings] section
    NULL,                                   // optional template name
    CreateMiniportWaveRTCsAudioRk3x,
    &MicArrayWaveMiniportFilterDescriptor,
    0,                                      // Interface properties
    NULL,
    MICARRAY_DEVICE_MAX_CHANNELS,
    MicArrayPinDeviceFormatsAndModes,
    SIZEOF_ARRAY(MicArrayPinDeviceFormatsAndModes),
    MicArray1TopologyPhysicalConnections,
    SIZEOF_ARRAY(MicArray1TopologyPhysicalConnections),
    ENDPOINT_NO_FLAGS,
};


//=============================================================================
//
// Render miniport pairs. NOTE: the split of render and capture is arbitrary and
// unnessary, this array could contain capture endpoints.
//
static
PENDPOINT_MINIPAIR  g_RenderEndpoints[] = 
{
    &HeadphoneMiniports
};

static
PENDPOINT_MINIPAIR  g_RenderHDMIEndpoints[] =
{
    &HdmiMiniports
};

#define g_cRenderEndpoints  (SIZEOF_ARRAY(g_RenderEndpoints))

//=============================================================================
//
// Capture miniport pairs. NOTE: the split of render and capture is arbitrary and
// unnessary, this array could contain render endpoints.
//
static
PENDPOINT_MINIPAIR  g_CaptureEndpoints[] =
{
    &MicJackMiniports
};

#define g_cCaptureEndpoints (SIZEOF_ARRAY(g_CaptureEndpoints))

//=============================================================================
//
// Total miniports = # endpoints * 2 (topology + wave).
//
#define g_MaxMiniports  ((g_cRenderEndpoints + g_cCaptureEndpoints) * 2)

#endif // _CSAUDIORK3X_MINIPAIRS_H_
