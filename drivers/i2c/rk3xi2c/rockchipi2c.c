#include "driver.h"

static ULONG Rk3xI2CDebugLevel = 100;
static ULONG Rk3xI2CDebugCatagories = DBG_INIT || DBG_PNP || DBG_IOCTL;

/* Reset all interrupt pending bits */
static inline void rk3x_i2c_clean_ipd(PRK3XI2C_CONTEXT pDevice)
{
	write32(pDevice, REG_IPD, REG_INT_ALL);
}

/**
 * rk3x_i2c_start - Generate a START condition, which triggers a REG_INT_START interrupt.
 */
static void rk3x_i2c_start(PRK3XI2C_CONTEXT pDevice)
{
	UINT32 val = read32(pDevice, REG_CON) & REG_CON_TUNING_MASK;

	write32(pDevice, REG_IEN, REG_INT_START);

	/* enable adapter with correct mode, send START condition */
	val |= REG_CON_EN | REG_CON_MOD(pDevice->mode) | REG_CON_START;

	/* if we want to react to NACK, set ACTACK bit */
	val |= REG_CON_ACTACK;

	write32(pDevice, REG_CON, val);
}

/**
 * rk3x_i2c_stop - Generate a STOP condition, which triggers a REG_INT_STOP interrupt.
 * @error: Error code to return in rk3x_i2c_xfer
 */
static void rk3x_i2c_stop(PRK3XI2C_CONTEXT pDevice, NTSTATUS status)
{
	unsigned int ctrl;

	pDevice->processed = 0;
	pDevice->currentMDLChain = NULL;
	RtlZeroBytes(&pDevice->currentDescriptor, sizeof(pDevice->currentDescriptor));
	pDevice->transactionStatus = status;

	if (pDevice->isLastMsg) {
		/* Enable stop interrupt */
		write32(pDevice, REG_IEN, REG_INT_STOP);

		pDevice->state = STATE_STOP;

		ctrl = read32(pDevice, REG_CON);
		ctrl |= REG_CON_STOP;
		write32(pDevice, REG_CON, ctrl);
	}
	else {
		/* Signal rk3x_i2c_xfer to start the next message. */
		pDevice->isBusy = FALSE;
		pDevice->state = STATE_IDLE;

		/*
		 * The HW is actually not capable of REPEATED START. But we can
		 * get the intended effect by resetting its internal state
		 * and issuing an ordinary START.
		 */
		ctrl = read32(pDevice, REG_CON) & REG_CON_TUNING_MASK;
		write32(pDevice, REG_CON, ctrl);

		/* signal that we are finished with the current msg */
		Rk3xI2CPrint(DEBUG_LEVEL_ERROR, DBG_IOCTL, "%s: Signal complete\n", __func__);
		WdfInterruptQueueDpcForIsr(pDevice->Interrupt);
	}
}

/**
 * rk3x_i2c_prepare_read - Setup a read according to i2c->msg
 */
static void rk3x_i2c_prepare_read(PRK3XI2C_CONTEXT pDevice)
{
	unsigned int len = pDevice->currentDescriptor.TransferLength - pDevice->processed;
	UINT32 con;

	con = read32(pDevice, REG_CON);

	/*
	 * The hw can read up to 32 bytes at a time. If we need more than one
	 * chunk, send an ACK after the last byte of the current chunk.
	 */
	if (len > 32) {
		len = 32;
		con &= ~REG_CON_LASTACK;
	}
	else {
		con |= REG_CON_LASTACK;
	}

	/* make sure we are in plain RX mode if we read a second chunk */
	if (pDevice->processed != 0) {
		con &= ~REG_CON_MOD_MASK;
		con |= REG_CON_MOD(REG_CON_MOD_RX);
	}

	write32(pDevice, REG_CON, con);
	write32(pDevice, REG_MRXCNT, len);
}

/**
 * rk3x_i2c_fill_transmit_buf - Fill the transmit buffer with data from msg
 */
static void rk3x_i2c_fill_transmit_buf(PRK3XI2C_CONTEXT pDevice)
{
	unsigned int i, j;
	UINT32 cnt = 0;
	UINT32 val;
	UINT8 byte;

	for (i = 0; i < 8; ++i) {
		val = 0;
		for (j = 0; j < 4; ++j) {
			if ((pDevice->processed == pDevice->currentDescriptor.TransferLength) && (cnt != 0))
				break;

			if (pDevice->processed == 0 && cnt == 0)
				byte = (pDevice->I2CAddress & 0x7f) << 1;
			else {
				NTSTATUS status = MdlChainGetByte(pDevice->currentMDLChain,
					pDevice->currentDescriptor.TransferLength,
					pDevice->processed++,
					&byte);
				if (!NT_SUCCESS(status)) { //Failed to get byte from MDL chain
					pDevice->transactionStatus = status;
				}
			}

			val |= byte << (j * 8);
			cnt++;
		}

		write32(pDevice, TXBUFFER_BASE + 4 * i, val);

		if (pDevice->processed == pDevice->currentDescriptor.TransferLength)
			break;
	}

	write32(pDevice, REG_MTXCNT, cnt);
}

/* IRQ handlers for individual states */

static void rk3x_i2c_handle_start(PRK3XI2C_CONTEXT pDevice, unsigned int ipd)
{
	if (!(ipd & REG_INT_START)) {
		rk3x_i2c_stop(pDevice, STATUS_IO_DEVICE_ERROR);
		Rk3xI2CPrint(DEBUG_LEVEL_ERROR, DBG_IOCTL, "unexpected irq in START: 0x%x\n", ipd);
		rk3x_i2c_clean_ipd(pDevice);
		return;
	}

	/* ack interrupt */
	write32(pDevice, REG_IPD, REG_INT_START);

	/* disable start bit */
	write32(pDevice, REG_CON, read32(pDevice, REG_CON) & ~REG_CON_START);

	/* enable appropriate interrupts and transition */
	if (pDevice->mode == REG_CON_MOD_TX) {
		write32(pDevice, REG_IEN, REG_INT_MBTF | REG_INT_NAKRCV);
		pDevice->state = STATE_WRITE;
		rk3x_i2c_fill_transmit_buf(pDevice);
	}
	else {
		/* in any other case, we are going to be reading. */
		write32(pDevice, REG_IEN, REG_INT_MBRF | REG_INT_NAKRCV);
		pDevice->state = STATE_READ;
		rk3x_i2c_prepare_read(pDevice);
	}
}

static void rk3x_i2c_handle_write(PRK3XI2C_CONTEXT pDevice, unsigned int ipd)
{
	if (!(ipd & REG_INT_MBTF)) {
		rk3x_i2c_stop(pDevice, STATUS_IO_DEVICE_ERROR);
		Rk3xI2CPrint(DEBUG_LEVEL_ERROR, DBG_IOCTL, "unexpected irq in WRITE: 0x%x\n", ipd);
		rk3x_i2c_clean_ipd(pDevice);
		return;
	}

	/* ack interrupt */
	write32(pDevice, REG_IPD, REG_INT_MBTF);

	/* are we finished? */
	if (pDevice->processed >= pDevice->currentDescriptor.TransferLength) {
		pDevice->isLastMsg = TRUE;
		rk3x_i2c_stop(pDevice, pDevice->transactionStatus);
	}
	else
		rk3x_i2c_fill_transmit_buf(pDevice);
}

static void rk3x_i2c_handle_read(PRK3XI2C_CONTEXT pDevice, unsigned int ipd)
{
	unsigned int i;
	unsigned int len = pDevice->currentDescriptor.TransferLength - pDevice->processed;
	UINT32 val = 0;
	UINT8 byte;

	/* we only care for MBRF here. */
	if (!(ipd & REG_INT_MBRF))
		return;

	/* ack interrupt (read also produces a spurious START flag, clear it too) */
	write32(pDevice, REG_IPD, REG_INT_MBRF | REG_INT_START);

	/* Can only handle a maximum of 32 bytes at a time */
	if (len > 32)
		len = 32;

	/* read the data from receive buffer */
	for (i = 0; i < len; ++i) {
		if (i % 4 == 0)
			val = read32(pDevice, RXBUFFER_BASE + (i / 4) * 4);

		byte = (val >> ((i % 4) * 8)) & 0xff;
		NTSTATUS status = MdlChainSetByte(pDevice->currentMDLChain,
			pDevice->currentDescriptor.TransferLength,
			pDevice->processed++,
			byte);
		if (!NT_SUCCESS(status)) {
			pDevice->transactionStatus = status;
			break;
		}
	}

	/* are we finished? */
	if (pDevice->processed >= pDevice->currentDescriptor.TransferLength) {
		pDevice->isLastMsg = TRUE;
		rk3x_i2c_stop(pDevice, pDevice->transactionStatus);
	}
	else
		rk3x_i2c_prepare_read(pDevice);
}

static void rk3x_i2c_handle_stop(PRK3XI2C_CONTEXT pDevice, unsigned int ipd)
{
	unsigned int con;

	if (!(ipd & REG_INT_STOP)) {
		rk3x_i2c_stop(pDevice, STATUS_IO_DEVICE_ERROR);
		Rk3xI2CPrint(DEBUG_LEVEL_ERROR, DBG_IOCTL, "unexpected irq in STOP: 0x%x\n", ipd);
		rk3x_i2c_clean_ipd(pDevice);
		return;
	}

	/* ack interrupt */
	write32(pDevice, REG_IPD, REG_INT_STOP);

	/* disable STOP bit */
	con = read32(pDevice, REG_CON);
	con &= ~REG_CON_STOP;
	write32(pDevice, REG_CON, con);

	pDevice->isBusy = FALSE;
	pDevice->state = STATE_IDLE;

	/* signal rk3x_i2c_xfer that we are finished */
	Rk3xI2CPrint(DEBUG_LEVEL_ERROR, DBG_IOCTL, "%s: Signal complete\n", __func__);
	WdfInterruptQueueDpcForIsr(pDevice->Interrupt);
}

BOOLEAN rk3x_i2c_irq(WDFINTERRUPT Interrupt, ULONG MessageID)
{
	UNREFERENCED_PARAMETER(MessageID);

	WDFDEVICE Device = WdfInterruptGetDevice(Interrupt);
	PRK3XI2C_CONTEXT pDevice = GetDeviceContext(Device);

	unsigned int ipd;

	ipd = read32(pDevice, REG_IPD);
	if (pDevice->state == STATE_IDLE) {
		Rk3xI2CPrint(DEBUG_LEVEL_ERROR, DBG_IOCTL, "irq in STATE_IDLE, ipd = 0x%x\n", ipd);
		rk3x_i2c_clean_ipd(pDevice);
		goto out;
	}

	Rk3xI2CPrint(DEBUG_LEVEL_INFO, DBG_IOCTL, "IRQ: state %d, ipd: %x\n", pDevice->state, ipd);

	/* Clean interrupt bits we don't care about */
	ipd &= ~(REG_INT_BRF | REG_INT_BTF);

	if (ipd & REG_INT_NAKRCV) {
		/*
		 * We got a NACK in the last operation. Depending on whether
		 * IGNORE_NAK is set, we have to stop the operation and report
		 * an error.
		 */
		write32(pDevice, REG_IPD, REG_INT_NAKRCV);

		ipd &= ~REG_INT_NAKRCV;

		rk3x_i2c_stop(pDevice, STATUS_INVALID_TRANSACTION);
	}

	/* is there anything left to handle? */
	if ((ipd & REG_INT_ALL) == 0)
		goto out;

	switch (pDevice->state) {
	case STATE_START:
		rk3x_i2c_handle_start(pDevice, ipd);
		break;
	case STATE_WRITE:
		rk3x_i2c_handle_write(pDevice, ipd);
		break;
	case STATE_READ:
		rk3x_i2c_handle_read(pDevice, ipd);
		break;
	case STATE_STOP:
		rk3x_i2c_handle_stop(pDevice, ipd);
		break;
	case STATE_IDLE:
		break;
	}

out:
	return TRUE;
}

void rk3x_i2c_dpc(WDFINTERRUPT Interrupt, WDFOBJECT AssociatedObject) {
	UNREFERENCED_PARAMETER(AssociatedObject);

	WDFDEVICE Device = WdfInterruptGetDevice(Interrupt);
	PRK3XI2C_CONTEXT pDevice = GetDeviceContext(Device);

	KeSetEvent(&pDevice->waitEvent, IO_NO_INCREMENT, FALSE);
}

/**
 * rk3x_i2c_get_spec - Get timing values of I2C specification
 * @speed: Desired SCL frequency
 *
 * Return: Matched i2c_spec_values.
 */
const struct i2c_spec_values* rk3x_i2c_get_spec(unsigned int speed)
{
	if (speed <= I2C_MAX_STANDARD_MODE_FREQ)
		return &standard_mode_spec;
	else if (speed <= I2C_MAX_FAST_MODE_FREQ)
		return &fast_mode_spec;
	else
		return &fast_mode_plus_spec;
}

NTSTATUS i2c_xfer_single(
	PRK3XI2C_CONTEXT pDevice,
	PPBC_TARGET pTarget,
	SPB_TRANSFER_DESCRIPTOR descriptor,
	PMDL mdlChain
) {
	NTSTATUS status = STATUS_SUCCESS;

	//Setup transaction
	if (pTarget->Settings.AddressMode != AddressMode7Bit) {
		return STATUS_INVALID_ADDRESS; //No support for 10 bit
	}

	UINT32 addr = (pTarget->Settings.Address & 0x7f) << 1;

	KeClearEvent(&pDevice->waitEvent);

	if (descriptor.Direction == SpbTransferDirectionFromDevice) { //xfer read
		addr |= 1; /* set read bit */

		/*
		* We have to transmit the target addr first. Use
		* MOD_REGISTER_TX for that purpose.
		*/
		pDevice->mode = REG_CON_MOD_REGISTER_TX;
		write32(pDevice, REG_MRXADDR,
			addr | REG_MRXADDR_VALID(0));
		write32(pDevice, REG_MRXRADDR, 0);
	}
	else {
		pDevice->mode = REG_CON_MOD_TX;
	}

	pDevice->currentDescriptor = descriptor;
	pDevice->currentMDLChain = mdlChain;

	pDevice->isLastMsg = FALSE;
	pDevice->I2CAddress = pTarget->Settings.Address;
	pDevice->isBusy = TRUE;
	pDevice->state = STATE_START;
	pDevice->processed = 0;
	pDevice->transactionStatus = STATUS_SUCCESS;

	rk3x_i2c_start(pDevice);

	LARGE_INTEGER Timeout;
	Timeout.QuadPart = -10 * 100 * WAIT_TIMEOUT;
	status = KeWaitForSingleObject(&pDevice->waitEvent, Executive, KernelMode, TRUE, &Timeout); //Wait for interrupt

	if (status == STATUS_TIMEOUT) {
		Rk3xI2CPrint(DEBUG_LEVEL_ERROR, DBG_IOCTL, "timeout, ipd: 0x%02x, state: %d\n",
			read32(pDevice, REG_IPD), pDevice->state);

		/* Force a STOP condition without interrupt */
		write32(pDevice, REG_IEN, 0);
		UINT32 val = read32(pDevice, REG_CON) & REG_CON_TUNING_MASK;
		val |= REG_CON_EN | REG_CON_STOP;
		write32(pDevice, REG_CON, val);

		pDevice->state = STATE_IDLE;

		status = STATUS_IO_TIMEOUT;
		return status;
	}

	status = pDevice->transactionStatus;

	return status;
}

NTSTATUS i2c_xfer(PRK3XI2C_CONTEXT pDevice,
	_In_ SPBTARGET SpbTarget,
	_In_ SPBREQUEST SpbRequest,
	_In_ ULONG TransferCount) {
	NTSTATUS status = STATUS_SUCCESS;
	PPBC_TARGET pTarget = GetTargetContext(SpbTarget);

	UINT32 transferredLength = 0;

	WdfWaitLockAcquire(pDevice->waitLock, NULL);

	//Set I2C Clocks
	pDevice->timings.bus_freq_hz = pTarget->Settings.ConnectionSpeed;
	updateClockSettings(pDevice);
	rk3x_i2c_adapt_div(pDevice, pDevice->baseClock);

	SPB_TRANSFER_DESCRIPTOR descriptor;
	for (int i = 0; i < TransferCount; i++) {
		PMDL mdlChain;

		SPB_TRANSFER_DESCRIPTOR_INIT(&descriptor);
		SpbRequestGetTransferParameters(SpbRequest,
			i,
			&descriptor,
			&mdlChain);

		status = i2c_xfer_single(pDevice, pTarget, descriptor, mdlChain);
		if (!NT_SUCCESS(status))
			break;

		transferredLength += descriptor.TransferLength;
		WdfRequestSetInformation(SpbRequest, transferredLength);
	}

	WdfWaitLockRelease(pDevice->waitLock);

	return status;
}