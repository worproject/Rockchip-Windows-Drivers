/*-
 *  SPDX-License-Identifier: BSD-3-Clause
 *
 *  Copyright (c) 2015, DataCore Software Corporation. All rights reserved.
 */

/*
 * DbgPrint support for WPP tracing.
 */

#ifndef _DCS_TRACEDBG_H_
#define _DCS_TRACEDBG_H_

/*
 * Define trace types
 */

struct TraceData {
	const void* data;
	unsigned short len;
	TraceData(const void* d, size_t l) : data(d), len((short)l) { }
	template<class T> explicit TraceData(const T* d) : data(d), len((short)sizeof(T)) { }
};

/*
 * CDbgPrintTrace - Expand trace output
 */

class CDbgPrintTrace {
	static const size_t MaxLength = 512;
	char out[MaxLength + 1];
	size_t curIndex;

	const char *pInfo;
	bool passive;

public:
	explicit CDbgPrintTrace(const char *s) : pInfo(s), curIndex(0) {
		passive = isPassive();
		out[0] = 0;
	}

	const char *Append(const char *s, size_t n = MaxLength) {
		if (!s) return Append("(null)");
		if (curIndex && n && *s == '\n' && out[curIndex-1] == '\n') curIndex--;
		if (n > MaxLength - curIndex) n = MaxLength - curIndex;
		while (n-- && *s) out[curIndex++] = *s++;
		out[curIndex] = 0;
		return out;
	}

	const char *Append(const wchar_t *s, size_t n = MaxLength) {
		if (!s) return Append("(null)");
		if (curIndex && n && *s == '\n' && out[curIndex-1] == '\n') curIndex--;
		if (n > MaxLength - curIndex) n = MaxLength - curIndex;
		for (; n-- && *s; s++) out[curIndex++] = (*s <= 0x7f) ? (char)*s : '?';
		out[curIndex] = 0;
		return out;
	}

	const char *Append(const char *&cp, va_list &ap) {
		bool wpp = (*++cp == '!');
		const char *ep = (wpp) ? strchr(cp + 1, '!') : strpbrk(cp, "cCdiouxXnpsSZ%");
		size_t n = (ep) ? (++ep) - cp + 1 - (wpp ? 3 : 0) : 0;
		char arg[40];

		if (!n || n >= sizeof(arg)) return out;

		strncpy(arg, ep - n - (wpp ? 1 : 0), n);
		arg[n] = 0;

		if (!wpp && strspn(arg, "cCdiouxXnpsSZ%lwIh.*0123456789+-# ") != n)
			return out;

		cp = ep;
		return (wpp) ? Expand(arg, ap) : Format(arg, ap);
	}

	static bool isPassive() {
#ifdef _DCS_KERNEL_
		return (KeGetCurrentIrql() == PASSIVE_LEVEL);
#else
		return true;
#endif
	}

private:
	const char *Expand(const char *arg, va_list &ap);
	const char *ExpandProject(const char *arg, va_list &ap);
	const char *ExpandCustom(const char *arg, va_list &ap);

	const char *Format(const char *arg, va_list &ap) {
		PrintArg(arg, ap);
		NextArg(arg, ap);
		return out;
	}

	const char *Print(const char *fmt, ...) {
		va_list ap;
		va_start(ap, fmt);
		PrintArg(fmt, ap);
		va_end(ap);
		return out;
	}

	const char *PrintArg(const char *fmt, va_list ap) {
#ifdef _DCS_KERNEL_
		const char *ep;
		if (!passive && *fmt == '%' && (ep = strpbrk(fmt + 1, "cCdiouxXnpsSZ%!")) != NULL) {
			if (*ep == 'Z' && ep[-1] == 'w') {
				UNICODE_STRING *u = va_arg(ap, UNICODE_STRING *);
				return (u) ? Append(u->Buffer, u->Length / sizeof(wchar_t)) : Append("(null)");
			}

			if (*ep == 'S' || (*ep == 's' && (ep[-1] == 'w' || ep[-1] == 'l')))
				return Append(va_arg(ap, wchar_t *));

			if (*ep == 'C' || (*ep == 'c' && (ep[-1] == 'w' || ep[-1] == 'l'))) {
				wchar_t c = va_arg(ap, wchar_t);
				return Append(&c, 1);
			}
		}
#endif
		size_t rc = _vsnprintf(out + curIndex, MaxLength - curIndex, fmt, ap);
		curIndex = (rc == -1) ? MaxLength : curIndex + rc;
		out[curIndex] = 0;
		return out;
	}

	#define va_next(ap,type) (void)va_arg(ap,type)

	void NextArg(const char *s, va_list &ap) {
		const char *xp;
		if (s[0] != '%' || s[1] == '%') return;

		for (xp = s; (xp = strchr(xp, '*')) != NULL; xp++)
			va_next(ap, int);

		if (strpbrk(s, "npsSZ")) va_next(ap, void *);

		else if (strpbrk(s, "cC")) {
			if (strpbrk(s, "lwC")) va_next(ap, wchar_t);
			else va_next(ap, char);
		}

		else if ((xp = strchr(s, 'I')) != NULL) {
			if (!strncmp(xp, "I64", 3)) va_next(ap, __int64);
			else if (!strncmp(xp, "I32", 3)) va_next(ap, __int32);
			else va_next(ap, size_t);
		}

		else if ((xp = strchr(s, 'h')) != NULL) {
			if (xp[1] == 'h') va_next(ap, char);
			else va_next(ap, short);
		}

		else if ((xp = strchr(s, 'l')) != NULL) {
			if (xp[1] == 'l') va_next(ap, long long);
			else va_next(ap, long);
		}

		else va_next(ap, int);
	}

	#undef va_next

	int GetTimeUnit(__int64 &t, int units) {
		int v = (int)((t < 0 ? (-t) : t) % units);
		t /= units;
		return v;
	}

	const char *GetInfo(int n) {
		const char *cp = pInfo;

		for (int i = 0; cp && *cp && i < n; i++)
			cp += strlen(cp) + 1;

		return (cp && *cp) ? cp : "(null)";
	}

	static const char *strpbrk(const char *cp, const char *list) {
		for (const char *xp = cp; *xp; xp++)
			if (strchr(list, *xp)) return xp;
		return NULL;
	}
};


/*
 * Type definition macros
 */

#define WPP_DEFINE_ALIAS(name,defn) \
	if (!strcmp(arg, #name)) { return Expand(#defn, ap); }

#define WPP_DEFINE_TYPE(name,type,fmt) \
	if (!strcmp(arg, #name)) { return Print(#fmt, va_arg(ap, type)); }


/*
 * List definition macros
 */

#define WPP_DEFINE_LIST(name,type,fmt,...) \
	if (!strcmp(arg, #name)) { \
		type value = va_arg(ap, type); \
		switch (value) { __VA_ARGS__ default: Print(#fmt, value); } \
		return out; \
	}

#define WPP_DEFINE_STATUS(name,type,fmt,...) \
	if (!strcmp(arg, #name)) { \
		type value = va_arg(ap, type); \
		Print(#fmt, value); \
		switch (value) { __VA_ARGS__ } \
		return out; \
	}

#define WPP_DEFINE_VALUE(value,name) \
	case value : Print("(%s)", #name); break;

#define WPP_DEFINE_ENUM_VALUE(value) \
	case value : Print("%s", #value); break;

/*
 * Define default WPP types
 */

__inline const char *CDbgPrintTrace::Expand(const char *arg, va_list &ap)
{
	if (ExpandCustom(arg, ap))
		return out;

	switch ((arg[0] << 8) | arg[1]) {
	case 'AN':
		WPP_DEFINE_TYPE(ANSTR, void *, %Z)
		break;

	case 'AR':
		WPP_DEFINE_TYPE(ARSTR, void *, %s)
		WPP_DEFINE_TYPE(ARWSTR, void *, %S)
		break;

	case 'AS':
		WPP_DEFINE_TYPE(ASTR, void *, %s)
		break;

	case 'bo':
		WPP_DEFINE_ALIAS(bool, BOOLEAN)
		WPP_DEFINE_ALIAS(bool8, BOOLEAN)
		WPP_DEFINE_LIST(bool16, unsigned short, %hu,
			WPP_DEFINE_VALUE(0, FALSE)
			WPP_DEFINE_VALUE(1, TRUE)
		)
		break;

	case 'BO':
		WPP_DEFINE_LIST(BOOLEAN, unsigned char, %hhu,
			WPP_DEFINE_VALUE(0, FALSE)
			WPP_DEFINE_VALUE(1, TRUE)
		)
		break;

	case 'CL':
		WPP_DEFINE_ALIAS(CLSID, GUID)
		break;

	case 'CS':
		WPP_DEFINE_TYPE(CSTR, void *, %Z)
		break;

	case 'da':
		WPP_DEFINE_ALIAS(datetime, TIMESTAMP)
		break;

	case 'DA':
		WPP_DEFINE_ALIAS(DATE, TIMESTAMP)
		break;

	case 'de':
		if (!strcmp(arg, "delta")) {
			__int64 t = va_arg(ap, __int64) / (10 * 1000);
			const char *sign = (t < 0) ? "-" : "";

			int msecs = GetTimeUnit(t, 1000);
			int seconds = GetTimeUnit(t, 60);
			if (!t) return Print("%s%d.%03ds", sign, seconds, msecs);

			int minutes = GetTimeUnit(t, 60);
			if (!t) return Print("%s%d:%02d.%03ds", sign, minutes, seconds, msecs);

			int hours = GetTimeUnit(t, 24);
			if (!t) return Print("%s%d:%02d:%02d.%03ds", sign, hours, minutes, seconds, msecs);

			int days = (int)((t < 0) ? (-t) : t);
			return Print("%s%d~%d:%02d:%02d.%03ds", sign, days, hours, minutes, seconds, msecs);
		}
		break;

	case 'du':
		WPP_DEFINE_ALIAS(due, TIMESTAMP)
		break;

	case 'FI':
		if (!strcmp(arg, "FILE"))
			return Append(GetInfo(0));
		break;

	case 'FL':
		if (!strcmp(arg, "FLAGS"))
			return Append(GetInfo(3));
		break;

	case 'FU':
		if (!strcmp(arg, "FUNC"))
			return Append(GetInfo(2));
		break;

	case 'gu':
		WPP_DEFINE_ALIAS(guid, GUID)
		break;

	case 'GU':
		if (!strcmp(arg, "GUID")) {
			GUID *pGuid = va_arg(ap, GUID *);

			if (!pGuid) return Append("(null)");

			return Print("%08lx-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
				pGuid->Data1, pGuid->Data2, pGuid->Data3, pGuid->Data4[0],
				pGuid->Data4[1], pGuid->Data4[2], pGuid->Data4[3], pGuid->Data4[4],
				pGuid->Data4[5], pGuid->Data4[6], pGuid->Data4[7]);
		}
		break;

	case 'HA':
		WPP_DEFINE_TYPE(HANDLE, void *, %p)
		break;

	case 'HE':
		if (!strcmp(arg, "HEXDUMP")) {
			TraceData v = va_arg(ap, TraceData);
			unsigned char *pData = (unsigned char *)v.data;
			unsigned short len = v.len;

			if (!pData || !len)
				return Append(len ? "(null)" : "<NULL>");

			for (short i = 0; i < len; i += 16) {
				char x[40], c[20];

				for (short j = 0; j < 16; j++) {
					sprintf(&x[j*2+j/4], (i+j < len) ? "%02x " : "   ", pData[i+j]);
					c[j] = (i+j >= len) ? '\0' : isprint(pData[i+j]) ? pData[i+j] : '.';
				}

				Print("\n\t%04x  %s  %.16s", i, x, c);
			}

			return Append("\n");
		}
		break;

	case 'hr':
		WPP_DEFINE_ALIAS(hresult, HRESULT)
		break;

	case 'HR':
		WPP_DEFINE_TYPE(HRESULT, int, %#010x)
		break;

	case 'II':
		WPP_DEFINE_ALIAS(IID, GUID)
		break;

	case 'ip':
		WPP_DEFINE_ALIAS(ipaddr, IPADDR)
		break;

	case 'IP':
		if (!strcmp(arg, "IPADDR")) {
			unsigned long ip = va_arg(ap, unsigned long);
			return Print("%d.%d.%d.%d", ip & 0xff,
				(ip >> 8) & 0xff, (ip >> 16) & 0xff, (ip >> 24) & 0xff);
		}
		break;

	case 'ir':
		WPP_DEFINE_LIST(irql, unsigned char, %hhu,
			WPP_DEFINE_VALUE(0, Low)
			WPP_DEFINE_VALUE(1, APC)
			WPP_DEFINE_VALUE(2, DPC)
			)
		break;

	case 'LE':
		if (!strcmp(arg, "LEVEL"))
			return Append(GetInfo(4));
		break;

	case 'LI':
		if (!strcmp(arg, "LINE"))
			return Append(GetInfo(1));

		WPP_DEFINE_ALIAS(LIBID, GUID)
		break;

	case 'ND':
		WPP_DEFINE_TYPE(NDIS_STATUS, int, %#010x)
		break;

	case 'NT':
		WPP_DEFINE_ALIAS(NTerror, WINERROR)
		break;

	case 'OB':
		WPP_DEFINE_TYPE(OBYTE, unsigned char, %#hho)
		break;

	case 'OI':
		WPP_DEFINE_TYPE(OINT, int, %#o)
		WPP_DEFINE_TYPE(OINT64, __int64, %#I64o)
		break;

	case 'OL':
		WPP_DEFINE_TYPE(OLONG, long, %#lo)
		WPP_DEFINE_TYPE(OLONGPTR, LONG_PTR, %#Io)
		break;

	case 'OS':
		WPP_DEFINE_TYPE(OSHORT, unsigned short, %#ho)
		break;

	case 'pn':
		WPP_DEFINE_LIST(pnpmj, unsigned char, %hhu,
			WPP_DEFINE_VALUE(0, IRP_MJ_CREATE)
			WPP_DEFINE_VALUE(1, IRP_MJ_CREATE_NAMED_PIPE)
			WPP_DEFINE_VALUE(2, IRP_MJ_CLOSE)
			WPP_DEFINE_VALUE(3, IRP_MJ_READ)
			WPP_DEFINE_VALUE(4, IRP_MJ_WRITE)
			WPP_DEFINE_VALUE(5, IRP_MJ_QUERY_INFORMATION)
			WPP_DEFINE_VALUE(6, IRP_MJ_SET_INFORMATION)
			WPP_DEFINE_VALUE(7, IRP_MJ_QUERY_EA)
			WPP_DEFINE_VALUE(8, IRP_MJ_SET_EA)
			WPP_DEFINE_VALUE(9, IRP_MJ_FLUSH_BUFFERS)
			WPP_DEFINE_VALUE(10, IRP_MJ_QUERY_VOLUME_INFORMATION)
			WPP_DEFINE_VALUE(11, IRP_MJ_SET_VOLUME_INFORMATION)
			WPP_DEFINE_VALUE(12, IRP_MJ_DIRECTORY_CONTROL)
			WPP_DEFINE_VALUE(13, IRP_MJ_FILE_SYSTEM_CONTROL)
			WPP_DEFINE_VALUE(14, IRP_MJ_DEVICE_CONTROL)
			WPP_DEFINE_VALUE(15, IRP_MJ_INTERNAL_DEVICE_CONTROL)
			WPP_DEFINE_VALUE(16, IRP_MJ_SHUTDOWN)
			WPP_DEFINE_VALUE(17, IRP_MJ_LOCK_CONTROL)
			WPP_DEFINE_VALUE(18, IRP_MJ_CLEANUP)
			WPP_DEFINE_VALUE(19, IRP_MJ_CREATE_MAILSLOT)
			WPP_DEFINE_VALUE(20, IRP_MJ_QUERY_SECURITY)
			WPP_DEFINE_VALUE(21, IRP_MJ_SET_SECURITY)
			WPP_DEFINE_VALUE(22, IRP_MJ_POWER)
			WPP_DEFINE_VALUE(23, IRP_MJ_SYSTEM_CONTROL)
			WPP_DEFINE_VALUE(24, IRP_MJ_DEVICE_CHANGE)
			WPP_DEFINE_VALUE(25, IRP_MJ_QUERY_QUOTA)
			WPP_DEFINE_VALUE(26, IRP_MJ_SET_QUOTA)
			WPP_DEFINE_VALUE(27, IRP_MJ_PNP)
			)

		WPP_DEFINE_LIST(pnpmn, unsigned char, %hhu,
			WPP_DEFINE_VALUE(0, IRP_MN_START_DEVICE)
			WPP_DEFINE_VALUE(1, IRP_MN_QUERY_REMOVE_DEVICE)
			WPP_DEFINE_VALUE(2, IRP_MN_REMOVE_DEVICE)
			WPP_DEFINE_VALUE(3, IRP_MN_CANCEL_REMOVE_DEVICE)
			WPP_DEFINE_VALUE(4, IRP_MN_STOP_DEVICE)
			WPP_DEFINE_VALUE(5, IRP_MN_QUERY_STOP_DEVICE)
			WPP_DEFINE_VALUE(6, IRP_MN_CANCEL_STOP_DEVICE)
			WPP_DEFINE_VALUE(7, IRP_MN_QUERY_DEVICE_RELATIONS)
			WPP_DEFINE_VALUE(8, IRP_MN_QUERY_INTERFACE)
			WPP_DEFINE_VALUE(9, IRP_MN_QUERY_CAPABILITIES)
			WPP_DEFINE_VALUE(10, IRP_MN_QUERY_RESOURCES)
			WPP_DEFINE_VALUE(11, IRP_MN_QUERY_RESOURCE_REQUIREMENTS)
			WPP_DEFINE_VALUE(12, IRP_MN_QUERY_DEVICE_TEXT)
			WPP_DEFINE_VALUE(13, IRP_MN_FILTER_RESOURCE_REQUIREMENTS)
			WPP_DEFINE_VALUE(14, IRP_MN_PNP_14)
			WPP_DEFINE_VALUE(15, IRP_MN_READ_CONFIG)
			WPP_DEFINE_VALUE(16, IRP_MN_WRITE_CONFIG)
			WPP_DEFINE_VALUE(17, IRP_MN_EJECT)
			WPP_DEFINE_VALUE(18, IRP_MN_SET_LOCK)
			WPP_DEFINE_VALUE(19, IRP_MN_QUERY_ID)
			WPP_DEFINE_VALUE(20, IRP_MN_QUERY_PNP_DEVICE_STATE)
			WPP_DEFINE_VALUE(21, IRP_MN_QUERY_BUS_INFORMATION)
			WPP_DEFINE_VALUE(22, IRP_MN_DEVICE_USAGE_NOTIFICATION)
			WPP_DEFINE_VALUE(23, IRP_MN_SURPRISE_REMOVAL)
			)
		break;

	case 'po':
		WPP_DEFINE_ALIAS(port, PORT)
		break;

	case 'PO':
		WPP_DEFINE_TYPE(PORT, unsigned short, %hu)
		break;

	case 'PT':
		WPP_DEFINE_TYPE(PTR, void *, %p)
		break;

	case 'SB':
		WPP_DEFINE_TYPE(SBYTE, signed char, %hhd)
		break;

	case 'SC':
		WPP_DEFINE_TYPE(SCHAR, unsigned char, %c)
		break;

	case 'SI':
		WPP_DEFINE_TYPE(SINT, int, %d)
		WPP_DEFINE_TYPE(SINT64, __int64, %I64d)
		break;

	case 'SL':
		WPP_DEFINE_TYPE(SLONG, long, %ld)
		WPP_DEFINE_TYPE(SLONGPTR, LONG_PTR, %Id)
		break;

	case 'SP':
		if (!strcmp(arg, "SPACE"))
			return Append(" ");
		break;

	case 'SS':
		WPP_DEFINE_TYPE(SSHORT, short, %hd)
		break;

	case 'st':
		WPP_DEFINE_ALIAS(status, STATUS)
		break;

	case 'ST':
		WPP_DEFINE_STATUS(STATUS, unsigned long, %#010x,
			WPP_DEFINE_VALUE(0, STATUS_SUCCESS)
			WPP_DEFINE_VALUE(0x80L, STATUS_ABANDONED)
			WPP_DEFINE_VALUE(0x101L, STATUS_ALERTED)
			WPP_DEFINE_VALUE(0x102L, STATUS_TIMEOUT)
			WPP_DEFINE_VALUE(0x103L, STATUS_PENDING)
			WPP_DEFINE_VALUE(0x104L, STATUS_REPARSE)
			WPP_DEFINE_VALUE(0x105L, STATUS_MORE_ENTRIES)
			WPP_DEFINE_VALUE(0x106L, STATUS_SOME_NOT_MAPPED)
			WPP_DEFINE_VALUE(0x117L, STATUS_BUFFER_ALL_ZEROS)
			WPP_DEFINE_VALUE(0x40000000L, STATUS_OBJECT_NAME_EXISTS)
			WPP_DEFINE_VALUE(0x80000005L, STATUS_BUFFER_OVERFLOW)
			WPP_DEFINE_VALUE(0x80000006L, STATUS_NO_MORE_FILES)
			WPP_DEFINE_VALUE(0x8000000DL, STATUS_PARTIAL_COPY)
			WPP_DEFINE_VALUE(0x80000010L, STATUS_DEVICE_OFF_LINE)
			WPP_DEFINE_VALUE(0x80000011L, STATUS_DEVICE_BUSY)
			WPP_DEFINE_VALUE(0x8000001AL, STATUS_NO_MORE_ENTRIES)
			WPP_DEFINE_VALUE(0x8000001CL, STATUS_MEDIA_CHANGED)
			WPP_DEFINE_VALUE(0x8000001DL, STATUS_BUS_RESET)
			WPP_DEFINE_VALUE(0x8000001EL, STATUS_END_OF_MEDIA)
			WPP_DEFINE_VALUE(0x8000001FL, STATUS_BEGINNING_OF_MEDIA)
			WPP_DEFINE_VALUE(0x80000022L, STATUS_NO_DATA_DETECTED)
			WPP_DEFINE_VALUE(0xC0000001L, STATUS_UNSUCCESSFUL)
			WPP_DEFINE_VALUE(0xC0000002L, STATUS_NOT_IMPLEMENTED)
			WPP_DEFINE_VALUE(0xC0000004L, STATUS_INFO_LENGTH_MISMATCH)
			WPP_DEFINE_VALUE(0xC0000005L, STATUS_ACCESS_VIOLATION)
			WPP_DEFINE_VALUE(0xC0000008L, STATUS_INVALID_HANDLE)
			WPP_DEFINE_VALUE(0xC000000DL, STATUS_INVALID_PARAMETER)
			WPP_DEFINE_VALUE(0xC000000EL, STATUS_NO_SUCH_DEVICE)
			WPP_DEFINE_VALUE(0xC000000FL, STATUS_NO_SUCH_FILE)
			WPP_DEFINE_VALUE(0xC0000010L, STATUS_INVALID_DEVICE_REQUEST)
			WPP_DEFINE_VALUE(0xC0000011L, STATUS_END_OF_FILE)
			WPP_DEFINE_VALUE(0xC0000012L, STATUS_WRONG_VOLUME)
			WPP_DEFINE_VALUE(0xC0000013L, STATUS_NO_MEDIA_IN_DEVICE)
			WPP_DEFINE_VALUE(0xC0000014L, STATUS_UNRECOGNIZED_MEDIA)
			WPP_DEFINE_VALUE(0xC0000015L, STATUS_NONEXISTENT_SECTOR)
			WPP_DEFINE_VALUE(0xC0000016L, STATUS_MORE_PROCESSING_REQUIRED)
			WPP_DEFINE_VALUE(0xC0000017L, STATUS_NO_MEMORY)
			WPP_DEFINE_VALUE(0xC0000018L, STATUS_CONFLICTING_ADDRESSES)
			WPP_DEFINE_VALUE(0xC0000022L, STATUS_ACCESS_DENIED)
			WPP_DEFINE_VALUE(0xC0000023L, STATUS_BUFFER_TOO_SMALL)
			WPP_DEFINE_VALUE(0xC0000024L, STATUS_OBJECT_TYPE_MISMATCH)
			WPP_DEFINE_VALUE(0xC000002AL, STATUS_NOT_LOCKED)
			WPP_DEFINE_VALUE(0xC0000030L, STATUS_INVALID_PARAMETER_MIX)
			WPP_DEFINE_VALUE(0xC0000033L, STATUS_OBJECT_NAME_INVALID)
			WPP_DEFINE_VALUE(0xC0000034L, STATUS_OBJECT_NAME_NOT_FOUND)
			WPP_DEFINE_VALUE(0xC0000035L, STATUS_OBJECT_NAME_COLLISION)
			WPP_DEFINE_VALUE(0xC0000037L, STATUS_PORT_DISCONNECTED)
			WPP_DEFINE_VALUE(0xC0000038L, STATUS_DEVICE_ALREADY_ATTACHED)
			WPP_DEFINE_VALUE(0xC0000039L, STATUS_OBJECT_PATH_INVALID)
			WPP_DEFINE_VALUE(0xC000003AL, STATUS_OBJECT_PATH_NOT_FOUND)
			WPP_DEFINE_VALUE(0xC000003CL, STATUS_DATA_OVERRUN)
			WPP_DEFINE_VALUE(0xC000003EL, STATUS_DATA_ERROR)
			WPP_DEFINE_VALUE(0xC0000043L, STATUS_SHARING_VIOLATION)
			WPP_DEFINE_VALUE(0xC0000044L, STATUS_QUOTA_EXCEEDED)
			WPP_DEFINE_VALUE(0xC000007FL, STATUS_DISK_FULL)
			WPP_DEFINE_VALUE(0xC0000098L, STATUS_FILE_INVALID)
			WPP_DEFINE_VALUE(0xC000009AL, STATUS_INSUFFICIENT_RESOURCES)
			WPP_DEFINE_VALUE(0xC000009CL, STATUS_DEVICE_DATA_ERROR)
			WPP_DEFINE_VALUE(0xC000009DL, STATUS_DEVICE_NOT_CONNECTED)
			WPP_DEFINE_VALUE(0xC000009EL, STATUS_DEVICE_POWER_FAILURE)
			WPP_DEFINE_VALUE(0xC00000A0L, STATUS_MEMORY_NOT_ALLOCATED)
			WPP_DEFINE_VALUE(0xC00000A2L, STATUS_MEDIA_WRITE_PROTECTED)
			WPP_DEFINE_VALUE(0xC00000A3L, STATUS_DEVICE_NOT_READY)
			WPP_DEFINE_VALUE(0xC00000AEL, STATUS_PIPE_BUSY)
			WPP_DEFINE_VALUE(0xC00000AFL, STATUS_ILLEGAL_FUNCTION)
			WPP_DEFINE_VALUE(0xC00000B5L, STATUS_IO_TIMEOUT)
			WPP_DEFINE_VALUE(0xC00000BBL, STATUS_NOT_SUPPORTED)
			WPP_DEFINE_VALUE(0xC00000C0L, STATUS_DEVICE_DOES_NOT_EXIST)
			WPP_DEFINE_VALUE(0xC00000C2L, STATUS_ADAPTER_HARDWARE_ERROR)
			WPP_DEFINE_VALUE(0xC00000D0L, STATUS_REQUEST_NOT_ACCEPTED)
			WPP_DEFINE_VALUE(0xC00000E5L, STATUS_INTERNAL_ERROR)
			WPP_DEFINE_VALUE(0xC00000E9L, STATUS_UNEXPECTED_IO_ERROR)
			WPP_DEFINE_VALUE(0xC00000EFL, STATUS_INVALID_PARAMETER_1)
			WPP_DEFINE_VALUE(0xC00000F0L, STATUS_INVALID_PARAMETER_2)
			WPP_DEFINE_VALUE(0xC00000F1L, STATUS_INVALID_PARAMETER_3)
			WPP_DEFINE_VALUE(0xC00000F2L, STATUS_INVALID_PARAMETER_4)
			WPP_DEFINE_VALUE(0xC00000F3L, STATUS_INVALID_PARAMETER_5)
			WPP_DEFINE_VALUE(0xC00000F4L, STATUS_INVALID_PARAMETER_6)
			WPP_DEFINE_VALUE(0xC00000F5L, STATUS_INVALID_PARAMETER_7)
			WPP_DEFINE_VALUE(0xC00000F6L, STATUS_INVALID_PARAMETER_8)
			WPP_DEFINE_VALUE(0xC00000F7L, STATUS_INVALID_PARAMETER_9)
			WPP_DEFINE_VALUE(0xC00000F8L, STATUS_INVALID_PARAMETER_10)
			WPP_DEFINE_VALUE(0xC0000101L, STATUS_DIRECTORY_NOT_EMPTY)
			WPP_DEFINE_VALUE(0xC0000120L, STATUS_CANCELLED)
			WPP_DEFINE_VALUE(0xC0000173L, STATUS_DEVICE_NOT_PARTITIONED)
			WPP_DEFINE_VALUE(0xC0000178L, STATUS_NO_MEDIA)
			WPP_DEFINE_VALUE(0xC0000182L, STATUS_DEVICE_CONFIGURATION_ERROR)
			WPP_DEFINE_VALUE(0xC0000183L, STATUS_DRIVER_INTERNAL_ERROR)
			WPP_DEFINE_VALUE(0xC0000184L, STATUS_INVALID_DEVICE_STATE)
			WPP_DEFINE_VALUE(0xC0000185L, STATUS_IO_DEVICE_ERROR)
			WPP_DEFINE_VALUE(0xC0000186L, STATUS_DEVICE_PROTOCOL_ERROR)
			WPP_DEFINE_VALUE(0xC0000188L, STATUS_LOG_FILE_FULL)
			WPP_DEFINE_VALUE(0xC0000194L, STATUS_POSSIBLE_DEADLOCK)
			WPP_DEFINE_VALUE(0xC0000206L, STATUS_INVALID_BUFFER_SIZE)
			WPP_DEFINE_VALUE(0xC000020DL, STATUS_CONNECTION_RESET)
			WPP_DEFINE_VALUE(0xC0000225L, STATUS_NOT_FOUND)
			WPP_DEFINE_VALUE(0xC000022DL, STATUS_RETRY)
			WPP_DEFINE_VALUE(0xC0000237L, STATUS_GRACEFUL_DISCONNECT)
			)
		break;

	case 'sy':
		WPP_DEFINE_LIST(sysctrl, unsigned char, %hhu,
			WPP_DEFINE_VALUE(0, IRP_MN_QUERY_ALL_DATA)
			WPP_DEFINE_VALUE(1, IRP_MN_QUERY_SINGLE_INSTANCE)
			WPP_DEFINE_VALUE(2, IRP_MN_CHANGE_SINGLE_INSTANCE)
			WPP_DEFINE_VALUE(3, IRP_MN_CHANGE_SINGLE_ITEM)
			WPP_DEFINE_VALUE(4, IRP_MN_ENABLE_EVENTS)
			WPP_DEFINE_VALUE(5, IRP_MN_DISABLE_EVENTS)
			WPP_DEFINE_VALUE(6, IRP_MN_ENABLE_COLLECTION)
			WPP_DEFINE_VALUE(7, IRP_MN_DISABLE_COLLECTION)
			WPP_DEFINE_VALUE(8, IRP_MN_REGINFO)
			WPP_DEFINE_VALUE(9, IRP_MN_EXECUTE_METHOD)
			WPP_DEFINE_VALUE(10, IRP_MN_Reserved_0a)
			WPP_DEFINE_VALUE(11, IRP_MN_REGINFO_EX)
			)
		break;

	case 'ti':
		WPP_DEFINE_ALIAS(time, TIMESTAMP)
		break;

	case 'TI':
		if (!strcmp(arg, "TIMESTAMP")) {
#ifdef _DCS_KERNEL_
			LARGE_INTEGER v;
			TIME_FIELDS t;

			v.QuadPart = va_arg(ap, unsigned __int64);
			RtlTimeToTimeFields(&v, &t);

			return Print("%02hu/%02hu/%04hu-%02hu:%02hu:%02hu.%03hu",
				t.Month, t.Day, t.Year, t.Hour, t.Minute, t.Second, t.Milliseconds);
#else
			unsigned __int64 stamp = va_arg(ap, unsigned __int64);
			SYSTEMTIME t;
			FILETIME v;

			v.dwLowDateTime = (DWORD)(stamp & 0xFFFFFFFF);
			v.dwHighDateTime = (DWORD)(stamp >> 32);

			if (!FileTimeToSystemTime(&v, &t))
				return Print("%#I64x", stamp);

			return Print("%02hu/%02hu/%04hu-%02hu:%02hu:%02hu.%03hu",
				t.wMonth, t.wDay, t.wYear, t.wHour, t.wMinute, t.wSecond, t.wMilliseconds);
#endif
		}

		WPP_DEFINE_ALIAS(TIME, TIMESTAMP)
		break;

	case 'UB':
		WPP_DEFINE_TYPE(UBYTE, unsigned char, %hhu)
		break;

	case 'UC':
		WPP_DEFINE_TYPE(UCHAR, unsigned char, %c)
		break;

	case 'UI':
		WPP_DEFINE_TYPE(UINT, unsigned int, %u)
		WPP_DEFINE_TYPE(UINT64, unsigned __int64, %I64u)
		break;

	case 'UL':
		WPP_DEFINE_TYPE(ULONG, unsigned long, %lu)
		WPP_DEFINE_TYPE(ULONGPTR, ULONG_PTR, %Iu)
		break;

	case 'US':
		WPP_DEFINE_TYPE(USTR, void *, %wZ)
		WPP_DEFINE_TYPE(USHORT, unsigned short, %hu)
		break;

	case 'WA':
		WPP_DEFINE_ALIAS(WAITTIME, TIMESTAMP)
		break;

	case 'wi':
		WPP_DEFINE_ALIAS(winerr, WINERROR)
		break;

	case 'WI':
		WPP_DEFINE_TYPE(WINERROR, unsigned int, %d)
		break;

	case 'WS':
		WPP_DEFINE_TYPE(WSTR, void *, %S)
		break;

	case 'XB':
		WPP_DEFINE_TYPE(XBYTE, unsigned char, %#04hhx)
		break;

	case 'XI':
		WPP_DEFINE_TYPE(XINT, int, %#010x)
		WPP_DEFINE_TYPE(XINT64, __int64, %#I64x)
		break;

	case 'XL':
		WPP_DEFINE_TYPE(XLONG, long, %#010lx)
		WPP_DEFINE_TYPE(XLONGPTR, LONG_PTR, %#Ix)
		break;

	case 'XS':
		WPP_DEFINE_TYPE(XSHORT, unsigned short, %#06hx)
		break;

	case 'XX':
		WPP_DEFINE_TYPE(XXINT64, __int64, %#I64X)
		break;
	}

	return Print("%#x", va_arg(ap, int));
}


/*
 * DbgPrintTrace info string
 */

#ifndef __GNUC__
#define WPPTRACEINFO(lv,flg) \
	(__FILE__ "\0" _DCS_TRACE_STR(__LINE__) "\0" __FUNCTION__ "\0" #flg "\0" #lv "\0")
#else
#define WPPTRACEINFO(lv,flg) \
	(__FILE__ "\0" _DCS_TRACE_STR(__LINE__) "\0")
#endif


/*
 * DbgPrintTraceMsg - Tracing output.
 */

__inline void DbgPrintTraceMsg(unsigned long componentId, unsigned long level,
							   const char *info, const char *fmt, ...)
{
	if (DbgQueryDebugFilterState(componentId, level) != TRUE || !fmt)
		return;

	va_list ap;
	va_start(ap, fmt);

	const char *cp = CDbgPrintTrace::isPassive() ? fmt : "%!";

	for (; (cp = strchr(cp, '!')) != NULL; cp++)
		if (cp != fmt && cp[-1] == '%') {
			CDbgPrintTrace out(info);
			const char *ip = fmt;

			for (const char *xp; (xp = strchr(ip, '%')) != NULL; ip = xp) {
				if (xp != ip) out.Append(ip, xp - ip);
				out.Append(xp, ap);
			}

			DbgPrintEx(componentId, level, "%s", out.Append(ip));
			return (void)va_end(ap);
		}

	vDbgPrintEx(componentId, level, fmt, ap);
	va_end(ap);
}

#endif	/* _DCS_TRACEDBG_H_ */
