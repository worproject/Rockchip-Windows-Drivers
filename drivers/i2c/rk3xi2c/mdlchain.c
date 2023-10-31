#include "driver.h"

NTSTATUS
MdlChainGetByte(
	PMDL pMdlChain,
	size_t Length,
	size_t Index,
	UCHAR* pByte)
{
	PMDL mdl = pMdlChain;
	size_t mdlByteCount;
	size_t currentOffset = Index;
	PUCHAR pBuffer;
	NTSTATUS status = STATUS_INFO_LENGTH_MISMATCH;

	// Check for out-of-bounds index

	if (Index < Length) {

		while (mdl != NULL) {

			mdlByteCount = MmGetMdlByteCount(mdl);

			if (currentOffset < mdlByteCount) {

				pBuffer = (PUCHAR)MmGetSystemAddressForMdlSafe(mdl,
					NormalPagePriority |
					MdlMappingNoExecute);

				if (pBuffer != NULL) {

					// Byte found, mark successful

					*pByte = pBuffer[currentOffset];
					status = STATUS_SUCCESS;
				}

				break;
			}

			currentOffset -= mdlByteCount;
			mdl = mdl->Next;
		}

		// If after walking the MDL the byte hasn't been found,
		// status will still be STATUS_INFO_LENGTH_MISMATCH
	}

	return status;
}

NTSTATUS
MdlChainSetByte(
	PMDL pMdlChain,
	size_t Length,
	size_t Index,
	UCHAR Byte
)
{
	PMDL mdl = pMdlChain;
	size_t mdlByteCount;
	size_t currentOffset = Index;
	PUCHAR pBuffer;
	NTSTATUS status = STATUS_INFO_LENGTH_MISMATCH;

	// Check for out-of-bounds index

	if (Index < Length) {

		while (mdl != NULL) {

			mdlByteCount = MmGetMdlByteCount(mdl);

			if (currentOffset < mdlByteCount) {

				pBuffer = (PUCHAR)MmGetSystemAddressForMdlSafe(mdl,
					NormalPagePriority |
					MdlMappingNoExecute);

				if (pBuffer != NULL) {

					// Byte found, mark successful

					pBuffer[currentOffset] = Byte;
					status = STATUS_SUCCESS;
				}

				break;
			}

			currentOffset -= mdlByteCount;
			mdl = mdl->Next;
		}

		// If after walking the MDL the byte hasn't been found,
		// status will still be STATUS_INFO_LENGTH_MISMATCH
	}

	return status;
}