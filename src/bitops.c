/*
 * Author: Pierre Pfister <pierre@darou.fr>
 *
 * Copyright (c) 2014 cisco Systems, Inc.
 */

#include "bitops.h"

#include <string.h>

void bbytecpy (uint8_t *dst, const uint8_t *src,
		uint8_t frombit, uint8_t nbits) {

	uint8_t mask = 0xff;
	mask <<= frombit;
	mask >>= 8 - nbits;
	mask <<= 8 - nbits - frombit;

	*dst &= ~mask;
	*dst |= (*src & mask);
}

void bmemcpy(void *dst, const void *src,
		size_t frombit, size_t nbits)
{
	// First bit that should not be copied
	size_t tobit = frombit + nbits;

	size_t frombyte = frombit >> 3;
	size_t tobyte = tobit >> 3;
	size_t nbyte = tobyte - frombyte;
	uint8_t frombitrem = frombit & 0x07;
	uint8_t tobitrem = tobit & 0x07;

	dst+=frombyte;
	src+=frombyte;

	if(!nbyte) {
		bbytecpy(dst, src, frombitrem, nbits);
		return;
	}

	if(frombitrem) {
		bbytecpy(dst, src, frombitrem, 8 - frombitrem);
		dst += 1;
		src += 1;
		nbyte -= 1;
	}

	memcpy(dst, src, nbyte);

	if(tobitrem)
		bbytecpy(dst + nbyte, src + nbyte, 0, tobitrem);
}

void bmemcpy_shift(void *dst, size_t dst_start,
		const void *src, size_t src_start,
		size_t nbits)
{
	dst += dst_start >> 3;
	dst_start &= 0x7;
	src += src_start >> 3;
	src_start &= 0x7;

	if(dst_start == src_start) {
		bmemcpy(dst, src, dst_start, nbits);
	} else {
		while(nbits) {
			uint8_t interm = *((uint8_t *)src);
			uint8_t n;
			int8_t shift = src_start - dst_start;
			if(shift > 0) {
				interm <<= shift;
				n = 8 - src_start;
				if(n > nbits)
					n = nbits;
				bbytecpy(dst, &interm, dst_start, n);
				dst_start += n;
				src_start = 0;
				src++;
			} else {
				interm >>= -shift;
				n = 8 - dst_start;
				if(n > nbits)
					n = nbits;
				bbytecpy(dst, &interm, dst_start, n);
				dst_start = 0;
				dst++;
				src_start += n;
			}
			nbits -= n;
		}
	}
}


int bmemcmp(const void *m1, const void *m2, size_t bitlen)
{
	size_t bytes = bitlen >> 3;
	int r;
	if( (r = memcmp(m1, m2, bytes)) )
		return r;

	uint8_t rembit = bitlen & 0x07;
	if(!rembit)
		return 0;

	uint8_t *p1 = ((uint8_t *) m1) + bytes;
	uint8_t *p2 = ((uint8_t *) m2) + bytes;
	uint8_t mask = ((uint8_t)0xff) << (8 - rembit);

	return ((int) (*p1 & mask)) - ((int) (*p2 & mask));
}
