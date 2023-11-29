#include "driver.h"

static ULONG Pl330DmaDebugLevel = 100;
static ULONG Pl330DmaDebugCatagories = DBG_INIT || DBG_PNP || DBG_IOCTL;

void ReadDmacConfiguration(PPL330DMA_CONTEXT pDevice) {
	const UINT32 crd = read32(pDevice, CRD);
	const UINT32 cr0 = read32(pDevice, CR0);
	UINT32 val;

	val = crd >> CRD_DATA_WIDTH_SHIFT;
	val &= CRD_DATA_WIDTH_MASK;
	pDevice->Config.DataBusWidth = 8 * (1 << val);

	val = crd >> CRD_DATA_BUFF_SHIFT;
	val &= CRD_DATA_BUFF_MASK;
	pDevice->Config.DataBufDepth = val + 1;

	val = crd >> CR0_NUM_CHANS_SHIFT;
	val &= CR0_NUM_CHANS_MASK;
	val += 1;
	pDevice->Config.NumChan = val;

	val = cr0;
	if (val & CR0_PERIPH_REQ_SET) {
		val = (val >> CR0_NUM_PERIPH_SHIFT) & CR0_NUM_PERIPH_MASK;
		val += 1;
		pDevice->Config.NumPeripherals = val;
		pDevice->Config.PeripheralNS = read32(pDevice, CR4);
	}
	else {
		pDevice->Config.NumPeripherals = 0;
	}

	val = cr0;
	if (val & CR0_BOOT_MAN_NS)
		pDevice->Config.Mode |= DMAC_MODE_NS;
	else
		pDevice->Config.Mode &= ~DMAC_MODE_NS;

	val = cr0 >> CR0_NUM_EVENTS_SHIFT;
	val &= CR0_NUM_EVENTS_MASK;
	val += 1;
	pDevice->Config.NumEvents = val;

	pDevice->Config.IrqNS = read32(pDevice, CR3);

	Pl330DmaPrint(DEBUG_LEVEL_INFO, DBG_INIT,
		"%s Data Width: %d, Channels: %d\n", __func__, pDevice->Config.DataBusWidth, pDevice->Config.NumChan);
}

BOOLEAN pl330_dma_irq(WDFINTERRUPT Interrupt, ULONG MessageID) {
	UNREFERENCED_PARAMETER(MessageID);
	Pl330DmaPrint(DEBUG_LEVEL_INFO, DBG_INIT,
		"%s %d\n", __func__, MessageID);

	WDFDEVICE FxDevice = WdfInterruptGetDevice(Interrupt);
	PPL330DMA_CONTEXT pDevice = GetDeviceContext(FxDevice);

	UINT32 val = read32(pDevice, FSM) + 0x1;
	Pl330DmaPrint(DEBUG_LEVEL_ERROR, DBG_IOCTL, "Reset Manager? %d\n", val);

	val = read32(pDevice, FSC) & ((1 << pDevice->Config.NumChan) - 1);
	Pl330DmaPrint(DEBUG_LEVEL_ERROR, DBG_IOCTL, "Reset Channels: %x\n", val);

	BOOLEAN queueDPC = FALSE;

	val = read32(pDevice, ES);
	pDevice->irqLastEvents = val;
	for (UINT32 ev = 0; ev < pDevice->Config.NumEvents; ev++) {
		if (val & (1 << ev)) { /* Event occurred */
			Pl330DmaPrint(DEBUG_LEVEL_ERROR, DBG_IOCTL, "Event on channel %d\n", ev);

			UINT32 intEn = read32(pDevice, INTEN);
			if (intEn & (1 << ev)) {
				write32(pDevice, INTCLR, (1 << ev)); //Clear the event

				queueDPC = TRUE;
			}
		}
	}

	if (queueDPC) {
		WdfInterruptQueueDpcForIsr(Interrupt);
	}

	return TRUE;
}

void pl330_dma_dpc(WDFINTERRUPT Interrupt, WDFOBJECT AssociatedObject) {
	UNREFERENCED_PARAMETER(AssociatedObject);

	WDFDEVICE Device = WdfInterruptGetDevice(Interrupt);
	PPL330DMA_CONTEXT pDevice = GetDeviceContext(Device);

	UINT32 val = pDevice->irqLastEvents;
	for (UINT32 ev = 0; ev < pDevice->Config.NumEvents; ev++) {
		if (val & (1 << ev)) { /* Event occurred */
			PPL330DMA_THREAD Thread = &pDevice->Channels[ev];
			for (int regEv = 0; regEv < MAX_NOTIF_EVENTS; regEv++) {
				if (Thread->registeredCallbacks[regEv].InUse) {
					Thread->registeredCallbacks[regEv].NotificationCallback(Thread->registeredCallbacks[regEv].CallbackContext);
				}
			}
		}
	}
}

/* Returns Time-Out */
static NTSTATUS UntilDmacIdle(PPL330DMA_THREAD Thread)
{
	PPL330DMA_CONTEXT pDevice = Thread->Device;
	UINT32 timeout_us = 5 * 1000; // 5 ms

	LARGE_INTEGER StartTime;
	KeQuerySystemTimePrecise(&StartTime);

	for (;;) {
		/* Until Manager is Idle */
		if (!(read32(pDevice, DBGSTATUS) & DBG_BUSY))
			return STATUS_SUCCESS;

		YieldProcessor();

		LARGE_INTEGER CurrentTime;
		KeQuerySystemTimePrecise(&CurrentTime);

		if (((CurrentTime.QuadPart - StartTime.QuadPart) / 10) > timeout_us) {
			return STATUS_IO_TIMEOUT;
		}
	}
}

static inline void _execute_DBGINSN(
	PPL330DMA_THREAD Thread,
	UINT8 insn[],
	BOOLEAN AsManager
) {
	PPL330DMA_CONTEXT pDevice = Thread->Device;
	UINT32 val;

	Pl330DmaPrint(DEBUG_LEVEL_INFO, DBG_INIT,
		"%s: entry\n", __func__);

	if (!NT_SUCCESS(UntilDmacIdle(Thread))) {
		Pl330DmaPrint(DEBUG_LEVEL_ERROR, DBG_INIT,
			"DMAC halted!\n");
		return;
	}

	val = (insn[0] << 16) | (insn[1] << 24);
	if (!AsManager) {
		val |= (1 << 0);
		val |= (Thread->Id << 8); /* Channel Number */
	}
	write32(pDevice, DBGINST0, val);

	val = *((UINT32*)&insn[2]);
	write32(pDevice, DBGINST1, val);

	/* Get going */
	write32(pDevice, DBGCMD, 0);
}

static inline BOOLEAN IsManager(PPL330DMA_THREAD Thread)
{
	return Thread->Device->Manager == Thread;
}

/* If manager of the thread is in Non-Secure mode */
static inline BOOLEAN _manager_ns(PPL330DMA_THREAD Thread)
{
	return (Thread->Device->Config.Mode & DMAC_MODE_NS) ? TRUE : FALSE;
}

static inline UINT32 GetRevision(UINT32 periph_id)
{
	return (periph_id >> PERIPH_REV_SHIFT) & PERIPH_REV_MASK;
}

static inline UINT32 _state(PPL330DMA_THREAD Thread)
{
	PPL330DMA_CONTEXT pDevice = Thread->Device;
	UINT32 val;

	if (IsManager(Thread))
		val = read32(pDevice, DS) & 0xf;
	else
		val = read32(pDevice, CS(Thread->Id)) & 0xf;

	switch (val) {
	case DS_ST_STOP:
		return PL330_STATE_STOPPED;
	case DS_ST_EXEC:
		return PL330_STATE_EXECUTING;
	case DS_ST_CMISS:
		return PL330_STATE_CACHEMISS;
	case DS_ST_UPDTPC:
		return PL330_STATE_UPDTPC;
	case DS_ST_WFE:
		return PL330_STATE_WFE;
	case DS_ST_FAULT:
		return PL330_STATE_FAULTING;
	case DS_ST_ATBRR:
		if (IsManager(Thread))
			return PL330_STATE_INVALID;
		else
			return PL330_STATE_ATBARRIER;
	case DS_ST_QBUSY:
		if (IsManager(Thread))
			return PL330_STATE_INVALID;
		else
			return PL330_STATE_QUEUEBUSY;
	case DS_ST_WFP:
		if (IsManager(Thread))
			return PL330_STATE_INVALID;
		else
			return PL330_STATE_WFP;
	case DS_ST_KILL:
		if (IsManager(Thread))
			return PL330_STATE_INVALID;
		else
			return PL330_STATE_KILLING;
	case DS_ST_CMPLT:
		if (IsManager(Thread))
			return PL330_STATE_INVALID;
		else
			return PL330_STATE_COMPLETING;
	case DS_ST_FLTCMP:
		if (IsManager(Thread))
			return PL330_STATE_INVALID;
		else
			return PL330_STATE_FAULT_COMPLETING;
	default:
		return PL330_STATE_INVALID;
	}
}

void _stop(PPL330DMA_THREAD Thread)
{
	PPL330DMA_CONTEXT pDevice = Thread->Device;
	UINT8 insn[6] = { 0, 0, 0, 0, 0, 0 };
	UINT32 inten = read32(pDevice, INTEN);

	if (_state(Thread) == PL330_STATE_FAULT_COMPLETING)
		UNTIL(Thread, PL330_STATE_FAULTING | PL330_STATE_KILLING);

	/* Return if nothing needs to be done */
	if (_state(Thread) == PL330_STATE_COMPLETING
		|| _state(Thread) == PL330_STATE_KILLING
		|| _state(Thread) == PL330_STATE_STOPPED)
		return;

	_emit_KILL(0, insn);

	_execute_DBGINSN(Thread, insn, IsManager(Thread));

	/* clear the event */
	if (inten & (1 << Thread->ev))
		write32(pDevice, INTCLR, 1 << Thread->ev);
	/* Stop generating interrupts for SEV */
	write32(pDevice, INTEN, inten & ~(1 << Thread->ev));

	Thread->ReqRunning = FALSE;
}

/* Start doing req 'idx' of thread 'Thread' */
static BOOLEAN _trigger(PPL330DMA_THREAD Thread)
{
	PPL330DMA_CONTEXT pDevice = Thread->Device;
	PPL330DMA_REQ pReq;
	struct _arg_GO go;
	unsigned ns;
	UINT8 insn[6] = { 0, 0, 0, 0, 0, 0 };

	/* Return if already ACTIVE */
	if (_state(Thread) != PL330_STATE_STOPPED)
		return TRUE;

	pReq = &Thread->Req[0];

	/* Return if req is running */
	if (Thread->ReqRunning)
		return TRUE;

	ns = 1; //Always Non-Secure

	/* See 'Abort Sources' point-4 at Page 2-25 */
	if (_manager_ns(Thread) && !ns)
		Pl330DmaPrint(DEBUG_LEVEL_INFO, DBG_IOCTL,
			"%s:%d Recipe for ABORT!\n",
			__func__, __LINE__);

	go.chan = Thread->Id;
	go.addr = pReq->MCbus.LowPart;
	go.ns = ns;
	Pl330DmaPrint(DEBUG_LEVEL_INFO, DBG_IOCTL,
		"%s: GO id %d, Addr 0x%x, NS %d\n", __func__, go.chan, go.addr, go.ns);
	_emit_GO(0, insn, &go);

	/* Set to generate interrupts for SEV */
	write32(pDevice, INTEN, read32(pDevice, INTEN) | (1 << Thread->ev));

	/* Only manager can execute GO */
	_execute_DBGINSN(Thread, insn, TRUE);

	Thread->ReqRunning = TRUE;

	return TRUE;
}

BOOLEAN _start(PPL330DMA_THREAD Thread)
{
	switch (_state(Thread)) {
	case PL330_STATE_FAULT_COMPLETING:
		UNTIL(Thread, PL330_STATE_FAULTING | PL330_STATE_KILLING);

		if (_state(Thread) == PL330_STATE_KILLING)
			UNTIL(Thread, PL330_STATE_STOPPED)

	case PL330_STATE_FAULTING:
		_stop(Thread);

	case PL330_STATE_KILLING:
	case PL330_STATE_COMPLETING:
		UNTIL(Thread, PL330_STATE_STOPPED)

	case PL330_STATE_STOPPED:
		return _trigger(Thread);

	case PL330_STATE_WFP:
	case PL330_STATE_QUEUEBUSY:
	case PL330_STATE_ATBARRIER:
	case PL330_STATE_UPDTPC:
	case PL330_STATE_CACHEMISS:
	case PL330_STATE_EXECUTING:
		return TRUE;

	case PL330_STATE_WFE: /* For RESUME, nothing yet */
	default:
		return FALSE;
	}
}