// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2014 Google Inc.
 * Copyright (c) 2016 Google, Inc
 * Copyright (C) 2015-2018 Intel Corporation.
 * Copyright (C) 2018 Siemens AG
 * Some code taken from coreboot cpulib.c
 */

#include <common.h>
#include <cpu.h>
#include <dm.h>
#include <errno.h>
#include <asm/acpigen.h>
#include <asm/cpu.h>
#include <asm/cpu_common.h>
#include <asm/intel_regs.h>
#include <asm/lapic.h>
#include <asm/lpc_common.h>
#include <asm/msr.h>
#include <asm/mtrr.h>
#include <asm/post.h>
#include <asm/microcode.h>

DECLARE_GLOBAL_DATA_PTR;

static int report_bist_failure(void)
{
	if (gd->arch.bist != 0) {
		post_code(POST_BIST_FAILURE);
		printf("BIST failed: %08x\n", gd->arch.bist);
		return -EFAULT;
	}

	return 0;
}

int cpu_common_init(void)
{
	struct udevice *dev, *lpc;
	int ret;

	/* Halt if there was a built in self test failure */
	ret = report_bist_failure();
	if (ret)
		return ret;

	enable_lapic();

	ret = microcode_update_intel();
	if (ret && ret != -EEXIST) {
		debug("%s: Microcode update failure (err=%d)\n", __func__, ret);
		return ret;
	}

	/* Enable upper 128bytes of CMOS */
	writel(1 << 2, RCB_REG(RC));

	/* Early chipset init required before RAM init can work */
	uclass_first_device(UCLASS_NORTHBRIDGE, &dev);

	ret = uclass_first_device(UCLASS_LPC, &lpc);
	if (ret)
		return ret;
	if (!lpc)
		return -ENODEV;

	/* Cause the SATA device to do its early init */
	uclass_first_device(UCLASS_AHCI, &dev);

	return 0;
}

int cpu_set_flex_ratio_to_tdp_nominal(void)
{
	msr_t flex_ratio, msr;
	u8 nominal_ratio;

	/* Check for Flex Ratio support */
	flex_ratio = msr_read(MSR_FLEX_RATIO);
	if (!(flex_ratio.lo & FLEX_RATIO_EN))
		return -EINVAL;

	/* Check for >0 configurable TDPs */
	msr = msr_read(MSR_PLATFORM_INFO);
	if (((msr.hi >> 1) & 3) == 0)
		return -EINVAL;

	/* Use nominal TDP ratio for flex ratio */
	msr = msr_read(MSR_CONFIG_TDP_NOMINAL);
	nominal_ratio = msr.lo & 0xff;

	/* See if flex ratio is already set to nominal TDP ratio */
	if (((flex_ratio.lo >> 8) & 0xff) == nominal_ratio)
		return 0;

	/* Set flex ratio to nominal TDP ratio */
	flex_ratio.lo &= ~0xff00;
	flex_ratio.lo |= nominal_ratio << 8;
	flex_ratio.lo |= FLEX_RATIO_LOCK;
	msr_write(MSR_FLEX_RATIO, flex_ratio);

	/* Set flex ratio in soft reset data register bits 11:6 */
	clrsetbits_le32(RCB_REG(SOFT_RESET_DATA), 0x3f << 6,
			(nominal_ratio & 0x3f) << 6);

	debug("CPU: Soft reset to set up flex ratio\n");

	/* Set soft reset control to use register value */
	setbits_le32(RCB_REG(SOFT_RESET_CTRL), 1);

	/* Issue warm reset, will be "CPU only" due to soft reset data */
	outb(0x0, IO_PORT_RESET);
	outb(SYS_RST | RST_CPU, IO_PORT_RESET);
	cpu_hlt();

	/* Not reached */
	return -EINVAL;
}

int cpu_intel_get_info(struct cpu_info *info, int bclk)
{
	msr_t msr;

	msr = msr_read(MSR_IA32_PERF_CTL);
	info->cpu_freq = ((msr.lo >> 8) & 0xff) * bclk * 1000000;
	info->features = 1 << CPU_FEAT_L1_CACHE | 1 << CPU_FEAT_MMU |
		1 << CPU_FEAT_UCODE | 1 << CPU_FEAT_DEVICE_ID;

	return 0;
}

int cpu_configure_thermal_target(struct udevice *dev)
{
	u32 tcc_offset;
	msr_t msr;
	int ret;

	ret = dev_read_u32(dev, "tcc-offset", &tcc_offset);
	if (!ret)
		return -ENOENT;

	/* Set TCC activaiton offset if supported */
	msr = msr_read(MSR_PLATFORM_INFO);
	if (msr.lo & (1 << 30)) {
		msr = msr_read(MSR_TEMPERATURE_TARGET);
		msr.lo &= ~(0xf << 24); /* Bits 27:24 */
		msr.lo |= (tcc_offset & 0xf) << 24;
		msr_write(MSR_TEMPERATURE_TARGET, msr);
	}

	return 0;
}

void cpu_set_perf_control(uint clk_ratio)
{
	msr_t perf_ctl;

	perf_ctl.lo = (clk_ratio & 0xff) << 8;
	perf_ctl.hi = 0;
	msr_write(MSR_IA32_PERF_CTL, perf_ctl);
	debug("CPU: frequency set to %d MHz\n", clk_ratio * INTEL_BCLK_MHZ);
}

bool cpu_config_tdp_levels(void)
{
	msr_t platform_info;

	/* Bits 34:33 indicate how many levels supported */
	platform_info = msr_read(MSR_PLATFORM_INFO);

	return ((platform_info.hi >> 1) & 3) != 0;
}

void cpu_set_p_state_to_turbo_ratio(void)
{
	msr_t msr;

	msr = msr_read(MSR_TURBO_RATIO_LIMIT);
	cpu_set_perf_control(msr.lo);
}

enum burst_mode_t cpu_get_burst_mode_state(void)
{
	enum burst_mode_t state;
	int burst_en, burst_cap;
	msr_t msr;
	uint eax;

	eax = cpuid_eax(0x6);
	burst_cap = eax & 0x2;
	msr = msr_read(MSR_IA32_MISC_ENABLE);
	burst_en = !(msr.hi & BURST_MODE_DISABLE);

	if (!burst_cap && burst_en)
		state = BURST_MODE_UNAVAILABLE;
	else if (burst_cap && !burst_en)
		state = BURST_MODE_DISABLED;
	else if (burst_cap && burst_en)
		state = BURST_MODE_ENABLED;
	else
		state = BURST_MODE_UNKNOWN;

	return state;
}

void cpu_set_burst_mode(bool burst_mode)
{
	msr_t msr;

	msr = msr_read(MSR_IA32_MISC_ENABLE);
	if (burst_mode)
		msr.hi &= ~BURST_MODE_DISABLE;
	else
		msr.hi |= BURST_MODE_DISABLE;
	msr_write(MSR_IA32_MISC_ENABLE, msr);
}

void cpu_set_eist(bool eist_status)
{
	msr_t msr;

	msr = msr_read(MSR_IA32_MISC_ENABLE);
	if (eist_status)
		msr.lo |= MISC_ENABLE_ENHANCED_SPEEDSTEP;
	else
		msr.lo &= ~MISC_ENABLE_ENHANCED_SPEEDSTEP;
	msr_write(MSR_IA32_MISC_ENABLE, msr);
}

int cpu_get_coord_type(void)
{
	return HW_ALL;
}

#if 0
/*
 * Set PERF_CTL MSR (0x199) P_Req with
 * Turbo Ratio which is the Maximum Ratio.
 */
void cpu_set_max_ratio(void)
{
	/* Check for configurable TDP option */
	if (get_turbo_state() == TURBO_ENABLED)
		cpu_set_p_state_to_turbo_ratio();
}

/*
 * Get the TDP Nominal Ratio from MSR 0x648 Bits 7:0.
 */
u8 cpu_get_tdp_nominal_ratio(void)
{
	u8 nominal_ratio;
	msr_t msr;

	msr = rdmsr(MSR_CONFIG_TDP_NOMINAL);
	nominal_ratio = msr.lo & 0xff;
	return nominal_ratio;
}

/*
 * Read PLATFORM_INFO MSR (0xCE).
 * Return Value of Bit 34:33 (CONFIG_TDP_LEVELS).
 *
 * Possible values of Bit 34:33 are -
 * 00 : Config TDP not supported
 * 01 : One Additional TDP level supported
 * 10 : Two Additional TDP level supported
 * 11 : Reserved
 */
int cpu_config_tdp_levels(void)
{
	msr_t platform_info;

	/* Bits 34:33 indicate how many levels supported */
	platform_info = rdmsr(MSR_PLATFORM_INFO);
	return (platform_info.hi >> 1) & 3;
}

static void set_perf_control_msr(msr_t msr)
{
	wrmsr(IA32_PERF_CTL, msr);
	printk(BIOS_DEBUG, "CPU: frequency set to %d MHz\n",
	       ((msr.lo >> 8) & 0xff) * CONFIG_CPU_BCLK_MHZ);
}

/*
 * TURBO_RATIO_LIMIT MSR (0x1AD) Bits 31:0 indicates the
 * factory configured values for of 1-core, 2-core, 3-core
 * and 4-core turbo ratio limits for all processors.
 *
 * 7:0 -	MAX_TURBO_1_CORE
 * 15:8 -	MAX_TURBO_2_CORES
 * 23:16 -	MAX_TURBO_3_CORES
 * 31:24 -	MAX_TURBO_4_CORES
 *
 * Set PERF_CTL MSR (0x199) P_Req with that value.
 */
void cpu_set_p_state_to_turbo_ratio(void)
{
	msr_t msr, perf_ctl;

	msr = rdmsr(MSR_TURBO_RATIO_LIMIT);
	perf_ctl.lo = (msr.lo & 0xff) << 8;
	perf_ctl.hi = 0;

	set_perf_control_msr(perf_ctl);
}

/*
 * CONFIG_TDP_NOMINAL MSR (0x648) Bits 7:0 tells Nominal
 * TDP level ratio to be used for specific processor (in units
 * of 100MHz).
 *
 * Set PERF_CTL MSR (0x199) P_Req with that value.
 */
void cpu_set_p_state_to_nominal_tdp_ratio(void)
{
	msr_t msr, perf_ctl;

	msr = rdmsr(MSR_CONFIG_TDP_NOMINAL);
	perf_ctl.lo = (msr.lo & 0xff) << 8;
	perf_ctl.hi = 0;

	set_perf_control_msr(perf_ctl);
}

/*
 * PLATFORM_INFO MSR (0xCE) Bits 15:8 tells
 * MAX_NON_TURBO_LIM_RATIO.
 *
 * Set PERF_CTL MSR (0x199) P_Req with that value.
 */
void cpu_set_p_state_to_max_non_turbo_ratio(void)
{
	msr_t msr, perf_ctl;

	/* Platform Info bits 15:8 give max ratio */
	msr = rdmsr(MSR_PLATFORM_INFO);
	perf_ctl.lo = msr.lo & 0xff00;
	perf_ctl.hi = 0;

	set_perf_control_msr(perf_ctl);
}

/*
 * Set PERF_CTL MSR (0x199) P_Req with the value
 * for maximum efficiency. This value is reported in PLATFORM_INFO MSR (0xCE)
 * in Bits 47:40 and is extracted with cpu_get_min_ratio().
 */
void cpu_set_p_state_to_min_clock_ratio(void)
{
	uint32_t min_ratio;
	msr_t perf_ctl;

	/* Read the minimum ratio for the best efficiency. */
	min_ratio = cpu_get_min_ratio();
	perf_ctl.lo = (min_ratio << 8) & 0xff00;
	perf_ctl.hi = 0;

	set_perf_control_msr(perf_ctl);
}

/*
 * Get the Burst/Turbo Mode State from MSR IA32_MISC_ENABLE 0x1A0
 * Bit 38 - TURBO_MODE_DISABLE Bit to get state ENABLED / DISABLED.
 * Also check for the cpuid 0x6 to check whether Burst mode unsupported.
 */
int cpu_get_burst_mode_state(void)
{

	msr_t msr;
	unsigned int eax;
	int burst_en, burst_cap, burst_state = BURST_MODE_UNKNOWN;

	eax = cpuid_eax(0x6);
	burst_cap = eax & 0x2;
	msr = rdmsr(IA32_MISC_ENABLE);
	burst_en = !(msr.hi & BURST_MODE_DISABLE);

	if (!burst_cap && burst_en) {
		burst_state = BURST_MODE_UNAVAILABLE;
	} else if (burst_cap && !burst_en) {
		burst_state = BURST_MODE_DISABLED;
	} else if (burst_cap && burst_en) {
		burst_state = BURST_MODE_ENABLED;
	}
	return burst_state;
}

/*
 * Program CPU Burst mode
 * true = Enable Burst mode.
 * false = Disable Burst mode.
 */
void cpu_burst_mode(bool burst_mode_status)
{
	msr_t msr;

	msr = rdmsr(IA32_MISC_ENABLE);
	if (burst_mode_status)
		msr.hi &= ~BURST_MODE_DISABLE;
	else
		msr.hi |= BURST_MODE_DISABLE;
	wrmsr(IA32_MISC_ENABLE, msr);
}

/*
 * Program Enhanced Intel Speed Step Technology
 * true = Enable EIST.
 * false = Disable EIST.
 */
void cpu_set_eist(bool eist_status)
{
	msr_t msr;

	msr = rdmsr(IA32_MISC_ENABLE);
	if (eist_status)
		msr.lo |= (1 << 16);
	else
		msr.lo &= ~(1 << 16);
	wrmsr(IA32_MISC_ENABLE, msr);
}

/*
 * Set Bit 6 (ENABLE_IA_UNTRUSTED_MODE) of MSR 0x120
 * UCODE_PCR_POWER_MISC MSR to enter IA Untrusted Mode.
 */
void cpu_enable_untrusted_mode(void *unused)
{
	msr_t msr;

	msr = rdmsr(MSR_POWER_MISC);
	msr.lo |= ENABLE_IA_UNTRUSTED;
	wrmsr(MSR_POWER_MISC, msr);
}

/*
 * This function fills in the number of Cores(physical) and Threads(virtual)
 * of the CPU in the function arguments. It also returns if the number of cores
 * and number of threads are equal.
 */
int cpu_read_topology(unsigned int *num_phys, unsigned int *num_virt)
{
	msr_t msr;
	msr = rdmsr(MSR_CORE_THREAD_COUNT);
	*num_virt = (msr.lo >> 0) & 0xffff;
	*num_phys = (msr.lo >> 16) & 0xffff;
	return (*num_virt == *num_phys);
}

int cpu_get_coord_type(void)
{
	return HW_ALL;
}
#endif

uint32_t cpu_get_min_ratio(void)
{
	msr_t msr;
	/* Get bus ratio limits and calculate clock speeds */
	msr = msr_read(MSR_PLATFORM_INFO);

	return (msr.hi >> 8) & 0xff;	/* Max Efficiency Ratio */
}

uint32_t cpu_get_max_ratio(void)
{
	msr_t msr;
	uint32_t ratio_max;

	if (cpu_config_tdp_levels()) {
		/* Set max ratio to nominal TDP ratio */
		msr = msr_read(MSR_CONFIG_TDP_NOMINAL);
		ratio_max = msr.lo & 0xff;
	} else {
		msr = msr_read(MSR_PLATFORM_INFO);
		/* Max Non-Turbo Ratio */
		ratio_max = (msr.lo >> 8) & 0xff;
	}
	return ratio_max;
}

uint32_t cpu_get_bus_clock(void)
{
	/* CPU bus clock is set by default here to 100MHz.
	 * This function returns the bus clock in KHz.
	 */
	return INTEL_BCLK_MHZ * 1000;
}

uint32_t cpu_get_power_max(void)
{
	msr_t msr;
	int power_unit;

	msr = msr_read(MSR_PKG_POWER_SKU_UNIT);
	power_unit = 2 << ((msr.lo & 0xf) - 1);
	msr = msr_read(MSR_PKG_POWER_SKU);
	return (msr.lo & 0x7fff) * 1000 / power_unit;
}

uint32_t cpu_get_max_turbo_ratio(void)
{
	msr_t msr;
	msr = msr_read(MSR_TURBO_RATIO_LIMIT);
	return msr.lo & 0xff;
}
#if 0
void mca_configure(void)
{
	msr_t msr;
	int i;
	int num_banks;

	printk(BIOS_DEBUG, "Clearing out pending MCEs\n");

	msr = rdmsr(IA32_MCG_CAP);
	num_banks = msr.lo & 0xff;
	msr.lo = msr.hi = 0;

	for (i = 0; i < num_banks; i++) {
		/* Clear the machine check status */
		wrmsr(IA32_MC0_STATUS + (i * 4), msr);
		/* Initialize machine checks */
		wrmsr(IA32_MC0_CTL + i * 4,
			(msr_t) {.lo = 0xffffffff, .hi = 0xffffffff});
	}
}

void cpu_lt_lock_memory(void *unused)
{
	msr_set_bit(MSR_LT_CONTROL, LT_CONTROL_LOCK_BIT);
}

int get_prmrr_size(void)
{
	msr_t msr;
	int i;
	int valid_size;

	if (CONFIG(SOC_INTEL_COMMON_BLOCK_SGX_PRMRR_DISABLED)) {
		printk(BIOS_DEBUG, "PRMRR disabled by config.\n");
		return 0;
	}

	msr = rdmsr(MSR_PRMRR_VALID_CONFIG);
	if (!msr.lo) {
		printk(BIOS_WARNING, "PRMRR not supported.\n");
		return 0;
	}

	printk(BIOS_DEBUG, "MSR_PRMRR_VALID_CONFIG = 0x%08x\n", msr.lo);

	/* find the first (greatest) value that is lower than or equal to the selected size */
	for (i = 8; i >= 0; i--) {
		valid_size = msr.lo & (1 << i);

		if (valid_size && valid_size <= CONFIG_SOC_INTEL_COMMON_BLOCK_SGX_PRMRR_SIZE)
			break;
		else if (i == 0)
			valid_size = 0;
	}

	/* die if we could not find a valid size within the limit */
	if (!valid_size)
		die("Unsupported PRMRR size limit %i MiB, check your config!\n",
			CONFIG_SOC_INTEL_COMMON_BLOCK_SGX_PRMRR_SIZE);

	printk(BIOS_DEBUG, "PRMRR size set to %i MiB\n", valid_size);

	valid_size *= MiB;

	return valid_size;
}
#endif
