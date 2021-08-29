// SPDX-License-Identifier: GPL-2.0+
/*
 * Implementation of vboot display callbacks
 *
 * Copyright 2018 Google LLC
 */

#include <common.h>
#include <cros_ec.h>
#include <dm.h>
#include <log.h>
#include <tpm_api.h>
#include <cros/memory.h>
#include <cros/health_info.h>
#include <cros/storage_test.h>
#include <cros/ui.h>
#include <cros/vboot.h>

static struct ui_log_info log;

uint32_t vb2ex_prepare_log_screen(enum vb2_screen screen, uint32_t locale_id,
				  const char *str)
{
	struct vboot_info *vboot = vboot_get();
	const struct ui_locale *locale;

	if (ui_get_locale_info(vboot, locale_id, &locale))
		return 0;
	if (ui_log_init(screen, locale->code, str, &log))
		return 0;
	return log.page_count;
}

uint32_t vb2ex_get_locale_count(void)
{
	struct vboot_info *vboot = vboot_get();

	return ui_get_locale_count(vboot);
}

#define DEBUG_INFO_EXTRA_LENGTH 256

const char *vb2ex_get_debug_info(struct vb2_context *ctx)
{
	struct vboot_info *vboot = ctx_to_vboot(ctx);
	static char *buf;
	size_t buf_size;
	char tpm_buf[80];
	char *vboot_buf;
	char *tpm_str = NULL;
	char batt_pct_str[16];

	/* Check if cache exists. */
	if (buf)
		return buf;

	/* Debug info from the vboot context. */
	vboot_buf = vb2api_get_debug_info(ctx);

	buf_size = strlen(vboot_buf) + DEBUG_INFO_EXTRA_LENGTH + 1;
	buf = malloc(buf_size);
	if (buf == NULL) {
		printf("%s: Failed to malloc string buffer\n", __func__);
		free(vboot_buf);
		return NULL;
	}

	/* States owned by firmware. */
	if (!IS_ENABLED(CONFIG_TPM_V1) && !IS_ENABLED(CONFIG_TPM_V2))
		tpm_str = "MOCK TPM";
	else {
		if (!tpm_report_state(tpm_buf, sizeof(tpm_buf)))
			tpm_str = tpm_buf;
	}

	if (!tpm_str)
		tpm_str = "(unsupported)";

	if (!IS_ENABLED(CONFIG_CROSEC)) {
		strncpy(batt_pct_str, "(unsupported)", sizeof(batt_pct_str));
	} else {
		struct udevice *cros_ec = board_get_cros_ec_dev();
		uint batt_pct;

		if (!cros_ec || cros_ec_read_batt_charge(cros_ec, &batt_pct))
			strncpy(batt_pct_str, "(read failure)",
				sizeof(batt_pct_str));
		else
			snprintf(batt_pct_str, sizeof(batt_pct_str),
				 "%u%%", batt_pct);
	}
	snprintf(buf, buf_size,
		 "%s\n"  /* vboot output does not include newline. */
		 "read-only firmware id: %s\n"
		 "active firmware id: %s\n"
		 "battery level: %s\n"
		 "TPM state: %s",
		 vboot_buf,
		 vboot->readonly_firmware_id,
		 vboot->firmware_id,
		 batt_pct_str, tpm_str);

	free(vboot_buf);

	buf[buf_size - 1] = '\0';
	printf("debug info: %s\n", buf);
	return buf;
}

const char *vb2ex_get_firmware_log(int reset)
{
	static char *buf;
	if (!buf || reset) {
		free(buf);
		buf = cbmem_console_snapshot();
		if (buf)
			printf("Read cbmem console: size=%zu\n", strlen(buf));
		else
			printf("Failed to read cbmem console\n");
	}
	return buf;
}

#define DEFAULT_DIAGNOSTIC_OUTPUT_SIZE (64 * KiB)

vb2_error_t vb2ex_diag_get_storage_health(const char **out)
{
	static char *buf;
	if (!buf)
		buf = malloc(DEFAULT_DIAGNOSTIC_OUTPUT_SIZE);
	*out = buf;
	if (!buf)
		return VB2_ERROR_UI_MEMORY_ALLOC;

	dump_all_health_info(buf, buf + DEFAULT_DIAGNOSTIC_OUTPUT_SIZE);

	return VB2_SUCCESS;
}

vb2_error_t vb2ex_diag_get_storage_test_log(const char **out)
{
	static char *buf;
	if (!buf)
		buf = malloc(DEFAULT_DIAGNOSTIC_OUTPUT_SIZE);
	*out = buf;
	if (!buf)
		return VB2_ERROR_UI_MEMORY_ALLOC;

	return diag_dump_storage_test_log(buf,
					  buf + DEFAULT_DIAGNOSTIC_OUTPUT_SIZE);
}

vb2_error_t vb2ex_diag_memory_quick_test(int reset, const char **out)
{
	*out = NULL;
	if (reset)
		VB2_TRY(memory_test_init(MEMORY_TEST_MODE_QUICK));
	return memory_test_run(out);
}

vb2_error_t vb2ex_diag_memory_full_test(int reset, const char **out)
{
	*out = NULL;
	if (reset)
		VB2_TRY(memory_test_init(MEMORY_TEST_MODE_FULL));
	return memory_test_run(out);
}

vb2_error_t vb2ex_display_ui(enum vb2_screen screen,
			     uint32_t locale_id,
			     uint32_t selected_item,
			     uint32_t disabled_item_mask,
			     uint32_t hidden_item_mask,
			     int timer_disabled,
			     uint32_t current_page,
			     enum vb2_ui_error error_code)
{
	struct vboot_info *vboot = vboot_get();
	vb2_error_t rv;
	const struct ui_locale *locale = NULL;
	const struct ui_screen_info *screen_info;
	printf("%s: screen=%#x, locale=%u, selected_item=%u, "
	       "disabled_item_mask=%#x, hidden_item_mask=%#x, "
	       "timer_disabled=%d, current_page=%u, error=%#x\n",
	       __func__,
	       screen, locale_id, selected_item,
	       disabled_item_mask, hidden_item_mask,
	       timer_disabled, current_page, error_code);

	rv = ui_get_locale_info(vboot, locale_id, &locale);
	if (rv == VB2_ERROR_UI_INVALID_LOCALE) {
		printf("Locale %u not found, falling back to locale 0",
		       locale_id);
		rv = ui_get_locale_info(vboot, 0, &locale);
	}
	if (rv)
		goto fail;

	screen_info = ui_get_screen_info(screen);
	if (!screen_info) {
		printf("%s: Not a valid screen: %#x\n", __func__, screen);
		rv = VB2_ERROR_UI_INVALID_SCREEN;
		goto fail;
	}

	struct ui_state state = {
		.screen = screen_info,
		.locale = locale,
		.selected_item = selected_item,
		.disabled_item_mask = disabled_item_mask,
		.hidden_item_mask = hidden_item_mask,
		.timer_disabled = timer_disabled,
		.log = &log,
		.current_page = current_page,
		.error_code = error_code,
	};

	static struct ui_state prev_state;
	static int has_prev_state = 0;

	rv = ui_display_screen(&state, has_prev_state ? &prev_state : NULL);
	flush_graphics_buffer();
	if (rv)
		goto fail;

	memcpy(&prev_state, &state, sizeof(struct ui_state));
	has_prev_state = 1;

	return VB2_SUCCESS;

 fail:
	has_prev_state = 0;
	return rv;
}
