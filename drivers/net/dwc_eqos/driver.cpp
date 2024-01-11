#include "precomp.h"
#include "device.h"
#include "trace.h"

/*
Possible areas for improvement:
- Tx segmentation offload.
- Run against network test suites and fix any issues.
- Power control, wake-on-LAN, ARP offload.
- Configure speed, duplex in Ndi\params.
- Multi-queue support?
*/

TRACELOGGING_DEFINE_PROVIDER(
    TraceProvider,
    "dwc_eqos",
    // {5d8331d3-70b3-5620-5664-db28f48a4b79} = Hash("dwc_eqos")
    (0x5d8331d3, 0x70b3, 0x5620, 0x56, 0x64, 0xdb, 0x28, 0xf4, 0x8a, 0x4b, 0x79));

static EVT_WDF_DRIVER_UNLOAD DriverUnload;
__declspec(code_seg("PAGE"))
void
DriverUnload(_In_ WDFDRIVER driver)
{
    // PASSIVE_LEVEL
    PAGED_CODE();
    UNREFERENCED_PARAMETER(driver);
    DevicePerfUnregister();
    TraceEntryExit(DriverUnload, LEVEL_INFO);
    TraceLoggingUnregister(TraceProvider);
}

extern "C" DRIVER_INITIALIZE DriverEntry;
__declspec(code_seg("INIT"))
NTSTATUS
DriverEntry(
    _In_ DRIVER_OBJECT* driverObject,
    _In_ UNICODE_STRING* registryPath)
{
    // PASSIVE_LEVEL
    PAGED_CODE();

    NTSTATUS status;

    TraceLoggingRegister(TraceProvider);
    TraceEntry(DriverEntry, LEVEL_INFO,
        TraceLoggingUnicodeString(registryPath));

    WDF_DRIVER_CONFIG config;
    WDF_DRIVER_CONFIG_INIT(&config, DeviceAdd);
    config.EvtDriverUnload = DriverUnload;
    config.DriverPoolTag = 'dwcE';

    WDFDRIVER driver;
    status = WdfDriverCreate(
        driverObject,
        registryPath,
        WDF_NO_OBJECT_ATTRIBUTES,
        &config,
        &driver);

    if (NT_SUCCESS(status))
    {
        DevicePerfRegister(driver);
    }

    TraceExitWithStatus(DriverEntry, LEVEL_INFO, status);

    if (!NT_SUCCESS(status))
    {
        TraceLoggingUnregister(TraceProvider);
    }

    return status;
}
