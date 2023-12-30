/*
Definitions shared between TxQueue and RxQueue.
*/
#pragma once

UINT32 constexpr QueueDescriptorSize = 64; // 64 == sizeof(TxDescriptor) == sizeof(RxDescriptor)
UINT32 constexpr QueueDescriptorMinCount = PAGE_SIZE / QueueDescriptorSize;
UINT32 constexpr QueueDescriptorMaxCount = 0x400;

// Alignment is mainly to make sure the allocation does not cross a 4GB boundary,
// but it also simplifies the QueueDescriptorAddressToIndex implementation.
auto const QueueDescriptorAlignment = QueueDescriptorMaxCount * QueueDescriptorSize;

// Given the size of the fragment ring, return the number of descriptors to
// allocate for the descriptor ring. This will be a power of 2 between
// QueueDescriptorMinCount and QueueDescriptorMaxCount.
_IRQL_requires_max_(PASSIVE_LEVEL)
UINT32
QueueDescriptorCount(UINT32 fragmentCount);

// Given a Current_App_RxDesc or Current_App_TxDesc address, return the corresponding
// index into the descriptor ring. Assumes that descriptor ring is aligned to
// QueueDescriptorAlignment. descPhysical and descCount are for assertions.
_IRQL_requires_max_(DISPATCH_LEVEL)
UINT32
QueueDescriptorAddressToIndex(UINT32 address, PHYSICAL_ADDRESS descPhysical, UINT32 descCount);
