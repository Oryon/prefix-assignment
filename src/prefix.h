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
 * of the address and prefix length. */
const char *prefix_ntop(char *dst, size_t bufflen, const struct in6_addr *addr, uint8_t plen);

/* Fills the buffer with a string representation
 * of the significant bits of the prefix. */
const char *prefix_ntopc(char *dst, size_t bufflen, const struct in6_addr *addr, uint8_t plen);

#endif /* PREFIX_H_ */
