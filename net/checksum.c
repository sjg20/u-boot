// SPDX-License-Identifier: BSD-2-Clause
/*
 * This file was originally taken from the FreeBSD project.
 *
 * Copyright (c) 2001 Charles Mott <cm@linktel.net>
 * Copyright (c) 2008 coresystems GmbH
 * All rights reserved.
 */

#include <common.h>
#include <net.h>

unsigned compute_ip_checksum(const void *vptr, unsigned nbytes)
{
	int sum, oddbyte;
	const unsigned short *ptr = vptr;

	sum = 0;
	while (nbytes > 1) {
		sum += *ptr++;
		nbytes -= 2;
	}
	if (nbytes == 1) {
		oddbyte = 0;
		((u8 *)&oddbyte)[0] = *(u8 *)ptr;
		((u8 *)&oddbyte)[1] = 0;
		sum += oddbyte;
	}
	sum = (sum >> 16) + (sum & 0xffff);
	sum += (sum >> 16);
	sum = ~sum & 0xffff;

	return sum;
}

unsigned add_ip_checksums(unsigned offset, unsigned sum, unsigned new)
{
	unsigned long checksum;

	sum = ~sum & 0xffff;
	new = ~new & 0xffff;
	if (offset & 1) {
		/*
		 * byte-swap the sum if it came from an odd offset; since the
		 * computation is endian independant this works.
		 */
		new = ((new >> 8) & 0xff) | ((new << 8) & 0xff00);
	}
	checksum = sum + new;
	if (checksum > 0xffff)
		checksum -= 0xffff;

	return (~checksum) & 0xffff;
}

int ip_checksum_ok(const void *addr, unsigned nbytes)
{
	return !(compute_ip_checksum(addr, nbytes) & 0xfffe);
}

/**
 * net_random_ethaddr - Generate software assigned random Ethernet address
 * @addr: Pointer to a six-byte array containing the Ethernet address
 *
 * Generate a random Ethernet address (MAC) that is not multicast
 * and has the local assigned bit set.
 */
void net_random_ethaddr(uchar *addr)
{
	int i;
	unsigned int seed = get_ticks();

	for (i = 0; i < 6; i++)
		addr[i] = rand_r(&seed);

	addr[0] &= 0xfe;	/* clear multicast bit */
	addr[0] |= 0x02;	/* set local assignment bit (IEEE802) */
}
