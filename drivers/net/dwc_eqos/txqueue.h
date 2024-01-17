/*
Transmit queue behavior. Similar to the receive queue.
*/
#pragma once
#include "queue_common.h"

struct DeviceContext;
struct DeviceConfig;
struct ChannelRegisters;
struct MtlQueueRegisters;

// 3 = 1 hole in the ring + 1 context descriptor + 1 TSE descriptor.
UINT32 constexpr TxMaximumNumberOfFragments = QueueDescriptorMinCount - 3;

UINT16 constexpr TxLayer4HeaderOffsetLimit = 0x3FF; // 10 bits.
UINT32 constexpr TxMaximumOffloadSize = 0x3FFFF; // 18 bits.

// Called by device.cpp AdapterCreateTxQueue.
_IRQL_requires_same_
_IRQL_requires_(PASSIVE_LEVEL)
NTSTATUS
TxQueueCreate(
    _Inout_ DeviceContext* deviceContext,
    _In_ DeviceConfig const& deviceConfig,
    _Inout_ NETTXQUEUE_INIT* queueInit,
    _In_ WDFDMAENABLER dma,
    _Inout_ ChannelRegisters* channelRegs,
    _Inout_ MtlQueueRegisters* mtlRegs);
