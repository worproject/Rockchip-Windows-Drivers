#pragma once

struct ChannelRegisters;
struct MtlQueueRegisters;

_IRQL_requires_same_
_IRQL_requires_(PASSIVE_LEVEL)
//__declspec(code_seg("PAGE")) // Nonpaged - on resume path.
NTSTATUS
TxQueueCreate(
    _In_ NETADAPTER adapter,
    _Inout_ NETTXQUEUE_INIT* queueInit,
    _In_ WDFDMAENABLER dma,
    _Inout_ ChannelRegisters* channelRegs,
    _Inout_ MtlQueueRegisters* mtlRegs);
