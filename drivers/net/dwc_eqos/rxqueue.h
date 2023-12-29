/*
Receive queue behavior. Similar to the transmit queue.
*/
#pragma once

struct DeviceContext;
struct ChannelRegisters;
auto constexpr RxBufferSize = 2048u;

// Called by device.cpp AdapterCreateRxQueue.
_IRQL_requires_same_
_IRQL_requires_(PASSIVE_LEVEL)
NTSTATUS
RxQueueCreate(
    _Inout_ DeviceContext* deviceContext,
    _Inout_ NETRXQUEUE_INIT* queueInit,
    _In_ WDFDMAENABLER dma,
    _Inout_ ChannelRegisters* channelRegs);
