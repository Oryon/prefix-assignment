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
#include <stdint.h>

#include <libubox/list.h>
#include <libubox/uloop.h>

#include "btrie.h"


/***************************
 *    Global parameters.   *
 ***************************/

/* Logging functions used by all pa files. */
#ifndef PA_WARNING
#define PA_WARNING(format, ...)
#endif
#ifndef PA_LOG
#define PA_LOG(format, ...)
#endif

/* Length, in byte, of a Node ID. */
#define PA_NODE_ID_LEN 8

/* Advertised Prefix Priority type. */
typedef uint8_t pa_priority;

/* Internal rule priority type. */
typedef uint16_t pa_rule_priority;

/* Delay, in milliseconds, between the events triggering
 * the prefix assignment routine and the actual time it is run.
 * The routine is never run synchronously, even when the delay is set to 0. */
#define PA_RUN_DELAY 20

/* Default flooding delay */
#define PA_DEFAULT_FLOODING_DELAY 10000



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
};

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
	const char *name;       /* Name, displayed in logs. */
};

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
	uint8_t adopted   : 1;          /* The AP will be adopted. */
	struct in6_addr prefix;         /* (if assigned) The AP prefix. */
	uint8_t plen;                   /* (if assigned) The AP prefix length. */
	pa_priority priority;           /* (if published) The Advertised Prefix Priority. */
	pa_rule_priority rule_priority; /* (if published) The internal rule priority. */
	struct pa_rule *rule;           /* (if published) The rule used to publish this prefix. */
	struct uloop_timeout routine_to;/* Timer used to schedule the routine. */
	struct uloop_timeout backoff_to;/* Timer used to backoff prefix generation, adoption or apply. */
	struct pa_pp *best_assignment;  /* (when in routine) The best current assignment. */
};

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

/* Argument passed to a rule. */
struct pa_rule_arg {
	/* The action to be done. */
	uint8_t action; //DESTROY, KEEP, CHANGE
	uint32_t backoff_timer;
	uint32_t apply_timer;
};

/* */
struct pa_rule_prefix_count {

};

/* This structure is a raw structure for prefix selection.
 * Specific rules and API are defined in pa_rules.h. */
struct pa_rule {
	struct list_head le; /* Linked in pa_core, the Link, or the DP. */

	/* Called whenever a rule is evaluated for a given Link/DP pair.
	 * Must return 0 when matching, a non-null value otherwise.
	 * When matching, the pa_rule_arg argument must be filled. */
	int (*apply_rule)(struct pa_rule *, struct pa_ap *, struct pa_rule_arg *);

	pa_rule_priority max_rule_priority;
	pa_rule_priority max_rule_override_priority;
	pa_priority max_priority;
	pa_priority max_override_priority;
};

void pa_rule_add(struct pa_core *, struct pa_rule *);
void pa_rule_del(struct pa_core *, struct pa_rule *);

/* Allows a rule to access the prefix count array.
 * This must not be computed by rules individually in order to avoid
 * redundant computations. */
const struct pa_rule_prefix_count *pa_rule_prefix_count(struct pa_core *, struct pa_rule_arg *);

#endif /* PA_CORE_H_ */
