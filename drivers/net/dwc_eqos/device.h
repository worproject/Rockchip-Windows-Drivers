#pragma once

void
DeviceSetNotificationRxQueue(
    _In_ NETADAPTER adapter,
    _In_opt_ NETPACKETQUEUE rxQueue);

void
DeviceSetNotificationTxQueue(
    _In_ NETADAPTER adapter,
    _In_opt_ NETPACKETQUEUE txQueue);

__declspec(code_seg("PAGE"))
EVT_WDF_DRIVER_DEVICE_ADD
DeviceAdd;
