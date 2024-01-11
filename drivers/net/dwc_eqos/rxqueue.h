/*
Receive queue behavior. Similar to the transmit queue.
*/
#pragma once

struct DeviceContext;
struct DeviceConfig;
struct ChannelRegisters;

// Called by device.cpp AdapterCreateRxQueue.
_IRQL_requires_same_
_IRQL_requires_(PASSIVE_LEVEL)
NTSTATUS
RxQueueCreate(
    _Inout_ DeviceContext* deviceContext,
    _In_ DeviceConfig const& deviceConfig,
    _Inout_ NETRXQUEUE_INIT* queueInit,
    _In_ WDFDMAENABLER dma,
    _Inout_ ChannelRegisters* channelRegs);
