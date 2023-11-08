#include "driver.h"

static ULONG Pl330DmaDebugLevel = 100;
static ULONG Pl330DmaDebugCatagories = DBG_INIT || DBG_PNP || DBG_IOCTL;

static void ResetThread(PPL330DMA_THREAD Thread) {
	PPL330DMA_CONTEXT pDevice = Thread->Device;

	Thread->Req[0].MCcpu = pDevice->MCodeCPU + (Thread->Id * pDevice->MCBufSz);
	Thread->Req[0].MCbus = MmGetPhysicalAddress(Thread->Req[0].MCcpu);
	RtlZeroMemory(&Thread->Req[0].Xfer, sizeof(XFER_SPEC));

	/*Thread->Req[1].MCcpu = Thread->Req[0].MCcpu + pDevice->MCBufSz / 2;
	Thread->Req[1].MCbus = MmGetPhysicalAddress(Thread->Req[1].MCcpu);
	Thread->Req[1].Xfer = NULL;*/

	for (int j = 0; j < MAX_NOTIF_EVENTS; j++) {
		Thread->registeredCallbacks[j].InUse = FALSE;
	}

	Thread->ReqRunning = FALSE;
}

static NTSTATUS AllocThreads(PPL330DMA_CONTEXT pDevice) {
	UINT8 chans = (UINT8)pDevice->Config.NumChan;

	pDevice->Channels = ExAllocatePoolZero(NonPagedPool, (1 + chans) * sizeof(PL330DMA_THREAD), PL330DMA_POOL_TAG);
	if (!pDevice->Channels) {
		return STATUS_NO_MEMORY;
	}

	//Init channel threads
	for (UINT8 i = 0; i < chans; i++) {
		PPL330DMA_THREAD Thread = &pDevice->Channels[i];
		Thread->Id = i;
		Thread->Device = pDevice;
		Thread->ev = i;
		ResetThread(Thread);

		Thread->Free = TRUE;
	}

	/* Manager is indexed at the end */
	PPL330DMA_THREAD Thread = &pDevice->Channels[chans];
	Thread->Id = chans;
	Thread->Device = pDevice;
	Thread->Free = FALSE;
	pDevice->Manager = Thread;
	return STATUS_SUCCESS;
}

NTSTATUS AllocResources(PPL330DMA_CONTEXT pDevice) {
	int chans = pDevice->Config.NumChan;

	//Use default MC Buffer Size
	pDevice->MCBufSz = MCODE_BUFF_PER_REQ * 2;

	//Allocate MicroCode buffer for 'chans' Channel threads

	PHYSICAL_ADDRESS ZeroAddr = { 0 };

	PHYSICAL_ADDRESS HighestAddr;
	HighestAddr.QuadPart = MAXUINT32;
	pDevice->MCodeCPU = MmAllocateContiguousMemorySpecifyCache(chans * pDevice->MCBufSz, ZeroAddr, HighestAddr, ZeroAddr, MmNonCached);
	if (!pDevice->MCodeCPU) {
		return STATUS_NO_MEMORY;
	}

	NTSTATUS status = AllocThreads(pDevice);
	if (!NT_SUCCESS(status)) {
		return status;
	}

	return STATUS_SUCCESS;
}

void FreeResources(PPL330DMA_CONTEXT pDevice) {
	if (pDevice->Channels) {
		//TODO: Release Channel Threads

		for (UINT32 i = 0; i < pDevice->Config.NumChan; i++) {
			_stop(pDevice->Channels);
		}

		//Free Channel Memory
		ExFreePoolWithTag(pDevice->Channels, PL330DMA_POOL_TAG);
	}

	//Free MCode
	if (pDevice->MCodeCPU)
		MmFreeContiguousMemory(pDevice->MCodeCPU);
}

PPL330DMA_THREAD GetHandle(PPL330DMA_CONTEXT pDevice, int Idx) {
	if (!pDevice)
		return NULL;
	for (UINT32 i = 0; i < pDevice->Config.NumChan; i++) {
		if (!pDevice->Channels[i].Free) {
			continue;
		}
		if (i == (UINT32)Idx || Idx == -1) {
			Pl330DmaPrint(DEBUG_LEVEL_ERROR, DBG_IOCTL, "Setting Channel 0x%llx to be used\n", &pDevice->Channels[i]);
			pDevice->Channels[i].Free = FALSE;
			return &pDevice->Channels[i];
		}
	}
	return NULL;
}

void StopThread(PPL330DMA_CONTEXT pDevice, PPL330DMA_THREAD Thread) {
	UNREFERENCED_PARAMETER(pDevice);
	if (!Thread)
		return;
	_stop(Thread);
}

void GetThreadRegisters(PPL330DMA_CONTEXT pDevice, PPL330DMA_THREAD Thread, UINT32* cpc, UINT32* sa, UINT32* da) {
	if (!Thread)
		return;
	if (cpc) {
		*cpc = read32(pDevice, CPC(Thread->Id));
	}
	if (sa) {
		*sa = read32(pDevice, SA(Thread->Id));
	}
	if (da) {
		*da = read32(pDevice, DA(Thread->Id));
	}
}

BOOLEAN FreeHandle(PPL330DMA_CONTEXT pDevice, PPL330DMA_THREAD Thread) {
	UNREFERENCED_PARAMETER(pDevice);
	if (!Thread)
		return FALSE;
	if (Thread->ReqRunning) {
		return FALSE;
	}
	Thread->Free = TRUE;
	return TRUE;
}

NTSTATUS RegisterNotificationCallback(
	PPL330DMA_CONTEXT pDevice,
	PPL330DMA_THREAD Thread,
	PDEVICE_OBJECT Fdo,
	PDMA_NOTIFICATION_CALLBACK NotificationCallback,
	PVOID CallbackContext) {
	UNREFERENCED_PARAMETER(pDevice);
	if (!Thread)
		return STATUS_INVALID_PARAMETER;

	BOOLEAN registered = FALSE;

	for (int i = 0; i < MAX_NOTIF_EVENTS; i++) {
		if (Thread->registeredCallbacks[i].InUse)
			continue;
		Thread->registeredCallbacks[i].InUse = TRUE;
		Thread->registeredCallbacks[i].Fdo = Fdo;
		Thread->registeredCallbacks[i].NotificationCallback = NotificationCallback;
		Thread->registeredCallbacks[i].CallbackContext = CallbackContext;

		InterlockedIncrement(&Fdo->ReferenceCount);

		registered = TRUE;
		break;
	}

	return registered ? STATUS_SUCCESS : STATUS_INSUFFICIENT_RESOURCES;
}

NTSTATUS UnregisterNotificationCallback(
	PPL330DMA_CONTEXT pDevice,
	PPL330DMA_THREAD Thread,
	PDMA_NOTIFICATION_CALLBACK NotificationCallback,
	PVOID CallbackContext) {
	UNREFERENCED_PARAMETER(pDevice);
	if (!Thread)
		return STATUS_INVALID_PARAMETER;

	BOOLEAN registered = FALSE;

	for (int i = 0; i < MAX_NOTIF_EVENTS; i++) {
		if (Thread->registeredCallbacks[i].InUse &&
			Thread->registeredCallbacks[i].NotificationCallback != NotificationCallback &&
			Thread->registeredCallbacks[i].CallbackContext != CallbackContext)
			continue;

		InterlockedDecrement(&Thread->registeredCallbacks[i].Fdo->ReferenceCount);

		RtlZeroMemory(&Thread->registeredCallbacks[i], sizeof(Thread->registeredCallbacks[i]));
		registered = TRUE;
		break;
	}

	return registered ? STATUS_SUCCESS : STATUS_INVALID_PARAMETER;
}