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
#include <stdbool.h>

bool prefix_contains(const struct in6_addr *p, uint8_t plen, const struct in6_addr *addr);

#define prefix_overlap(p1, plen1, p2, plen2) ((plen1 > plen2)?prefix_contains(p2, plen2, p1):prefix_contains(p1, plen1, p2))

#endif /* PREFIX_H_ */
