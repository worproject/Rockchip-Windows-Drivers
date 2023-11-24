#pragma once

UINT32 constexpr QueueDescriptorSize = 64;
UINT32 constexpr QueueDescriptorMinCount = PAGE_SIZE / QueueDescriptorSize;
UINT32 constexpr QueueDescriptorMaxCount = 0x400;

bool constexpr QueueBurstLengthX8 = true;
UINT32 constexpr QueueBurstLength = 64u / (QueueBurstLengthX8 ? 8 : 1); // TODO: load from ACPI?

// Alignment is mainly to make sure the allocation does not cross a 4GB boundary,
// but it also simplifies working with the addresses.
auto const QueueDescriptorAlignment = QueueDescriptorMaxCount * QueueDescriptorSize;

_IRQL_requires_max_(PASSIVE_LEVEL)
UINT32
QueueDescriptorCount(UINT32 fragmentCount);

_IRQL_requires_max_(DISPATCH_LEVEL)
UINT32
QueueDescriptorAddressToIndex(UINT32 address, PHYSICAL_ADDRESS descPhysical, UINT32 descCount);
