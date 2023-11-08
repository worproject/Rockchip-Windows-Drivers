#include "driver.h"

static int _ldst_memtomem(BOOLEAN dryRun, UINT8 buf[], int cyc)
{
	int off = 0;

	while (cyc--) {
		off += _emit_LD(dryRun, &buf[off], ALWAYS);
		off += _emit_ST(dryRun, &buf[off], ALWAYS);
	}

	return off;
}

static UINT32 _emit_load(BOOLEAN dryRun, UINT8 buf[],
	enum pl330_cond cond, PXFER_SPEC pxs)
{
	int off = 0;

	if (!pxs->involvesDevice || (pxs->involvesDevice && pxs->toDevice)) {
		off += _emit_LD(dryRun, &buf[off], cond);
	}
	else {
		if (cond == ALWAYS) {
			off += _emit_LDP(dryRun, &buf[off], SINGLE,
				pxs->Peripheral);
			off += _emit_LDP(dryRun, &buf[off], BURST,
				pxs->Peripheral);
		}
		else {
			off += _emit_LDP(dryRun, &buf[off], cond,
				pxs->Peripheral);
		}
	}

	return off;
}

static UINT32 _emit_store(BOOLEAN dryRun, UINT8 buf[],
	enum pl330_cond cond, PXFER_SPEC pxs)
{
	int off = 0;

	if (!pxs->involvesDevice || (pxs->involvesDevice && !pxs->toDevice)) {
		off += _emit_ST(dryRun, &buf[off], cond);
	}
	else {
		if (cond == ALWAYS) {
			off += _emit_STP(dryRun, &buf[off], SINGLE,
				pxs->Peripheral);
			off += _emit_STP(dryRun, &buf[off], BURST,
				pxs->Peripheral);
		}
		else {
			off += _emit_STP(dryRun, &buf[off], cond,
				pxs->Peripheral);
		}
	}

	return off;
}

static inline int _ldst_peripheral(BOOLEAN dryRun, UINT8 buf[],
	PXFER_SPEC pxs, int cyc,
	enum pl330_cond cond)
{
	int off = 0;

	/*
	 * do FLUSHP at beginning to clear any stale dma requests before the
	 * first WFP.
	 */
	off += _emit_FLUSHP(dryRun, &buf[off], pxs->Peripheral);

	while (cyc--) {
		off += _emit_WFP(dryRun, &buf[off], cond, pxs->Peripheral);
		off += _emit_load(dryRun, &buf[off], cond, pxs);
		off += _emit_store(dryRun, &buf[off], cond, pxs);
	}

	return off;
}

static int _bursts(BOOLEAN dryRun, UINT8 buf[], PXFER_SPEC pxs, int cyc) {
	int off = 0;
	enum pl330_cond cond = BURST;

	if (pxs->involvesDevice) {
		off += _ldst_peripheral(dryRun, &buf[off], pxs, cyc,
			cond);
	}
	else {
		off += _ldst_memtomem(dryRun, &buf[off], cyc);
	}
	return off;
}

/*
 * only the unaligned bursts transfers have the dregs.
 * transfer dregs with a reduced size burst to peripheral,
 * or a reduced size burst for mem-to-mem.
 */
static int _dregs(BOOLEAN dryRun, UINT8 buf[],
	PXFER_SPEC pxs, int transfer_length)
{
	int off = 0;
	int dregs_ccr;

	if (transfer_length == 0)
		return off;

	if (pxs->involvesDevice) {
		/*
		 * dregs_len = (total bytes - BURST_TO_BYTE(bursts, ccr)) /
		 *             BRST_SIZE(ccr)
		 * the dregs len must be smaller than burst len,
		 * so, for higher efficiency, we can modify CCR
		 * to use a reduced size burst len for the dregs.
		 */
		dregs_ccr = pxs->ccr;
		dregs_ccr &= ~((0xf << CC_SRCBRSTLEN_SHFT) |
			(0xf << CC_DSTBRSTLEN_SHFT));
		dregs_ccr |= (((transfer_length - 1) & 0xf) <<
			CC_SRCBRSTLEN_SHFT);
		dregs_ccr |= (((transfer_length - 1) & 0xf) <<
			CC_DSTBRSTLEN_SHFT);
		off += _emit_MOV(dryRun, &buf[off], CCR, dregs_ccr);
		off += _ldst_peripheral(dryRun, &buf[off], pxs, 1,
			BURST);
	}
	else {
		dregs_ccr = pxs->ccr;
		dregs_ccr &= ~((0xf << CC_SRCBRSTLEN_SHFT) |
			(0xf << CC_DSTBRSTLEN_SHFT));
		dregs_ccr |= (((transfer_length - 1) & 0xf) <<
			CC_SRCBRSTLEN_SHFT);
		dregs_ccr |= (((transfer_length - 1) & 0xf) <<
			CC_DSTBRSTLEN_SHFT);
		off += _emit_MOV(dryRun, &buf[off], CCR, dregs_ccr);

		off += _ldst_memtomem(dryRun, &buf[off], 1);
	}

	return off;
}

static int _period(BOOLEAN dryRun, UINT8 buf[],
	PXFER_SPEC pxs, unsigned long bursts, int ev)
{
	unsigned int lcnt1, ljmp1;
	int cyc, off = 0, num_dregs = 0;
	struct _arg_LPEND lpend;

	if (bursts > 256) {
		lcnt1 = 256;
		cyc = bursts / 256;
	}
	else {
		lcnt1 = bursts;
		cyc = 1;
	}

	/* loop1 */
	off += _emit_LP(dryRun, &buf[off], 1, lcnt1);
	ljmp1 = off;
	off += _bursts(dryRun, &buf[off], pxs, cyc);
	lpend.cond = ALWAYS;
	lpend.forever = FALSE;
	lpend.loop = 1;
	lpend.bjump = off - ljmp1;
	off += _emit_LPEND(dryRun, &buf[off], &lpend);

	/* remainder */
	lcnt1 = bursts - (lcnt1 * cyc);

	if (lcnt1) {
		off += _emit_LP(dryRun, &buf[off], 1, lcnt1);
		ljmp1 = off;
		off += _bursts(dryRun, &buf[off], pxs, 1);
		lpend.cond = ALWAYS;
		lpend.forever = FALSE;
		lpend.loop = 1;
		lpend.bjump = off - ljmp1;
		off += _emit_LPEND(dryRun, &buf[off], &lpend);
	}

	num_dregs = BYTE_MOD_BURST_LEN(pxs->periodBytes, pxs->ccr);

	if (num_dregs) {
		off += _dregs(dryRun, &buf[off], pxs, num_dregs);
		off += _emit_MOV(dryRun, &buf[off], CCR, pxs->ccr);
	}

	off += _emit_SEV(dryRun, &buf[off], ev);

	return off;
}

static int _loop_cyclic(BOOLEAN dryRun,
	UINT8 buf[], unsigned long bursts,
	PXFER_SPEC pxs, int ev)
{
	int off, periods, residue, i;
	unsigned int lcnt0, ljmp0, ljmpfe;
	struct _arg_LPEND lpend;

	off = 0;
	ljmpfe = off;
	lcnt0 = pxs->numPeriods;
	periods = 1;

	while (lcnt0 > 256) {
		periods++;
		lcnt0 = pxs->numPeriods / periods;
	}

	residue = pxs->numPeriods % periods;

	/* forever loop */
	off += _emit_MOV(dryRun, &buf[off], SAR, pxs->srcAddr);
	off += _emit_MOV(dryRun, &buf[off], DAR, pxs->dstAddr);

	/* loop0 */
	off += _emit_LP(dryRun, &buf[off], 0, lcnt0);
	ljmp0 = off;

	for (i = 0; i < periods; i++)
		off += _period(dryRun, &buf[off], pxs, bursts, ev);

	lpend.cond = ALWAYS;
	lpend.forever = FALSE;
	lpend.loop = 0;
	lpend.bjump = off - ljmp0;
	off += _emit_LPEND(dryRun, &buf[off], &lpend);

	for (i = 0; i < residue; i++)
		off += _period(dryRun, &buf[off], pxs, bursts, ev);

	lpend.cond = ALWAYS;
	lpend.forever = TRUE;
	lpend.loop = 1;
	lpend.bjump = off - ljmpfe;
	off += _emit_LPEND(dryRun, &buf[off], &lpend);

	return off;
}

NTSTATUS SubmitAudioDMA(
	PPL330DMA_CONTEXT pDevice,
	PPL330DMA_THREAD Thread,
	BOOLEAN fromDevice,
	UINT32 srcAddr, UINT32 dstAddr,
	UINT32 len, UINT32 periodLen
) {
	UINT32 ccr = fromDevice ? CC_DSTINC : CC_SRCINC;

	ccr |= CC_SRCNS | CC_DSTNS;

	UINT32 addrWidth = 4;
	UINT32 maxBurst = 8;

	//port width
	ccr |= ((maxBurst - 1) << CC_SRCBRSTLEN_SHFT);
	ccr |= ((maxBurst - 1) << CC_DSTBRSTLEN_SHFT);

	UINT32 burstSz = __ffs(addrWidth); //bus width = 4 bytes
	ccr |= (burstSz << CC_SRCBRSTSIZE_SHFT);
	ccr |= (burstSz << CC_DSTBRSTSIZE_SHFT);
	ccr |= (CCTRL0 << CC_SRCCCTRL_SHFT);
	ccr |= (CCTRL0 << CC_DSTCCTRL_SHFT);
	ccr |= (SWAP_NO << CC_SWAP_SHFT);

	PL330_DBGMC_START(Thread->Req[0].MCbus.LowPart);

	UINT8* buf = Thread->Req[0].MCcpu;

	//DMAMOV CCR, ccr
	int off = 0;
	off += _emit_MOV(FALSE, &buf[off], CCR, ccr);

	XFER_SPEC pxs = { 0 };
	pxs.ccr = ccr;
	pxs.involvesDevice = TRUE;
	pxs.toDevice = !fromDevice;
	pxs.Peripheral = Thread->Id;

	pxs.numPeriods = len / periodLen;
	pxs.srcAddr = srcAddr;
	pxs.dstAddr = dstAddr;
	pxs.periodBytes = periodLen;

	Thread->Req[0].Xfer = pxs;

	UINT32 bursts = BYTE_TO_BURST(periodLen, ccr);
	off += _loop_cyclic(FALSE, &buf[off], bursts, &pxs, Thread->Id);

	BOOLEAN started = _start(Thread);
	if (started)
		return STATUS_SUCCESS;
	return STATUS_UNSUCCESSFUL;
}