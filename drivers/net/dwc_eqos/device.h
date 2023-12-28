/*
Device behavior. Includes adapter and interrupt since they are 1:1 with the device.
*/
#pragma once

// Referenced in driver.cpp DriverEntry.
// Called by WDF.
__declspec(code_seg("PAGE"))
EVT_WDF_DRIVER_DEVICE_ADD
DeviceAdd;

// Called by driver.cpp DriverEntry.
__declspec(code_seg("INIT"))
void
DevicePerfRegister(_In_ WDFDRIVER driver);

// Called by driver.cpp DriverUnload.
__declspec(code_seg("PAGE"))
void
DevicePerfUnregister();

// Called by rxqueue.cpp RxQueueSetNotificationEnabled.
void
DeviceSetNotificationRxQueue(
    _In_ NETADAPTER adapter,
    _In_opt_ NETPACKETQUEUE rxQueue);

// Called by txqueue.cpp TxQueueSetNotificationEnabled.
void
DeviceSetNotificationTxQueue(
    _In_ NETADAPTER adapter,
    _In_opt_ NETPACKETQUEUE txQueue);
