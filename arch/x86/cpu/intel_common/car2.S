/* SPDX-License-Identifier: GPL-2.0 */
/*
 * This file was modified from the coreboot version.
 *
 * Copyright (C) 2015-2016 Intel Corp.
 */

#include <config.h>
#include <asm/msr-index.h>
#include <asm/mtrr.h>
#include <asm/post.h>
#include <asm/processor.h>
#include <asm/processor-flags.h>

#define KiB 1024

#define IS_POWER_OF_2(x)	(!((x) & ((x) - 1)))

/*
 * macro: clear_car
 * Clears the region between CONFIG_DCACHE_RAM_BASE and
 * CONFIG_DCACHE_RAM_BASE + CONFIG_DCACHE_RAM_SIZE to populate
 * cachelines.
 * Clobbers %eax, %ecx, %edi.
 */
.macro clear_car
	/* Clear the cache memory region. This will also fill up the cache */
	movl	$CONFIG_DCACHE_RAM_BASE, %edi
	movl	$CONFIG_DCACHE_RAM_SIZE, %ecx
	shr	$0x02, %ecx
	xor	%eax, %eax
	cld
	rep	stosl
.endm

.global car_init
car_init:
	post_code(POST_CAR_START)

	/*
	 * Use the MTRR default type MSR as a proxy for detecting INIT#.
	 * Reset the system if any known bits are set in that MSR. That is
	 * an indication of the CPU not being properly reset.
	 */
check_for_clean_reset:
	mov	$MTRR_DEF_TYPE_MSR, %ecx
	rdmsr
	and	$(MTRR_DEF_TYPE_EN | MTRR_DEF_TYPE_FIX_EN), %eax
	cmp	$0, %eax
	jz	no_reset
	/* perform warm reset */
	movw	$IO_PORT_RESET, %dx
	movb	$(SYS_RST | RST_CPU), %al
	outb	%al, %dx

no_reset:
	post_code(POST_CAR_SIPI)

	/* Clear/disable fixed MTRRs */
	mov	$fixed_mtrr_list_size, %ebx
	xor	%eax, %eax
	xor	%edx, %edx

clear_fixed_mtrr:
	add	$-2, %ebx
	movzwl	fixed_mtrr_list(%ebx), %ecx
	wrmsr
	jnz	clear_fixed_mtrr

	post_code(POST_CAR_MTRR)

	/* Figure out how many MTRRs we have, and clear them out */
	mov	$MTRR_CAP_MSR, %ecx
	rdmsr
	movzb	%al, %ebx		/* Number of variable MTRRs */
	mov	$MTRR_PHYS_BASE_MSR(0), %ecx
	xor	%eax, %eax
	xor	%edx, %edx

clear_var_mtrr:
	wrmsr
	inc	%ecx
	wrmsr
	inc	%ecx
	dec	%ebx
	jnz	clear_var_mtrr

	post_code(POST_CAR_UNCACHEABLE)

	/* Configure default memory type to uncacheable (UC) */
	mov	$MTRR_DEF_TYPE_MSR, %ecx
	rdmsr
	/* Clear enable bits and set default type to UC */
	and	$~(MTRR_DEF_TYPE_MASK | MTRR_DEF_TYPE_EN | \
		 MTRR_DEF_TYPE_FIX_EN), %eax
	wrmsr

	/*
	 * Configure MTRR_PHYS_MASK_HIGH for proper addressing above 4GB
	 * based on the physical address size supported for this processor
	 * This is based on read from CPUID EAX = 080000008h, EAX bits [7:0]
	 *
	 * Examples:
	 *  MTRR_PHYS_MASK_HIGH = 00000000Fh  For 36 bit addressing
	 *  MTRR_PHYS_MASK_HIGH = 0000000FFh  For 40 bit addressing
	 */

	movl	$0x80000008, %eax	/* Address sizes leaf */
	cpuid
	sub	$32, %al
	movzx	%al, %eax
	xorl	%esi, %esi
	bts	%eax, %esi
	dec	%esi			/* esi <- MTRR_PHYS_MASK_HIGH */

	post_code(POST_CAR_BASE_ADDRESS)

#if IS_POWER_OF_2(CONFIG_DCACHE_RAM_SIZE)
	/* Configure CAR region as write-back (WB) */
	mov	$MTRR_PHYS_BASE_MSR(0), %ecx
	mov	$CONFIG_DCACHE_RAM_BASE, %eax
	or	$MTRR_TYPE_WRBACK, %eax
	xor	%edx,%edx
	wrmsr

	/* Configure the MTRR mask for the size region */
	mov	$MTRR_PHYS_MASK(0), %ecx
	mov	$CONFIG_DCACHE_RAM_SIZE, %eax	/* size mask */
	dec	%eax
	not	%eax
	or	$MTRR_PHYS_MASK_VALID, %eax
	movl	%esi, %edx	/* edx <- MTRR_PHYS_MASK_HIGH */
	wrmsr
#elif (CONFIG_DCACHE_RAM_SIZE == 768 * KiB) /* 768 KiB */
	/* Configure CAR region as write-back (WB) */
	mov	$MTRR_PHYS_BASE_MSR(0), %ecx
	mov	$CONFIG_DCACHE_RAM_BASE, %eax
	or	$MTRR_TYPE_WRBACK, %eax
	xor	%edx,%edx
	wrmsr

	mov	$MTRR_PHYS_MASK_MSR(0), %ecx
	mov	$(512 * KiB), %eax	/* size mask */
	dec	%eax
	not	%eax
	or	$MTRR_PHYS_MASK_VALID, %eax
	movl	%esi, %edx	/* edx <- MTRR_PHYS_MASK_HIGH */
	wrmsr

	mov	$MTRR_PHYS_BASE_MSR(1), %ecx
	mov	$(CONFIG_DCACHE_RAM_BASE + 512 * KiB), %eax
	or	$MTRR_TYPE_WRBACK, %eax
	xor	%edx,%edx
	wrmsr

	mov	$MTRR_PHYS_MASK_MSR(1), %ecx
	mov	$(256 * KiB), %eax	/* size mask */
	dec	%eax
	not	%eax
	or	$MTRR_PHYS_MASK_VALID, %eax
	movl	%esi, %edx	/* edx <- MTRR_PHYS_MASK_HIGH */
	wrmsr
#else
#error "DCACHE_RAM_SIZE is not a power of 2 and setup code is missing"
#endif
	post_code(POST_CAR_FILL)

	/* Enable variable MTRRs */
	mov	$MTRR_DEF_TYPE_MSR, %ecx
	rdmsr
	or	$MTRR_DEF_TYPE_EN, %eax
	wrmsr

	/* Enable caching */
	mov	%cr0, %eax
	and	$~(X86_CR0_CD | X86_CR0_NW), %eax
	invd
	mov	%eax, %cr0

#if IS_ENABLED(CONFIG_INTEL_CAR_NEM)
	jmp	car_nem
#elif IS_ENABLED(CONFIG_INTEL_CAR_CQOS)
	jmp	car_cqos
#elif IS_ENABLED(CONFIG_INTEL_CAR_NEM_ENHANCED)
	jmp	car_nem_enhanced
#else
#error "No CAR mechanism selected:
#endif
	jmp	car_init_ret

fixed_mtrr_list:
	.word	MTRR_FIX_64K_00000_MSR
	.word	MTRR_FIX_16K_80000_MSR
	.word	MTRR_FIX_16K_A0000_MSR
	.word	MTRR_FIX_4K_C0000_MSR
	.word	MTRR_FIX_4K_C8000_MSR
	.word	MTRR_FIX_4K_D0000_MSR
	.word	MTRR_FIX_4K_D8000_MSR
	.word	MTRR_FIX_4K_E0000_MSR
	.word	MTRR_FIX_4K_E8000_MSR
	.word	MTRR_FIX_4K_F0000_MSR
	.word	MTRR_FIX_4K_F8000_MSR
fixed_mtrr_list_size = . - fixed_mtrr_list

#if IS_ENABLED(CONFIG_INTEL_CAR_NEM)
.global car_nem
car_nem:
	/* Disable cache eviction (setup stage) */
	mov	$MSR_EVICT_CTL, %ecx
	rdmsr
	or	$0x1, %eax
	wrmsr

	post_code(0x26)

	clear_car

	post_code(0x27)

	/* Disable cache eviction (run stage) */
	mov	$MSR_EVICT_CTL, %ecx
	rdmsr
	or	$0x2, %eax
	wrmsr

	post_code(0x28)

	jmp	car_init_ret

#elif IS_ENABLED(CONFIG_INTEL_CAR_CQOS)
.global car_cqos
car_cqos:
	/*
	 * Create CBM_LEN_MASK based on CBM_LEN
	 * Get CPUID.(EAX=10H, ECX=2H):EAX.CBM_LEN[bits 4:0]
	 */
	mov	$0x10, %eax
	mov	$0x2,  %ecx
	cpuid
	and	$0x1f, %eax
	add	$1, %al

	mov	$1, %ebx
	mov	%al, %cl
	shl	%cl, %ebx
	sub	$1, %ebx

	/* Store the CBM_LEN_MASK in mm3 for later use */
	movd	%ebx, %mm3

	/*
	 * Disable both L1 and L2 prefetcher. For yet-to-understood reason,
	 * prefetchers slow down filling cache with rep stos in CQOS mode.
	 */
	mov	$MSR_PREFETCH_CTL, %ecx
	rdmsr
	or	$(PREFETCH_L1_DISABLE | PREFETCH_L2_DISABLE), %eax
	wrmsr

#if (CONFIG_DCACHE_RAM_SIZE == CONFIG_L2_CACHE_SIZE)
/*
 * If CAR size is set to full L2 size, mask is calculated as all-zeros.
 * This is not supported by the CPU/uCode.
 */
#error "CQOS CAR may not use whole L2 cache area"
#endif

	/* Calculate how many bits to be used for CAR */
	xor	%edx, %edx
	mov	$CONFIG_DCACHE_RAM_SIZE, %eax	/* dividend */
	mov	$CONFIG_CACHE_QOS_SIZE_PER_BIT, %ecx	/* divisor */
	div	%ecx		/* result is in eax */
	mov	%eax, %ecx	/* save to ecx */
	mov	$1, %ebx
	shl	%cl, %ebx
	sub	$1, %ebx	/* resulting mask is is in ebx */

	/* Set this mask for initial cache fill */
	mov	$MSR_L2_QOS_MASK(0), %ecx
	rdmsr
	mov	%ebx, %eax
	wrmsr

	/* Set CLOS selector to 0 */
	mov	$MSR_IA32_PQR_ASSOC, %ecx
	rdmsr
	and	$~MSR_IA32_PQR_ASSOC_MASK, %edx	/* select mask 0 */
	wrmsr

	/* We will need to block CAR region from evicts */
	mov	$MSR_L2_QOS_MASK(1), %ecx
	rdmsr
	/* Invert bits that are to be used for cache */
	mov	%ebx, %eax
	xor	$~0, %eax			/* invert 32 bits */

	/*
	 * Use CBM_LEN_MASK stored in mm3 to set bits based on Capacity Bit
	 * Mask Length.
	 */
	movd	%mm3, %ebx
	and	%ebx, %eax
	wrmsr

	post_code(0x26)

	/* Clear the cache memory region. This will also fill up the cache */
	movl	$CONFIG_DCACHE_RAM_BASE, %edi
	movl	$CONFIG_DCACHE_RAM_SIZE, %ecx
	shr	$0x02, %ecx
	xor	%eax, %eax
	cld
	rep	stosl

	post_code(0x27)

	/* Cache is populated. Use mask 1 that will block evicts */
	mov	$MSR_IA32_PQR_ASSOC, %ecx
	rdmsr
	and	$~MSR_IA32_PQR_ASSOC_MASK, %edx	/* clear index bits first */
	or	$1, %edx			/* select mask 1 */
	wrmsr

	/* Enable prefetchers */
	mov	$MSR_PREFETCH_CTL, %ecx
	rdmsr
	and	$~(PREFETCH_L1_DISABLE | PREFETCH_L2_DISABLE), %eax
	wrmsr

	post_code(0x28)

	jmp	car_init_ret

#elif IS_ENABLED(CONFIG_INTEL_CAR_NEM_ENHANCED)
.global car_nem_enhanced
car_nem_enhanced:
	/* Disable cache eviction (setup stage) */
	mov	$MSR_EVICT_CTL, %ecx
	rdmsr
	or	$0x1, %eax
	wrmsr
	post_code(0x26)

	/* Create n-way set associativity of cache */
	xorl	%edi, %edi
find_llc_subleaf:
	movl	%edi, %ecx
	movl	$0x04, %eax
	cpuid
	inc	%edi
	and	$0xe0, %al	/* EAX[7:5] = Cache Level */
	cmp	$0x60, %al	/* Check to see if it is LLC */
	jnz	find_llc_subleaf

	/*
	 * Calculate the total LLC size
	 * (Line_Size + 1) * (Sets + 1) * (Partitions + 1) * (Ways + 1)
	 * (EBX[11:0] + 1) * (ECX + 1) * (EBX[21:12] + 1) * EBX[31:22] + 1)
	 */

	mov	%ebx, %eax
	and	$0xFFF, %eax
	inc	%eax
	inc	%ecx
	mul	%ecx
	mov	%eax, %ecx
	mov	%ebx, %eax
	shr	$12, %eax
	and	$0x3FF, %eax
	inc	%eax
	mul	%ecx
	shr	$22, %ebx
	inc	%ebx
	mov	%ebx, %edx
	mul	%ebx /* eax now holds total LLC size */

	/*
	 * The number of the ways that we want to protect from eviction
	 * can be calculated as RW data stack size / way size where way
	 * size is Total LLC size / Total number of LLC ways.
	 */
	div	%ebx /* way size */
	mov	%eax, %ecx

	/*
	 * Check if way size if bigger than the cache ram size.
	 * Then we need to allocate just one way for non-eviction
	 * of RW data.
	 */
	movl	$0x01, %eax
	mov	%eax, %edx /* back up data_ways in edx */
	cmp	$CONFIG_DCACHE_RAM_SIZE, %ecx
	jnc	set_eviction_mask

	/*
	 * RW data size / way size is equal to number of
	 * ways to be configured for non-eviction
	 */
	mov	$CONFIG_DCACHE_RAM_SIZE, %eax
	div	%ecx
	mov	%eax, %edx /* back up data_ways in edx */
	mov	%eax, %ecx
	movl	$0x01, %eax
	shl	%cl, %eax
	subl	$0x01, %eax

set_eviction_mask:
	mov	%ebx, %edi /* back up number of ways */
	mov	%eax, %esi /* back up the non-eviction mask */
#if CONFIG(CAR_HAS_SF_MASKS)
	mov	%edx, %eax /* restore data_ways in eax */
	/*
	 * Calculate SF masks 2:
	 * 1. if CONFIG_SF_MASK_2WAYS_PER_BIT: data_ways = data_ways / 2
	 * 2. Program MSR 0x1892 Non-Eviction Mask #2
	 *    IA32_CR_SF_QOS_MASK_2 = ((1 << data_ways) - 1)
	 */
#if CONFIG(SF_MASK_2WAYS_PER_BIT)
	cmp	$0x01, %eax /* Skip Step 1 if data_ways = 1 */
	jz	program_sf2
	movl	$0x01, %ecx /* Step 1 */
	shr	%cl, %eax
#endif
	/* Step 2 */
	mov	%eax, %ecx
	movl	$0x01, %eax
	shl	%cl, %eax
	subl	$0x01, %eax
program_sf2:
	mov	%eax, %ebx /* back up IA32_CR_SF_QOS_MASK_2 in ebx */
	xorl	%edx, %edx
	mov	$IA32_CR_SF_QOS_MASK_2, %ecx
	wrmsr

	/*
	 * Calculate the SF Mask 1:
	 * 1. Calculate SFWayCnt = IA32_SF_QOS_INFO & Bit [5:0]
	 * 2. if CONFIG_SF_MASK_2WAYS_PER_BIT: SFWayCnt = SFWayCnt / 2
	 * 3. Set SF_MASK_1 = ((1 << SFWayCnt) - 1) - IA32_CR_SF_QOS_MASK_2
	 */
	mov	$IA32_SF_QOS_INFO, %ecx
	rdmsr
	and	$IA32_SF_WAY_COUNT_MASK, %eax /* Step 1 */
#if CONFIG(SF_MASK_2WAYS_PER_BIT)
	/* Assumption: skip checking SFWayCnt = 1 i.e. 1 way LLC (which is not practical) */
	movl	$0x01, %ecx /* Step 2 */
	shr	%cl, %eax
#endif
	/* Step 3 */
	mov	%eax, %ecx
	movl	$0x01, %eax
	shl	%cl, %eax
	subl	$0x01, %eax
	sub	%ebx, %eax
	xorl	%edx, %edx
	movl	$IA32_CR_SF_QOS_MASK_1, %ecx
	wrmsr
#endif
#if CONFIG(CAR_HAS_L3_PROTECTED_WAYS)
	/* Set MSR 0xC85 L3_Protected_ways = ((1 << data ways) - 1) */
	mov	%esi, %eax
	xorl	%edx, %edx
	mov	$IA32_L3_PROTECTED_WAYS, %ecx
	wrmsr
#endif
	/*
	 * Program MSR 0xC91 IA32_L3_MASK_1
	 * This MSR contain one bit per each way of LLC
	 * - If this bit is '0' - the way is protected from eviction
	 * - If this bit is '1' - the way is not protected from eviction
	 */
	mov	$0x1, %eax
	mov	%edi, %ecx
	shl	%cl, %eax
	subl	$0x01, %eax
	mov	%eax, %ecx
	mov	%esi, %eax

	xor	$~0, %eax	/* invert 32 bits */
	and	%ecx, %eax
	movl	$IA32_L3_MASK_1, %ecx
	xorl	%edx, %edx
	wrmsr
	/*
	 * Program MSR 0xC92 IA32_L3_MASK_2
	 * This MSR contain one bit per each way of LLC
	 * - If this bit is '0' - the way is protected from eviction
	 * - If this bit is '1' - the way is not protected from eviction
	 */
	mov	%esi, %eax
	movl	$IA32_L3_MASK_2, %ecx
	xorl	%edx, %edx
	wrmsr
	/*
	 * Set IA32_PQR_ASSOC
	 *
	 * Possible values:
	 * 0: Default value, no way mask should be applied
	 * 1: Apply way mask 1 to LLC
	 * 2: Apply way mask 2 to LLC
	 * 3: Shouldn't be use in NEM Mode
	 */
	movl	$IA32_PQR_ASSOC, %ecx
	xorl	%eax, %eax
	xorl	%edx, %edx
#if CONFIG(COS_MAPPED_TO_MSB)
	movl	$0x02, %edx
#else
	movl	$0x02, %eax
#endif
	wrmsr

	clear_car

	/*
	 * Set IA32_PQR_ASSOC
	 * At this stage we apply LLC_WAY_MASK_1 to the cache.
	*/
	movl	$IA32_PQR_ASSOC, %ecx
	xorl	%eax, %eax
	xorl	%edx, %edx
#if CONFIG(COS_MAPPED_TO_MSB)
	movl	$0x01, %edx
#else
	movl	$0x01, %eax
#endif
	wrmsr

	post_code(0x27)
	/*
	 * Enable No-Eviction Mode Run State by setting
	 * NO_EVICT_MODE MSR 2E0h bit [1] = '1'.
	 */

	movl	$MSR_EVICT_CTL, %ecx
	rdmsr
	orl	$0x02, %eax
	wrmsr

	post_code(0x28)

	jmp	car_init_ret
#endif

#if CONFIG_IS_ENABLED(X86_16BIT_INIT)
_dt_ucode_base_size:
	/* These next two fields are filled in by binman */
.globl ucode_base
ucode_base:	/* Declared in microcode.h */
	.long	0			/* microcode base */
.globl ucode_size
ucode_size:	/* Declared in microcode.h */
	.long	0			/* microcode size */
	.long	CONFIG_SYS_MONITOR_BASE	/* code region base */
	.long	CONFIG_SYS_MONITOR_LEN	/* code region size */
#endif
