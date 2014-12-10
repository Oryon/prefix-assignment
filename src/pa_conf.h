/*
 * Author: Pierre Pfister <pierre pfister@darou.fr>
 *
 * Copyright (c) 2014 Cisco Systems, Inc.
 *
 * The Prefix Assignment Algorithm behavior depends on
 * some defined values. This file contains a configuration example.
 * You can modify it, or use a different one and build
 * the pa_* files with -DPA_CONF=your_conf_file_path.
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
 *   (Mandatory) */
typedef uint16_t pa_rule_priority;

/* Delay, in milliseconds, between the events
 * triggering the prefix assignment routine and
 * the actual time it is run.
 * The routine is never run synchronously, even
 * when the delay is set to 0.
 *   (Optional - Default to 20) */
#define PA_RUN_DELAY 20

/* The pa_ap structure contains PA_AP_USERS
 * void * pointers, to be used by users for
 * storing private data.
 *    (Optional) */
#define PA_AP_USERS 2

/* pa_link and pa_dp structures contain a
 * pa_user_id field to identify a user owning
 * the structure.
 *    (Optional) */
#define PA_USER_ID uint8_t



#endif /* PA_CONF_H_ */
