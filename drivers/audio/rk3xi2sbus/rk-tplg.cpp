#include "driver.h"
#include <acpiioct.h>
#include "rk-tplg.h"

DEFINE_GUID(GUID_ACPI_DSD,
	0xdaffd814, 0x6eba, 0x4d8c, 0x8a, 0x91, 0xbc, 0x9b, 0xbf, 0x4a, 0xa3, 0x01);

void copyDSDParam(PACPI_METHOD_ARGUMENT dsdParameterData, char** buf) {
	RtlZeroMemory(buf, 32);
	RtlCopyMemory(buf, dsdParameterData->Data, min(dsdParameterData->DataLength, 32));
}

void copyDSDParamNum(PACPI_METHOD_ARGUMENT dsdParameterData, UINT32* buf) {
	RtlZeroMemory(buf, sizeof(*buf));
	RtlCopyMemory(buf, dsdParameterData->Data, min(dsdParameterData->DataLength, sizeof(*buf)));
}

NTSTATUS
GetRKTplg(
	_In_ WDFDEVICE FxDevice,
	RK_TPLG* rkTplg
)
{
	if (!rkTplg) {
		return STATUS_INVALID_PARAMETER;
	}

	NTSTATUS status = STATUS_ACPI_NOT_INITIALIZED;
	ACPI_EVAL_INPUT_BUFFER_EX inputBuffer;
	RtlZeroMemory(&inputBuffer, sizeof(inputBuffer));

	inputBuffer.Signature = ACPI_EVAL_INPUT_BUFFER_SIGNATURE_EX;
	status = RtlStringCchPrintfA(
		inputBuffer.MethodName,
		sizeof(inputBuffer.MethodName),
		"_DSD"
	);
	if (!NT_SUCCESS(status)) {
		return status;
	}

	ACPI_EVAL_OUTPUT_BUFFER outputSizeBuffer = { 0 };

	WDF_MEMORY_DESCRIPTOR outputSizeMemDesc;
	WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&outputSizeMemDesc, &outputSizeBuffer, (ULONG)sizeof(outputSizeBuffer));

	WDF_MEMORY_DESCRIPTOR inputMemDesc;
	WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&inputMemDesc, &inputBuffer, (ULONG)sizeof(inputBuffer));

	// Send the request along
	status = WdfIoTargetSendInternalIoctlSynchronously(
		WdfDeviceGetIoTarget(FxDevice),
		NULL,
		IOCTL_ACPI_EVAL_METHOD_EX,
		&inputMemDesc,
		&outputSizeMemDesc,
		NULL,
		NULL
	);

	if (status != STATUS_BUFFER_OVERFLOW) {
		// Method might not exist?
		return status;
	}

	WDFMEMORY outputMemory;
	PACPI_EVAL_OUTPUT_BUFFER outputBuffer;
	size_t outputArgumentBufferSize = outputSizeBuffer.Length;
	size_t outputBufferSize = FIELD_OFFSET(ACPI_EVAL_OUTPUT_BUFFER, Argument) + outputArgumentBufferSize;

	WDF_OBJECT_ATTRIBUTES attributes;
	WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
	attributes.ParentObject = FxDevice;

	status = WdfMemoryCreate(&attributes,
		NonPagedPoolNx,
		0,
		outputBufferSize,
		&outputMemory,
		(PVOID*)&outputBuffer);
	if (!NT_SUCCESS(status)) {
		return status;
	}

	RtlZeroMemory(outputBuffer, outputBufferSize);

	WDF_MEMORY_DESCRIPTOR outputMemDesc;
	WDF_MEMORY_DESCRIPTOR_INIT_HANDLE(&outputMemDesc, outputMemory, NULL);

	status = WdfIoTargetSendInternalIoctlSynchronously(
		WdfDeviceGetIoTarget(FxDevice),
		NULL,
		IOCTL_ACPI_EVAL_METHOD_EX,
		&inputMemDesc,
		&outputMemDesc,
		NULL,
		NULL
	);
	if (!NT_SUCCESS(status)) {
		goto Exit;
	}

	if (outputBuffer->Signature != ACPI_EVAL_OUTPUT_BUFFER_SIGNATURE) {
		goto Exit;
	}

	RKI2SBusPrint(DEBUG_LEVEL_ERROR, DBG_PNP,
		"Evaluted %s successfully\n", inputBuffer.MethodName);

	if (outputBuffer->Count % 2) {
		status = STATUS_ACPI_INVALID_DATA;
		goto Exit;
	}

	status = STATUS_NOT_FOUND;

	RtlZeroMemory(rkTplg, sizeof(*rkTplg));
	rkTplg->magic = RKTPLG_MAGIC;
	rkTplg->length = sizeof(*rkTplg);

	PACPI_METHOD_ARGUMENT currArgument = &outputBuffer->Argument[0];
	for (ULONG i = 0; i < outputBuffer->Count; i += 2) {
		PACPI_METHOD_ARGUMENT guidArg = currArgument;
		currArgument = ACPI_METHOD_NEXT_ARGUMENT(currArgument);
		PACPI_METHOD_ARGUMENT packageArg = currArgument;
		currArgument = ACPI_METHOD_NEXT_ARGUMENT(currArgument);

		if (guidArg->Type != ACPI_METHOD_ARGUMENT_BUFFER ||
			guidArg->DataLength != 16 ||
			packageArg->Type != ACPI_METHOD_ARGUMENT_PACKAGE) {
			break;
		}

		if (memcmp(guidArg->Data, &GUID_ACPI_DSD, guidArg->DataLength) != 0) {
			continue;
		}

		status = STATUS_SUCCESS;

#define CHECK_STR(dsdParameter, str) (dsdParameter->DataLength >= strlen(str) && strncmp((const char *)&dsdParameter->Data[0], str, dsdParameter->DataLength) == 0)

		FOR_EACH_ACPI_METHOD_ARGUMENT(dsdParameter, (PACPI_METHOD_ARGUMENT)packageArg->Data, (PACPI_METHOD_ARGUMENT)(packageArg->Data + packageArg->DataLength)) {
			PACPI_METHOD_ARGUMENT dsdParameterName = (PACPI_METHOD_ARGUMENT)dsdParameter->Data;
			PACPI_METHOD_ARGUMENT dsdParameterData = ACPI_METHOD_NEXT_ARGUMENT(dsdParameterName);

			if (CHECK_STR(dsdParameterName, "rockchip,dma")) {
				copyDSDParam(dsdParameterData, (char **)&rkTplg->dma_name);
			}
			else if (CHECK_STR(dsdParameterName, "rockchip,tplg")) {
				copyDSDParam(dsdParameterData, (char**)&rkTplg->audio_tplg);
			}
			else if (CHECK_STR(dsdParameterName, "rockchip,tx")) {
				copyDSDParamNum(dsdParameterData, &rkTplg->tx);
			}
			else if (CHECK_STR(dsdParameterName, "rockchip,rx")) {
				copyDSDParamNum(dsdParameterData, &rkTplg->rx);
			}
			else if (CHECK_STR(dsdParameterName, "rockchip,i2s-tx-route")) {
				copyDSDParamNum(dsdParameterData, &rkTplg->i2s_tx_route);
			}
			else if (CHECK_STR(dsdParameterName, "rockchip,i2s-rx-route")) {
				copyDSDParamNum(dsdParameterData, &rkTplg->i2s_rx_route);
			}
		}
	}

Exit:
	if (outputMemory != WDF_NO_HANDLE) {
		WdfObjectDelete(outputMemory);
	}
	return status;
}