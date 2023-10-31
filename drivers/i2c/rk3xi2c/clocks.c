#include "driver.h"

static ULONG Rk3xI2CDebugLevel = 100;
static ULONG Rk3xI2CDebugCatagories = DBG_INIT || DBG_PNP || DBG_IOCTL;

const struct i2c_spec_values* rk3x_i2c_get_spec(unsigned int speed);

#define DIV_ROUND_UP(n, d) (((n) + (d) - 1) / (d))

/**
 * rk3x_i2c_v1_calc_timings - Calculate timing values for desired SCL frequency
 * @clk_rate: I2C input clock rate
 * @t: Known I2C timing information
 * @t_calc: Caculated rk3x private timings that would be written into regs
 *
 * Return: %0 on success, -%EINVAL if the goal SCL rate is too slow. In that case
 * a best-effort divider value is returned in divs. If the target rate is
 * too high, we silently use the highest possible rate.
 * The following formulas are v1's method to calculate timings.
 *
 * l = divl + 1;
 * h = divh + 1;
 * s = sda_update_config + 1;
 * u = start_setup_config + 1;
 * p = stop_setup_config + 1;
 * T = Tclk_i2c;
 *
 * tHigh = 8 * h * T;
 * tLow = 8 * l * T;
 *
 * tHD;sda = (l * s + 1) * T;
 * tSU;sda = [(8 - s) * l + 1] * T;
 * tI2C = 8 * (l + h) * T;
 *
 * tSU;sta = (8h * u + 1) * T;
 * tHD;sta = [8h * (u + 1) - 1] * T;
 * tSU;sto = (8h * p + 1) * T;
 */
NTSTATUS rk3x_i2c_v1_calc_timings(unsigned long clk_rate,
	struct i2c_timings* t,
	struct rk3x_i2c_calced_timings* t_calc)
{
	unsigned long min_low_ns, min_high_ns;
	unsigned long min_setup_start_ns, min_setup_data_ns;
	unsigned long min_setup_stop_ns, max_hold_data_ns;

	unsigned long clk_rate_khz, scl_rate_khz;

	unsigned long min_low_div, min_high_div;

	unsigned long min_div_for_hold, min_total_div;
	unsigned long extra_div, extra_low_div;
	unsigned long sda_update_cfg, stp_sta_cfg, stp_sto_cfg;

	const struct i2c_spec_values* spec;
	NTSTATUS status = STATUS_SUCCESS;

	/* Support standard-mode, fast-mode and fast-mode plus */
	if (t->bus_freq_hz > I2C_MAX_FAST_MODE_PLUS_FREQ)
		t->bus_freq_hz = I2C_MAX_FAST_MODE_PLUS_FREQ;

	/* prevent scl_rate_khz from becoming 0 */
	if (t->bus_freq_hz < 1000)
		t->bus_freq_hz = 1000;

	/*
	 * min_low_ns: The minimum number of ns we need to hold low to
	 *	       meet I2C specification, should include fall time.
	 * min_high_ns: The minimum number of ns we need to hold high to
	 *	        meet I2C specification, should include rise time.
	 */
	spec = rk3x_i2c_get_spec(t->bus_freq_hz);

	/* calculate min-divh and min-divl */
	clk_rate_khz = DIV_ROUND_UP(clk_rate, 1000);
	scl_rate_khz = t->bus_freq_hz / 1000;
	min_total_div = DIV_ROUND_UP(clk_rate_khz, scl_rate_khz * 8);

	min_high_ns = t->scl_rise_ns + spec->min_high_ns;
	min_high_div = DIV_ROUND_UP(clk_rate_khz * min_high_ns, 8 * 1000000);

	min_low_ns = t->scl_fall_ns + spec->min_low_ns;
	min_low_div = DIV_ROUND_UP(clk_rate_khz * min_low_ns, 8 * 1000000);

	/*
	 * Final divh and divl must be greater than 0, otherwise the
	 * hardware would not output the i2c clk.
	 */
	min_high_div = (min_high_div < 1) ? 2 : min_high_div;
	min_low_div = (min_low_div < 1) ? 2 : min_low_div;

	/* These are the min dividers needed for min hold times. */
	min_div_for_hold = (min_low_div + min_high_div);

	/*
	 * This is the maximum divider so we don't go over the maximum.
	 * We don't round up here (we round down) since this is a maximum.
	 */
	if (min_div_for_hold >= min_total_div) {
		/*
		 * Time needed to meet hold requirements is important.
		 * Just use that.
		 */
		t_calc->div_low = min_low_div;
		t_calc->div_high = min_high_div;
	}
	else {
		/*
		 * We've got to distribute some time among the low and high
		 * so we don't run too fast.
		 * We'll try to split things up by the scale of min_low_div and
		 * min_high_div, biasing slightly towards having a higher div
		 * for low (spend more time low).
		 */
		extra_div = min_total_div - min_div_for_hold;
		extra_low_div = DIV_ROUND_UP(min_low_div * extra_div,
			min_div_for_hold);

		t_calc->div_low = min_low_div + extra_low_div;
		t_calc->div_high = min_high_div + (extra_div - extra_low_div);
	}

	/*
	 * calculate sda data hold count by the rules, data_upd_st:3
	 * is a appropriate value to reduce calculated times.
	 */
	for (sda_update_cfg = 3; sda_update_cfg > 0; sda_update_cfg--) {
		max_hold_data_ns = DIV_ROUND_UP((sda_update_cfg
			* (t_calc->div_low) + 1)
			* 1000000, clk_rate_khz);
		min_setup_data_ns = DIV_ROUND_UP(((8 - sda_update_cfg)
			* (t_calc->div_low) + 1)
			* 1000000, clk_rate_khz);
		if ((max_hold_data_ns < spec->max_data_hold_ns) &&
			(min_setup_data_ns > spec->min_data_setup_ns))
			break;
	}

	/* calculate setup start config */
	min_setup_start_ns = t->scl_rise_ns + spec->min_setup_start_ns;
	stp_sta_cfg = DIV_ROUND_UP(clk_rate_khz * min_setup_start_ns
		- 1000000, 8 * 1000000 * (t_calc->div_high));

	/* calculate setup stop config */
	min_setup_stop_ns = t->scl_rise_ns + spec->min_setup_stop_ns;
	stp_sto_cfg = DIV_ROUND_UP(clk_rate_khz * min_setup_stop_ns
		- 1000000, 8 * 1000000 * (t_calc->div_high));

	t_calc->tuning = REG_CON_SDA_CFG(--sda_update_cfg) |
		REG_CON_STA_CFG(--stp_sta_cfg) |
		REG_CON_STO_CFG(--stp_sto_cfg);

	t_calc->div_low--;
	t_calc->div_high--;

	/* Maximum divider supported by hw is 0xffff */
	if (t_calc->div_low > 0xffff) {
		t_calc->div_low = 0xffff;
		status = STATUS_INVALID_PARAMETER;
	}

	if (t_calc->div_high > 0xffff) {
		t_calc->div_high = 0xffff;
		status = STATUS_INVALID_PARAMETER;
	}

	return status;
}

void rk3x_i2c_adapt_div(PRK3XI2C_CONTEXT pDevice, unsigned long clk_rate)
{
	struct i2c_timings* t = &pDevice->timings;
	struct rk3x_i2c_calced_timings calc;
	UINT64 t_low_ns, t_high_ns;
	UINT32 val;
	NTSTATUS status;

	status = pDevice->calcTimings(clk_rate, t, &calc);
	if (!NT_SUCCESS(status)) {
		Rk3xI2CPrint(DEBUG_LEVEL_ERROR, DBG_PNP, "Could not reach SCL freq %u", t->bus_freq_hz);
	}

	WdfInterruptDisable(pDevice->Interrupt);

	val = read32(pDevice, REG_CON);
	val &= ~REG_CON_TUNING_MASK;
	val |= calc.tuning;
	write32(pDevice, REG_CON, val);
	write32(pDevice, REG_CLKDIV,
		(calc.div_high << 16) | (calc.div_low & 0xffff));

	WdfInterruptEnable(pDevice->Interrupt);

	t_low_ns = (((UINT64)calc.div_low + 1) * 8 * 1000000000) / clk_rate;
	t_high_ns = (((UINT64)calc.div_high + 1) * 8 * 1000000000) /
		clk_rate;
	Rk3xI2CPrint(DEBUG_LEVEL_ERROR, DBG_PNP,
		"CLK %lukhz, Req %uns, Act low %lluns high %lluns\n",
		clk_rate / 1000,
		1000000000 / t->bus_freq_hz,
		t_low_ns, t_high_ns);
}

void updateClockSettings(PRK3XI2C_CONTEXT pDevice) {
	struct i2c_timings* t = &pDevice->timings;
	UINT32 defaultVal;

	defaultVal = t->bus_freq_hz <= I2C_MAX_STANDARD_MODE_FREQ ? 1000 :
		t->bus_freq_hz <= I2C_MAX_FAST_MODE_FREQ ? 300 : 120;
	t->scl_rise_ns = defaultVal;

	defaultVal = t->bus_freq_hz <= I2C_MAX_FAST_MODE_FREQ ? 300 : 120;
	t->scl_fall_ns = defaultVal;

	t->scl_int_delay_ns = 0;
	t->sda_fall_ns = t->scl_fall_ns;
	t->sda_hold_ns = 0;
	t->digital_filter_width_ns = 0;
	t->analog_filter_cutoff_freq_hz = 0;
}