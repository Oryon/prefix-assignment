/*
 * Author: Pierre Pfister <pierre@darou.fr>
 *
 * Copyright (c) 2014 cisco Systems, Inc.
 *
 * Prefix manipulation utilities.
 *
 */


#ifndef PREFIX_H_
#define PREFIX_H_

#include <netinet/in.h>

/* Fills the buffer with a string representation
 * of the address and prefix length.
 * Returns dst on success, or NULL if the buffer is not
 * long enough to contain the prefix. */
const char *prefix_ntop(char *dst, size_t bufflen, const struct in6_addr *addr, uint8_t plen);

/* Fills the buffer with a string representation
 * of the significant bits of the prefix.
 * Returns dst on success, or NULL if the buffer is not
 * long enough to contain the prefix. */
const char *prefix_ntopc(char *dst, size_t bufflen, const struct in6_addr *prefix, uint8_t plen);

/* Reads a prefix from a null-terminated string.
 * Returns 1 on success, 0 on error (Similarly to inet_pton).
 */
int prefix_pton(const char *src, struct in6_addr *addr, uint8_t *plen);

#endif /* PREFIX_H_ */
