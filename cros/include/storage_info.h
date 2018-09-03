/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright 2020 Google Inc.
 */

#ifndef __CROS_STORAGE_INFO_H__
#define __CROS_STORAGE_INFO_H__

typedef enum BlockDevTestOpsType {
	BLOCKDEV_TEST_OPS_TYPE_STOP = 0,
	BLOCKDEV_TEST_OPS_TYPE_SHORT,
	BLOCKDEV_TEST_OPS_TYPE_EXTENDED,
} BlockDevTestOpsType;

#define EXT_CSD_REV_1_0		0	/* Revision 1.0 for MMC v4.0 */
#define EXT_CSD_REV_1_1		1	/* Revision 1.1 for MMC v4.1 */
#define EXT_CSD_REV_1_2		2	/* Revision 1.2 for MMC v4.2 */
#define EXT_CSD_REV_1_3		3	/* Revision 1.3 for MMC v4.3 */
#define EXT_CSD_REV_1_4		4	/* Revision 1.4 Obsolete */
#define EXT_CSD_REV_1_5		5	/* Revision 1.5 for MMC v4.41 */
#define EXT_CSD_REV_1_6		6	/* Revision 1.6 for MMC v4.5, v4.51 */
#define EXT_CSD_REV_1_7		7	/* Revision 1.7 for MMC v5.0, v5.01 */
#define EXT_CSD_REV_1_8		8	/* Revision 1.8 for MMC v5.1 */

#define EXT_CSD_PRE_EOL_INFO			267	/* RO */
#define EXT_CSD_DEVICE_LIFE_TIME_EST_TYP_A	268	/* RO */
#define EXT_CSD_DEVICE_LIFE_TIME_EST_TYP_B	269	/* RO */
#define EXT_CSD_VENDOR_HEALTH_REPORT_FIRST	270	/* RO */
#define EXT_CSD_VENDOR_HEALTH_REPORT_LAST	301	/* RO */

#define EXT_CSD_VENDOR_HEALTH_REPORT_SIZE                                      \
	(EXT_CSD_VENDOR_HEALTH_REPORT_LAST -                                   \
	 EXT_CSD_VENDOR_HEALTH_REPORT_FIRST + 1)

typedef struct {
	uint8_t csd_rev;
	uint8_t device_life_time_est_type_a;
	uint8_t device_life_time_est_type_b;
	uint8_t pre_eol_info;
	uint8_t vendor_proprietary_health_report
		[EXT_CSD_VENDOR_HEALTH_REPORT_SIZE];
} MmcHealthData;

/* NVMe S.M.A.R.T Log Data */
// Reference linux kernel v5.7 (include/linux/nvme.h)
typedef struct {
	uint8_t  critical_warning;
	uint16_t temperature;
	uint8_t  avail_spare;
	uint8_t  spare_thresh;
	uint8_t  percent_used;
	uint8_t  endu_grp_crit_warn_sumry;
	uint8_t  rsvd7[25];
	//
	// 128bit integers
	//
	uint8_t  data_units_read[16];
	uint8_t  data_units_written[16];
	uint8_t  host_reads[16];
	uint8_t  host_writes[16];
	uint8_t  ctrl_busy_time[16];
	uint8_t  power_cycles[16];
	uint8_t  power_on_hours[16];
	uint8_t  unsafe_shutdowns[16];
	uint8_t  media_errors[16];
	uint8_t  num_err_log_entries[16];

	uint32_t warning_temp_time;
	uint32_t critical_comp_time;
	uint16_t temp_sensor[8];

	uint32_t thm_temp1_trans_count;
	uint32_t thm_temp2_trans_count;
	uint32_t thm_temp1_total_time;
	uint32_t thm_temp2_total_time;

	uint8_t  rsvd232[280];
} __attribute__((packed)) NvmeSmartLogData;

/* NVMe Self Test Result Log Data as of Nvm Express 1.4 Spec */
typedef struct {
	uint8_t  current_operation;
	uint8_t  current_completion;
	uint8_t  rsvd1[2]; /* Reserved as of Nvm Express 1.4 Spec */
	uint8_t  status;
	uint8_t  segment_number;
	uint8_t  valid_diag_info;
	uint8_t  rsvd2[1]; /* Reserved as of Nvm Express 1.4 Spec */
	uint64_t poh;
	uint32_t nsid;
	uint64_t failing_lba;
	uint8_t  status_code_type;
	uint8_t  status_code;
	uint16_t vendor_specific;
} __attribute__((packed)) NvmeTestLogData;

typedef enum {
	STORAGE_INFO_TYPE_UNKNOWN = 0,
	STORAGE_INFO_TYPE_NVME,
	STORAGE_INFO_TYPE_MMC,
} StorageInfoType;

typedef struct HealthInfo {
	StorageInfoType type;

	union {
		NvmeSmartLogData nvme_data;
		MmcHealthData mmc_data;
	} data;
} HealthInfo;

typedef struct StorageTestLog {
	StorageInfoType type;

	union {
// #ifdef CONFIG_DRIVER_STORAGE_NVME
		NvmeTestLogData nvme_data;
// #endif
	} data;
} StorageTestLog;

#endif
