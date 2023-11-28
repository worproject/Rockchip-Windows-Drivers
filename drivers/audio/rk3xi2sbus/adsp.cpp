#include "driver.h"
#define ADSP_DECL 1
#include "adsp.h"

NTSTATUS ADSPGetResources(_In_ PVOID _context, _Out_ _PCI_BAR* acpBar, PTPLG_INFO tplgInfo) {
	if (!_context)
		return STATUS_NO_SUCH_DEVICE;

	PPDO_DEVICE_DATA devData = (PPDO_DEVICE_DATA)_context;
	if (!devData->FdoContext) {
		return STATUS_NO_SUCH_DEVICE;
	}

	if (acpBar) {
		*acpBar = devData->FdoContext->m_MMIO;
	}

	if (tplgInfo) {
		if (devData->FdoContext->rkTplg) {
			tplgInfo->rkTplg = devData->FdoContext->rkTplg;
			tplgInfo->rkTplgSz = devData->FdoContext->rkTplgSz;
		}
	}

	return STATUS_SUCCESS;
}

NTSTATUS ADSPSetPowerState(_In_ PVOID _context, _In_ DEVICE_POWER_STATE powerState) {
	if (!_context)
		return STATUS_NO_SUCH_DEVICE;

	PPDO_DEVICE_DATA devData = (PPDO_DEVICE_DATA)_context;
	if (!devData->FdoContext) {
		return STATUS_NO_SUCH_DEVICE;
	}

	NTSTATUS status = STATUS_SUCCESS;
	if (powerState == PowerDeviceD3) {
		WdfDeviceResumeIdle(devData->FdoContext->WdfDevice);
	} else if (powerState == PowerDeviceD0) {
		status = WdfDeviceStopIdle(devData->FdoContext->WdfDevice, TRUE);
	}
	return status;
}

NTSTATUS ADSPRegisterInterrupt(_In_ PVOID _context, _In_ PADSP_INTERRUPT_CALLBACK callback, _In_ PADSP_DPC_CALLBACK dpcCallback, _In_ PVOID callbackContext) {
	if (!_context)
		return STATUS_NO_SUCH_DEVICE;

	PPDO_DEVICE_DATA devData = (PPDO_DEVICE_DATA)_context;
	if (!devData->FdoContext) {
		return STATUS_NO_SUCH_DEVICE;
	}

	devData->FdoContext->dspInterruptCallback = callback;
	devData->FdoContext->dspDPCCallback = dpcCallback;
	devData->FdoContext->dspInterruptContext = callbackContext;

	return STATUS_SUCCESS;
}

NTSTATUS ADSPUnregisterInterrupt(_In_ PVOID _context) {
	if (!_context)
		return STATUS_NO_SUCH_DEVICE;

	PPDO_DEVICE_DATA devData = (PPDO_DEVICE_DATA)_context;
	if (!devData->FdoContext) {
		return STATUS_NO_SUCH_DEVICE;
	}

	devData->FdoContext->dspInterruptCallback = NULL;
	devData->FdoContext->dspInterruptContext = NULL;
	return STATUS_SUCCESS;
}

NTSTATUS ADSPQueueDpcForInterrupt(_In_ PVOID _context) {
	if (!_context)
		return STATUS_NO_SUCH_DEVICE;

	PPDO_DEVICE_DATA devData = (PPDO_DEVICE_DATA)_context;
	if (!devData->FdoContext) {
		return STATUS_NO_SUCH_DEVICE;
	}

	BOOL ret = WdfInterruptQueueDpcForIsr(devData->FdoContext->Interrupt);
	return ret ? STATUS_SUCCESS : STATUS_UNSUCCESSFUL;
}

RKDSP_BUS_INTERFACE RKDSP_BusInterface(PVOID Context) {
	RKDSP_BUS_INTERFACE busInterface;
	RtlZeroMemory(&busInterface, sizeof(RKDSP_BUS_INTERFACE));

	busInterface.Size = sizeof(RKDSP_BUS_INTERFACE);
	busInterface.Version = 1;
	busInterface.Context = Context;
	busInterface.InterfaceReference = WdfDeviceInterfaceReferenceNoOp;
	busInterface.InterfaceDereference = WdfDeviceInterfaceDereferenceNoOp;
	busInterface.GetResources = ADSPGetResources;
	busInterface.SetDSPPowerState = ADSPSetPowerState;
	busInterface.RegisterInterrupt = ADSPRegisterInterrupt;
	busInterface.UnregisterInterrupt = ADSPUnregisterInterrupt;
	busInterface.QueueDPCForInterrupt = ADSPQueueDpcForInterrupt;

	return busInterface;
}