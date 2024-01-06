#ifndef _ROCKCHIP_TPLG
#define _ROCKCHIP_TPLG

#define RKTPLG_MAGIC '$RKC'

typedef struct _RK_TPLG {
	UINT32 magic;
	UINT32 length;
	char dma_name[32];
	UINT32 tx;
	UINT32 rx;
	char audio_tplg[32];
	UINT32 i2s_tx_route;
	UINT32 i2s_rx_route;
} RK_TPLG, *PRK_TPLG;

NTSTATUS GetRKTplg(WDFDEVICE FxDevice, RK_TPLG *rkTplg);
#endif