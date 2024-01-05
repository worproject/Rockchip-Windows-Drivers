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
    NET_EXTENSION packetChecksum;
    NET_EXTENSION fragmentLogical;
    UINT32 descCount;   // A power of 2 between QueueDescriptorMinCount and QueueDescriptorMaxCount.
    UINT8 txPbl;
    bool txChecksumOffload;

    UINT16 lastVlanTag;
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

    context->lastVlanTag = 0; // Make no assumptions about the device's current vlan tag.
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
    txControl.TxPbl = context->txPbl;
    Write32(&context->channelRegs->Tx_Control, txControl);

    TraceEntryExit(TxQueueStart, LEVEL_INFO);
}

// Starting at pktIndex, scan forward until we reach packetRing->NextIndex or a
// non-ignored packet. If we reach a non-ignored packet, sets *pktLastFrag to the last
// fragment of the found packet and returns the packet's index. Otherwise, sets
// *pktLastFrag to 0xFFFFFFFF and returns packetRing->NextIndex.
static UINT32
SkipIgnorePackets(UINT32 pktIndex, _In_ TxQueueContext const* context, _Out_ UINT32* pktLastFrag)
{
    // DISPATCH_LEVEL
    auto const pktNext = context->packetRing->NextIndex;
    for (; pktIndex != pktNext; pktIndex = NetRingIncrementIndex(context->packetRing, pktIndex))
    {
        auto const pkt = NetRingGetPacketAtIndex(context->packetRing, pktIndex);
        if (!pkt->Ignore)
        {
            NT_ASSERT(pkt->FragmentCount != 0);
            *pktLastFrag = NetRingAdvanceIndex(context->fragmentRing, pkt->FragmentIndex, pkt->FragmentCount - 1u);
            return pktIndex;
        }
    }

    *pktLastFrag = 0xFFFFFFFF;
    return pktNext;
}

static EVT_PACKET_QUEUE_ADVANCE TxQueueAdvance;
static void
TxQueueAdvance(_In_ NETPACKETQUEUE queue)
{
    // DISPATCH_LEVEL
    auto const context = TxQueueGetContext(queue);
    auto const descMask = context->descCount - 1u;
    auto const descEnd = context->descEnd;
    UINT32 descIndex, pktIndex, pktLastFrag, fragIndex;
    UINT32 doneFrags = 0, queuedFrags = 0;

    /*
    Packet indexes:
    [pktBegin] old [pktNext] new [pktEnd] owned by NetAdapterCx [pktBegin]

    Descriptor indexes:
    [descBegin] TRANSMITTED TRANSMITTING [descEnd] EMPTY [descBegin-1]
    */

    // Indicate transmitted packets.

    // Process any descriptors that the adapter has handed back to us.
    // Release the corresponding fragments and packets.
    pktIndex = SkipIgnorePackets(context->packetRing->BeginIndex, context, &pktLastFrag);
    fragIndex = context->fragmentRing->BeginIndex;
    for (descIndex = context->descBegin; descIndex != descEnd; descIndex = (descIndex + 1) & descMask)
    {
        NT_ASSERT(pktIndex != context->packetRing->NextIndex);
        NT_ASSERT(pktLastFrag != 0xFFFFFFFF);

        auto const& desc = context->descVirtual[descIndex];
        auto const descWrite = desc.Write;

        if (descWrite.Own)
        {
            // Descriptor is still owned by the DMA engine.
            break;
        }

        NT_ASSERT(descWrite.PacketIndex == pktIndex);
        NT_ASSERT(descWrite.FragmentIndex == fragIndex);

        if (descWrite.ContextType)
        {
            // Ignore context descriptors.
            continue;
        }

        if (descWrite.ErrorSummary ||
            descWrite.ContextType ||
            descWrite.DescriptorError ||
            (descWrite.LastDescriptor != 0) != (fragIndex == pktLastFrag))
        {
            TraceWrite("TxQueueAdvance-error", LEVEL_ERROR,
                TraceLoggingUInt32(descIndex),
                TraceLoggingHexInt32(reinterpret_cast<UINT32 const*>(&descWrite)[3], "TDES3"),
                TraceLoggingUInt32(fragIndex),
                TraceLoggingUInt32(pktLastFrag));
        }

        // Non-context descriptors map one-to-one with fragments.

        if (fragIndex == pktLastFrag)
        {
            // Packet is complete. Move to the next one.
            pktIndex = SkipIgnorePackets(NetRingIncrementIndex(context->packetRing, pktIndex), context, &pktLastFrag);
        }

        // Fragment is complete. Move to the next one.
        fragIndex = NetRingIncrementIndex(context->fragmentRing, fragIndex);
    }

    // Return the completed packets and fragments to NetAdapterCx.
    context->packetRing->BeginIndex = pktIndex;
    context->fragmentRing->BeginIndex = fragIndex;

    auto const descBegin = descIndex;
    context->descBegin = descBegin;

    // Fill more descriptors.

    pktIndex = context->packetRing->NextIndex;

    // Number of EMPTY is (descBegin-1) - descEnd (wrapping around if necessary).
    auto const pktEnd = context->packetRing->EndIndex;
    auto const txChecksumOffload = context->txChecksumOffload;
    auto descEmpty = (descBegin - 1 - descEnd) & descMask;
    descIndex = descEnd;
    while (descEmpty != 0)
    {
        NT_ASSERT(descIndex != ((descBegin - 1) & descMask));

        if (pktIndex == pktEnd)
        {
            break;
        }

        auto const pkt = NetRingGetPacketAtIndex(context->packetRing, pktIndex);
        if (!pkt->Ignore)
        {
            fragIndex = pkt->FragmentIndex;

            // If checksum offload is disabled by hardware then we can't call
            // NetExtensionGetPacketChecksum because device.cpp didn't call
            // NetAdapterOffloadSetTxChecksumCapabilities.
            // If offload is disabled by software then the extension will be zeroed.
            auto const checksum = txChecksumOffload
                ? *NetExtensionGetPacketChecksum(&context->packetChecksum, pktIndex)
                : NET_PACKET_CHECKSUM{}; // Disabled by hardware.
            auto const checksumInsertion =
                checksum.Layer4 ? TxChecksumInsertionEnabledIncludingPseudo
                : checksum.Layer3 ? TxChecksumInsertionEnabledHeaderOnly
                : TxChecksumInsertionDisabled;

            UINT32 const fragmentCount = pkt->FragmentCount;
            NT_ASSERT(fragmentCount != 0);

            auto const ieee8021Q = NetExtensionGetPacketIeee8021Q(&context->packetIeee8021Q, pktIndex);
            if (ieee8021Q->TxTagging != 0)
            {
                UINT16 const newTag = (ieee8021Q->PriorityCodePoint << 13) | ieee8021Q->VlanIdentifier;
                NT_ASSERT(newTag != 0);
                if (newTag == context->lastVlanTag)
                {
                    goto NoContextPacket;
                }

                if (fragmentCount + 1 > descEmpty)
                {
                    break;
                }

                TxDescriptorContext descCtx = {};
                descCtx.VlanTag = newTag;
                descCtx.VlanTagValid = true;
                descCtx.ContextType = true;
                descCtx.Own = true;
#if DBG
                descCtx.PacketIndex = pktIndex;
                descCtx.FragmentIndex = fragIndex;
#endif

                context->descVirtual[descIndex].Context = descCtx;
                descIndex = (descIndex + 1) & descMask;
                descEmpty -= 1;
            }
            else
            {
            NoContextPacket:

                if (fragmentCount > descEmpty)
                {
                    break;
                }
            }

            UINT32 frameLength = 0;
            for (unsigned i = 0, fragIndex2 = fragIndex; i != fragmentCount; i += 1)
            {
                frameLength += NetRingGetFragmentAtIndex(context->fragmentRing, fragIndex2)->ValidLength & 0x03FFFFFF; // 26 bits
                fragIndex2 = NetRingIncrementIndex(context->fragmentRing, fragIndex2);
            }
            NT_ASSERT(frameLength <= 0x7FFF);
            frameLength &= 0x7FFF;

            for (unsigned i = 0; i != fragmentCount; i += 1)
            {
                auto const frag = NetRingGetFragmentAtIndex(context->fragmentRing, fragIndex);
                NT_ASSERT(frag->ValidLength <= frag->Capacity - frag->Offset);
                auto const fragLogicalAddress = NetExtensionGetFragmentLogicalAddress(&context->fragmentLogical, fragIndex)->LogicalAddress;

                TxDescriptorRead descRead = {};
                descRead.Buf1Ap = static_cast<UINT32>(fragLogicalAddress);
                descRead.Buf2Ap = static_cast<UINT32>(fragLogicalAddress >> 32);
                NT_ASSERT(frag->ValidLength <= 0x3FFF); // 14 bits
                descRead.Buf1Length = frag->ValidLength & 0x3FFF;
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

                context->descVirtual[descIndex].Read = descRead;
                descIndex = (descIndex + 1) & descMask;
                fragIndex = NetRingIncrementIndex(context->fragmentRing, fragIndex);
                queuedFrags += 1;
            }

            descEmpty -= fragmentCount;
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
        context->txPbl = deviceConfig.txPbl;
        context->txChecksumOffload = deviceConfig.txCoeSel;

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

        if (context->txChecksumOffload)
        {
            NET_EXTENSION_QUERY_INIT(&query,
                NET_PACKET_EXTENSION_CHECKSUM_NAME,
                NET_PACKET_EXTENSION_CHECKSUM_VERSION_1,
                NetExtensionTypePacket);
            NetTxQueueGetExtension(queue, &query, &context->packetChecksum);
        }

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
