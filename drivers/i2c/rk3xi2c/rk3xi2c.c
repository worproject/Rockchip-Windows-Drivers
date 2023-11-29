#include "driver.h"
#include "stdint.h"

#define bool int
#define MS_IN_US 1000

static ULONG Rk3xI2CDebugLevel = 100;
static ULONG Rk3xI2CDebugCatagories = DBG_INIT || DBG_PNP || DBG_IOCTL;

UINT32 read32(PRK3XI2C_CONTEXT pDevice, UINT32 reg)
{
	return READ_REGISTER_NOFENCE_ULONG((ULONG *)((CHAR*)pDevice->MMIOAddress + reg));
}

void write32(PRK3XI2C_CONTEXT pDevice, UINT32 reg, UINT32 val) {
	WRITE_REGISTER_NOFENCE_ULONG((ULONG *)((CHAR*)pDevice->MMIOAddress + reg), val);
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

	Rk3xI2CPrint(DEBUG_LEVEL_INFO, DBG_INIT,
		"Driver Entry\n");

	WDF_DRIVER_CONFIG_INIT(&config, Rk3xI2CEvtDeviceAdd);

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
		Rk3xI2CPrint(DEBUG_LEVEL_ERROR, DBG_INIT,
			"WdfDriverCreate failed with status 0x%x\n", status);
	}

	return status;
}

static NTSTATUS GetIntegerProperty(
	_In_ WDFDEVICE FxDevice,
	char* propertyStr,
	UINT32* property
) {
	PRK3XI2C_CONTEXT pDevice = GetDeviceContext(FxDevice);
	WDFMEMORY outputMemory = WDF_NO_HANDLE;

	NTSTATUS status = STATUS_ACPI_NOT_INITIALIZED;

	size_t inputBufferLen = sizeof(ACPI_GET_DEVICE_SPECIFIC_DATA) + strlen(propertyStr) + 1;
	ACPI_GET_DEVICE_SPECIFIC_DATA* inputBuffer = ExAllocatePoolWithTag(NonPagedPool, inputBufferLen, RK3XI2C_POOL_TAG);
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
	size_t outputArgumentBufferSize = 8;
	size_t outputBufferSize = FIELD_OFFSET(ACPI_EVAL_OUTPUT_BUFFER, Argument) + sizeof(ACPI_METHOD_ARGUMENT_V1) + outputArgumentBufferSize;

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
		Rk3xI2CPrint(
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
		*property = 0;
		RtlCopyMemory(property, outputBuffer->Argument->Data, min(sizeof(*property), outputBuffer->Argument->DataLength));
	}

Exit:
	if (inputBuffer) {
		ExFreePoolWithTag(inputBuffer, RK3XI2C_POOL_TAG);
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
	PRK3XI2C_CONTEXT pDevice = GetDeviceContext(FxDevice);
	NTSTATUS status = STATUS_INSUFFICIENT_RESOURCES;
	ULONG resourceCount;
	BOOLEAN mmioFound;

	UNREFERENCED_PARAMETER(FxResourcesRaw);

	resourceCount = WdfCmResourceListGetCount(FxResourcesTranslated);
	mmioFound = FALSE;

	Rk3xI2CPrint(DEBUG_LEVEL_INFO, DBG_INIT,
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

	pDevice->busNumber = 0xff; //Set default to -1
	status = GetIntegerProperty(FxDevice, "rockchip,bclk", &pDevice->baseClock);
	if (!NT_SUCCESS(status)) {
		return status;
	}

	Rk3xI2CPrint(DEBUG_LEVEL_INFO, DBG_INIT,
		"Base Clock: %d\n", pDevice->baseClock);

	//status = GetIntegerProperty(FxDevice, "rockchip,grf", &pDevice->busNumber); // only seems to be needed for rk3055/rk3188 
	pDevice->calcTimings = rk3x_i2c_v1_calc_timings; //for rk3399 / rk356x / rk3588

	//Set default timings
	pDevice->timings.bus_freq_hz = I2C_MAX_STANDARD_MODE_FREQ;
	updateClockSettings(pDevice);
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

	PRK3XI2C_CONTEXT pDevice = GetDeviceContext(FxDevice);
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
	UNREFERENCED_PARAMETER(FxPreviousState);
	NTSTATUS status = STATUS_SUCCESS;
	PRK3XI2C_CONTEXT pDevice = GetDeviceContext(FxDevice);

	UINT32 version = (read32(pDevice, REG_CON) & (0xff << 16)) >> 16;
	Rk3xI2CPrint(DEBUG_LEVEL_INFO, DBG_INIT,
		"Version: %d\n", version);

	return status;
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
	UNREFERENCED_PARAMETER(FxTargetState);

	NTSTATUS status = STATUS_SUCCESS;

	return status;
}

NTSTATUS OnTargetConnect(
	_In_  WDFDEVICE  SpbController,
	_In_  SPBTARGET  SpbTarget
) {
	UNREFERENCED_PARAMETER(SpbController);

	PPBC_TARGET pTarget = GetTargetContext(SpbTarget);

	//
	// Get target connection parameters.
	//

	SPB_CONNECTION_PARAMETERS params;
	SPB_CONNECTION_PARAMETERS_INIT(&params);

	SpbTargetGetConnectionParameters(SpbTarget, &params);
	PRH_QUERY_CONNECTION_PROPERTIES_OUTPUT_BUFFER  connection = (PRH_QUERY_CONNECTION_PROPERTIES_OUTPUT_BUFFER)params.ConnectionParameters;

	if (connection->PropertiesLength < sizeof(PNP_SERIAL_BUS_DESCRIPTOR)) {
		Rk3xI2CPrint(DEBUG_LEVEL_INFO, DBG_PNP,
			"Invalid connection properties (length = %lu, "
			"expected = %Iu)\n",
			connection->PropertiesLength,
			sizeof(PNP_SERIAL_BUS_DESCRIPTOR));
		return STATUS_INVALID_PARAMETER;
	}

	PPNP_SERIAL_BUS_DESCRIPTOR descriptor = (PPNP_SERIAL_BUS_DESCRIPTOR)connection->ConnectionProperties;
	if (descriptor->SerialBusType != I2C_SERIAL_BUS_TYPE) {
		Rk3xI2CPrint(DEBUG_LEVEL_INFO, DBG_PNP,
			"Bus type %c not supported, only I2C\n",
			descriptor->SerialBusType);
		return STATUS_INVALID_PARAMETER;
	}

	PPNP_I2C_SERIAL_BUS_DESCRIPTOR i2cDescriptor = (PPNP_I2C_SERIAL_BUS_DESCRIPTOR)connection->ConnectionProperties;
	pTarget->Settings.Address = (ULONG)i2cDescriptor->SlaveAddress;

	USHORT I2CFlags = i2cDescriptor->SerialBusDescriptor.TypeSpecificFlags;

	pTarget->Settings.AddressMode = ((I2CFlags & I2C_SERIAL_BUS_SPECIFIC_FLAG_10BIT_ADDRESS) == 0) ? AddressMode7Bit : AddressMode10Bit;
	pTarget->Settings.ConnectionSpeed = i2cDescriptor->ConnectionSpeed;

	if (pTarget->Settings.AddressMode != AddressMode7Bit) {
		return STATUS_NOT_SUPPORTED;
	}

	return STATUS_SUCCESS;
}

VOID OnSpbIoRead(
	_In_ WDFDEVICE SpbController,
	_In_ SPBTARGET SpbTarget,
	_In_ SPBREQUEST SpbRequest,
	_In_ size_t Length
)
{
	PRK3XI2C_CONTEXT pDevice = GetDeviceContext(SpbController);

	NTSTATUS status = i2c_xfer(pDevice, SpbTarget, SpbRequest, 1);
	SpbRequestComplete(SpbRequest, status);
}

VOID OnSpbIoWrite(
	_In_ WDFDEVICE SpbController,
	_In_ SPBTARGET SpbTarget,
	_In_ SPBREQUEST SpbRequest,
	_In_ size_t Length
)
{
	PRK3XI2C_CONTEXT pDevice = GetDeviceContext(SpbController);

	NTSTATUS status = i2c_xfer(pDevice, SpbTarget, SpbRequest, 1);
	SpbRequestComplete(SpbRequest, status);
}

VOID OnSpbIoSequence(
	_In_ WDFDEVICE SpbController,
	_In_ SPBTARGET SpbTarget,
	_In_ SPBREQUEST SpbRequest,
	_In_ ULONG TransferCount
)
{

	PRK3XI2C_CONTEXT pDevice = GetDeviceContext(SpbController);

	NTSTATUS status = i2c_xfer(pDevice, SpbTarget, SpbRequest, TransferCount);
	SpbRequestComplete(SpbRequest, status);
}

NTSTATUS
Rk3xI2CEvtDeviceAdd(
IN WDFDRIVER       Driver,
IN PWDFDEVICE_INIT DeviceInit
)
{
	NTSTATUS                      status = STATUS_SUCCESS;
	WDF_OBJECT_ATTRIBUTES         attributes;
	WDFDEVICE                     device;
	PRK3XI2C_CONTEXT             devContext;
	WDF_INTERRUPT_CONFIG interruptConfig;

	UNREFERENCED_PARAMETER(Driver);

	PAGED_CODE();

	Rk3xI2CPrint(DEBUG_LEVEL_INFO, DBG_PNP,
		"Rk3xI2CEvtDeviceAdd called\n");

	status = SpbDeviceInitConfig(DeviceInit);
	if (!NT_SUCCESS(status)) {
		Rk3xI2CPrint(DEBUG_LEVEL_ERROR, DBG_PNP,
			"SpbDeviceInitConfig failed with status code 0x%x\n", status);
		return status;
	}

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

	WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, RK3XI2C_CONTEXT);

	//
	// Create a framework device object.This call will in turn create
	// a WDM device object, attach to the lower stack, and set the
	// appropriate flags and attributes.
	//

	status = WdfDeviceCreate(&DeviceInit, &attributes, &device);

	if (!NT_SUCCESS(status))
	{
		Rk3xI2CPrint(DEBUG_LEVEL_ERROR, DBG_PNP,
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
		rk3x_i2c_irq,
		rk3x_i2c_dpc);

	status = WdfInterruptCreate(
		device,
		&interruptConfig,
		WDF_NO_OBJECT_ATTRIBUTES,
		&devContext->Interrupt);

	if (!NT_SUCCESS(status))
	{
		Rk3xI2CPrint(DEBUG_LEVEL_ERROR, DBG_PNP,
			"Error creating WDF interrupt object - %!STATUS!",
			status);

		return status;
	}

	//Create spin lock
	status = WdfWaitLockCreate(WDF_NO_OBJECT_ATTRIBUTES, &devContext->waitLock);
	if (!NT_SUCCESS(status))
	{
		Rk3xI2CPrint(DEBUG_LEVEL_ERROR, DBG_PNP,
			"WdfWaitLockCreate failed with status code 0x%x\n", status);

		return status;
	}

	//Create Event
	KeInitializeEvent(&devContext->waitEvent, NotificationEvent, FALSE);
	
	//
	// Bind a SPB controller object to the device.
	//

	SPB_CONTROLLER_CONFIG spbConfig;
	SPB_CONTROLLER_CONFIG_INIT(&spbConfig);

	spbConfig.PowerManaged = WdfTrue;
	spbConfig.EvtSpbTargetConnect = OnTargetConnect;
	spbConfig.EvtSpbIoRead = OnSpbIoRead;
	spbConfig.EvtSpbIoWrite = OnSpbIoWrite;
	spbConfig.EvtSpbIoSequence = OnSpbIoSequence;

	status = SpbDeviceInitialize(devContext->FxDevice, &spbConfig);
	if (!NT_SUCCESS(status))
	{
		Rk3xI2CPrint(DEBUG_LEVEL_ERROR, DBG_PNP,
			"SpbDeviceInitialize failed with status code 0x%x\n", status);

		return status;
	}

	WDF_OBJECT_ATTRIBUTES targetAttributes;
	WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&targetAttributes, PBC_TARGET);

	SpbControllerSetTargetAttributes(devContext->FxDevice, &targetAttributes);

	return status;
}