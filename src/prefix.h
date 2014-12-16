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

const char *prefix_ntop(char *dst, size_t bufflen, const struct in6_addr *addr, uint8_t plen);

#define PREFIX_REPR(p, plen) (plen?prefix_ntop(alloca(INET6_ADDRSTRLEN + 4), INET6_ADDRSTRLEN + 4,  p, plen):"::/0")

#endif /* PREFIX_H_ */
