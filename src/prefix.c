/*
 * Author: Pierre Pfister <pierre@darou.fr>
 *
 * Copyright (c) 2014 cisco Systems, Inc.
 *
 */

#include "prefix.h"

#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *addr_ntop(char *dst, size_t bufflen, const struct in6_addr *addr)
{
	int i = IN6_IS_ADDR_V4MAPPED(addr);
	if(i) {
		return inet_ntop(AF_INET, &addr->s6_addr32[3], dst, bufflen);
	} else {
		return inet_ntop(AF_INET6, addr, dst, bufflen);
	}
}

const char *prefix_ntop(char *dst, size_t bufflen, const struct in6_addr *addr, uint8_t plen)
{
	if(bufflen < 4 || !addr_ntop(dst, bufflen - 4, addr))
		return NULL;

	char *str = dst + strlen(dst);
	if(plen >= 96 && IN6_IS_ADDR_V4MAPPED(addr)) {
		sprintf(str, "/%d", plen - 96);
	} else {
		sprintf(str, "/%d", plen);
	}
	return dst;
}

const char *prefix_ntopc(char *dst, size_t bufflen, const struct in6_addr *addr, uint8_t plen)
{
	struct in6_addr p = {.s6_addr={}};
	size_t bytes = plen >> 3;
	memcpy(&p, addr, bytes);
	uint8_t rembit = plen & 0x07;
	if(rembit)
		p.s6_addr[bytes] = (0xff << (8 - rembit)) & addr->s6_addr[bytes];

	return prefix_ntop(dst, bufflen, &p, plen);
}
