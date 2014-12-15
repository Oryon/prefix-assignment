/*
 * Author: Pierre Pfister <pierre pfister@darou.fr>
 *
 * Copyright (c) 2014 Cisco Systems, Inc.
 *
 * The Prefix Assignment Algorithm is generic.
 * It can run on different flooding mechanisms,
 * using different Node IDs, prefix space, and
 * configuration variables.
 * This file lists all parameters, mandatory or
 * optional, specifying the behavior of the algorithm.
 * You can modify it, or use a different one and build the code
 * using it.
 *
 */

#ifndef PA_CONF_H_
#define PA_CONF_H_



/**********************************
 *    Node ID Space Definition    *
 **********************************/

/* In this example, 32bits long Node IDs
 * are defined. */

/* The Node ID array type.
 *   (Mandatory) */
#include <stdint.h>
#define PA_NODE_ID_TYPE uint32_t

/* The Node ID array length. Set it to 1 in
 * order to use a single structure instead
 * of an array.
 *   (Mandatory) */
#define PA_NODE_ID_LEN  1

/* Node ID comparison function.
 *   (Mandatory) */
#define PA_NODE_ID_CMP(node_id1, node_id2) \
	(*(node_id1) > *(node_id2))

/* Node ID print format and arguments.
 *   (Optional - Default to hexdump) */
#include <inttypes.h>
#define PA_NODE_ID_P   "0x%08"PRIx32
#define PA_NODE_ID_PA(node_id) *(node_id)


/**********************************
 *     Prefix Space Specific      *
 **********************************/

/* The prefix storage type and prefix length type.
 *    (Mandatory)
 */
#include <netinet/in.h>
typedef struct in6_addr pa_prefix;
typedef uint8_t pa_plen;

/* Prefix manipulation functions.
 *    (Mandatory)
 */
#include "prefix.h"
#define pa_prefix_equals(p1, plen1, p2, plen2) \
	prefix_equals(p1, plen1, p2, plen2)

#define pa_prefix_contains(p1, plen1, p2) \
	prefix_contains(p1, plen1, p2)

#define pa_prefix_cpy(sp, splen, dp, dplen) \
	do{*(dp) = *(sp); dplen = splen} while(0)

#define pa_prefix_tostring(p, plen) \
	PREFIX_REPR(p, plen)

/**********************************
 *   Flooding Mechanism Specific  *
 **********************************/

/* Advertised Prefix Priority type.
 *   (Mandatory) */
typedef uint8_t pa_priority;

/* Default flooding delay in milliseconds.
 * Set when pa_core is initialized.
 *    (Optional - Default to 10000) */
#define PA_DEFAULT_FLOODING_DELAY 10000



/**********************************
 *     Implementation specific    *
 **********************************/

/* Internal rule priority type.
 * The value ZERO is reserved.
 * The higher the value, the higher the rule priority.
 * When a rule is removed, all Assigned Prefixes which
 * were published by the rule are unpublished and left
 * for adoption to other rules.
 *   (Mandatory) */
typedef uint16_t pa_rule_priority;

/* Delay, in milliseconds, between the events
 * triggering the prefix assignment routine and
 * the actual time it is run.
 * The routine is never run synchronously, even
 * when the delay is set to 0.
 *   (Optional - Default to 20) */
#define PA_RUN_DELAY 20

/* The pa_ldp structure contains PA_LDP_USERS
 * void * pointers, to be used by users for
 * storing private data.
 *    (Optional) */
#define PA_LDP_USERS 2

/* The Link structure is provided by user(s).
 * When contained in different larger struct,
 * it may be useful to identify the type of
 * struct it is included in.
 *    (Optional) */
#define PA_LINK_TYPE

/* The Delegated Prefix structure is provided
 * by user(s). When contained in different larger
 * struct, it may be useful to identify the
 * type of struct it is included in.
 *    (Optional) */
#define PA_DP_TYPE


#endif /* PA_CONF_H_ */
