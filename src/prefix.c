/*
 * Author: Pierre Pfister <pierre@darou.fr>
 *
 * Copyright (c) 2014 cisco Systems, Inc.
 *
 */

#include "prefix.h"

uint8_t prefix_contains(const struct in6_addr *p, uint8_t plen, const struct in6_addr *addr)
{
	int blen = plen >> 3;
	if(blen && memcmp(p, addr, blen))
		return 0;

	int rem = plen & 0x07;
	if(rem && ((p->s6_addr[blen] ^ addr->s6_addr[blen]) >> (8 - rem)))
		return 0;

	return 1;
}
