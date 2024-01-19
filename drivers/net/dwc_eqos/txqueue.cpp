#include "precomp.h"
#include "txqueue.h"
#include "queue_common.h"
#include "device.h"
#include "registers.h"
#include "descriptors.h"
#include "trace.h"

static_assert(sizeof(TxDescriptor) == QueueDescriptorSize);

struct TxQueueContext
{
    ChannelRegisters* channelRegs;
    MtlQueueRegisters* mtlRegs;
    DeviceContext* deviceContext;
    NET_RING* packetRing;
    NET_RING* fragmentRing;
    WDFCOMMONBUFFER descBuffer;
    TxDescriptor* descVirtual;
    PHYSICAL_ADDRESS descPhysical;
    NET_EXTENSION packetIeee8021Q;
    NET_EXTENSION packetGso;
    NET_EXTENSION packetChecksum;
    NET_EXTENSION fragmentLogical;
    UINT32 descCount;   // A power of 2 between QueueDescriptorMinCount and QueueDescriptorMaxCount.
    UINT16 vlanId;
    UINT8 txPbl;

    UINT16 lastMss;     // MSS set by the most recent context descriptor.
    UINT16 lastVlanTag; // VLAN tag set by the most recent context descriptor.
    UINT32 descBegin;   // Start of the TRANSMIT region.
    UINT32 descEnd;     // End of the TRANSMIT region, start of the EMPTY region.
};
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(TxQueueContext, TxQueueGetContext)

#if DBG

// Gets channelRegs->Current_App_TxDesc, converted to an index.
static UINT32
GetDescNext(_In_ TxQueueContext* context)
{
    // DISPATCH_LEVEL
    auto const current = Read32(&context->channelRegs->Current_App_TxDesc);
    return QueueDescriptorAddressToIndex(current, context->descPhysical, context->descCount);
}

#endif // DBG

// Sets channelRegs->TxDesc_Tail_Pointer to the physical address of the descriptor at the given index.
static void
SetDescEnd(
    _In_ TxQueueContext* context,
    _In_ UINT32 index)
{
    // DISPATCH_LEVEL
    NT_ASSERT(index < context->descCount);
    UINT32 const offset = index * sizeof(TxDescriptor);
    Write32(&context->channelRegs->TxDesc_Tail_Pointer, context->descPhysical.LowPart + offset);
    context->descEnd = index;
}

static EVT_PACKET_QUEUE_START TxQueueStart;
static void
TxQueueStart(_In_ NETPACKETQUEUE queue)
{
    // PASSIVE_LEVEL, nonpaged (resume path)
    auto const context = TxQueueGetContext(queue);

    context->lastMss = 0xFFFF; // Make no assumptions about the device's current MSS.
    context->lastVlanTag = 0;  // Make no assumptions about the device's current vlan tag.
    context->descBegin = 0;
    context->descEnd = 0;

    Write32(&context->channelRegs->TxDesc_List_Address_Hi,context->descPhysical.HighPart);
    Write32(&context->channelRegs->TxDesc_List_Address, context->descPhysical.LowPart);
    Write32(&context->channelRegs->TxDesc_Ring_Length, context->descCount - 1);
    SetDescEnd(context, context->descEnd);

#if DBG
    auto const descNext = GetDescNext(context);
    NT_ASSERT(descNext == context->descEnd); // If this fires, we need more reset logic.
#endif

    ChannelTxControl_t txControl = {};
    txControl.Start = true;
    txControl.OperateOnSecondPacket = true;
    txControl.TcpSegmentation = true;
    txControl.TcpSegmentationMode = 0; // TSO+USO mode.
    txControl.TxPbl = context->txPbl;
    Write32(&context->channelRegs->Tx_Control, txControl);

    TraceEntryExit(TxQueueStart, LEVEL_INFO);
}

// Starting at pktIndex, scan forward until we reach packetRing->NextIndex or a
// non-ignored packet. If we reach a non-ignored packet, set *pktFragIndex to
// pkt->FragmentIndex andreturn the packet's index. Otherwise, leave
// *pktFragIndex unmodified and return packetRing->NextIndex.
static UINT32
SkipIgnorePackets(_In_ NET_RING const* packetRing, UINT32 pktIndex, _Inout_ UINT32* pktFragIndex)
{
    // DISPATCH_LEVEL
    auto const pktNext = packetRing->NextIndex;
    for (; pktIndex != pktNext; pktIndex = NetRingIncrementIndex(packetRing, pktIndex))
    {
        auto const pkt = NetRingGetPacketAtIndex(packetRing, pktIndex);
        if (!pkt->Ignore)
        {
            NT_ASSERT(pkt->FragmentCount != 0);
            *pktFragIndex = pkt->FragmentIndex;
            break;
        }
    }

    return pktIndex;
}

static EVT_PACKET_QUEUE_ADVANCE TxQueueAdvance;
static void
TxQueueAdvance(_In_ NETPACKETQUEUE queue)
{
    // DISPATCH_LEVEL
    auto const context = TxQueueGetContext(queue);
    auto const descMask = context->descCount - 1u;
    auto const descEnd = context->descEnd;
    UINT32 descIndex, pktIndex, pktFragIndex;
    UINT32 doneFrags = 0, queuedFrags = 0;

    /*
    Packet indexes:
    [pktBegin] old [pktNext] new [pktEnd] owned by NetAdapterCx [pktBegin]

    Descriptor indexes:
    [descBegin] TRANSMITTED TRANSMITTING [descEnd] EMPTY [descBegin-1]
    */

    // Indicate transmitted packets.

    // Process any descriptors that the adapter has handed back to us.
    pktFragIndex = context->fragmentRing->BeginIndex;
    pktIndex = context->packetRing->BeginIndex;
    pktIndex = SkipIgnorePackets(context->packetRing, pktIndex, &pktFragIndex);
    for (descIndex = context->descBegin; descIndex != descEnd; descIndex = (descIndex + 1) & descMask)
    {
        NT_ASSERT(pktIndex != context->packetRing->NextIndex);

        auto const& desc = context->descVirtual[descIndex];
        auto const descWrite = desc.Write;

        if (descWrite.Own)
        {
            // Descriptor is still owned by the DMA engine.
            break;
        }

        NT_ASSERT(descWrite.PacketIndex == pktIndex);

        if (descWrite.ContextType)
        {
            // Ignore context descriptors.
            continue;
        }

        if (descWrite.LastDescriptor)
        {
            if (descWrite.ErrorSummary ||
                descWrite.ContextType ||
                descWrite.DescriptorError)
            {
                TraceWrite("TxQueueAdvance-error", LEVEL_ERROR,
                    TraceLoggingUInt32(descIndex),
                    TraceLoggingHexInt32(reinterpret_cast<UINT32 const*>(&descWrite)[3], "TDES3"),
                    TraceLoggingUInt32(pktIndex));
            }

            // Packet is complete. Move to the next one.
            auto const pkt = NetRingGetPacketAtIndex(context->packetRing, pktIndex);
            pktFragIndex = NetRingAdvanceIndex(context->fragmentRing, pkt->FragmentIndex, pkt->FragmentCount);
            pktIndex = NetRingIncrementIndex(context->packetRing, pktIndex);
            pktIndex = SkipIgnorePackets(context->packetRing, pktIndex, &pktFragIndex);
        }
    }

    // Return the completed packets and fragments to NetAdapterCx.
    context->packetRing->BeginIndex = pktIndex;
    context->fragmentRing->BeginIndex = pktFragIndex;

    auto const descBegin = descIndex;
    context->descBegin = descBegin;

    // Fill more descriptors.

    pktIndex = context->packetRing->NextIndex;

    auto const pktEnd = context->packetRing->EndIndex;
    descIndex = descEnd;

#define EMPTY_DESC_REMAINING(descIndex) ((descBegin - 1u - descIndex) & descMask) // -1 because ring has a hole.
    while (EMPTY_DESC_REMAINING(descIndex) != 0)
    {
        if (pktIndex == pktEnd)
        {
            break;
        }

        auto const pkt = NetRingGetPacketAtIndex(context->packetRing, pktIndex);
        if (!pkt->Ignore)
        {
            UINT32 fragIndex = pkt->FragmentIndex;

            UINT32 const fragmentCount = pkt->FragmentCount;
            NT_ASSERT(fragmentCount != 0);

            auto const ieee8021Q = *NetExtensionGetPacketIeee8021Q(&context->packetIeee8021Q, pktIndex);
            UINT16 const ieee8021QPriority = (ieee8021Q.TxTagging & NetPacketTxIeee8021qActionFlagPriorityRequired)
                ? ieee8021Q.PriorityCodePoint : 0u;
            UINT16 const ieee8021QVlan = (ieee8021Q.TxTagging & NetPacketTxIeee8021qActionFlagVlanRequired)
                ? ieee8021Q.VlanIdentifier : context->vlanId;
            UINT16 const vlanTag = (ieee8021QPriority << 13) | ieee8021QVlan;
            auto const vlanTagControl = vlanTag != 0 ? TxVlanTagControlInsert : TxVlanTagControlNone;
            auto const contextTagNeeded = vlanTag != 0 && vlanTag != context->lastVlanTag;

            auto const gso = NetExtensionGetPacketGso(&context->packetGso, pktIndex);
            NT_ASSERT(gso->TCP.Mss <= 0x3FFF); // 14 bits
            auto const gsoMss = static_cast<UINT16>(gso->TCP.Mss);

            if (gsoMss != 0) // segmentation offload enabled
            {
                auto const layout = pkt->Layout;
                NT_ASSERT(layout.Layer4Type == NetPacketLayer4TypeTcp || layout.Layer4Type == NetPacketLayer4TypeUdp);
                if (layout.Layer4Type == NetPacketLayer4TypeTcp)
                {
                    NT_ASSERT(layout.Layer4HeaderLength >= 4u * 5u);
                    NT_ASSERT(layout.Layer4HeaderLength <= 4u * 15u);
                }
                else
                {
                    NT_ASSERT(layout.Layer4HeaderLength == sizeof(UINT32) * 2u);
                }

                // TSO/USO: Headers up to the payload.
                UINT16 const headerLength = layout.Layer2HeaderLength + layout.Layer3HeaderLength + layout.Layer4HeaderLength;
                NT_ASSERT(headerLength <= TxLayer4HeaderOffsetLimit);

                UINT32 packetLength = 0;
                unsigned userDescriptorsNeeded = 0;
                for (unsigned i = 0, fragIndex2 = fragIndex; i != fragmentCount; i += 1)
                {
                    auto const fragLength = static_cast<UINT32>(NetRingGetFragmentAtIndex(context->fragmentRing, fragIndex2)->ValidLength);
                    packetLength += fragLength;
                    userDescriptorsNeeded += (fragLength + QueueDescriptorLengthMax - 1u) / QueueDescriptorLengthMax;
                    NT_ASSERT(packetLength <= TxMaximumOffloadSize + headerLength);
                    fragIndex2 = NetRingIncrementIndex(context->fragmentRing, fragIndex2);
                }
                NT_ASSERT(headerLength <= packetLength);
                NT_ASSERT(userDescriptorsNeeded <= TxMaximumNumberOfFragments);

                bool const contextMssNeeded = gsoMss != context->lastMss;
                bool const contextNeeded = contextTagNeeded || contextMssNeeded;

                // We might need a context descriptor.
                // We will need a header descriptor.
                if (userDescriptorsNeeded + contextNeeded + 1u > EMPTY_DESC_REMAINING(descIndex))
                {
                    // Wait until more descriptors are free.
                    break;
                }

                if (contextNeeded)
                {
                    TxDescriptorContext descCtx = {};
                    descCtx.MaximumSegmentSize = gsoMss;
                    descCtx.VlanTag = vlanTag;
                    descCtx.VlanTagValid = contextTagNeeded;
                    descCtx.OneStepInputOrMssValid = true;
                    descCtx.ContextType = true;
                    descCtx.Own = true;
#if DBG
                    descCtx.PacketIndex = pktIndex;
                    descCtx.FragmentIndex = fragIndex;
#endif

                    NT_ASSERT(EMPTY_DESC_REMAINING(descIndex) != 0);
                    context->descVirtual[descIndex].Context = descCtx;
                    descIndex = (descIndex + 1) & descMask;
                    context->lastMss = gsoMss;
                    if (contextTagNeeded)
                    {
                        context->lastVlanTag = vlanTag;
                    }
                }

                auto const frag0LogicalAddress = NetExtensionGetFragmentLogicalAddress(&context->fragmentLogical, fragIndex)->LogicalAddress;
                NT_ASSERT(headerLength <= NetRingGetFragmentAtIndex(context->fragmentRing, fragIndex)->ValidLength);

                TxDescriptorReadTso descTso = {};
                descTso.Buf1Ap = static_cast<UINT32>(frag0LogicalAddress);
                descTso.Buf2Ap = static_cast<UINT32>(frag0LogicalAddress >> 32);
                descTso.Buf1Length = headerLength;
                descTso.VlanTagControl = vlanTagControl;
                descTso.TcpPayloadLength = packetLength - headerLength;
                descTso.TcpSegmentationEnable = true;
                descTso.TcpHeaderLength = layout.Layer4HeaderLength / 4u;
                descTso.FirstDescriptor = true;
                descTso.Own = true;
#if DBG
                descTso.PacketIndex = pktIndex;
                descTso.FragmentIndex = fragIndex;
#endif

                NT_ASSERT(EMPTY_DESC_REMAINING(descIndex) != 0);
                context->descVirtual[descIndex].ReadTso = descTso;
                descIndex = (descIndex + 1) & descMask;

                unsigned nextFragmentStart = headerLength;
                for (unsigned i = 0; i != fragmentCount; i += 1)
                {
                    unsigned fragPos = nextFragmentStart; // Skip header for first fragment.
                    nextFragmentStart = 0; // Don't skip header for subsequent fragments.
                    auto const frag = NetRingGetFragmentAtIndex(context->fragmentRing, fragIndex);
                    auto const fragLogicalAddress = NetExtensionGetFragmentLogicalAddress(&context->fragmentLogical, fragIndex)->LogicalAddress;
                    auto const fragLength = static_cast<UINT32>(frag->ValidLength);
                    NT_ASSERT(fragLength != 0); // Otherwise we might not set LastDescriptor.

                    while (fragPos != fragLength)
                    {
                        auto const bufLogicalAddress = fragLogicalAddress + fragPos;
                        auto const bufLength = fragLength - fragPos < QueueDescriptorLengthMax
                            ? static_cast<UINT16>(fragLength - fragPos)
                            : QueueDescriptorLengthMax;
                        fragPos += bufLength;
                        auto const lastDesc = fragPos == fragLength && i == fragmentCount - 1u;

                        TxDescriptorRead descRead = {};
                        descRead.Buf1Ap = static_cast<UINT32>(bufLogicalAddress);
                        descRead.Buf2Ap = static_cast<UINT32>(bufLogicalAddress >> 32);
                        descRead.Buf1Length = bufLength;
                        descRead.VlanTagControl = vlanTagControl;
                        descRead.InterruptOnCompletion = lastDesc;
                        descRead.LastDescriptor = lastDesc;
                        descRead.FirstDescriptor = false;
                        descRead.Own = true;
#if DBG
                        descRead.PacketIndex = pktIndex;
                        descRead.FragmentIndex = fragIndex;
#endif

                        NT_ASSERT(EMPTY_DESC_REMAINING(descIndex) != 0);
                        context->descVirtual[descIndex].Read = descRead;
                        descIndex = (descIndex + 1) & descMask;
                    }

                    fragIndex = NetRingIncrementIndex(context->fragmentRing, fragIndex);
                    queuedFrags += 1;
                }
            }
            else // segmentation offload disabled
            {
                // If offload is disabled by software then the extension will be zeroed.
                auto const checksum = *NetExtensionGetPacketChecksum(&context->packetChecksum, pktIndex);
                auto const checksumInsertion =
                    checksum.Layer4 ? TxChecksumInsertionEnabledIncludingPseudo
                    : checksum.Layer3 ? TxChecksumInsertionEnabledHeaderOnly
                    : TxChecksumInsertionDisabled;

                // We might need a context descriptor.
                if (fragmentCount + contextTagNeeded > EMPTY_DESC_REMAINING(descIndex))
                {
                    // Wait until more descriptors are free.
                    break;
                }

                if (contextTagNeeded)
                {
                    TxDescriptorContext descCtx = {};
                    descCtx.VlanTag = vlanTag;
                    descCtx.VlanTagValid = true;
                    descCtx.ContextType = true;
                    descCtx.Own = true;
#if DBG
                    descCtx.PacketIndex = pktIndex;
                    descCtx.FragmentIndex = fragIndex;
#endif

                    NT_ASSERT(EMPTY_DESC_REMAINING(descIndex) != 0);
                    context->descVirtual[descIndex].Context = descCtx;
                    descIndex = (descIndex + 1) & descMask;
                    TraceWrite("TxTag", LEVEL_INFO, TraceLoggingHexInt16(vlanTag));
                    context->lastVlanTag = vlanTag;
                }

                UINT32 frameLength = 0;
                for (unsigned i = 0, fragIndex2 = fragIndex; i != fragmentCount; i += 1)
                {
                    frameLength += static_cast<UINT32>(NetRingGetFragmentAtIndex(context->fragmentRing, fragIndex2)->ValidLength);
                    NT_ASSERT(frameLength <= 0x7FFF);
                    fragIndex2 = NetRingIncrementIndex(context->fragmentRing, fragIndex2);
                }

                for (unsigned i = 0; i != fragmentCount; i += 1)
                {
                    auto const frag = NetRingGetFragmentAtIndex(context->fragmentRing, fragIndex);
                    auto const fragLogicalAddress = NetExtensionGetFragmentLogicalAddress(&context->fragmentLogical, fragIndex)->LogicalAddress;

                    TxDescriptorRead descRead = {};
                    descRead.Buf1Ap = static_cast<UINT32>(fragLogicalAddress);
                    descRead.Buf2Ap = static_cast<UINT32>(fragLogicalAddress >> 32);
                    NT_ASSERT(frag->ValidLength <= QueueDescriptorLengthMax); // 14 bits
                    descRead.Buf1Length = frag->ValidLength;
                    descRead.VlanTagControl = vlanTagControl;
                    descRead.InterruptOnCompletion = i == fragmentCount - 1u;
                    descRead.FrameLength = static_cast<UINT16>(frameLength);
                    descRead.ChecksumInsertion = checksumInsertion;
                    descRead.LastDescriptor = i == fragmentCount - 1u;
                    descRead.FirstDescriptor = i == 0;
                    descRead.Own = true;
#if DBG
                    descRead.PacketIndex = pktIndex;
                    descRead.FragmentIndex = fragIndex;
#endif

                    NT_ASSERT(EMPTY_DESC_REMAINING(descIndex) != 0);
                    context->descVirtual[descIndex].Read = descRead;
                    descIndex = (descIndex + 1) & descMask;
                    fragIndex = NetRingIncrementIndex(context->fragmentRing, fragIndex);
                    queuedFrags += 1;
                }
            } // segmentation offload enabled/disabled
        }

        pktIndex = NetRingIncrementIndex(context->packetRing, pktIndex);
    }

    if (descIndex != descEnd)
    {
        SetDescEnd(context, descIndex);
        context->packetRing->NextIndex = pktIndex;
    }

    DeviceAddStatisticsTxQueue(context->deviceContext, doneFrags);

    TraceEntryExit(TxQueueAdvance, LEVEL_VERBOSE,
        TraceLoggingUInt32(doneFrags),
        TraceLoggingUInt32(queuedFrags));
}

static EVT_PACKET_QUEUE_SET_NOTIFICATION_ENABLED TxQueueSetNotificationEnabled;
static void
TxQueueSetNotificationEnabled(
    _In_ NETPACKETQUEUE queue,
    _In_ BOOLEAN notificationEnabled)
{
    // PASSIVE_LEVEL, nonpaged (resume path)
    auto const context = TxQueueGetContext(queue);
    DeviceSetNotificationTxQueue(context->deviceContext, notificationEnabled ? queue : nullptr);
    TraceEntryExit(TxQueueSetNotificationEnabled, LEVEL_VERBOSE,
        TraceLoggingBoolean(notificationEnabled, "enabled"));
}

static EVT_PACKET_QUEUE_CANCEL TxQueueCancel;
static void
TxQueueCancel(_In_ NETPACKETQUEUE queue)
{
    // DISPATCH_LEVEL (verifier says so)
    auto const context = TxQueueGetContext(queue);
    unsigned char mode; // For tracing: 0 = empty, 1 = flushed, 2 = reset.
    unsigned retry;

    // Shut down.

    ChannelTxControl_t txControl = {};
    txControl.Start = false;
    Write32(&context->channelRegs->Tx_Control, txControl);

    // Flush.

    if (context->packetRing->BeginIndex == context->packetRing->EndIndex)
    {
        // Queue empty.
        mode = 0;
        retry = 0;
        NT_ASSERT(Read32(&context->mtlRegs->Tx_Debug).NotEmpty == 0);
    }
    else
    {
        // Wait for a potential in-progress packet to be sent.

        unsigned constexpr MaxRetry = 100;
        for (retry = 0; retry != MaxRetry; retry += 1)
        {
            auto debug = Read32(&context->mtlRegs->Tx_Debug);

            if (debug.ReadStatus != MtlTxReadStatus_Read && debug.NotEmpty == 0)
            {
                break;
            }

            KeStallExecutionProcessor(20);
        }

        if (retry != MaxRetry)
        {
            // Last chance for any packets to be reported as sent.
            mode = 1;
            TxQueueAdvance(queue);
        }
        else
        {
            // Give up and reset the Tx queue.
            mode = 2;
            TraceWrite("TxQueueCancel-timeout", LEVEL_WARNING);
            auto txOperationMode = Read32(&context->mtlRegs->Tx_Operation_Mode);
            txOperationMode.FlushTxQueue = true;
            Write32(&context->mtlRegs->Tx_Operation_Mode, txOperationMode);
            for (retry = 0;; retry += 1)
            {
                KeStallExecutionProcessor(20);
                if (!Read32(&context->mtlRegs->Tx_Operation_Mode).FlushTxQueue)
                {
                    break;
                }
            }

            context->descBegin = 0;
            context->descEnd = 0;
        }

        NT_ASSERT(context->descBegin == context->descEnd);
        NT_ASSERT(context->descBegin == GetDescNext(context));

        // Discard any packets that are still in the queue.

        context->packetRing->BeginIndex = context->packetRing->EndIndex;
        context->fragmentRing->BeginIndex = context->fragmentRing->EndIndex;
    }

    TraceEntryExit(TxQueueCancel, LEVEL_INFO,
        TraceLoggingUInt8(mode),    // 0 = empty, 1 = flushed, 2 = reset.
        TraceLoggingUInt32(retry));
}

static EVT_PACKET_QUEUE_STOP TxQueueStop;
__declspec(code_seg("PAGE"))
static void
TxQueueStop(_In_ NETPACKETQUEUE queue)
{
    // PASSIVE_LEVEL
    PAGED_CODE();
    auto const context = TxQueueGetContext(queue);

    context->descBegin = 0;
    context->descEnd = 0;
    DeviceSetNotificationTxQueue(context->deviceContext, nullptr);

    TraceEntryExit(TxQueueStop, LEVEL_INFO);
}

static EVT_WDF_OBJECT_CONTEXT_CLEANUP TxQueueCleanup;
static void
TxQueueCleanup(_In_ WDFOBJECT queue)
{
    // DISPATCH_LEVEL
    auto context = TxQueueGetContext(queue);
    if (context->descBuffer)
    {
        WdfObjectDelete(context->descBuffer);
        context->descBuffer = nullptr;
    }

    TraceEntryExit(TxQueueCleanup, LEVEL_VERBOSE);
}

_Use_decl_annotations_ NTSTATUS
TxQueueCreate(
    DeviceContext* deviceContext,
    DeviceConfig const& deviceConfig,
    NETTXQUEUE_INIT* queueInit,
    WDFDMAENABLER dma,
    ChannelRegisters* channelRegs,
    MtlQueueRegisters* mtlRegs)
{
    // PASSIVE_LEVEL, nonpaged (resume path)
    NTSTATUS status;
    NETPACKETQUEUE queue = nullptr;

    // Create queue.

    {
        NET_PACKET_QUEUE_CONFIG config;
        NET_PACKET_QUEUE_CONFIG_INIT(&config, TxQueueAdvance, TxQueueSetNotificationEnabled, TxQueueCancel);
        config.EvtStart = TxQueueStart;
        config.EvtStop = TxQueueStop;

        WDF_OBJECT_ATTRIBUTES attributes;
        WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, TxQueueContext);
        attributes.EvtCleanupCallback = TxQueueCleanup;

        status = NetTxQueueCreate(queueInit, &attributes, &config, &queue);
        if (!NT_SUCCESS(status))
        {
            TraceWrite("NetTxQueueCreate-failed", LEVEL_ERROR,
                TraceLoggingNTStatus(status));
            goto Done;
        }
    }

    // Configure queue.

    {
        auto const rings = NetTxQueueGetRingCollection(queue);

        auto const context = TxQueueGetContext(queue);
        context->channelRegs = channelRegs;
        context->mtlRegs = mtlRegs;
        context->deviceContext = deviceContext;
        context->packetRing = NetRingCollectionGetPacketRing(rings);
        context->fragmentRing = NetRingCollectionGetFragmentRing(rings);
        context->descCount = QueueDescriptorCount(context->fragmentRing->NumberOfElements);
        context->vlanId = deviceConfig.vlanId;
        context->txPbl = deviceConfig.txPbl;

        TraceWrite("TxQueueCreate-size", LEVEL_VERBOSE,
            TraceLoggingHexInt32(context->packetRing->NumberOfElements, "packets"),
            TraceLoggingHexInt32(context->fragmentRing->NumberOfElements, "fragments"),
            TraceLoggingHexInt32(context->descCount, "descriptors"));

        WDF_COMMON_BUFFER_CONFIG bufferConfig;
        WDF_COMMON_BUFFER_CONFIG_INIT(&bufferConfig, QueueDescriptorAlignment - 1);
        status = WdfCommonBufferCreateWithConfig(
            dma,
            sizeof(TxDescriptor) * context->descCount,
            &bufferConfig,
            WDF_NO_OBJECT_ATTRIBUTES,
            &context->descBuffer);
        if (!NT_SUCCESS(status))
        {
            TraceWrite("WdfCommonBufferCreate-failed", LEVEL_ERROR,
                TraceLoggingNTStatus(status));
            goto Done;
        }

        context->descVirtual = static_cast<TxDescriptor*>(
            WdfCommonBufferGetAlignedVirtualAddress(context->descBuffer));
        memset(context->descVirtual, 0, sizeof(TxDescriptor) * context->descCount);
        context->descPhysical =
            WdfCommonBufferGetAlignedLogicalAddress(context->descBuffer);
        TraceWrite("TxQueueCreate-desc", LEVEL_VERBOSE,
            TraceLoggingHexInt64(context->descPhysical.QuadPart, "physical"),
            TraceLoggingPointer(context->descVirtual, "virtual"));

        NET_EXTENSION_QUERY query;

        NET_EXTENSION_QUERY_INIT(&query,
            NET_PACKET_EXTENSION_IEEE8021Q_NAME,
            NET_PACKET_EXTENSION_IEEE8021Q_VERSION_1,
            NetExtensionTypePacket);
        NetTxQueueGetExtension(queue, &query, &context->packetIeee8021Q);

        NET_EXTENSION_QUERY_INIT(&query,
            NET_PACKET_EXTENSION_GSO_NAME,
            NET_PACKET_EXTENSION_GSO_VERSION_1,
            NetExtensionTypePacket);
        NetTxQueueGetExtension(queue, &query, &context->packetGso);

        NET_EXTENSION_QUERY_INIT(&query,
            NET_PACKET_EXTENSION_CHECKSUM_NAME,
            NET_PACKET_EXTENSION_CHECKSUM_VERSION_1,
            NetExtensionTypePacket);
        NetTxQueueGetExtension(queue, &query, &context->packetChecksum);

        NET_EXTENSION_QUERY_INIT(&query,
            NET_FRAGMENT_EXTENSION_LOGICAL_ADDRESS_NAME,
            NET_FRAGMENT_EXTENSION_LOGICAL_ADDRESS_VERSION_1,
            NetExtensionTypeFragment);
        NetTxQueueGetExtension(queue, &query, &context->fragmentLogical);
    }

    status = STATUS_SUCCESS;

Done:

    TraceEntryExitWithStatus(TxQueueCreate, LEVEL_INFO, status);
    return status;
}
