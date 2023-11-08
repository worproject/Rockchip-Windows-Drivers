#include "driver.h"
#include "stdint.h"

#define bool int
#define MS_IN_US 1000

static ULONG Pl330DmaDebugLevel = 100;
static ULONG Pl330DmaDebugCatagories = DBG_INIT || DBG_PNP || DBG_IOCTL;

void udelay(ULONG usec) {
	LARGE_INTEGER Interval;
	Interval.QuadPart = -10 * (LONGLONG)usec;
	KeDelayExecutionThread(KernelMode, FALSE, &Interval);
}

UINT32 read32(PPL330DMA_CONTEXT pDevice, UINT32 reg)
{
	return *(UINT32 *)((CHAR *)pDevice->MMIOAddress + reg);
}

void write32(PPL330DMA_CONTEXT pDevice, UINT32 reg, UINT32 val) {
	*(UINT32 *)((CHAR *)pDevice->MMIOAddress + reg) = val;
}

NTSTATUS
DriverEntry(
__in PDRIVER_OBJECT  DriverObject,
__in PUNICODE_STRING RegistryPath
)
{
	NTSTATUS               status = STATUS_SUCCESS;
	WDF_DRIVER_CONFIG      config;
	WDF_OBJECT_ATTRIBUTES  attributes;

	Pl330DmaPrint(DEBUG_LEVEL_INFO, DBG_INIT,
		"Driver Entry\n");

	WDF_DRIVER_CONFIG_INIT(&config, Pl330DmaEvtDeviceAdd);

	WDF_OBJECT_ATTRIBUTES_INIT(&attributes);

	//
	// Create a framework driver object to represent our driver.
	//

	status = WdfDriverCreate(DriverObject,
		RegistryPath,
		&attributes,
		&config,
		WDF_NO_HANDLE
		);

	if (!NT_SUCCESS(status))
	{
		Pl330DmaPrint(DEBUG_LEVEL_ERROR, DBG_INIT,
			"WdfDriverCreate failed with status 0x%x\n", status);
	}

	return status;
}

static NTSTATUS GetStringProperty(
	_In_ WDFDEVICE FxDevice,
	char* propertyStr,
	char* property,
	UINT32 propertyLen
) {
	WDFMEMORY outputMemory = WDF_NO_HANDLE;

	NTSTATUS status = STATUS_ACPI_NOT_INITIALIZED;

	size_t inputBufferLen = sizeof(ACPI_GET_DEVICE_SPECIFIC_DATA) + strlen(propertyStr) + 1;
	ACPI_GET_DEVICE_SPECIFIC_DATA* inputBuffer = ExAllocatePoolZero(NonPagedPool, inputBufferLen, PL330DMA_POOL_TAG);
	if (!inputBuffer) {
		goto Exit;
	}
	RtlZeroMemory(inputBuffer, inputBufferLen);

	inputBuffer->Signature = IOCTL_ACPI_GET_DEVICE_SPECIFIC_DATA_SIGNATURE;

	unsigned char uuidend[] = { 0x8a, 0x91, 0xbc, 0x9b, 0xbf, 0x4a, 0xa3, 0x01 };

	inputBuffer->Section.Data1 = 0xdaffd814;
	inputBuffer->Section.Data2 = 0x6eba;
	inputBuffer->Section.Data3 = 0x4d8c;
	memcpy(inputBuffer->Section.Data4, uuidend, sizeof(uuidend)); //Avoid Windows defender false positive

	strcpy(inputBuffer->PropertyName, propertyStr);
	inputBuffer->PropertyNameLength = strlen(propertyStr) + 1;

	PACPI_EVAL_OUTPUT_BUFFER outputBuffer;
	size_t outputBufferSize = FIELD_OFFSET(ACPI_EVAL_OUTPUT_BUFFER, Argument) + sizeof(ACPI_METHOD_ARGUMENT_V1) + propertyLen;

	WDF_OBJECT_ATTRIBUTES attributes;
	WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
	attributes.ParentObject = FxDevice;
	status = WdfMemoryCreate(&attributes,
		NonPagedPoolNx,
		0,
		outputBufferSize,
		&outputMemory,
		&outputBuffer);
	if (!NT_SUCCESS(status)) {
		goto Exit;
	}

	WDF_MEMORY_DESCRIPTOR inputMemDesc;
	WDF_MEMORY_DESCRIPTOR outputMemDesc;
	WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&inputMemDesc, inputBuffer, (ULONG)inputBufferLen);
	WDF_MEMORY_DESCRIPTOR_INIT_HANDLE(&outputMemDesc, outputMemory, NULL);

	status = WdfIoTargetSendInternalIoctlSynchronously(
		WdfDeviceGetIoTarget(FxDevice),
		NULL,
		IOCTL_ACPI_GET_DEVICE_SPECIFIC_DATA,
		&inputMemDesc,
		&outputMemDesc,
		NULL,
		NULL
	);
	if (!NT_SUCCESS(status)) {
		Pl330DmaPrint(
			DEBUG_LEVEL_ERROR,
			DBG_IOCTL,
			"Error getting device data - 0x%x\n",
			status);
		goto Exit;
	}

	if (outputBuffer->Signature != ACPI_EVAL_OUTPUT_BUFFER_SIGNATURE_V1 &&
		outputBuffer->Count < 1 &&
		outputBuffer->Argument->Type != ACPI_METHOD_ARGUMENT_INTEGER &&
		outputBuffer->Argument->DataLength < 1) {
		status = STATUS_ACPI_INVALID_ARGUMENT;
		goto Exit;
	}

	if (property) {
		RtlZeroMemory(property, propertyLen);
		RtlCopyMemory(property, outputBuffer->Argument->Data, min(propertyLen, outputBuffer->Argument->DataLength));
	}

Exit:
	if (inputBuffer) {
		ExFreePoolWithTag(inputBuffer, PL330DMA_POOL_TAG);
	}
	if (outputMemory != WDF_NO_HANDLE) {
		WdfObjectDelete(outputMemory);
	}
	return status;
}

NTSTATUS
OnPrepareHardware(
_In_  WDFDEVICE     FxDevice,
_In_  WDFCMRESLIST  FxResourcesRaw,
_In_  WDFCMRESLIST  FxResourcesTranslated
)
/*++

Routine Description:

This routine caches the SPB resource connection ID.

Arguments:

FxDevice - a handle to the framework device object
FxResourcesRaw - list of translated hardware resources that
the PnP manager has assigned to the device
FxResourcesTranslated - list of raw hardware resources that
the PnP manager has assigned to the device

Return Value:

Status

--*/
{
	PPL330DMA_CONTEXT pDevice = GetDeviceContext(FxDevice);
	NTSTATUS status = STATUS_INSUFFICIENT_RESOURCES;
	ULONG resourceCount;
	BOOLEAN mmioFound;

	UNREFERENCED_PARAMETER(FxResourcesRaw);

	resourceCount = WdfCmResourceListGetCount(FxResourcesTranslated);
	mmioFound = FALSE;

	Pl330DmaPrint(DEBUG_LEVEL_INFO, DBG_INIT,
		"%s\n", __func__);

	for (ULONG i = 0; i < resourceCount; i++)
	{
		PCM_PARTIAL_RESOURCE_DESCRIPTOR pDescriptor;

		pDescriptor = WdfCmResourceListGetDescriptor(
			FxResourcesTranslated, i);

		switch (pDescriptor->Type)
		{
		case CmResourceTypeMemory:
			if (!mmioFound) {
				pDevice->MMIOAddress = MmMapIoSpace(pDescriptor->u.Memory.Start, pDescriptor->u.Memory.Length, MmNonCached);
				pDevice->MMIOSize = pDescriptor->u.Memory.Length;

				mmioFound = TRUE;
			}
			break;
		}
	}

	if (!pDevice->MMIOAddress || !mmioFound) {
		status = STATUS_NOT_FOUND; //MMIO is required
		return status;
	}

	ReadDmacConfiguration(pDevice);

	status = AllocResources(pDevice);

	if (!NT_SUCCESS(status)) {
		return status;
	}

	return status;
}

NTSTATUS
OnReleaseHardware(
_In_  WDFDEVICE     FxDevice,
_In_  WDFCMRESLIST  FxResourcesTranslated
)
/*++

Routine Description:

Arguments:

FxDevice - a handle to the framework device object
FxResourcesTranslated - list of raw hardware resources that
the PnP manager has assigned to the device

Return Value:

Status

--*/
{
	NTSTATUS status = STATUS_SUCCESS;

	UNREFERENCED_PARAMETER(FxResourcesTranslated);

	PPL330DMA_CONTEXT pDevice = GetDeviceContext(FxDevice);

	FreeResources(pDevice);

	if (pDevice->MMIOAddress) {
		MmUnmapIoSpace(pDevice->MMIOAddress, pDevice->MMIOSize);
	}

	return status;
}

NTSTATUS
OnD0Entry(
_In_  WDFDEVICE               FxDevice,
_In_  WDF_POWER_DEVICE_STATE  FxPreviousState
)
/*++

Routine Description:

This routine allocates objects needed by the driver.

Arguments:

FxDevice - a handle to the framework device object
FxPreviousState - previous power state

Return Value:

Status

--*/
{
	UNREFERENCED_PARAMETER(FxDevice);
	UNREFERENCED_PARAMETER(FxPreviousState);

	return STATUS_SUCCESS;
}

NTSTATUS
OnD0Exit(
_In_  WDFDEVICE               FxDevice,
_In_  WDF_POWER_DEVICE_STATE  FxTargetState
)
/*++

Routine Description:

This routine destroys objects needed by the driver.

Arguments:

FxDevice - a handle to the framework device object
FxTargetState - target power state

Return Value:

Status

--*/
{
	UNREFERENCED_PARAMETER(FxDevice);
	UNREFERENCED_PARAMETER(FxTargetState);
	return STATUS_SUCCESS;
}

NTSTATUS
Pl330DmaEvtDeviceAdd(
IN WDFDRIVER       Driver,
IN PWDFDEVICE_INIT DeviceInit
)
{
	NTSTATUS                      status = STATUS_SUCCESS;
	WDF_OBJECT_ATTRIBUTES         attributes;
	WDFDEVICE                     device;
	PPL330DMA_CONTEXT             devContext;
	WDF_QUERY_INTERFACE_CONFIG    qiConfig;
	WDF_INTERRUPT_CONFIG interruptConfig;

	UNREFERENCED_PARAMETER(Driver);

	PAGED_CODE();

	Pl330DmaPrint(DEBUG_LEVEL_INFO, DBG_PNP,
		"Pl330DmaEvtDeviceAdd called\n");

	{
		WDF_PNPPOWER_EVENT_CALLBACKS pnpCallbacks;
		WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnpCallbacks);

		pnpCallbacks.EvtDevicePrepareHardware = OnPrepareHardware;
		pnpCallbacks.EvtDeviceReleaseHardware = OnReleaseHardware;
		pnpCallbacks.EvtDeviceD0Entry = OnD0Entry;
		pnpCallbacks.EvtDeviceD0Exit = OnD0Exit;

		WdfDeviceInitSetPnpPowerEventCallbacks(DeviceInit, &pnpCallbacks);
	}

	//
	// Setup the device context
	//

	WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, PL330DMA_CONTEXT);

	// Set DeviceType
	WdfDeviceInitSetDeviceType(DeviceInit, FILE_DEVICE_CONTROLLER);

	//
	// Create a framework device object.This call will in turn create
	// a WDM device object, attach to the lower stack, and set the
	// appropriate flags and attributes.
	//

	status = WdfDeviceCreate(&DeviceInit, &attributes, &device);

	if (!NT_SUCCESS(status))
	{
		Pl330DmaPrint(DEBUG_LEVEL_ERROR, DBG_PNP,
			"WdfDeviceCreate failed with status code 0x%x\n", status);

		return status;
	}

	{
		WDF_DEVICE_STATE deviceState;
		WDF_DEVICE_STATE_INIT(&deviceState);

		deviceState.NotDisableable = WdfFalse;
		WdfDeviceSetDeviceState(device, &deviceState);
	}

	devContext = GetDeviceContext(device);

	devContext->FxDevice = device;

	//
	// Create an interrupt object for hardware notifications
	//
	WDF_INTERRUPT_CONFIG_INIT(
		&interruptConfig,
		pl330_dma_irq,
		pl330_dma_dpc);

	status = WdfInterruptCreate(
		device,
		&interruptConfig,
		WDF_NO_OBJECT_ATTRIBUTES,
		&devContext->Interrupt);

	if (!NT_SUCCESS(status))
	{
		Pl330DmaPrint(DEBUG_LEVEL_ERROR, DBG_PNP,
			"Error creating WDF interrupt object - %!STATUS!",
			status);

		return status;
	}

	//
	// Create a second interrupt object for hardware notifications
	//
	WDF_INTERRUPT_CONFIG_INIT(
		&interruptConfig,
		pl330_dma_irq,
		NULL);

	status = WdfInterruptCreate(
		device,
		&interruptConfig,
		WDF_NO_OBJECT_ATTRIBUTES,
		&devContext->Interrupt2);

	if (!NT_SUCCESS(status))
	{
		Pl330DmaPrint(DEBUG_LEVEL_ERROR, DBG_PNP,
			"Error creating WDF interrupt object 2 - %!STATUS!",
			status);

		return status;
	}

	char dmaName[10];
	status = GetStringProperty(device, "ctlrName", dmaName, sizeof(dmaName));
	if (!NT_SUCCESS(status))
	{
		Pl330DmaPrint(DEBUG_LEVEL_ERROR, DBG_PNP,
			"Error getting ctlrName - %!STATUS!",
			status);

		return status;
	}

	DECLARE_UNICODE_STRING_SIZE(dosDeviceName, 25);
	status = RtlUnicodeStringPrintf(&dosDeviceName, SYMBOLIC_NAME_PREFIX, dmaName);
	if (!NT_SUCCESS(status)) {
		Pl330DmaPrint(DEBUG_LEVEL_ERROR, DBG_PNP,
			"Error building symbolic link name - %!STATUS!",
			status);

		return status;
	}

	status = WdfDeviceCreateSymbolicLink(device,
		&dosDeviceName
	);
	if (!NT_SUCCESS(status)) {
		Pl330DmaPrint(DEBUG_LEVEL_ERROR, DBG_PNP,
			"WdfDeviceCreateSymbolicLink failed 0x%x\n", status);
		return status;
	}

	{ // V1
		PL330DMA_INTERFACE_STANDARD Pl330DMAInterface;
		RtlZeroMemory(&Pl330DMAInterface, sizeof(Pl330DMAInterface));

		Pl330DMAInterface.InterfaceHeader.Size = sizeof(Pl330DMAInterface);
		Pl330DMAInterface.InterfaceHeader.Version = 1;
		Pl330DMAInterface.InterfaceHeader.Context = (PVOID)devContext;

		//
		// Let the framework handle reference counting.
		//
		Pl330DMAInterface.InterfaceHeader.InterfaceReference = WdfDeviceInterfaceReferenceNoOp;
		Pl330DMAInterface.InterfaceHeader.InterfaceDereference = WdfDeviceInterfaceDereferenceNoOp;

		Pl330DMAInterface.GetChannel = GetHandle;
		Pl330DMAInterface.FreeChannel = FreeHandle;
		Pl330DMAInterface.StopDMA = StopThread;
		Pl330DMAInterface.GetThreadRegisters = GetThreadRegisters;
		Pl330DMAInterface.SubmitAudioDMA = SubmitAudioDMA;

		Pl330DMAInterface.RegisterNotificationCallback = RegisterNotificationCallback;
		Pl330DMAInterface.UnregisterNotificationCallback = UnregisterNotificationCallback;

		WDF_QUERY_INTERFACE_CONFIG_INIT(&qiConfig,
			(PINTERFACE)&Pl330DMAInterface,
			&GUID_PL330DMA_INTERFACE_STANDARD,
			NULL);

		status = WdfDeviceAddQueryInterface(device, &qiConfig);
		if (!NT_SUCCESS(status)) {
			Pl330DmaPrint(DEBUG_LEVEL_ERROR, DBG_PNP,
				"WdfDeviceAddQueryInterface failed 0x%x\n", status);

			return status;
		}
	}

	return status;
}