// SPDX-License-Identifier: GPL-2.0+
/*
 * Implementation of APIs provided by firmware and exported to vboot_reference.
 * They includes debug output, memory allocation, timer and delay, etc.
 *
 * Copyright 2018 Google LLC
 */

#include <common.h>
#include <dm.h>
#include <log.h>
#include <sound.h>
#include <cros/cros_common.h>
#include <cros/vboot.h>
#include <linux/delay.h>

DECLARE_GLOBAL_DATA_PTR;

#define TICKS_PER_MSEC		(CONFIG_SYS_HZ / 1000)
#define MAX_MSEC_PER_LOOP	((u32)((UINT32_MAX / TICKS_PER_MSEC) / 2))

void vb2ex_msleep(u32 msec)
{
	u32 delay, start;

	/*
	 * Can't use entire UINT32_MAX range in the max delay, because it
	 * pushes get_timer() too close to wraparound. So use /2.
	 */
	while (msec > MAX_MSEC_PER_LOOP) {
		vb2ex_msleep(MAX_MSEC_PER_LOOP);
		msec -= MAX_MSEC_PER_LOOP;
	}

	delay = msec * TICKS_PER_MSEC;
	start = get_timer(0);

	while (get_timer(start) < delay)
		udelay(100);
}

void vb2ex_beep(u32 msec, u32 frequency)
{
	struct udevice *dev;
	int ret;

	ret = uclass_first_device_err(UCLASS_SOUND, &dev);
	if (!ret)
		ret = sound_setup(dev);
	if (ret) {
		log_debug("Failed to initialise sound.\n");
		return;
	}

	printf("About to beep for %d ms at %d Hz.\n", msec, frequency);
	if (!msec)
		return;

	if (frequency) {
		if (sound_beep(dev, msec, frequency)) {
			log_debug("Failed to play beep.\n");
			return;
		}
	} else {
		vb2ex_msleep(msec);
	}
}

u32 vb2ex_mtime(void)
{
	return get_timer(0);
}

void vb2ex_printf(const char *func, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	if (func)
		printf("%s: ", func);
	vprintf(fmt, ap);
	va_end(ap);
}

void vb2ex_abort(void)
{
        panic("vboot has aborted execution; exit\n");
}

void *xmalloc(size_t size)
{
	void *ptr;

	ptr = malloc(size);
	if (!ptr)
		panic("Cannot alloc");

	return ptr;
}

void *xzalloc(size_t size)
{
	void *ptr;

	ptr = calloc(1, size);
	if (!ptr)
		panic("Cannot alloc");

	return ptr;
}
