// SPDX-License-Identifier: Intel
/*
 * Copyright (C) 2015-2016 Intel Corp.
 * (Written by Andrey Petrov <andrey.petrov@intel.com> for Intel Corp.)
 * (Written by Alexandru Gagniuc <alexandrux.gagniuc@intel.com> for Intel Corp.)
 * Mostly taken from coreboot fsp2_0/memory_init.c
 */

#include <common.h>
#include <binman_sym.h>
#include <cbfs.h>
#include <dm.h>
#include <spi_flash.h>
#include <asm/mrccache.h>
#include <asm/hob.h>
#include <asm/fsp/fsp_infoheader.h>
#include <asm/fsp2/fsp_api.h>
#include <asm/fsp2/fsp_internal.h>
#include <asm/arch/fsp/fsp_configs.h>
#include <asm/arch/fsp/fsp_m_upd.h>

binman_sym_declare(ulong, intel_fsp_m, image_pos);
binman_sym_declare(ulong, intel_fsp_m, size);

/*
 * ODT settings:
 * If ODT PIN to LP4 DRAM is pulled HIGH for ODT_A and HIGH for ODT_B,
 * choose ODT_A_B_HIGH_HIGH. If ODT PIN to LP4 DRAM is pulled HIGH for ODT_A
 * and LOW for ODT_B, choose ODT_A_B_HIGH_LOW.
 *
 * Note that the enum values correspond to the interpreted UPD fields
 * within Ch[3:0]_OdtConfig parameters.
 */
enum {
	ODT_A_B_HIGH_LOW	= 0 << 1,
	ODT_A_B_HIGH_HIGH	= 1 << 1,
	N_WR_24			= 1 << 5,
};

/*
 * LPDDR4 helper routines for configuring the memory UPD for LPDDR4 operation.
 * There are four physical LPDDR4 channels, each 32-bits wide. There are two
 * logical channels using two physical channels together to form a 64-bit
 * interface to memory for each logical channel.
 */

enum {
	LP4_PHYS_CH0A,
	LP4_PHYS_CH0B,
	LP4_PHYS_CH1A,
	LP4_PHYS_CH1B,

	LP4_NUM_PHYS_CHANNELS,
};

/*
 * The DQs within a physical channel can be bit-swizzled within each byte.
 * Within a channel the bytes can be swapped, but the DQs need to be routed
 * with the corresponding DQS (strobe).
 */
enum {
	LP4_DQS0,
	LP4_DQS1,
	LP4_DQS2,
	LP4_DQS3,

	LP4_NUM_BYTE_LANES,
	DQ_BITS_PER_DQS		= 8,
};

/* Provide bit swizzling per DQS and byte swapping within a channel. */
struct lpddr4_chan_swizzle_cfg {
	u8 dqs[LP4_NUM_BYTE_LANES][DQ_BITS_PER_DQS];
};

struct lpddr4_swizzle_cfg {
	struct lpddr4_chan_swizzle_cfg phys[LP4_NUM_PHYS_CHANNELS];
};

static int prepare_mrc_cache_type(enum mrc_type_t type,
				  struct mrc_data_container **cachep)
{
	struct mrc_data_container *cache;
	struct mrc_region entry;
	int ret;

	ret = mrccache_get_region(type, NULL, &entry);
	if (ret)
		return ret;
	cache = mrccache_find_current(&entry);
	if (!cache)
		return -ENOENT;

	log_debug("MRC at %x, size %x\n", (uint)cache->data, cache->data_size);
	*cachep = cache;

	return 0;
}

static int prepare_mrc_cache(struct fspm_upd *upd)
{
	struct mrc_data_container *cache;
	int ret;

	ret = prepare_mrc_cache_type(MRC_TYPE_NORMAL, &cache);
	if (ret)
		return log_msg_ret("Cannot get normal cache", ret);
	upd->arch.nvs_buffer_ptr = cache->data;

	ret = prepare_mrc_cache_type(MRC_TYPE_VAR, &cache);
	if (ret)
		return log_msg_ret("Cannot get var cache", ret);
	upd->config.variable_nvs_buffer_ptr = cache->data;

	return 0;
}

static int get_coreboot_fsp(ulong map_base, ulong *fsp_m_posp,
			    ulong *fsp_m_sizep)
{
	/* Hard-coded position of CBFS in ROM */
	ulong cbfs_base = 0x205000;
	ulong cbfs_size = 0x1bb000;
	struct cbfs_priv *cbfs;
	int ret;

	ret = cbfs_init_mem(map_base + cbfs_base, cbfs_size, &cbfs);
	if (ret)
		return ret;
	if (!ret) {
		const struct cbfs_cachenode *node;

		node = cbfs_find_file(cbfs, "fspm.bin");
		if (!node) {
			printf("no node\n");
			return -ENOENT;
		}

		*fsp_m_posp = (ulong)node->data;
		*fsp_m_sizep = node->data_length;
	}

	return 0;
}

static void setup_sdram(struct fsp_m_config *cfg,
			const struct lpddr4_swizzle_cfg *swizzle_cfg)
{
	const struct lpddr4_chan_swizzle_cfg *sch;
	/* Number of bytes to copy per DQS. */
	const size_t sz = DQ_BITS_PER_DQS;
	int chan;

	cfg->memory_down = 1;
	cfg->scrambler_support = 1;
	cfg->channel_hash_mask = 0x36;
	cfg->slice_hash_mask = 9;
	cfg->interleaved_mode = 2;
	cfg->channels_slices_enable = 0;
	cfg->min_ref_rate2x_enable = 0;
	cfg->dual_rank_support_enable = 1;

	/* LPDDR4 is memory down so no SPD addresses. */
	cfg->dimm0_spd_address = 0;
	cfg->dimm1_spd_address = 0;

	for (chan = 0; chan < 4; chan++) {
		struct fsp_ram_channel *ch = &cfg->chan[chan];

		ch->rank_enable = 1;
		ch->device_width = 1;
		ch->dram_density = 2;
		ch->option = 3;
		ch->odt_config = ODT_A_B_HIGH_HIGH;
	}

	/*
	 * CH0_DQB byte lanes in the bit swizzle configuration field are
	 * not 1:1. The mapping within the swizzling field is:
	 *   indices [0:7]   - byte lane 1 (DQS1) DQ[8:15]
	 *   indices [8:15]  - byte lane 0 (DQS0) DQ[0:7]
	 *   indices [16:23] - byte lane 3 (DQS3) DQ[24:31]
	 *   indices [24:31] - byte lane 2 (DQS2) DQ[16:23]
	 */
	sch = &swizzle_cfg->phys[LP4_PHYS_CH0B];
	memcpy(&cfg->ch_bit_swizzling[0][0], &sch->dqs[LP4_DQS1], sz);
	memcpy(&cfg->ch_bit_swizzling[0][8], &sch->dqs[LP4_DQS0], sz);
	memcpy(&cfg->ch_bit_swizzling[0][16], &sch->dqs[LP4_DQS3], sz);
	memcpy(&cfg->ch_bit_swizzling[0][24], &sch->dqs[LP4_DQS2], sz);

	/*
	 * CH0_DQA byte lanes in the bit swizzle configuration field are 1:1.
	 */
	sch = &swizzle_cfg->phys[LP4_PHYS_CH0A];
	memcpy(&cfg->ch_bit_swizzling[1][0], &sch->dqs[LP4_DQS0], sz);
	memcpy(&cfg->ch_bit_swizzling[1][8], &sch->dqs[LP4_DQS1], sz);
	memcpy(&cfg->ch_bit_swizzling[1][16], &sch->dqs[LP4_DQS2], sz);
	memcpy(&cfg->ch_bit_swizzling[1][24], &sch->dqs[LP4_DQS3], sz);

	sch = &swizzle_cfg->phys[LP4_PHYS_CH1B];
	memcpy(&cfg->ch_bit_swizzling[2][0], &sch->dqs[LP4_DQS1], sz);
	memcpy(&cfg->ch_bit_swizzling[2][8], &sch->dqs[LP4_DQS0], sz);
	memcpy(&cfg->ch_bit_swizzling[2][16], &sch->dqs[LP4_DQS3], sz);
	memcpy(&cfg->ch_bit_swizzling[2][24], &sch->dqs[LP4_DQS2], sz);

	/*
	 * CH0_DQA byte lanes in the bit swizzle configuration field are 1:1.
	 */
	sch = &swizzle_cfg->phys[LP4_PHYS_CH1A];
	memcpy(&cfg->ch_bit_swizzling[3][0], &sch->dqs[LP4_DQS0], sz);
	memcpy(&cfg->ch_bit_swizzling[3][8], &sch->dqs[LP4_DQS1], sz);
	memcpy(&cfg->ch_bit_swizzling[3][16], &sch->dqs[LP4_DQS2], sz);
	memcpy(&cfg->ch_bit_swizzling[3][24], &sch->dqs[LP4_DQS3], sz);
}

/**
 * setup_config() - Set up the config structure for FSP-M
 *
 * @dev: Hostbridge device containing config
 * @upd: Config data to fill in
 * @return 0 if OK, -ve on error
 */
static int setup_config(struct udevice *dev, struct fspm_upd *upd)
{
	struct fsp_m_config *cfg = &upd->config;
	struct fspm_arch_upd *arch = &upd->arch;

	arch->nvs_buffer_ptr = NULL;
	prepare_mrc_cache(upd);
	arch->stack_base = (void *)0xfef96000;
	arch->boot_loader_tolum_size = 0x2000;

	arch->boot_mode = FSP_BOOT_WITH_FULL_CONFIGURATION;
	cfg->serial_debug_port_type = 2;
	cfg->serial_debug_port_device = 2;
	cfg->serial_debug_port_stride_size = 2;
	cfg->serial_debug_port_address = 0;

	cfg->package = 1;
	/* Don't enforce a memory size limit. */
	cfg->memory_size_limit = 0;
	cfg->low_memory_max_value = 2048;  /* 2 GB */
	/* No restrictions on memory above 4GiB */
	cfg->high_memory_max_value = 0;

	/* Always default to attempt to use saved training data. */
	cfg->disable_fast_boot = 0;

	const u8 *swizzle_data;

	swizzle_data = dev_read_u8_array_ptr(dev, "lpddr4-swizzle",
					     LP4_NUM_BYTE_LANES *
					     DQ_BITS_PER_DQS *
					     LP4_NUM_PHYS_CHANNELS);
	if (!swizzle_data)
		return log_msg_ret("Cannot read swizzel data", -EINVAL);

	setup_sdram(cfg, (struct lpddr4_swizzle_cfg *)swizzle_data);

	cfg->pre_mem_gpio_table_ptr = 0;

	cfg->profile = 0xb;
	cfg->msg_level_mask = 0;

	/* other */
	cfg->skip_cse_rbp = 1;
	cfg->periodic_retraining_disable = 0;
	cfg->enable_s3_heci2 = 0;

	return 0;
}

int fsp_memory_init(bool s3wake, bool use_spi_flash)
{
	struct fspm_upd upd, *fsp_upd;
	fsp_memory_init_func func;
	struct fsp_header *hdr;
	struct hob_header *hob;
	struct udevice *dev, *sf;
	ulong fsp_m_pos = 0;
	ulong fsp_m_size = 0;
	size_t map_size;
	ulong map_base;
	ulong base;
	u32 offset;
	int ret;

	ret = uclass_first_device_err(UCLASS_NORTHBRIDGE, &dev);
	if (ret)
		return log_msg_ret("Cannot get northbridge", ret);
	ret = uclass_first_device_err(UCLASS_SPI_FLASH, &sf);
	if (ret)
		return log_msg_ret("Cannot get SPI flash", ret);
	ret = spi_flash_get_mmap(sf, &map_base, &map_size, &offset);
	if (ret)
		return log_msg_ret("Could not get flash mmap", ret);

	ret = -ENOENT;
	if (false)
		/* Support using a hybrid image build by coreboot */
		ret = get_coreboot_fsp(map_base, &fsp_m_pos, &fsp_m_size);
	if (ret) {
		ulong mask = CONFIG_ROM_SIZE - 1;

		fsp_m_pos = binman_sym(ulong, intel_fsp_m, image_pos);
		fsp_m_size = binman_sym(ulong, intel_fsp_m, size);
		if (fsp_m_pos != BINMAN_SYM_MISSING) {
			ret = 0;
			if (use_spi_flash)
				fsp_m_pos &= mask;
			else
				fsp_m_pos += (map_base & mask);
		} else {
			ret = -ENOENT;
		}
	}
	if (ret)
		return log_msg_ret("Cannot find FSP_M", ret);

	/* Use memory-mapped SPI flash by default as it is simpler */
	ret = fsp_get_header(fsp_m_pos, fsp_m_size, use_spi_flash, &hdr, &base);
	if (ret)
		return log_msg_ret("fsp_get_header", ret);

	/* Copy over the default config */
	fsp_upd = (struct fspm_upd *)(base + hdr->cfg_region_off);
	if (fsp_upd->header.signature != FSPM_UPD_SIGNATURE)
		return log_msg_ret("Bad UPD signature", -EPERM);
	memcpy(&upd, fsp_upd, sizeof(upd));

	ret = setup_config(dev, &upd);
	if (ret)
		return log_msg_ret("Could not setup config", ret);
	debug("SDRAM init...");
	func = (fsp_memory_init_func)(base + hdr->fsp_mem_init);
	ret = func(&upd, &hob);
	if (ret)
		return log_msg_ret("SDRAM init fail\n", ret);

	gd->arch.hob_list = hob;
	debug("done\n");

	return 0;
}
