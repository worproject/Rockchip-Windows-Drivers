/*
Receive queue behavior. Similar to the transmit queue.
*/
#pragma once

struct ChannelRegisters;
auto constexpr RxBufferSize = 2048u;

// Called by device.cpp AdapterCreateRxQueue.
_IRQL_requires_same_
_IRQL_requires_(PASSIVE_LEVEL)
NTSTATUS
RxQueueCreate(
    _In_ NETADAPTER adapter,
    _Inout_ NETRXQUEUE_INIT* queueInit,
    _In_ WDFDMAENABLER dma,
    _Inout_ ChannelRegisters* channelRegs);
