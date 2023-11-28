#include "definitions.h"
#include "hw.h"

NTSTATUS CCsAudioRk3xHW::connectDMA() {
	WDF_OBJECT_ATTRIBUTES objectAttributes;

	WDF_OBJECT_ATTRIBUTES_INIT(&objectAttributes);
	objectAttributes.ParentObject = this->m_wdfDevice;

	WDFIOTARGET ioTarget;

	NTSTATUS status = WdfIoTargetCreate(this->m_wdfDevice,
		&objectAttributes,
		&ioTarget
	);
	if (!NT_SUCCESS(status))
	{
		DbgPrint("Error creating IoTarget object - 0x%x\n",
			status);
		if (ioTarget)
			WdfObjectDelete(ioTarget);
		return status;
	}

	DECLARE_UNICODE_STRING_SIZE(busDosDeviceName, 25);
	status = RtlUnicodeStringPrintf(&busDosDeviceName, L"\\DosDevices\\%hs", this->rkTPLG.dma_name);
	if (!NT_SUCCESS(status)) {
		DbgPrint("Error building symbolic link name - %!STATUS!",
			status);

		return status;
	}

	WDF_IO_TARGET_OPEN_PARAMS openParams;
	WDF_IO_TARGET_OPEN_PARAMS_INIT_OPEN_BY_NAME(
		&openParams,
		&busDosDeviceName,
		(GENERIC_READ | GENERIC_WRITE));

	openParams.ShareAccess = FILE_SHARE_READ | FILE_SHARE_WRITE;
	openParams.CreateDisposition = FILE_OPEN;
	openParams.FileAttributes = FILE_ATTRIBUTE_NORMAL;

	PL330DMA_INTERFACE_STANDARD DmaInterface;
	RtlZeroMemory(&DmaInterface, sizeof(DmaInterface));

	status = WdfIoTargetOpen(ioTarget, &openParams);
	if (!NT_SUCCESS(status))
	{
		DbgPrint("Error opening IoTarget object - 0x%x\n",
			status);
		WdfObjectDelete(ioTarget);
		return status;
	}

	status = WdfIoTargetQueryForInterface(ioTarget,
		&GUID_PL330DMA_INTERFACE_STANDARD,
		(PINTERFACE)&DmaInterface,
		sizeof(DmaInterface),
		1,
		NULL);
	WdfIoTargetClose(ioTarget);
	ioTarget = NULL;
	if (!NT_SUCCESS(status)) {
		DbgPrint("WdfFdoQueryForInterface failed 0x%x\n", status);
		return status;
	}

	this->m_DMAInterface = DmaInterface;
	return status;
}