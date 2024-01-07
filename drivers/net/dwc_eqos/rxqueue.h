/*
Receive queue behavior. Similar to the transmit queue.
*/
#pragma once

struct DeviceContext;
struct DeviceConfig;
struct ChannelRegisters;

// NetAdapterCx appears to allocate fragment buffers in multiples of PAGE_SIZE,
// so there's no reason to use a size smaller than this. 4KB buffers allow us
// to receive jumbo packets up to 4088 bytes.
auto constexpr RxBufferSize = 4096u;

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
