/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright 2012 Google Inc.
 */

#ifndef __CROS_PHYSMEM_H__
#define __CROS_PHYSMEM_H__

typedef void (*PhysMapFunc)(uint64_t phys_addr, void *s, uint64_t n,
			    void *data);

/*
 * Run a function on physical memory which may not be accessible directly.
 *
 * In this function, it will remapping physical memory when needed and then pass
 * then accessible pointer to the function. Due to the mapping limitation, this
 * function may split physical memory to multiple segment and call `callback`
 * multiple times separately.
 *
 * @param s			The physical address to start.
 * @param n			The number of bytes to operate.
 * @param func	The function which do the actually work.
 * @param data	The data that can be used in func.
 */
void arch_phys_map(uint64_t s, uint64_t n, PhysMapFunc func, void *data);

#endif /* __CROS_PHYSMEM_H__ */
