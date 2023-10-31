#ifndef __CROS_EC_REGS_H__
#define __CROS_EC_REGS_H__

#define BITS_PER_LONG sizeof(LONG) * 8
#define BITS_PER_LONG_LONG sizeof(LONGLONG) * 8
#define BIT(nr) (1UL << (nr))

#define GENMASK(h, l) (((~0UL) - (1UL << (l)) + 1) & (~0UL >> (BITS_PER_LONG - 1 - (h))))
#define GENMASK_ULL(h, l) (((~0ULL) - (1ULL << (l)) + 1) & (~0ULL >> (BITS_PER_LONG_LONG - 1 - (h))))

typedef enum ADDRESS_MODE
{
    AddressMode7Bit,
    AddressMode10Bit
}
ADDRESS_MODE, * PADDRESS_MODE;

typedef struct PBC_TARGET_SETTINGS
{
    // TODO: Update this structure to include other
    //       target settings needed to configure the
    //       controller (i.e. connection speed, phase/
    //       polarity for SPI).

    ADDRESS_MODE                  AddressMode;
    USHORT                        Address;
    ULONG                         ConnectionSpeed;
}
PBC_TARGET_SETTINGS, * PPBC_TARGET_SETTINGS;

/////////////////////////////////////////////////
//
// Context definitions.
//
/////////////////////////////////////////////////

typedef struct PBC_TARGET   PBC_TARGET, * PPBC_TARGET;

struct PBC_TARGET
{
    // TODO: Update this structure with variables that 
    //       need to be stored in the target context.

    // Handle to the SPB target.
    SPBTARGET                      SpbTarget;

    // Target specific settings.
    PBC_TARGET_SETTINGS            Settings;
};

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(PBC_TARGET, GetTargetContext);

#define I2C_SERIAL_BUS_TYPE 0x01
#define I2C_SERIAL_BUS_SPECIFIC_FLAG_10BIT_ADDRESS 0x0001

/* Register Map */
#define REG_CON        0x00 /* control register */
#define REG_CLKDIV     0x04 /* clock divisor register */
#define REG_MRXADDR    0x08 /* slave address for REGISTER_TX */
#define REG_MRXRADDR   0x0c /* slave register address for REGISTER_TX */
#define REG_MTXCNT     0x10 /* number of bytes to be transmitted */
#define REG_MRXCNT     0x14 /* number of bytes to be received */
#define REG_IEN        0x18 /* interrupt enable */
#define REG_IPD        0x1c /* interrupt pending */
#define REG_FCNT       0x20 /* finished count */

/* Data buffer offsets */
#define TXBUFFER_BASE 0x100
#define RXBUFFER_BASE 0x200

/* REG_CON bits */
#define REG_CON_EN        BIT(0)
enum {
    REG_CON_MOD_TX = 0,      /* transmit data */
    REG_CON_MOD_REGISTER_TX, /* select register and restart */
    REG_CON_MOD_RX,          /* receive data */
    REG_CON_MOD_REGISTER_RX, /* broken: transmits read addr AND writes
                  * register addr */
};
#define REG_CON_MOD(mod)  ((mod) << 1)
#define REG_CON_MOD_MASK  (BIT(1) | BIT(2))
#define REG_CON_START     BIT(3)
#define REG_CON_STOP      BIT(4)
#define REG_CON_LASTACK   BIT(5) /* 1: send NACK after last received byte */
#define REG_CON_ACTACK    BIT(6) /* 1: stop if NACK is received */

#define REG_CON_TUNING_MASK GENMASK_ULL(15, 8)

#define REG_CON_SDA_CFG(cfg) ((cfg) << 8)
#define REG_CON_STA_CFG(cfg) ((cfg) << 12)
#define REG_CON_STO_CFG(cfg) ((cfg) << 14)

/* REG_MRXADDR bits */
#define REG_MRXADDR_VALID(x) BIT(24 + (x)) /* [x*8+7:x*8] of MRX[R]ADDR valid */

/* REG_IEN/REG_IPD bits */
#define REG_INT_BTF       BIT(0) /* a byte was transmitted */
#define REG_INT_BRF       BIT(1) /* a byte was received */
#define REG_INT_MBTF      BIT(2) /* master data transmit finished */
#define REG_INT_MBRF      BIT(3) /* master data receive finished */
#define REG_INT_START     BIT(4) /* START condition generated */
#define REG_INT_STOP      BIT(5) /* STOP condition generated */
#define REG_INT_NAKRCV    BIT(6) /* NACK received */
#define REG_INT_ALL       0x7f

/* Constants */
#define WAIT_TIMEOUT      1000 /* ms */
#define DEFAULT_SCL_RATE  (100 * 1000) /* Hz */

/**
 * struct i2c_spec_values - I2C specification values for various modes
 * @min_hold_start_ns: min hold time (repeated) START condition
 * @min_low_ns: min LOW period of the SCL clock
 * @min_high_ns: min HIGH period of the SCL cloc
 * @min_setup_start_ns: min set-up time for a repeated START conditio
 * @max_data_hold_ns: max data hold time
 * @min_data_setup_ns: min data set-up time
 * @min_setup_stop_ns: min set-up time for STOP condition
 * @min_hold_buffer_ns: min bus free time between a STOP and
 * START condition
 */
struct i2c_spec_values {
    unsigned long min_hold_start_ns;
    unsigned long min_low_ns;
    unsigned long min_high_ns;
    unsigned long min_setup_start_ns;
    unsigned long max_data_hold_ns;
    unsigned long min_data_setup_ns;
    unsigned long min_setup_stop_ns;
    unsigned long min_hold_buffer_ns;
};

static const struct i2c_spec_values standard_mode_spec = {
    .min_hold_start_ns = 4000,
    .min_low_ns = 4700,
    .min_high_ns = 4000,
    .min_setup_start_ns = 4700,
    .max_data_hold_ns = 3450,
    .min_data_setup_ns = 250,
    .min_setup_stop_ns = 4000,
    .min_hold_buffer_ns = 4700,
};

static const struct i2c_spec_values fast_mode_spec = {
    .min_hold_start_ns = 600,
    .min_low_ns = 1300,
    .min_high_ns = 600,
    .min_setup_start_ns = 600,
    .max_data_hold_ns = 900,
    .min_data_setup_ns = 100,
    .min_setup_stop_ns = 600,
    .min_hold_buffer_ns = 1300,
};

static const struct i2c_spec_values fast_mode_plus_spec = {
    .min_hold_start_ns = 260,
    .min_low_ns = 500,
    .min_high_ns = 260,
    .min_setup_start_ns = 260,
    .max_data_hold_ns = 400,
    .min_data_setup_ns = 50,
    .min_setup_stop_ns = 260,
    .min_hold_buffer_ns = 500,
};

/**
 * struct rk3x_i2c_calced_timings - calculated V1 timings
 * @div_low: Divider output for low
 * @div_high: Divider output for high
 * @tuning: Used to adjust setup/hold data time,
 * setup/hold start time and setup stop time for
 * v1's calc_timings, the tuning should all be 0
 * for old hardware anyone using v0's calc_timings.
 */
struct rk3x_i2c_calced_timings {
    unsigned long div_low;
    unsigned long div_high;
    unsigned int tuning;
};

enum rk3x_i2c_state {
    STATE_IDLE,
    STATE_START,
    STATE_READ,
    STATE_WRITE,
    STATE_STOP
};

NTSTATUS
MdlChainGetByte(
    PMDL pMdlChain,
    size_t Length,
    size_t Index,
    UCHAR* pByte);

NTSTATUS
MdlChainSetByte(
    PMDL pMdlChain,
    size_t Length,
    size_t Index,
    UCHAR Byte
);

#endif /* __CROS_EC_REGS_H__ */