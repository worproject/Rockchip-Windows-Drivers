//
// Copyright (c) Microsoft Corporation. All rights reserved.
// Copyright 2020 NXP
// Copyright (c) 2023 Mario Bălănică <mariobalanica02@gmail.com>
// 
// Licensed under the MIT License.
//
// Module Name:
//
//   acpiutil.hpp
//
// Abstract:
//
//  This module contains utility functions declarations to access a PDO ACPI
//  stored information, traverse its objects, verify, execute its method and
//  parse its returned content if there is any
//
// Environment:
//
//  Kernel mode only
//

#ifndef __ACPIUTIL_HPP__
#define __ACPIUTIL_HPP__
#pragma once
#include <acpiioct.h>

#define NONPAGED_SEGMENT_BEGIN \
    __pragma(code_seg(push)) \
    //__pragma(code_seg(.text))

#define NONPAGED_SEGMENT_END \
    __pragma(code_seg(pop))

#define PAGED_SEGMENT_BEGIN \
    __pragma(code_seg(push)) \
    __pragma(code_seg("PAGE"))

#define PAGED_SEGMENT_END \
    __pragma(code_seg(pop))

#define INIT_SEGMENT_BEGIN \
    __pragma(code_seg(push)) \
    __pragma(code_seg("INIT"))

#define INIT_SEGMENT_END \
    __pragma(code_seg(pop))

//
// Memory allocation tag for memory allocated and used as
// ACPI_EVAL_OUTPUT_BUFFER
//
#define ACPI_TAG_EVAL_OUTPUT_BUFFER             ULONG('BaeA')

//
// Memory allocation tag for memory allocated and used as
// ACPI_EVAL_INPUT_BUFFER_*
//
#define ACPI_TAG_EVAL_INPUT_BUFFER              ULONG('BieA')

//
// Memory allocation tag for memory allocated and used
// for IRP intermediate buffer
//
#define ACPI_TAG_IOCTL_INTERMEDIATE_BUFFER      ULONG('BceA')

//
// Device Specific Data (_DSD) ACPI method name as ULONG
//
#define ACPI_METHOD_ULONG_DSD                   ULONG('DSD_')

//
// Device Specific Method (_DSM) ACPI method name as ULONG
//
#define ACPI_METHOD_ULONG_DSM                   ULONG('MSD_')

//
// Device Specific Method (_DSM) takes 4 args:
//  Arg0 : Buffer containing a UUID [16 bytes]
//  Arg1 : Integer containing the Revision ID
//  Arg2 : Integer containing the Function Index 
//  Arg3 : Package that contains function-specific arguments
//
#define ACPI_DSM_ARGUMENT_COUNT         4

//
// The reserved function index for querying supported _DSM functions
//
#define ACPI_DSM_FUNCTION_IDX_QUERY     0


//
// Device Property UUID per spec
// http://www.uefi.org/sites/default/files/resources/_DSD-device-properties-UUID.pdf
//
// {DAFFD814-6EBA-4D8C-8A91-BC9BBF4AA301}
//
DEFINE_GUID(
    ACPI_DEVICE_PROPERTIES_DSD_GUID,
    0xDAFFD814, 0x6EBA, 0x4D8C, 0x8A, 0x91, 0xBC, 0x9B, 0xBF, 0x4A, 0xA3, 0x01);

_IRQL_requires_max_(APC_LEVEL)
NTSTATUS
AcpiSendIoctlSynchronously(
	_In_ DEVICE_OBJECT* PdoPtr,
	_In_ ULONG IoControlCode,
	_In_reads_bytes_(InputBufferSize) ACPI_EVAL_INPUT_BUFFER* InputBufferPtr,
	_In_ ULONG InputBufferSize,
	_Out_writes_bytes_to_(OutputBufferSize, *BytesReturnedCountPtr) ACPI_EVAL_OUTPUT_BUFFER UNALIGNED* OutputBufferPtr,
	_In_ ULONG OutputBufferSize,
	_Out_opt_ ULONG* BytesReturnedCountPtr);

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
AcpiQueryDsd(
    _In_ DEVICE_OBJECT* PdoPtr,
    _Outptr_result_bytebuffer_maybenull_((*DsdBufferPptr)->Length) ACPI_EVAL_OUTPUT_BUFFER UNALIGNED** DsdBufferPptr);

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
AcpiParseDsdAsDeviceProperties(
    _In_ const ACPI_EVAL_OUTPUT_BUFFER UNALIGNED* DsdBufferPtr,
    _Outptr_ const ACPI_METHOD_ARGUMENT UNALIGNED** DevicePropertiesPkgPptr);

template<class T>
_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
AcpiDevicePropertiesQueryIntegerValue(
    _In_ const ACPI_METHOD_ARGUMENT UNALIGNED* DevicePropertiesPkgPtr,
    _In_z_ const CHAR* KeyNamePtr,
    _Out_ T* ValuePtr
    )
{
    if (DevicePropertiesPkgPtr == nullptr) {
        return STATUS_INVALID_PARAMETER_1;
    }

    if (KeyNamePtr == nullptr) {
        return STATUS_INVALID_PARAMETER_2;
    }

    if (ValuePtr == nullptr) {
        return STATUS_INVALID_PARAMETER_3;
    }

    UINT32 Uint32Value;
    NTSTATUS status = AcpiDevicePropertiesQueryIntegerValue(DevicePropertiesPkgPtr, KeyNamePtr, &Uint32Value);
    if (NT_SUCCESS(status)) {
        *ValuePtr = static_cast<T>(Uint32Value);
    }

    return status;
}

template<>
_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
AcpiDevicePropertiesQueryIntegerValue<UINT32>(
    _In_ const ACPI_METHOD_ARGUMENT UNALIGNED* DevicePropertiesPkgPtr,
    _In_z_ const CHAR* KeyNamePtr,
    _Out_ UINT32* Value);

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
AcpiDevicePropertiesQueryIntegerValueArray(
    _In_ const ACPI_METHOD_ARGUMENT UNALIGNED* ListEntryPtr,
    _Out_ UINT32* ValuePtr,
    _In_ UINT32 Count,
    _Inout_ const ACPI_METHOD_ARGUMENT UNALIGNED** currentEntryNextPtr);

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
AcpiDevicePropertiesQueryStringValue(
    _In_ const ACPI_METHOD_ARGUMENT UNALIGNED* DevicePropertiesPkgPtr,
    _In_z_ const CHAR* KeyNamePtr,
    _In_ UINT32 MaxLength,
    _Out_ UINT32* OutLength,
    _Out_writes_z_(MaxLength) PCHAR ValuePtr);

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
AcpiDevicePropertiesQueryIntegerArrayValue(
    _In_ const ACPI_METHOD_ARGUMENT UNALIGNED* DevicePropertiesPkgPtr,
    _In_ const CHAR* KeyNamePtr,
    _Inout_ UINT32* BufferPtr,
    _In_ UINT32 Length
);

_IRQL_requires_same_
NTSTATUS
AcpiPackageGetNextArgument(
	_In_ const ACPI_METHOD_ARGUMENT UNALIGNED* PkgPtr,
	_Inout_ const ACPI_METHOD_ARGUMENT UNALIGNED** ArgumentPptr);

_IRQL_requires_max_(APC_LEVEL)
NTSTATUS
AcpiQueryDsm(
    _In_ DEVICE_OBJECT* PdoPtr,
    _In_ const GUID* GuidPtr,
    _In_ UINT32 RevisionId,
    _Out_ UINT32* SupportedFunctionsMaskPtr);

_IRQL_requires_max_(APC_LEVEL)
NTSTATUS
AcpiEvaluateMethod(
    _In_ DEVICE_OBJECT* PdoPtr,
    _In_reads_bytes_(InputBufferSize) ACPI_EVAL_INPUT_BUFFER* InputBufferPtr,
    _In_ UINT32 InputBufferSize,
    _Outptr_result_bytebuffer_maybenull_((*ReturnBufferPptr)->Length) ACPI_EVAL_OUTPUT_BUFFER UNALIGNED** ReturnBufferPptr);

_IRQL_requires_max_(APC_LEVEL)
NTSTATUS
AcpiExecuteDsmFunction(
    _In_ DEVICE_OBJECT* PdoPtr,
    _In_reads_bytes_(sizeof(GUID)) const GUID* GuidPtr,
    _In_ UINT32 RevisionId,
    _In_ UINT32 FunctionIdx,
    _In_reads_bytes_(FunctionArgumentsSize) ACPI_METHOD_ARGUMENT* FunctionArgumentsPtr,
    _In_ USHORT FunctionArgumentsSize,
    _Outptr_opt_result_bytebuffer_maybenull_((*ReturnBufferPptr)->Length) ACPI_EVAL_OUTPUT_BUFFER UNALIGNED** ReturnBufferPptr);

_IRQL_requires_same_
NTSTATUS
AcpiDevicePropertiesQueryValue(
    _In_ const ACPI_METHOD_ARGUMENT UNALIGNED* DevicePropertiesPkgPtr,
    _In_z_ const CHAR* KeyNamePtr,
    _Out_ const ACPI_METHOD_ARGUMENT UNALIGNED** ValuePtr);

#endif // __ACPIUTIL_HPP__