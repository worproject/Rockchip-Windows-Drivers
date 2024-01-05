/*
Device behavior. Includes adapter and interrupt since they are 1:1 with the device.
*/
#pragma once

struct DeviceContext; // TODO: if we do multi-queue, make a DeviceQueueContext struct instead.

// Information about the device provided to the queues.
struct DeviceConfig
{
    bool txCoeSel;      // MAC_HW_Feature0\TXCOESEL (hardware support for tx checksum offload).
    bool rxCoeSel;      // MAC_HW_Feature0\RXCOESEL (hardware support for rx checksum offload).
    bool pblX8;         // _DSD\snps,pblx8 (default = 1).
    UINT8 pbl;          // _DSD\snps,pbl (default = 8; effect depends on pblX8).
    UINT8 txPbl;        // _DSD\snps,txpbl (default = pbl; effect depends on pblX8).
    UINT8 rxPbl;        // _DSD\snps,rxpbl (default = pbl; effect depends on pblX8).
    bool fixed_burst;   // _DSD\snps,fixed-burst (default = 0).
    bool mixed_burst;   // _DSD\snps,mixed-burst (default = 1).
    UINT8 wr_osr_lmt;   // AXIC\snps,wr_osr_lmt (default = 4).
    UINT8 rd_osr_lmt;   // AXIC\snps,rd_osr_lmt (default = 8).
    UINT8 blen : 7;     // AXIC\snps,blen bitmask of 7 booleans 4..256 (default = 4, 8, 16).
    bool txFlowControl; // Adapter configuration (Ndi\params\*FlowControl).
    bool rxFlowControl; // Adapter configuration (Ndi\params\*FlowControl).
};

// Referenced in driver.cpp DriverEntry.
// Called by WDF.
__declspec(code_seg("PAGE"))
EVT_WDF_DRIVER_DEVICE_ADD
DeviceAdd;

// Called by driver.cpp DriverEntry.
_IRQL_requires_max_(PASSIVE_LEVEL)
__declspec(code_seg("INIT"))
void
DevicePerfRegister(_In_ WDFDRIVER driver);

// Called by driver.cpp DriverUnload.
_IRQL_requires_max_(PASSIVE_LEVEL)
__declspec(code_seg("PAGE"))
void
DevicePerfUnregister();

// Called by rxqueue.cpp RxQueueSetNotificationEnabled.
_IRQL_requires_max_(PASSIVE_LEVEL)
void
DeviceSetNotificationRxQueue(
    _Inout_ DeviceContext* context,
    _In_opt_ NETPACKETQUEUE rxQueue);

// Called by txqueue.cpp TxQueueSetNotificationEnabled.
_IRQL_requires_max_(PASSIVE_LEVEL)
void
DeviceSetNotificationTxQueue(
    _Inout_ DeviceContext* context,
    _In_opt_ NETPACKETQUEUE txQueue);

// Called by rxqueue.cpp RxQueueAdvance.
_IRQL_requires_max_(DISPATCH_LEVEL)
void
DeviceAddStatisticsRxQueue(
    _Inout_ DeviceContext* context,
    UINT32 ownDescriptors,
    UINT32 doneFragments);

// Called by txqueue.cpp TxQueueAdvance.
_IRQL_requires_max_(DISPATCH_LEVEL)
void
DeviceAddStatisticsTxQueue(
    _Inout_ DeviceContext* context,
    UINT32 doneFragments);
