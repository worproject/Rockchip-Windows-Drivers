#include "driver.h"
#include "rk-tplg.h"

EVT_WDF_DEVICE_PREPARE_HARDWARE Fdo_EvtDevicePrepareHardware;
EVT_WDF_DEVICE_RELEASE_HARDWARE Fdo_EvtDeviceReleaseHardware;
EVT_WDF_DEVICE_D0_ENTRY Fdo_EvtDeviceD0Entry;
EVT_WDF_DEVICE_D0_EXIT Fdo_EvtDeviceD0Exit;
EVT_WDF_DEVICE_SELF_MANAGED_IO_INIT Fdo_EvtDeviceSelfManagedIoInit;

NTSTATUS
Fdo_Initialize(
    _In_ PFDO_CONTEXT FdoCtx
);

NTSTATUS
Fdo_Create(
	_Inout_ PWDFDEVICE_INIT DeviceInit
)
{
    WDF_CHILD_LIST_CONFIG      config;
	WDF_OBJECT_ATTRIBUTES attributes;
	WDF_PNPPOWER_EVENT_CALLBACKS pnpPowerCallbacks;
    PFDO_CONTEXT fdoCtx;
    WDFDEVICE wdfDevice;
    NTSTATUS status;

    RKI2SBusPrint(DEBUG_LEVEL_INFO, DBG_INIT,
        "%s\n", __func__);

    WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnpPowerCallbacks);
    pnpPowerCallbacks.EvtDevicePrepareHardware = Fdo_EvtDevicePrepareHardware;
    pnpPowerCallbacks.EvtDeviceReleaseHardware = Fdo_EvtDeviceReleaseHardware;
    pnpPowerCallbacks.EvtDeviceD0Entry = Fdo_EvtDeviceD0Entry;
    pnpPowerCallbacks.EvtDeviceD0Exit = Fdo_EvtDeviceD0Exit;
    pnpPowerCallbacks.EvtDeviceSelfManagedIoInit = Fdo_EvtDeviceSelfManagedIoInit;
    WdfDeviceInitSetPnpPowerEventCallbacks(DeviceInit, &pnpPowerCallbacks);

    //
    // WDF_ DEVICE_LIST_CONFIG describes how the framework should handle
    // dynamic child enumeration on behalf of the driver writer.
    // Since we are a bus driver, we need to specify identification description
    // for our child devices. This description will serve as the identity of our
    // child device. Since the description is opaque to the framework, we
    // have to provide bunch of callbacks to compare, copy, or free
    // any other resources associated with the description.
    //
    WDF_CHILD_LIST_CONFIG_INIT(&config,
        sizeof(PDO_IDENTIFICATION_DESCRIPTION),
        Bus_EvtDeviceListCreatePdo // callback to create a child device.
    );

    //
    // This function pointer will be called when the framework needs to copy a
    // identification description from one location to another.  An implementation
    // of this function is only necessary if the description contains description
    // relative pointer values (like  LIST_ENTRY for instance) .
    // If set to NULL, the framework will use RtlCopyMemory to copy an identification .
    // description. In this sample, it's not required to provide these callbacks.
    // they are added just for illustration.
    //
    config.EvtChildListIdentificationDescriptionDuplicate =
        Bus_EvtChildListIdentificationDescriptionDuplicate;

    //
    // This function pointer will be called when the framework needs to compare
    // two identificaiton descriptions.  If left NULL a call to RtlCompareMemory
    // will be used to compare two identificaiton descriptions.
    //
    config.EvtChildListIdentificationDescriptionCompare =
        Bus_EvtChildListIdentificationDescriptionCompare;
    //
    // This function pointer will be called when the framework needs to free a
    // identification description.  An implementation of this function is only
    // necessary if the description contains dynamically allocated memory
    // (by the driver writer) that needs to be freed. The actual identification
    // description pointer itself will be freed by the framework.
    //
    config.EvtChildListIdentificationDescriptionCleanup =
        Bus_EvtChildListIdentificationDescriptionCleanup;

    //
    // Tell the framework to use the built-in childlist to track the state
    // of the device based on the configuration we just created.
    //
    WdfFdoInitSetDefaultChildListConfig(DeviceInit,
        &config,
        WDF_NO_OBJECT_ATTRIBUTES);

    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, FDO_CONTEXT);
    status = WdfDeviceCreate(&DeviceInit, &attributes, &wdfDevice);
    if (!NT_SUCCESS(status)) {
        RKI2SBusPrint(DEBUG_LEVEL_ERROR, DBG_INIT,
            "WdfDriverCreate failed %x\n", status);
        goto Exit;
    }

    {
        WDF_DEVICE_POWER_POLICY_IDLE_SETTINGS IdleSettings;

        WDF_DEVICE_POWER_POLICY_IDLE_SETTINGS_INIT(&IdleSettings, IdleCannotWakeFromS0);
        IdleSettings.IdleTimeoutType = SystemManagedIdleTimeoutWithHint;
        IdleSettings.IdleTimeout = 1000;
        IdleSettings.UserControlOfIdleSettings = IdleDoNotAllowUserControl;

        WdfDeviceAssignS0IdleSettings(wdfDevice, &IdleSettings);
    }

    {
        WDF_DEVICE_STATE deviceState;
        WDF_DEVICE_STATE_INIT(&deviceState);

        deviceState.NotDisableable = WdfFalse;
        WdfDeviceSetDeviceState(wdfDevice, &deviceState);
    }

    fdoCtx = Fdo_GetContext(wdfDevice);
    fdoCtx->WdfDevice = wdfDevice;

    status = Fdo_Initialize(fdoCtx);
    if (!NT_SUCCESS(status))
    {
        goto Exit;
    }

Exit:
    return status;
}

BOOLEAN dsp_interrupt(
    WDFINTERRUPT Interrupt,
    ULONG MessageID) {

    UNREFERENCED_PARAMETER(MessageID);

    WDFDEVICE Device = WdfInterruptGetDevice(Interrupt);
    PFDO_CONTEXT fdoCtx = Fdo_GetContext(Device);

    BOOLEAN handled = FALSE;

    if (fdoCtx->dspInterruptCallback) {
        handled = (BOOLEAN)fdoCtx->dspInterruptCallback(fdoCtx->dspInterruptContext);
    }

    return handled;
}

VOID dsp_dpc(
    WDFINTERRUPT Interrupt,
    WDFOBJECT AssociatedObject) {

    UNREFERENCED_PARAMETER(AssociatedObject);

    WDFDEVICE Device = WdfInterruptGetDevice(Interrupt);
    PFDO_CONTEXT fdoCtx = Fdo_GetContext(Device);

    if (fdoCtx->dspDPCCallback) {
        fdoCtx->dspDPCCallback(fdoCtx->dspInterruptContext);
    }
}

NTSTATUS
Fdo_Initialize(
    _In_ PFDO_CONTEXT FdoCtx
)
{
    NTSTATUS status;
    WDFDEVICE device;
    WDF_INTERRUPT_CONFIG interruptConfig;

    device = FdoCtx->WdfDevice;

    RKI2SBusPrint(DEBUG_LEVEL_INFO, DBG_INIT,
        "%s\n", __func__);

    //
    // Create an interrupt object for hardware notifications
    //
    WDF_INTERRUPT_CONFIG_INIT(
        &interruptConfig,
        dsp_interrupt,
        dsp_dpc);

    status = WdfInterruptCreate(
        device,
        &interruptConfig,
        WDF_NO_OBJECT_ATTRIBUTES,
        &FdoCtx->Interrupt);

    if (!NT_SUCCESS(status))
    {
        RKI2SBusPrint(DEBUG_LEVEL_ERROR, DBG_PNP,
            "Error creating WDF interrupt object - %!STATUS!",
            status);

        return status;
    }

    return STATUS_SUCCESS;
}

NTSTATUS
Fdo_EvtDevicePrepareHardware(
    _In_ WDFDEVICE Device,
    _In_ WDFCMRESLIST ResourcesRaw,
    _In_ WDFCMRESLIST ResourcesTranslated
)
{
    UNREFERENCED_PARAMETER(ResourcesRaw);

    BOOLEAN fMmioFound = FALSE;
    NTSTATUS status;
    PFDO_CONTEXT fdoCtx;
    ULONG resourceCount;

    fdoCtx = Fdo_GetContext(Device);
    resourceCount = WdfCmResourceListGetCount(ResourcesTranslated);

    RKI2SBusPrint(DEBUG_LEVEL_INFO, DBG_INIT,
        "%s\n", __func__);

    for (ULONG i = 0; i < resourceCount; i++)
    {
        PCM_PARTIAL_RESOURCE_DESCRIPTOR pDescriptor;

        pDescriptor = WdfCmResourceListGetDescriptor(
            ResourcesTranslated, i);

        switch (pDescriptor->Type)
        {
        case CmResourceTypeMemory:
            //Look for MMIO
            if (fMmioFound == FALSE) {
                RKI2SBusPrint(DEBUG_LEVEL_INFO, DBG_INIT,
                    "Found MMIO: 0x%llx (size 0x%lx)\n", pDescriptor->u.Memory.Start.QuadPart, pDescriptor->u.Memory.Length);

                fdoCtx->m_MMIO.Base.Base = MmMapIoSpace(pDescriptor->u.Memory.Start, pDescriptor->u.Memory.Length, MmNonCached);
                fdoCtx->m_MMIO.PhysAddr = pDescriptor->u.Memory.Start;
                fdoCtx->m_MMIO.Len = pDescriptor->u.Memory.Length;

                RKI2SBusPrint(DEBUG_LEVEL_INFO, DBG_INIT,
                    "Mapped to %p\n", fdoCtx->m_MMIO.Base.baseptr);
                fMmioFound = TRUE;
            }
            break;
        }
    }

    if (fdoCtx->m_MMIO.Base.Base == NULL) {
        status = STATUS_NOT_FOUND; //MMIO is required
        return status;
    }

    fdoCtx->rkTplg = NULL;
    fdoCtx->rkTplgSz = 0;

    { //Check topology for Rockchip I2S
        RK_TPLG rkTplg = { 0 };
        NTSTATUS status2 = GetRKTplg(Device, &rkTplg);
        if (NT_SUCCESS(status2) && rkTplg.magic == RKTPLG_MAGIC) {
            fdoCtx->rkTplg = ExAllocatePoolZero(NonPagedPool, rkTplg.length, RKI2SBUS_POOL_TAG);
            RtlCopyMemory(fdoCtx->rkTplg, &rkTplg, rkTplg.length);
            fdoCtx->rkTplgSz = rkTplg.length;
        }
    }

    status = STATUS_SUCCESS;

    return status;
}

NTSTATUS
Fdo_EvtDeviceReleaseHardware(
    _In_ WDFDEVICE Device,
    _In_ WDFCMRESLIST ResourcesTranslated
)
{
    PFDO_CONTEXT fdoCtx;

    UNREFERENCED_PARAMETER(ResourcesTranslated);

    fdoCtx = Fdo_GetContext(Device);

    RKI2SBusPrint(DEBUG_LEVEL_INFO, DBG_INIT,
        "%s\n", __func__);

    if (fdoCtx->rkTplg)
        ExFreePoolWithTag(fdoCtx->rkTplg, RKI2SBUS_POOL_TAG);

    if (fdoCtx->m_MMIO.Base.Base)
        MmUnmapIoSpace(fdoCtx->m_MMIO.Base.Base, fdoCtx->m_MMIO.Len);

    return STATUS_SUCCESS;
}

NTSTATUS
Fdo_EvtDeviceD0Entry(
    _In_ WDFDEVICE Device,
    _In_ WDF_POWER_DEVICE_STATE PreviousState
)
{
    UNREFERENCED_PARAMETER(PreviousState);

    NTSTATUS status;
    PFDO_CONTEXT fdoCtx;

    fdoCtx = Fdo_GetContext(Device);

    RKI2SBusPrint(DEBUG_LEVEL_INFO, DBG_INIT,
        "%s\n", __func__);

    status = STATUS_SUCCESS;

    if (!NT_SUCCESS(status)) {
        return status;
    }

    return status;
}

NTSTATUS
Fdo_EvtDeviceD0Exit(
    _In_ WDFDEVICE Device,
    _In_ WDF_POWER_DEVICE_STATE TargetState
)
{
    UNREFERENCED_PARAMETER(Device);
    UNREFERENCED_PARAMETER(TargetState);

    return STATUS_SUCCESS;
}

NTSTATUS
Fdo_EvtDeviceSelfManagedIoInit(
    _In_ WDFDEVICE Device
)
{
    NTSTATUS status = STATUS_SUCCESS;
    PFDO_CONTEXT fdoCtx;

    fdoCtx = Fdo_GetContext(Device);

    WdfChildListBeginScan(WdfFdoGetDefaultChildList(Device));

    fdoCtx->dspInterruptCallback = NULL;

    PDO_IDENTIFICATION_DESCRIPTION description;
    //
    // Initialize the description with the information about the detected codec.
    //
    WDF_CHILD_IDENTIFICATION_DESCRIPTION_HEADER_INIT(
        &description.Header,
        sizeof(description)
    );

    description.FdoContext = fdoCtx;
    description.CodecIds.IsDSP = TRUE;

    //
    // Call the framework to add this child to the childlist. This call
    // will internaly call our DescriptionCompare callback to check
    // whether this device is a new device or existing device. If
    // it's a new device, the framework will call DescriptionDuplicate to create
    // a copy of this description in nonpaged pool.
    // The actual creation of the child device will happen when the framework
    // receives QUERY_DEVICE_RELATION request from the PNP manager in
    // response to InvalidateDeviceRelations call made as part of adding
    // a new child.
    //
    status = WdfChildListAddOrUpdateChildDescriptionAsPresent(
        WdfFdoGetDefaultChildList(Device), &description.Header,
        NULL); // AddressDescription

    WdfChildListEndScan(WdfFdoGetDefaultChildList(Device));

    RKI2SBusPrint(DEBUG_LEVEL_INFO, DBG_INIT,
        "scan complete\n");
    return status;
}