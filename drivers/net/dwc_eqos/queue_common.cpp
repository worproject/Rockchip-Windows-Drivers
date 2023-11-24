#include "precomp.h"
#include "queue_common.h"

static_assert((QueueDescriptorAlignment & (QueueDescriptorAlignment - 1)) == 0,
    "QueueDescriptorAlignment must be a power of 2.");

_Use_decl_annotations_ UINT32
QueueDescriptorCount(UINT32 fragmentCount)
{
    // PASSIVE_LEVEL, nonpaged (resume path)
    auto clamped =
        fragmentCount < QueueDescriptorMinCount ? QueueDescriptorMinCount :
        fragmentCount > QueueDescriptorMaxCount ? QueueDescriptorMaxCount :
        fragmentCount;

    // Round up to a power of 2.
    ULONG bits;
    _BitScanReverse(&bits, clamped - 1);
    auto const count = 2u << bits;
    NT_ASSERT(count >= QueueDescriptorMinCount);
    NT_ASSERT(count <= QueueDescriptorMaxCount);
    NT_ASSERT((count & (count - 1)) == 0);

    return count;
}

_Use_decl_annotations_ UINT32
QueueDescriptorAddressToIndex(UINT32 address, PHYSICAL_ADDRESS descPhysical, UINT32 descCount)
{
    // DISPATCH_LEVEL
    UNREFERENCED_PARAMETER(descPhysical);
    UNREFERENCED_PARAMETER(descCount);
    NT_ASSERT(address == 0 || address >= descPhysical.LowPart);
    NT_ASSERT(address < descPhysical.LowPart + descCount * QueueDescriptorSize);
    NT_ASSERT(address % QueueDescriptorSize == 0);
    return (address & (QueueDescriptorAlignment - 1)) / QueueDescriptorSize;
}
