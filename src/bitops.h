/*
 * Author: Pierre Pfister <pierre@darou.fr>
 *
 * Copyright (c) 2014 cisco Systems, Inc.
 *
 * Advanced bitwise operations.
 *
 */

#ifndef BITOPS_H_
#define BITOPS_H_

#include <stdint.h>
#include <stdlib.h>

/* Copy some bits from a byte to another */
void bbytecpy (uint8_t *dst, const uint8_t *src,
		uint8_t frombit, uint8_t nbits);

/* Compare two prefixes of same bit length. */
int bmemcmp(const void *m1, const void *m2, size_t bitlen);

/* Copy bits from a buffer to another, starting
 * from the same bit index. */
void bmemcpy(void *dst, const void *src,
		size_t frombit, size_t nbits);

/* Copy bits from a buffer to another, starting
 * from different bit indexes. */
void bmemcpy_shift(void *dst, size_t dst_start,
		const void *src, size_t src_start,
		size_t nbits);

#endif /* BITOPS_H_ */
