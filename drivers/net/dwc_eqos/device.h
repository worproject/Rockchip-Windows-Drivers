/*
Device behavior. Includes adapter and interrupt since they are 1:1 with the device.
*/
#pragma once

struct DeviceContext;

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
    UINT32 ownDescriptors,
    UINT32 doneFragments);
