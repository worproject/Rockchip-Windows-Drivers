#if !defined(_RK3XI2C_H_)
#define _RK3XI2C_H_

#pragma warning(disable:4200)  // suppress nameless struct/union warning
#pragma warning(disable:4201)  // suppress nameless struct/union warning
#pragma warning(disable:4214)  // suppress bit field types other than int warning
#include <initguid.h>
#include <wdm.h>

#pragma warning(default:4200)
#pragma warning(default:4201)
#pragma warning(default:4214)
#include <wdf.h>
#include <acpiioct.h>
#include <SPBCx.h>

#define RESHUB_USE_HELPER_ROUTINES
#include <reshub.h>
#include <pshpack1.h>

typedef struct _PNP_I2C_SERIAL_BUS_DESCRIPTOR {
    PNP_SERIAL_BUS_DESCRIPTOR SerialBusDescriptor;
    ULONG ConnectionSpeed;
    USHORT SlaveAddress;
    // follwed by optional Vendor Data
    // followed by PNP_IO_DESCRIPTOR_RESOURCE_NAME
} PNP_I2C_SERIAL_BUS_DESCRIPTOR, * PPNP_I2C_SERIAL_BUS_DESCRIPTOR;

#include <poppack.h>

#include "rk3xi2c.h"

//
// String definitions
//

#define DRIVERNAME                 "rk3xi2c.sys: "

#define RK3XI2C_POOL_TAG            (ULONG) 'CIKR'
#define RK3XI2C_HARDWARE_IDS        L"CoolStar\\RK3XI2C\0\0"
#define RK3XI2C_HARDWARE_IDS_LENGTH sizeof(RK3XI2C_HARDWARE_IDS)

#define true 1
#define false 0

/* I2C Frequency Modes */
#define I2C_MAX_STANDARD_MODE_FREQ	100000
#define I2C_MAX_FAST_MODE_FREQ		400000
#define I2C_MAX_FAST_MODE_PLUS_FREQ	1000000
#define I2C_MAX_TURBO_MODE_FREQ		1400000
#define I2C_MAX_HIGH_SPEED_MODE_FREQ	3400000
#define I2C_MAX_ULTRA_FAST_MODE_FREQ	5000000

/**
 * struct i2c_timings - I2C timing information
 * @bus_freq_hz: the bus frequency in Hz
 * @scl_rise_ns: time SCL signal takes to rise in ns; t(r) in the I2C specification
 * @scl_fall_ns: time SCL signal takes to fall in ns; t(f) in the I2C specification
 * @scl_int_delay_ns: time IP core additionally needs to setup SCL in ns
 * @sda_fall_ns: time SDA signal takes to fall in ns; t(f) in the I2C specification
 * @sda_hold_ns: time IP core additionally needs to hold SDA in ns
 * @digital_filter_width_ns: width in ns of spikes on i2c lines that the IP core
 *	digital filter can filter out
 * @analog_filter_cutoff_freq_hz: threshold frequency for the low pass IP core
 *	analog filter
 */
struct i2c_timings {
    UINT32 bus_freq_hz;
    UINT32 scl_rise_ns;
    UINT32 scl_fall_ns;
    UINT32 scl_int_delay_ns;
    UINT32 sda_fall_ns;
    UINT32 sda_hold_ns;
    UINT32 digital_filter_width_ns;
    UINT32 analog_filter_cutoff_freq_hz;
};

typedef struct _RK3XI2C_CONTEXT
{
	//
	// Handle back to the WDFDEVICE
	//

	WDFDEVICE FxDevice;

    PVOID MMIOAddress;
    SIZE_T MMIOSize;

    //I2C Link
    UINT32 baseClock;
    PVOID Rk3xI2CBusContext;
    WDFIOTARGET busIoTarget;
    UINT8 busNumber;

    WDFINTERRUPT Interrupt;

    //Synchronization
    WDFWAITLOCK waitLock;
    KEVENT waitEvent;
    BOOLEAN isBusy;

    //Current Message
    PMDL currentMDLChain;
    SPB_TRANSFER_DESCRIPTOR currentDescriptor;
    UINT8 I2CAddress;
    unsigned int mode;
    BOOLEAN isLastMsg;

    //State Machine
    enum rk3x_i2c_state state;
    UINT32 processed;
    NTSTATUS transactionStatus;

    //SOC Data
    struct i2c_timings timings;
    NTSTATUS (*calcTimings)(unsigned long, struct i2c_timings*, struct rk3x_i2c_calced_timings*);
} RK3XI2C_CONTEXT, *PRK3XI2C_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(RK3XI2C_CONTEXT, GetDeviceContext)

//
// Function definitions
//

DRIVER_INITIALIZE DriverEntry;

EVT_WDF_DRIVER_UNLOAD Rk3xI2CDriverUnload;

EVT_WDF_DRIVER_DEVICE_ADD Rk3xI2CEvtDeviceAdd;

EVT_WDF_IO_QUEUE_IO_INTERNAL_DEVICE_CONTROL Rk3xI2CEvtInternalDeviceControl;

UINT32 read32(PRK3XI2C_CONTEXT pDevice, UINT32 reg);
void write32(PRK3XI2C_CONTEXT pDevice, UINT32 reg, UINT32 val);

BOOLEAN rk3x_i2c_irq(WDFINTERRUPT Interrupt, ULONG MessageID);
NTSTATUS i2c_xfer(PRK3XI2C_CONTEXT pDevice,
    _In_ SPBTARGET SpbTarget,
    _In_ SPBREQUEST SpbRequest,
    _In_ ULONG TransferCount);
NTSTATUS rk3x_i2c_v1_calc_timings(unsigned long clk_rate,
    struct i2c_timings* t,
    struct rk3x_i2c_calced_timings* t_calc);
void rk3x_i2c_adapt_div(PRK3XI2C_CONTEXT pDevice, unsigned long clk_rate);
void updateClockSettings(PRK3XI2C_CONTEXT pDevice);

//
// Helper macros
//

#define DEBUG_LEVEL_ERROR   1
#define DEBUG_LEVEL_INFO    2
#define DEBUG_LEVEL_VERBOSE 3

#define DBG_INIT  1
#define DBG_PNP   2
#define DBG_IOCTL 4

#if 0
#define Rk3xI2CPrint(dbglevel, dbgcatagory, fmt, ...) {          \
    if (Rk3xI2CDebugLevel >= dbglevel &&                         \
        (Rk3xI2CDebugCatagories && dbgcatagory))                 \
		    {                                                           \
        DbgPrint(DRIVERNAME);                                   \
        DbgPrint(fmt, __VA_ARGS__);                             \
		    }                                                           \
}
#else
#define Rk3xI2CPrint(dbglevel, fmt, ...) {                       \
}
#endif
#endif