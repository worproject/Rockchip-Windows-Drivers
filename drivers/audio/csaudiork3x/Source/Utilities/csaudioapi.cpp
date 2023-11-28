#include "definitions.h"
#include "hw.h"

VOID CsAudioCallbackFunction(
	CCsAudioRk3xHW* hw,
	CsAudioArg* arg,
	PVOID Argument2
) {
	if (!hw) {
		return;
	}

	if (Argument2 == &hw->CsAudioArg2) {
		return;
	}

	CsAudioArg localArg;
	RtlZeroMemory(&localArg, sizeof(CsAudioArg));
	RtlCopyMemory(&localArg, arg, min(arg->argSz, sizeof(CsAudioArg)));

	hw->CSAudioAPICalled(localArg);
}

NTSTATUS CCsAudioRk3xHW::CSAudioAPIInit() {
	NTSTATUS status;

	UNICODE_STRING CSAudioCallbackAPI;
	RtlInitUnicodeString(&CSAudioCallbackAPI, L"\\CallBack\\CsAudioCallbackAPI");


	OBJECT_ATTRIBUTES attributes;
	InitializeObjectAttributes(&attributes,
		&CSAudioCallbackAPI,
		OBJ_KERNEL_HANDLE | OBJ_OPENIF | OBJ_CASE_INSENSITIVE | OBJ_PERMANENT,
		NULL,
		NULL
	);
	status = ExCreateCallback(&this->CSAudioAPICallback, &attributes, TRUE, TRUE);
	if (!NT_SUCCESS(status)) {
		return status;
	}

	this->CSAudioAPICallbackObj = ExRegisterCallback(this->CSAudioAPICallback,
		(PCALLBACK_FUNCTION)CsAudioCallbackFunction,
		this
	);
	if (!this->CSAudioAPICallbackObj) {

		return STATUS_NO_CALLBACK_ACTIVE;
	}

	CsAudioArg arg;
	RtlZeroMemory(&arg, sizeof(CsAudioArg));
	arg.argSz = sizeof(CsAudioArg);
	arg.endpointType = CSAudioEndpointTypeDSP;
	arg.endpointRequest = CSAudioEndpointRegister;
	ExNotifyCallback(this->CSAudioAPICallback, &arg, &CsAudioArg2);

	return status;
}

NTSTATUS CCsAudioRk3xHW::CSAudioAPIDeinit() {
	if (this->CSAudioAPICallbackObj) {
		ExUnregisterCallback(this->CSAudioAPICallbackObj);
		this->CSAudioAPICallbackObj = NULL;
	}

	if (this->CSAudioAPICallback) {
		ObfDereferenceObject(this->CSAudioAPICallback);
		this->CSAudioAPICallback = NULL;
	}
	return STATUS_SUCCESS;
}

void CCsAudioRk3xHW::CSAudioAPICalled(CsAudioArg arg) {
	if (arg.endpointRequest == CSAudioEndpointRegister) {
		CsAudioArg newArg;
		RtlZeroMemory(&newArg, sizeof(CsAudioArg));
		newArg.argSz = sizeof(CsAudioArg);
		newArg.endpointType = arg.endpointType;
		newArg.endpointRequest = CSAudioEndpointStop;
		ExNotifyCallback(this->CSAudioAPICallback, &newArg, &CsAudioArg2);
	}
}

CSAudioEndpointType CCsAudioRk3xHW::GetCSAudioEndpoint(eDeviceType deviceType) {
	switch (deviceType) {
	case eOutputDevice:
		return CSAudioEndpointTypeHeadphone;
	case eInputDevice:
		return CSAudioEndpointTypeMicJack;
	}
	return CSAudioEndpointTypeDSP;
}

eDeviceType CCsAudioRk3xHW::GetDeviceType(CSAudioEndpointType endpointType) {
	switch (endpointType) {
	case CSAudioEndpointTypeHeadphone:
		return eOutputDevice;
	case CSAudioEndpointTypeMicJack:
		return eInputDevice;
	}
	return eOutputDevice;
}