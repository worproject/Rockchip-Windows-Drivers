#pragma once

struct ChannelRegisters;
auto constexpr RxBufferSize = 2048u;

_IRQL_requires_same_
_IRQL_requires_(PASSIVE_LEVEL)
//__declspec(code_seg("PAGE")) // Nonpaged - on resume path.
NTSTATUS
RxQueueCreate(
    _In_ NETADAPTER adapter,
    _Inout_ NETRXQUEUE_INIT* queueInit,
    _In_ WDFDMAENABLER dma,
    _Inout_ ChannelRegisters* channelRegs);
