#if !defined(_RKI2SBUS_BUSPDO_H_)
#define _RKI2SBUS_BUSPDO_H_

#define MAX_INSTANCE_ID_LEN 80

typedef struct _CODEC_IDS {
    BOOL IsDSP;
} CODEC_IDS, * PCODEC_IDS;

typedef struct _PDO_IDENTIFICATION_DESCRIPTION
{
    WDF_CHILD_IDENTIFICATION_DESCRIPTION_HEADER Header; // should contain this header

    PFDO_CONTEXT FdoContext;

    CODEC_IDS CodecIds;

} PDO_IDENTIFICATION_DESCRIPTION, * PPDO_IDENTIFICATION_DESCRIPTION;

//
// This is PDO device-extension.
//
typedef struct _PDO_DEVICE_DATA
{
    PFDO_CONTEXT FdoContext;

    CODEC_IDS CodecIds;

} PDO_DEVICE_DATA, * PPDO_DEVICE_DATA;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(PDO_DEVICE_DATA, PdoGetData)

extern "C" {

    NTSTATUS
        Bus_EvtChildListIdentificationDescriptionDuplicate(
            WDFCHILDLIST DeviceList,
            PWDF_CHILD_IDENTIFICATION_DESCRIPTION_HEADER SourceIdentificationDescription,
            PWDF_CHILD_IDENTIFICATION_DESCRIPTION_HEADER DestinationIdentificationDescription
        );

    BOOLEAN
        Bus_EvtChildListIdentificationDescriptionCompare(
            WDFCHILDLIST DeviceList,
            PWDF_CHILD_IDENTIFICATION_DESCRIPTION_HEADER FirstIdentificationDescription,
            PWDF_CHILD_IDENTIFICATION_DESCRIPTION_HEADER SecondIdentificationDescription
        );

    VOID
        Bus_EvtChildListIdentificationDescriptionCleanup(
            _In_ WDFCHILDLIST DeviceList,
            _Inout_ PWDF_CHILD_IDENTIFICATION_DESCRIPTION_HEADER IdentificationDescription
        );

    NTSTATUS
        Bus_EvtDeviceListCreatePdo(
            WDFCHILDLIST DeviceList,
            PWDF_CHILD_IDENTIFICATION_DESCRIPTION_HEADER IdentificationDescription,
            PWDFDEVICE_INIT ChildInit
        );

}

#endif
