/*
Transmit queue behavior. Similar to the receive queue.
*/
#pragma once

struct DeviceContext;
struct ChannelRegisters;
struct MtlQueueRegisters;

// Called by device.cpp AdapterCreateTxQueue.
_IRQL_requires_same_
_IRQL_requires_(PASSIVE_LEVEL)
NTSTATUS
TxQueueCreate(
    _Inout_ DeviceContext* deviceContext,
    _Inout_ NETTXQUEUE_INIT* queueInit,
    _In_ WDFDMAENABLER dma,
    _Inout_ ChannelRegisters* channelRegs,
    _Inout_ MtlQueueRegisters* mtlRegs);
