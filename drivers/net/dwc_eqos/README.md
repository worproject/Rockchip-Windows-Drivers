# Synopsys DesignWare Ethernet Quality of Service (GMAC) Driver

This is a driver for the Synopsys DesignWare Ethernet Quality of Service (EQoS)
controller found in the RK35xx SoCs, supporting 1Gbps ethernet connections.

## Compatibility

EQoS is a configurable IP block that can be customized and added to a SoC. This
driver has been tested only on the RK3588(s) and assumes the presence of
optional features that may be missing on other SoCs. With minor fixes, it would
probably work on other EQoS-based SoCs. The driver specifically checks for the
following:

- `GMAC_MAC_Version.RKVER` must be 0x51 or 0x52 (other values untested).
- `GMAC_MAC_HW_Feature0.SAVLANINS` must be enabled (require VLAN insertion support).
- `GMAC_MAC_HW_Feature0.RXCOESEL` and `TXCOESEL` must be enabled (require checksum offload support).
- `GMAC_MAC_HW_Feature1.TSOEN` must be enabled (require TCP/UDP segmentation offload support).

There may be other requirements that are assumed but not checked.

## ACPI Configuration

This driver uses ACPI properties to configure the EQoS controller's DMA behavior:

- `_DSD\snps,pblx8` (default = 1): Controls the value of `GMAC_DMA_CHx_Control.PBLx8`, i.e. controls whether PBL values are treated as 1-beat units (0) or 8-beat units (1, default).
- `_DSD\snps,pbl` (default = 8): Default value for `txpbl` and `rxpbl`.
- `_DSD\snps,txpbl` (default = `pbl`): Controls the value of `GMAC_DMA_CHx_Tx_Control.TxPBL`, i.e. transmit programmable burst length.
- `_DSD\snps,rxpbl` (default = `pbl`): Controls the value of `GMAC_DMA_CHx_Rx_Control.RxPBL`, i.e. receive programmable burst length.
- `_DSD\snps,fixed-burst` (default = 0): Controls the value of `GMAC_DMA_SysBus_Mode.FB`.
- `_DSD\snps,mixed-burst` (default = 1): Controls the value of `GMAC_DMA_SysBus_Mode.Bit14`.
- `_DSD\snps,axi-config` (default = none): Controls the `$(AXIC)` method name to use for the remaining properties. If not present, the driver will use default values for the remaining properties. Should generally be set to string `"AXIC"`.
- `$(AXIC)\snps,wr_osr_lmt` (default = 4): Controls the value of `GMAC_DMA_SysBus_Mode.WR_OSR_LMT`, i.e. AXI maximum write outstanding request limit.
- `$(AXIC)\snps,rd_osr_lmt` (default = 8): Controls the value of `GMAC_DMA_SysBus_Mode.RD_OSR_LMT`, i.e. AXI maximum read outstanding request limit.
- `$(AXIC)\snps,blen` (default = `{ 16, 8, 4 }`): Controls the values of `GMAC_DMA_SysBus_Mode.BLENx` (x = 4, 8, 16, 32, 64, 128, 256), i.e. AXI burst length. Should be a list of 7 integers, e.g. `Package () { 0, 0, 0, 0, 16, 8, 4 }`.

## Areas for improvement:

- Run against network test suites and fix any issues.
- Memory optimizations? Current implementation uses system-managed buffers.
  System-managed buffer size is tied to MTU. When jumbo frames are enabled,
  this is wasteful since most packets are still 1522 bytes or less. If we
  used driver-managed buffers and updated the Rx queue to handle multi-buffer
  packets, we could use 1536-byte or 2048-byte buffers for the Rx queue, saving
  about 2MB per device when JumboPacket = 9014.
- Configure speed, duplex via Ndi\params?
- Power control, wake-on-LAN, ARP offload?
- Multi-queue, RSS support?
- Make it more generic (test with other EQoS-based SoCs)?
