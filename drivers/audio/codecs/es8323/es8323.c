#define DESCRIPTOR_DEF
#include "driver.h"
#include "stdint.h"

#define bool int
#define MS_IN_US 1000

static ULONG Es8323DebugLevel = 100;
static ULONG Es8323DebugCatagories = DBG_INIT || DBG_PNP || DBG_IOCTL;

struct _coeff_div {
	UINT32 mclk;
	UINT32 rate;
	UINT16 fs;
	UINT8 sr : 4;
	UINT8 usb : 1;
};

/* codec hifi mclk clock divider coefficients */
static const struct _coeff_div coeff_div[] = {
	/* 8k */
	{2048000, 8000, 256, 0x2, 0x0},
	{4096000, 8000, 512, 0x4, 0x0},
	{12288000, 8000, 1536, 0xa, 0x0},
	{11289600, 8000, 1408, 0x9, 0x0},
	{18432000, 8000, 2304, 0xc, 0x0},
	{16934400, 8000, 2112, 0xb, 0x0},
	{12000000, 8000, 1500, 0xb, 0x1},

	/* 11.025k */
	{11289600, 11025, 1024, 0x7, 0x0},
	{16934400, 11025, 1536, 0xa, 0x0},
	{12000000, 11025, 1088, 0x9, 0x1},

	/* 16k */
	{4096000, 16000, 256, 0x2, 0x0},
	{8192000, 16000, 512, 0x4, 0x0},
	{12288000, 16000, 768, 0x6, 0x0},
	{18432000, 16000, 1152, 0x8, 0x0},
	{12000000, 16000, 750, 0x7, 0x1},

	/* 22.05k */
	{11289600, 22050, 512, 0x4, 0x0},
	{16934400, 22050, 768, 0x6, 0x0},
	{12000000, 22050, 544, 0x6, 0x1},

	/* 32k */
	{8192000, 32000, 256, 0x2, 0x0},
	{16384000, 32000, 512, 0x4, 0x0},
	{12288000, 32000, 384, 0x3, 0x0},
	{18432000, 32000, 576, 0x5, 0x0},
	{12000000, 32000, 375, 0x4, 0x1},

	/* 44.1k */
	{11289600, 44100, 256, 0x2, 0x0},
	{16934400, 44100, 384, 0x3, 0x0},
	{12000000, 44100, 272, 0x3, 0x1},

	/* 48k */
	{12288000, 48000, 256, 0x2, 0x0},
	{18432000, 48000, 384, 0x3, 0x0},
	{12000000, 48000, 250, 0x2, 0x1},

	/* 88.2k */
	{11289600, 88200, 128, 0x0, 0x0},
	{16934400, 88200, 192, 0x1, 0x0},
	{12000000, 88200, 136, 0x1, 0x1},

	/* 96k */
	{12288000, 96000, 128, 0x0, 0x0},
	{18432000, 96000, 192, 0x1, 0x0},
	{12000000, 96000, 125, 0x0, 0x1},
};

NTSTATUS
DriverEntry(
__in PDRIVER_OBJECT  DriverObject,
__in PUNICODE_STRING RegistryPath
)
{
	NTSTATUS               status = STATUS_SUCCESS;
	WDF_DRIVER_CONFIG      config;
	WDF_OBJECT_ATTRIBUTES  attributes;

	Es8323Print(DEBUG_LEVEL_INFO, DBG_INIT,
		"Driver Entry\n");

	WDF_DRIVER_CONFIG_INIT(&config, Es8323EvtDeviceAdd);

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
		Es8323Print(DEBUG_LEVEL_ERROR, DBG_INIT,
			"WdfDriverCreate failed with status 0x%x\n", status);
	}

	return status;
}

void udelay(ULONG usec) {
	LARGE_INTEGER Interval;
	Interval.QuadPart = -10 * (LONGLONG)usec;
	KeDelayExecutionThread(KernelMode, FALSE, &Interval);
}

/* ES8288 has 8-bit register addresses, and 8-bit register data */
struct es8323_init_reg {
	uint8_t reg;
	uint8_t val;
};

NTSTATUS es8323_reg_read(
	_In_ PES8323_CONTEXT pDevice,
	uint8_t reg,
	uint8_t* data
) {
	uint8_t raw_data = 0;
	NTSTATUS status = SpbXferDataSynchronously(&pDevice->I2CContext, &reg, sizeof(reg), &raw_data, sizeof(raw_data));
	*data = raw_data;
	return status;
}

NTSTATUS es8323_reg_write(
	_In_ PES8323_CONTEXT pDevice,
	uint8_t reg,
	uint8_t data
) {
	uint8_t buf[2];
	buf[0] = reg;
	buf[1] = data;
	return SpbWriteDataSynchronously(&pDevice->I2CContext, buf, sizeof(buf));
}

NTSTATUS es8323_reg_update(
	_In_ PES8323_CONTEXT pDevice,
	uint8_t reg,
	uint8_t mask,
	uint8_t val
) {
	uint8_t tmp = 0, orig = 0;

	NTSTATUS status = es8323_reg_read(pDevice, reg, &orig);
	if (!NT_SUCCESS(status)) {
		return status;
	}

	tmp = orig & ~mask;
	tmp |= val & mask;

	if (tmp != orig) {
		status = es8323_reg_write(pDevice, reg, tmp);
	}
	return status;
}

/*static void debug_dump_regs(PES8323_CONTEXT pDevice)
{
	uint8_t i, reg_byte;
	// Show all codec regs
	for (i = 0; i <= ES8323_DACCONTROL30; i += 16) {
		uint32_t regs[16];
		for (int j = 0; j < 16; j++) {
			es8323_reg_read(pDevice, (uint8_t)(i + j), &reg_byte);
			regs[j] = reg_byte;
		}
		DbgPrint("%02x: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x \n", i, regs[0], regs[1], regs[2], regs[3], regs[4], regs[5], regs[6], regs[7],
			regs[8], regs[9], regs[10], regs[11], regs[12], regs[13], regs[14], regs[15]);
	}
}*/

NTSTATUS BOOTCODEC(
	_In_  PES8323_CONTEXT  pDevice
	)
{
	NTSTATUS status = 0;

	pDevice->ConnectInterrupt = false;

	Es8323Print(DEBUG_LEVEL_INFO, DBG_PNP,
		"Initializing ES8323!\n");

	//Reset

	status = es8323_reg_write(pDevice, ES8323_CONTROL1, 0x80);
	if (!NT_SUCCESS(status)) {
		return status;
	}
	status = es8323_reg_write(pDevice, ES8323_CONTROL1, 0x00);
	if (!NT_SUCCESS(status)) {
		return status;
	}

	//Enable

	status = es8323_reg_write(pDevice, 0x01, 0x60);
	if (!NT_SUCCESS(status)) {
		return status;
	}
	status = es8323_reg_write(pDevice, 0x02, 0xF3);
	if (!NT_SUCCESS(status)) {
		return status;
	}
	status = es8323_reg_write(pDevice, 0x02, 0xF0);
	if (!NT_SUCCESS(status)) {
		return status;
	}
	status = es8323_reg_write(pDevice, 0x2B, 0x80);
	if (!NT_SUCCESS(status)) {
		return status;
	}
	status = es8323_reg_write(pDevice, 0x00, 0x36);
	if (!NT_SUCCESS(status)) {
		return status;
	}
	status = es8323_reg_write(pDevice, 0x08, 0x00);
	if (!NT_SUCCESS(status)) {
		return status;
	}
	status = es8323_reg_write(pDevice, 0x04, 0x00);
	if (!NT_SUCCESS(status)) {
		return status;
	}
	status = es8323_reg_write(pDevice, 0x06, 0xC3);
	if (!NT_SUCCESS(status)) {
		return status;
	}
	status = es8323_reg_write(pDevice, 0x19, 0x02);
	if (!NT_SUCCESS(status)) {
		return status;
	}
	status = es8323_reg_write(pDevice, 0x09, 0x00);
	if (!NT_SUCCESS(status)) {
		return status;
	}
	status = es8323_reg_write(pDevice, 0x0A, 0x00);
	if (!NT_SUCCESS(status)) {
		return status;
	}
	status = es8323_reg_write(pDevice, 0x0B, 0x02);
	if (!NT_SUCCESS(status)) {
		return status;
	}
	status = es8323_reg_write(pDevice, 0x0C, 0x4C);
	if (!NT_SUCCESS(status)) {
		return status;
	}
	status = es8323_reg_write(pDevice, 0x0D, 0x02);
	if (!NT_SUCCESS(status)) {
		return status;
	}
	status = es8323_reg_write(pDevice, 0x10, 0x00);
	if (!NT_SUCCESS(status)) {
		return status;
	}
	status = es8323_reg_write(pDevice, 0x11, 0x00);
	if (!NT_SUCCESS(status)) {
		return status;
	}
	status = es8323_reg_write(pDevice, 0x12, 0xea);
	if (!NT_SUCCESS(status)) {
		return status;
	}
	status = es8323_reg_write(pDevice, 0x13, 0xc0);
	if (!NT_SUCCESS(status)) {
		return status;
	}
	status = es8323_reg_write(pDevice, 0x14, 0x05);
	if (!NT_SUCCESS(status)) {
		return status;
	}
	status = es8323_reg_write(pDevice, 0x15, 0x06);
	if (!NT_SUCCESS(status)) {
		return status;
	}
	status = es8323_reg_write(pDevice, 0x16, 0x53);
	if (!NT_SUCCESS(status)) {
		return status;
	}

	status = es8323_reg_write(pDevice, 0x17, 0x18);
	if (!NT_SUCCESS(status)) {
		return status;
	}
	status = es8323_reg_write(pDevice, 0x18, 0x02);
	if (!NT_SUCCESS(status)) {
		return status;
	}
	status = es8323_reg_write(pDevice, 0x1A, 0x00);
	if (!NT_SUCCESS(status)) {
		return status;
	}
	status = es8323_reg_write(pDevice, 0x1B, 0x00);
	if (!NT_SUCCESS(status)) {
		return status;
	}
	status = es8323_reg_write(pDevice, 0x27, 0xB8);
	if (!NT_SUCCESS(status)) {
		return status;
	}
	status = es8323_reg_write(pDevice, 0x2A, 0xB8);
	if (!NT_SUCCESS(status)) {
		return status;
	}
	status = es8323_reg_write(pDevice, 0x35, 0xA0);
	if (!NT_SUCCESS(status)) {
		return status;
	}

	udelay(18000);

	status = es8323_reg_write(pDevice, 0x2E, 0x1E);
	if (!NT_SUCCESS(status)) {
		return status;
	}
	status = es8323_reg_write(pDevice, 0x2F, 0x1E);
	if (!NT_SUCCESS(status)) {
		return status;
	}
	status = es8323_reg_write(pDevice, 0x30, 0x1E);
	if (!NT_SUCCESS(status)) {
		return status;
	}
	status = es8323_reg_write(pDevice, 0x31, 0x1E);
	if (!NT_SUCCESS(status)) {
		return status;
	}
	status = es8323_reg_write(pDevice, 0x03, 0x09);
	if (!NT_SUCCESS(status)) {
		return status;
	}
	status = es8323_reg_write(pDevice, 0x02, 0x00);
	if (!NT_SUCCESS(status)) {
		return status;
	}

	udelay(18000);

	status = es8323_reg_write(pDevice, 0x04, 0x3C);
	if (!NT_SUCCESS(status)) {
		return status;
	}

	//standby / prepare bias

	status = es8323_reg_write(pDevice, ES8323_ANAVOLMANAG, 0x7C);
	if (!NT_SUCCESS(status)) {
		return status;
	}
	status = es8323_reg_write(pDevice, ES8323_CHIPLOPOW1, 0x00);
	if (!NT_SUCCESS(status)) {
		return status;
	}
	status = es8323_reg_write(pDevice, ES8323_CHIPLOPOW2, 0x00);
	if (!NT_SUCCESS(status)) {
		return status;
	}
	status = es8323_reg_write(pDevice, ES8323_CHIPPOWER, 0x00);
	if (!NT_SUCCESS(status)) {
		return status;
	}
	status = es8323_reg_write(pDevice, ES8323_ADCPOWER, 0x59);
	if (!NT_SUCCESS(status)) {
		return status;
	}

	//Hw Params

	{
		UINT32 coeff = 0x17;
		UINT8 val;
		status = es8323_reg_read(pDevice, ES8323_IFACE, &val);
		if (!NT_SUCCESS(status)) {
			return status;
		}
		UINT16 srate = val & 0x80;
		status = es8323_reg_read(pDevice, ES8323_ADC_IFACE, &val);
		if (!NT_SUCCESS(status)) {
			return status;
		}
		UINT16 adciface = val & 0xE3;
		status = es8323_reg_read(pDevice, ES8323_DAC_IFACE, &val);
		if (!NT_SUCCESS(status)) {
			return status;
		}
		UINT16 daciface = val & 0xC7;

		//16-bit
		adciface |= 0x000C;
		daciface |= 0x0018;

		status = es8323_reg_write(pDevice, ES8323_DAC_IFACE, daciface & 0xff);
		if (!NT_SUCCESS(status)) {
			return status;
		}
		status = es8323_reg_write(pDevice, ES8323_ADC_IFACE, adciface & 0xff);
		if (!NT_SUCCESS(status)) {
			return status;
		}

		//coeff
		status = es8323_reg_write(pDevice, ES8323_IFACE, srate & 0xff);
		if (!NT_SUCCESS(status)) {
			return status;
		}
		status = es8323_reg_write(pDevice, ES8323_ADCCONTROL5,
			coeff_div[coeff].sr |
			coeff_div[coeff].usb << 4);
		if (!NT_SUCCESS(status)) {
			return status;
		}
		status = es8323_reg_write(pDevice, ES8323_DACCONTROL2,
			coeff_div[coeff].sr |
			coeff_div[coeff].usb << 4);
		if (!NT_SUCCESS(status)) {
			return status;
		}
	}


	status = es8323_reg_write(pDevice, ES8323_ADCPOWER, 0x09);
	if (!NT_SUCCESS(status)) {
		return status;
	}

	//Set ALC Capture
	status = es8323_reg_write(pDevice, ES8323_ADCCONTROL10, 0xfa); //max / min pga
	if (!NT_SUCCESS(status)) {
		return status;
	}
	status = es8323_reg_write(pDevice, ES8323_ADCCONTROL11, 0xf0); //target volume / hold time
	if (!NT_SUCCESS(status)) {
		return status;
	}
	status = es8323_reg_write(pDevice, ES8323_ADCCONTROL12, 0x05); //decay / attack time
	if (!NT_SUCCESS(status)) {
		return status;
	}
	status = es8323_reg_write(pDevice, ES8323_ADCCONTROL14, 0x4b); //threshold / ng switch
	if (!NT_SUCCESS(status)) {
		return status;
	}

	status = es8323_reg_write(pDevice, ES8323_DACCONTROL7, 0x02); //dac stereo 3d
	if (!NT_SUCCESS(status)) {
		return status;
	}

	//Output playback
	status = es8323_reg_write(pDevice, ES8323_DACCONTROL16, 0x02); //maybe not needed?
	if (!NT_SUCCESS(status)) {
		return status;
	}

	status = es8323_reg_write(pDevice, ES8323_DACCONTROL24, 0x21); //output 1 volume
	if (!NT_SUCCESS(status)) {
		return status;
	}
	status = es8323_reg_write(pDevice, ES8323_DACCONTROL25, 0x21); //output 1 volume
	if (!NT_SUCCESS(status)) {
		return status;
	}

	status = es8323_reg_write(pDevice, ES8323_DACCONTROL26, 0x21); //output 2 volume
	if (!NT_SUCCESS(status)) {
		return status;
	}
	status = es8323_reg_write(pDevice, ES8323_DACCONTROL27, 0x21); //output 2 volume
	if (!NT_SUCCESS(status)) {
		return status;
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
	PES8323_CONTEXT pDevice = GetDeviceContext(FxDevice);
	BOOLEAN fSpbResourceFound = FALSE;
	NTSTATUS status = STATUS_INSUFFICIENT_RESOURCES;

	UNREFERENCED_PARAMETER(FxResourcesRaw);

	//
	// Parse the peripheral's resources.
	//

	ULONG resourceCount = WdfCmResourceListGetCount(FxResourcesTranslated);

	for (ULONG i = 0; i < resourceCount; i++)
	{
		PCM_PARTIAL_RESOURCE_DESCRIPTOR pDescriptor;
		UCHAR Class;
		UCHAR Type;

		pDescriptor = WdfCmResourceListGetDescriptor(
			FxResourcesTranslated, i);

		switch (pDescriptor->Type)
		{
		case CmResourceTypeConnection:
			//
			// Look for I2C or SPI resource and save connection ID.
			//
			Class = pDescriptor->u.Connection.Class;
			Type = pDescriptor->u.Connection.Type;
			if (Class == CM_RESOURCE_CONNECTION_CLASS_SERIAL &&
				Type == CM_RESOURCE_CONNECTION_TYPE_SERIAL_I2C)
			{
				if (fSpbResourceFound == FALSE)
				{
					status = STATUS_SUCCESS;
					pDevice->I2CContext.I2cResHubId.LowPart = pDescriptor->u.Connection.IdLowPart;
					pDevice->I2CContext.I2cResHubId.HighPart = pDescriptor->u.Connection.IdHighPart;
					fSpbResourceFound = TRUE;
				}
				else
				{
				}
			}
			break;
		default:
			//
			// Ignoring all other resource types.
			//
			break;
		}
	}

	//
	// An SPB resource is required.
	//

	if (fSpbResourceFound == FALSE)
	{
		status = STATUS_NOT_FOUND;
		return status;
	}

	status = SpbTargetInitialize(FxDevice, &pDevice->I2CContext);
	if (!NT_SUCCESS(status))
	{
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
	PES8323_CONTEXT pDevice = GetDeviceContext(FxDevice);
	NTSTATUS status = STATUS_SUCCESS;

	UNREFERENCED_PARAMETER(FxResourcesTranslated);

	SpbTargetDeinitialize(FxDevice, &pDevice->I2CContext);

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

	PES8323_CONTEXT pDevice = GetDeviceContext(FxDevice);
	NTSTATUS status = STATUS_SUCCESS;

	BOOTCODEC(pDevice);

	return status;
}

NTSTATUS
OnD0Exit(
_In_  WDFDEVICE               FxDevice,
_In_  WDF_POWER_DEVICE_STATE  FxPreviousState
)
/*++

Routine Description:

This routine destroys objects needed by the driver.

Arguments:

FxDevice - a handle to the framework device object
FxPreviousState - previous power state

Return Value:

Status

--*/
{
	UNREFERENCED_PARAMETER(FxPreviousState);

	PES8323_CONTEXT pDevice = GetDeviceContext(FxDevice);

	es8323_reg_write(pDevice, ES8323_DACCONTROL3, 0x06);
	es8323_reg_write(pDevice, ES8323_DACCONTROL26, 0x00);
	es8323_reg_write(pDevice, ES8323_DACCONTROL27, 0x00);
	es8323_reg_write(pDevice, ES8323_ADCPOWER, 0xFF);
	es8323_reg_write(pDevice, ES8323_DACPOWER, 0xc0);
	es8323_reg_write(pDevice, ES8323_CHIPPOWER, 0xF3);
	es8323_reg_write(pDevice, ES8323_CONTROL1, 0x00);
	es8323_reg_write(pDevice, ES8323_CONTROL2, 0x58);
	es8323_reg_write(pDevice, ES8323_DACCONTROL21, 0x9c);

	pDevice->ConnectInterrupt = false;

	return STATUS_SUCCESS;
}

BOOLEAN OnInterruptIsr(
	WDFINTERRUPT Interrupt,
	ULONG MessageID) {
	UNREFERENCED_PARAMETER(Interrupt);
	UNREFERENCED_PARAMETER(MessageID);

	//DbgPrint("Jack Interrupt!\n");
	return TRUE;
}

NTSTATUS
Es8323EvtDeviceAdd(
IN WDFDRIVER       Driver,
IN PWDFDEVICE_INIT DeviceInit
)
{
	NTSTATUS                      status = STATUS_SUCCESS;
	WDF_IO_QUEUE_CONFIG           queueConfig;
	WDF_OBJECT_ATTRIBUTES         attributes;
	WDFDEVICE                     device;
	WDF_INTERRUPT_CONFIG interruptConfig;
	WDFQUEUE                      queue;
	UCHAR                         minorFunction;
	PES8323_CONTEXT               devContext;

	UNREFERENCED_PARAMETER(Driver);

	PAGED_CODE();

	Es8323Print(DEBUG_LEVEL_INFO, DBG_PNP,
		"Es8323EvtDeviceAdd called\n");

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
	// Because we are a virtual device the root enumerator would just put null values 
	// in response to IRP_MN_QUERY_ID. Lets override that.
	//

	minorFunction = IRP_MN_QUERY_ID;

	status = WdfDeviceInitAssignWdmIrpPreprocessCallback(
		DeviceInit,
		Es8323EvtWdmPreprocessMnQueryId,
		IRP_MJ_PNP,
		&minorFunction,
		1
		);
	if (!NT_SUCCESS(status))
	{
		Es8323Print(DEBUG_LEVEL_ERROR, DBG_PNP,
			"WdfDeviceInitAssignWdmIrpPreprocessCallback failed Status 0x%x\n", status);

		return status;
	}

	//
	// Setup the device context
	//

	WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, ES8323_CONTEXT);

	//
	// Create a framework device object.This call will in turn create
	// a WDM device object, attach to the lower stack, and set the
	// appropriate flags and attributes.
	//

	status = WdfDeviceCreate(&DeviceInit, &attributes, &device);

	if (!NT_SUCCESS(status))
	{
		Es8323Print(DEBUG_LEVEL_ERROR, DBG_PNP,
			"WdfDeviceCreate failed with status code 0x%x\n", status);

		return status;
	}

	WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&queueConfig, WdfIoQueueDispatchParallel);

	queueConfig.EvtIoInternalDeviceControl = Es8323EvtInternalDeviceControl;

	status = WdfIoQueueCreate(device,
		&queueConfig,
		WDF_NO_OBJECT_ATTRIBUTES,
		&queue
		);

	if (!NT_SUCCESS(status))
	{
		Es8323Print(DEBUG_LEVEL_ERROR, DBG_PNP,
			"WdfIoQueueCreate failed 0x%x\n", status);

		return status;
	}

	//
	// Create manual I/O queue to take care of hid report read requests
	//

	devContext = GetDeviceContext(device);

	WDF_IO_QUEUE_CONFIG_INIT(&queueConfig, WdfIoQueueDispatchManual);

	queueConfig.PowerManaged = WdfFalse;

	status = WdfIoQueueCreate(device,
		&queueConfig,
		WDF_NO_OBJECT_ATTRIBUTES,
		&devContext->ReportQueue
		);

	if (!NT_SUCCESS(status))
	{
		Es8323Print(DEBUG_LEVEL_ERROR, DBG_PNP,
			"WdfIoQueueCreate failed 0x%x\n", status);

		return status;
	}

	//
	// Create an interrupt object for hardware notifications
	//
	WDF_INTERRUPT_CONFIG_INIT(
		&interruptConfig,
		OnInterruptIsr,
		NULL);
	interruptConfig.PassiveHandling = TRUE;

	status = WdfInterruptCreate(
		device,
		&interruptConfig,
		WDF_NO_OBJECT_ATTRIBUTES,
		&devContext->Interrupt);

	if (!NT_SUCCESS(status))
	{
		Es8323Print(DEBUG_LEVEL_ERROR, DBG_PNP,
			"Error creating WDF interrupt object - %!STATUS!",
			status);

		return status;
	}

	devContext->FxDevice = device;

	return status;
}

NTSTATUS
Es8323EvtWdmPreprocessMnQueryId(
WDFDEVICE Device,
PIRP Irp
)
{
	NTSTATUS            status;
	PIO_STACK_LOCATION  IrpStack, previousSp;
	PDEVICE_OBJECT      DeviceObject;
	PWCHAR              buffer;

	PAGED_CODE();

	//
	// Get a pointer to the current location in the Irp
	//

	IrpStack = IoGetCurrentIrpStackLocation(Irp);

	//
	// Get the device object
	//
	DeviceObject = WdfDeviceWdmGetDeviceObject(Device);


	Es8323Print(DEBUG_LEVEL_VERBOSE, DBG_PNP,
		"Es8323EvtWdmPreprocessMnQueryId Entry\n");

	//
	// This check is required to filter out QUERY_IDs forwarded
	// by the HIDCLASS for the parent FDO. These IDs are sent
	// by PNP manager for the parent FDO if you root-enumerate this driver.
	//
	previousSp = ((PIO_STACK_LOCATION)((UCHAR *)(IrpStack)+
		sizeof(IO_STACK_LOCATION)));

	if (previousSp->DeviceObject == DeviceObject)
	{
		//
		// Filtering out this basically prevents the Found New Hardware
		// popup for the root-enumerated Es8323 on reboot.
		//
		status = Irp->IoStatus.Status;
	}
	else
	{
		switch (IrpStack->Parameters.QueryId.IdType)
		{
		case BusQueryDeviceID:
		case BusQueryHardwareIDs:
			//
			// HIDClass is asking for child deviceid & hardwareids.
			// Let us just make up some id for our child device.
			//
			buffer = (PWCHAR)ExAllocatePoolZero(
				NonPagedPool,
				ES8323_HARDWARE_IDS_LENGTH,
				ES8323_POOL_TAG
				);

			if (buffer)
			{
				//
				// Do the copy, store the buffer in the Irp
				//
				RtlCopyMemory(buffer,
					ES8323_HARDWARE_IDS,
					ES8323_HARDWARE_IDS_LENGTH
					);

				Irp->IoStatus.Information = (ULONG_PTR)buffer;
				status = STATUS_SUCCESS;
			}
			else
			{
				//
				//  No memory
				//
				status = STATUS_INSUFFICIENT_RESOURCES;
			}

			Irp->IoStatus.Status = status;
			//
			// We don't need to forward this to our bus. This query
			// is for our child so we should complete it right here.
			// fallthru.
			//
			IoCompleteRequest(Irp, IO_NO_INCREMENT);

			break;

		default:
			status = Irp->IoStatus.Status;
			IoCompleteRequest(Irp, IO_NO_INCREMENT);
			break;
		}
	}

	Es8323Print(DEBUG_LEVEL_VERBOSE, DBG_IOCTL,
		"Es8323EvtWdmPreprocessMnQueryId Exit = 0x%x\n", status);

	return status;
}

VOID
Es8323EvtInternalDeviceControl(
IN WDFQUEUE     Queue,
IN WDFREQUEST   Request,
IN size_t       OutputBufferLength,
IN size_t       InputBufferLength,
IN ULONG        IoControlCode
)
{
	NTSTATUS            status = STATUS_SUCCESS;
	WDFDEVICE           device;
	PES8323_CONTEXT     devContext;

	UNREFERENCED_PARAMETER(OutputBufferLength);
	UNREFERENCED_PARAMETER(InputBufferLength);

	device = WdfIoQueueGetDevice(Queue);
	devContext = GetDeviceContext(device);

	switch (IoControlCode)
	{
	default:
		status = STATUS_NOT_SUPPORTED;
		break;
	}

	WdfRequestComplete(Request, status);

	return;
}
