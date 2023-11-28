/*++

Copyright (c) Microsoft Corporation All Rights Reserved

Module Name:

    headphonetopo.cpp

Abstract:

    Implementation of topology miniport for the headphone (jack).
--*/

#pragma warning (disable : 4127)

#include "definitions.h"
#include "endpoints.h"
#include "mintopo.h"
#include "headphonetopo.h"
#include "headphonetoptable.h"


#pragma code_seg("PAGE")
//=============================================================================
NTSTATUS
PropertyHandler_HeadphoneTopoFilter
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

    DPF_ENTER(("[PropertyHandler_HeadphoneTopoFilter]"));

    // PropertryRequest structure is filled by portcls. 
    // MajorTarget is a pointer to miniport object for miniports.
    //
    NTSTATUS            ntStatus = STATUS_INVALID_DEVICE_REQUEST;
    PCMiniportTopology  pMiniport = (PCMiniportTopology)PropertyRequest->MajorTarget;

    if (IsEqualGUIDAligned(*PropertyRequest->PropertyItem->Set, KSPROPSETID_Jack))
    {
        if (PropertyRequest->PropertyItem->Id == KSPROPERTY_JACK_DESCRIPTION)
        {
            ntStatus = pMiniport->PropertyHandlerJackDescription(
                PropertyRequest,
                ARRAYSIZE(HeadphoneJackDescriptions),
                HeadphoneJackDescriptions
                );
        }
        else if (PropertyRequest->PropertyItem->Id == KSPROPERTY_JACK_DESCRIPTION2)
        {
            ntStatus = pMiniport->PropertyHandlerJackDescription2(
                PropertyRequest,
                ARRAYSIZE(HeadphoneJackDescriptions),
                HeadphoneJackDescriptions,
                0 // jack capabilities
                );
        }
    }

    return ntStatus;
} // PropertyHandler_HeadphoneTopoFilter

//=============================================================================
NTSTATUS
PropertyHandler_HeadphoneTopology
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

    DPF_ENTER(("[PropertyHandler_HeadphoneTopology]"));

    // PropertryRequest structure is filled by portcls. 
    // MajorTarget is a pointer to miniport object for miniports.
    //
    PCMiniportTopology pMiniport = (PCMiniportTopology)PropertyRequest->MajorTarget;

    return pMiniport->PropertyHandlerGeneric(PropertyRequest);
} // PropertyHandler_HeadphoneTopology

#pragma code_seg()
