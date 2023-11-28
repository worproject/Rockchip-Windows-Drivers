/*++

Copyright (c) Microsoft Corporation All Rights Reserved

Module Name:

    usbhsmictopo.cpp

Abstract:

    Implementation of topology miniport for the mic (external: headphone).

--*/
#pragma warning (disable : 4127)

#include "definitions.h"
#include "endpoints.h"
#include "mintopo.h"
#include "micjacktopo.h"
#include "micjacktoptable.h"

//=============================================================================
#pragma code_seg("PAGE")
NTSTATUS
PropertyHandler_MicJackTopoFilter
(
    _In_ PPCPROPERTY_REQUEST      PropertyRequest
)
/*++

Routine Description:

  Redirects property request to miniport object

Arguments:

  PropertyRequest -

Return Value:

  NT status code.

--*/
{
    PAGED_CODE();

    ASSERT(PropertyRequest);

    DPF_ENTER(("[PropertyHandler_MicJackTopoFilter]"));

    // PropertryRequest structure is filled by portcls. 
    // MajorTarget is a pointer to miniport object for miniports.
    //
    NTSTATUS            ntStatus = STATUS_INVALID_DEVICE_REQUEST;
    PCMiniportTopology  pMiniport = (PCMiniportTopology)PropertyRequest->MajorTarget;

    if (IsEqualGUIDAligned(*PropertyRequest->PropertyItem->Set, KSPROPSETID_Jack))
    {
        switch (PropertyRequest->PropertyItem->Id)
        {
        case KSPROPERTY_JACK_DESCRIPTION:
            ntStatus = pMiniport->PropertyHandlerJackDescription(
                PropertyRequest,
                ARRAYSIZE(MicJackDescriptions),
                MicJackDescriptions);
            break;

        case KSPROPERTY_JACK_DESCRIPTION2:
            ntStatus = pMiniport->PropertyHandlerJackDescription2(
                PropertyRequest,
                ARRAYSIZE(MicJackDescriptions),
                MicJackDescriptions,
                0 // jack capabilities
            );
            break;
        }
    }

    return ntStatus;
} // PropertyHandler_MicJackTopoFilter

//=============================================================================
NTSTATUS
PropertyHandler_MicJackTopology
(
    _In_ PPCPROPERTY_REQUEST      PropertyRequest
)
/*++

Routine Description:

  Redirects property request to miniport object

Arguments:

  PropertyRequest -

Return Value:

  NT status code.

--*/
{
    PAGED_CODE();

    ASSERT(PropertyRequest);

    DPF_ENTER(("[PropertyHandler_MicJackTopology]"));

    // PropertryRequest structure is filled by portcls. 
    // MajorTarget is a pointer to miniport object for miniports.
    //
    PCMiniportTopology pMiniport = (PCMiniportTopology)PropertyRequest->MajorTarget;

    return pMiniport->PropertyHandlerGeneric(PropertyRequest);
} // PropertyHandler_HeadphoneTopology

#pragma code_seg()
