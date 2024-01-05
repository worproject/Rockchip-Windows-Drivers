#pragma once

/*
Structures used for collecting performance counter data.
Needs to be kept in sync with dwc_eqos_perf.man.

Performance counters are not installed by default.

Install:    lodctr /m:dwc_eqos_perf.man
Uninstall:  unlodctr /p:dwc_eqos

Counter data can be collected using tools like perfmon or typeperf.

These performance counters are designed for debugging and performance tuning,
avoiding runtime overhead when they are not used. The values are only 32-bits,
so some of them will roll-over quickly. Collecting 64-bit values is possible,
but would require handling rollover interrupts and tracking the counter values
within the driver (doesn't seem worthwhile at this point).
*/

// Values incremented during ISR or DPC.
struct PERF_DEBUG_DATA
{
    UINT32 IsrHandled;
    UINT32 IsrIgnored;
    UINT32 DpcLinkState;
    UINT32 DpcRx;
    UINT32 DpcTx;
    UINT32 DpcAbnormalStatus;
    UINT32 DpcFatalBusError;
    UINT32 RxOwnDescriptors;
    UINT32 RxDoneFragments;
    UINT32 TxDoneFragments;
};

// Each value corresponds directly to a register in the device.
struct PERF_MAC_DATA
{
    UINT32 Mac_Configuration;
    UINT32 Tx_Octet_Count_Good_Bad;
    UINT32 Tx_Packet_Count_Good_Bad;
    UINT32 Tx_Underflow_Error_Packets;
    UINT32 Tx_Carrier_Error_Packets;
    UINT32 Tx_Octet_Count_Good;
    UINT32 Tx_Packet_Count_Good;
    UINT32 Tx_Pause_Packets;
    UINT32 Rx_Packets_Count_Good_Bad;
    UINT32 Rx_Octet_Count_Good_Bad;
    UINT32 Rx_Octet_Count_Good;
    UINT32 Rx_Multicast_Packets_Good;
    UINT32 Rx_Crc_Error_Packets;
    UINT32 Rx_Oversize_Packets_Good;
    UINT32 Rx_Length_Error_Packets;
    UINT32 Rx_Pause_Packets;
    UINT32 Rx_Fifo_Overflow_Packets;
    UINT32 Rx_Watchdog_Error_Packets;
    UINT32 RxIPv4_Good_Packets;
    UINT32 RxIPv4_Header_Error_Packets;
    UINT32 RxIPv6_Good_Packets;
    UINT32 RxIPv6_Header_Error_Packets;
    UINT32 RxUdp_Error_Packets;
    UINT32 RxTcp_Error_Packets;
    UINT32 RxIcmp_Error_Packets;
    UINT32 RxIPv4_Header_Error_Octets;
    UINT32 RxIPv6_Header_Error_Octets;
    UINT32 RxUdp_Error_Octets;
    UINT32 RxTcp_Error_Octets;
    UINT32 RxIcmp_Error_Octets;
    UINT32 Mmc_Tx_Fpe_Fragment_Cntr;
    UINT32 Mmc_Tx_Hold_Req_Cntr;
    UINT32 Mmc_Rx_Packet_Smd_Err_Cntr;
    UINT32 Mmc_Rx_Packet_Assembly_OK_Cntr;
    UINT32 Mmc_Rx_Fpe_Fragment_Cntr;
};
