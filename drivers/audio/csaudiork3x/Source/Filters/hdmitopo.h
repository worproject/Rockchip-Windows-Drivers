
/*++

Copyright (c) Microsoft Corporation All Rights Reserved

Module Name:

    hdmitopo.h

Abstract:

    Declaration of topology miniport for hdmi.
--*/

#ifndef _CSAUDIORK3X_HDMITOPO_H_
#define _CSAUDIORK3X_HDMITOPO_H_

NTSTATUS PropertyHandler_HdmiTopoFilter(_In_ PPCPROPERTY_REQUEST PropertyRequest);

NTSTATUS PropertyHandler_HdmiTopology(_In_ PPCPROPERTY_REQUEST PropertyRequest);

#endif // _CSAUDIORK3X_HEADPHONETOPO_H_
