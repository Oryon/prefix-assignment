/*
 * Author: Pierre Pfister <pierre pfister@darou.fr>
 *
 * Copyright (c) 2014 Cisco Systems, Inc.
 *
 * Implementation of the Distributed Prefix Assignment Algorithm.
 *
 * The algorithm is specified as an IETF Internet Draft:
 * https://tools.ietf.org/html/draft-ietf-homenet-prefix-assignment
 *
 */

#ifndef PA_CORE_H_
#define PA_CORE_H_

#include <netinet/in.h>
#include <inttypes.h>

#include <libubox/list.h>
#include <libubox/uloop.h>

#include "btrie.h"
#include "prefix.h"


/***************************
 *    Global parameters.   *
 ***************************/

#ifndef PA_WARNING
#define PA_WARNING(format, ...)
#endif
#ifndef PA_INFO
#define PA_INFO(format, ...)
#endif
#ifndef PA_DEBUG
#define PA_DEBUG(format, ...)
#endif

/* Node ID byte length. */
#define PA_NODE_ID_LEN 4

/* Node ID comparison function. */
#include <string.h>
#define PA_NODE_ID_CMP(id1, id2) memcmp(id1, id2, PA_NODE_ID_LEN)

/* Node ID print format and arguments. */
#define PA_NODE_ID_P   "0x%08"PRIx32
#define PA_NODE_ID_PA(node_id) *((uint32_t *)node_id)

/* Advertised Prefix Priority type. */
typedef uint8_t pa_priority;

/* Internal rule priority type. */
typedef uint16_t pa_rule_priority;

/* Delay, in milliseconds, between the events triggering
 * the prefix assignment routine and the actual time it is run.
 * The routine is never run synchronously, even when the delay is set to 0. */
#define PA_RUN_DELAY 20

/* Default flooding delay in milliseconds.
 * Set when pa_core is initialized. */
#define PA_DEFAULT_FLOODING_DELAY 10000

/* There may be multiple 'users' using the structures.
 * struct pa_ap contains PA_AP_USERS void * pointers for users.
 * struct pa_dp and pa_link contains a pa_user_id field to identify the provider of the dp or link.
 * Each user must be given a unique pa_user_id.
 */
#define PA_AP_USERS 2
typedef uint8_t pa_user_id;



/***************************
 *       Generic API       *
 ***************************/

/* Structure containing state specific to the overall algorithm. */
struct pa_core {
	struct btrie prefixes;           /* btrie containing all Assigned and Advertised Prefixes */
	uint8_t node_id[PA_NODE_ID_LEN]; /* The Node ID of the local node. Initial value is 0. */
	uint32_t flooding_delay;         /* The Flooding Delay. Initial value is PA_DEFAULT_FLOODING_DELAY. */
	struct list_head users;
	struct list_head links;
	struct list_head dps;
	struct list_head rules;
};

/* Initializes a pa_core structure.
 * Once initialized, it can be configured with rules, prefixes and links.
 * Returns 0 upon success and -1 otherwise.
 */
void pa_core_init(struct pa_core *core);

/* Sets the local node ID.
 * This operation run or schedule
 */
void pa_core_set_node_id(struct pa_core *core, const uint8_t[PA_NODE_ID_LEN]);

/* Sets the flooding delay to the specified value.
 * When the delay is increased, all running timers are increased by old_flooding_delay - new_flooding_delay
 * When the delay is decreased, running timers are set to min(remaining, new_flooding_delay)
 * flooding_delay must be smaller than 2 << 31 because it is multiplied by 2 in a uint32_t.
 */
void pa_core_set_flooding_delay(struct pa_core *core, uint32_t flooding_delay);



/*
 * Structure used to identify a Shared or Private Link.
 */
struct pa_link {
	struct list_head le;  /* Linked in pa_core. */
	struct list_head aps; /* List of Assigned Prefixes assigned to this Link. */
	const char *name;     /* Name, displayed in logs. */
	pa_user_id user_id;   /* Identifies the provider of the link. */
};

/* Link print format and arguments. */
#define PA_LINK_P "%s"
#define PA_LINK_PA(pa_link) (pa_link)->name?(pa_link)->name:"null"

/* Adds and deletes a Link for prefix assignment */
int pa_link_add(struct pa_core *, struct pa_link *);
void pa_link_del(struct pa_link *);

/*
 * Structure used to identify a Delegated Prefix.
 */
struct pa_dp {
	struct list_head le;    /* Linked in pa_core. */
	struct list_head aps;   /* List of Assigned Prefixes from that Delegated Prefix. */
	struct in6_addr prefix; /* The delegated prefix value. */
	uint8_t plen;           /* The prefix length. */
	pa_user_id user_id;     /* Identifies the provider of the link. */
};

/* Delegated Prefix print format and arguments */
#define PA_DP_P "%s"
#define PA_DP_PA(pa_dp) PREFIX_REPR(&(pa_dp)->prefix, (pa_dp)->plen)

/* Adds and deletes a Delegated Prefix */
int pa_dp_add(struct pa_core *, struct pa_dp *);
void pa_dp_del(struct pa_dp *);

/*
 * Structure used to link all prefixes in the same tree.
 */
struct pa_pentry {
	struct btrie_element be; /* The btrie element. */
	uint8_t type;            /* Prefix type. */
#define PAT_AP 0x01
#define PAT_PP 0x02
};

/*
 * Structure used to identify a Link/Delegated Prefix pair.
 * It may or may not contain an Assigned Prefix.
 */
struct pa_ap {
	struct pa_pentry in_core;       /* Used to link aps and pps in the same btrie */
	struct list_head in_link;       /* Linked in the link structure. */
	struct list_head in_dp;         /* Linked in the Delegated Prefix structure. */
	struct pa_link *link;           /* The Link associated with the AP. */
	struct pa_dp *dp;               /* The DP associated with the AP. */
	struct pa_core *core;           /* Back-pointer to the associated pa_core struct */
	uint8_t assigned  : 1;          /* There is an associated Assigned Prefix. */
	uint8_t published : 1;          /* The AP is published. */
	uint8_t applied   : 1;          /* The AP is applied. */
	uint8_t adopting  : 1;          /* The AP will be adopted (Only set during backoff). */
	uint8_t valid     : 1;          /* (if assigned, in routine) Whether the routine will destroy the AP. */
	uint8_t backoff   : 1;          /* (in routine) The routine is executed following backoff timeout. */
	struct in6_addr prefix;         /* (if assigned) The AP prefix. */
	uint8_t plen;                   /* (if assigned) The AP prefix length. */
	pa_priority priority;           /* (if published) The Advertised Prefix Priority. */
	pa_rule_priority rule_priority; /* (if published) The internal rule priority. */
	struct pa_rule *rule;           /* (if published) The rule used to publish this prefix. */
	struct uloop_timeout routine_to;/* Timer used to schedule the routine. */
	struct uloop_timeout backoff_to;/* Timer used to backoff prefix generation, adoption or apply. */
	struct pa_pp *best_assignment;  /* (in routine) The best current assignment. */
#if PA_AP_USERS != 0
	void *users[PA_AP_USERS];
#endif
};

/* Assigned Prefix print format and arguments */
#define PA_AP_P "%s%%"PA_LINK_P" from "PA_DP_P" flags (%s %s %s)"
#define PA_AP_PA(pa_ap) ((pa_ap)->assigned)?PREFIX_REPR(&(pa_ap)->prefix, (pa_ap)->plen):"no-prefix", \
	PA_LINK_PA((pa_ap)->link), PA_DP_PA((pa_ap)->dp), \
	((pa_ap)->published)?"Published":"-", ((pa_ap)->applied)?"Applied":"-", ((pa_ap)->adopting)?"Adopting":"-"

/*
 * Structure used to identify an Advertised Prefix.
 */
struct pa_pp {
	struct pa_pentry in_core;     /* Used to link aps and pps in the same btrie */
	uint8_t node_id[PA_NODE_ID_LEN]; /* The node ID of the node advertising the prefix. */
	struct in6_addr prefix;       /* The Advertised Prefix). */
	uint8_t plen;                 /* The Advertised Prefix length. */
	pa_priority priority;         /* The Advertised Prefix Priority. */
	struct pa_link *link;         /* Advertised Prefix associated Shared Link (or null). */
};

/* Advertised Prefix print format and arguments */
#define PA_PP_P "%s%%"PA_LINK_P"@"PA_NODE_ID_P":(%d)"
#define PA_PP_PA(pa_pp) PREFIX_REPR(&(pa_pp)->prefix, (pa_pp)->plen), \
	PA_LINK_PA((pa_pp)->link), PA_NODE_ID_PA((pa_pp)->node_id), (pa_pp)->priority

/* Adds a new Advertised Prefix. */
int pa_pp_add(struct pa_core *, struct pa_pp *);

/* Removes an added Advertised Prefix. */
void pa_pp_del(struct pa_core *, struct pa_pp *);

/* Tell the content of the Advertised Prefix was changes. */
void pa_pp_update(struct pa_core *, struct pa_pp *);


/***************************
 *         User API        *
 ***************************/

struct pa_user {
	struct list_head le; /* Linked in pa_core. */

	/* These callbacks are called when the assigned, published
	 * and applied values are changed. They are not called
	 * when a Link/DP is created.
	 * When switched to 0, associated values are still present.
	 * i.e. the prefix and plen are valid when assigned == 0
	 * i.e. the priorities and rule are still valid when published == 0 */
	void (*assigned)(struct pa_user *, struct pa_ap *);
	void (*published)(struct pa_user *, struct pa_ap *);
	void (*applied)(struct pa_user *, struct pa_ap *);
};

/* pa_user print format and argument */
#define PA_USER_P "%p - %d:%d:%d"
#define PA_USER_PA(pa_user) pa_user, (pa_user)->assigned, (pa_user)->published, (pa_user)->applied

/* Adds a user which will receive events callback.
 * When added, the user does not receive callbacks for existing prefixes. */
#define pa_user_register(core, user) list_add(&(core)->users, &(user)->le)

/* Unregister a user. */
#define pa_user_unregister(user) list_del(&(user)->le)

#define pa_for_each_link(pa_core, pa_link) list_for_each_entry(pa_link, &(pa_core)->links, le)

#define pa_for_each_ap_in_link(pa_link, pa_ap) list_for_each_entry(pa_ap, &(pa_link)->aps, in_link)

#define pa_for_each_dp(pa_core, pa_dp) list_for_each_entry(pa_dp, &(pa_core)->dps, le)

#define pa_for_each_ap_in_dp(pa_dp, pa_ap) list_for_each_entry(pa_ap, &(pa_dp)->aps, in_dp)


/***************************
 *   Configuration API     *
 ***************************/

/* Result of the rule callback function.
 * Tells the algorithm what the rule wants to do.
 * Returning PA_RULE_CREATE does not mean a prefix may be found.
 * If PA_RULE_CREATE is returned, get_prefix function may be called
 * later to select a prefix.
 * This two-steps approach intends to avoid useless computations. */
enum pa_rule_target {
	PA_RULE_NO_MATCH, /* The rule does not match. */
	PA_RULE_CREATE,   /* The rule would like to create a new prefix,
	 	 	 	 	 	 get_prefix is called later to get, maybe, an available prefix. */
	PA_RULE_ADOPT,    /* The rule wants to adopt the Current Prefix. */
	PA_RULE_DESTROY   /* The rule wants to destroy the Current Prefix. */
};

/* This structure is a raw structure for prefix selection.
 * Specific rules and API are defined in pa_rules.h. */
struct pa_rule {
	struct list_head le; /* Linked in pa_core, the Link, or the DP. */

	const char *name; /* Rule name, displayed in logs. */

	/* See if a rule matches.
	 * If PA_RULE_ADOPT or PA_RULE_DESTROY are returned,
	 * the rule_priority pointer must be set to the priority
	 * used by the rule.
	 */
	enum pa_rule_target (*match)(struct pa_rule *, struct pa_ap *,
			pa_rule_priority *rule_priority);

	/* When match returned PA_RULE_CREATE, this function may be called afterward.
	 * best_rule_priority indicates the best other matching rule found
	 * until now.
	 * prefix, plen and priority must be set to the values
	 * to be used by the algorithm for the new assignment. */
	int (*get_prefix)(struct pa_rule *, struct pa_ap *,
			pa_rule_priority best_rule_priority,
			pa_rule_priority *rule_priority,
			struct in6_addr *prefix, uint8_t *plen, pa_priority *priority);
};

/* pa_rule print format and argument */
#define PA_RULE_P "'%s'@%p"
#define PA_RULE_PA(rule) (rule)->name?(rule)->name:"no-name", rule

void pa_rule_add(struct pa_core *, struct pa_rule *);
void pa_rule_del(struct pa_core *, struct pa_rule *);

/* Allows a rule to access the prefix count array.
 * This must not be computed by rules individually in order to avoid
 * redundant computations. */
const struct pa_rule_prefix_count *pa_rule_prefix_count(struct pa_core *, struct pa_rule_arg *);

#endif /* PA_CORE_H_ */
