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
 * Configuration defaults  *
 ***************************/

#include "pa_conf.h"

#ifndef PA_WARNING
#define PA_WARNING(format, ...) do{}while(0)
#endif
#ifndef PA_INFO
#define PA_INFO(format, ...) do{}while(0)
#endif
#ifndef PA_DEBUG
#define PA_DEBUG(format, ...) do{}while(0)
#endif

#ifndef PA_NODE_ID_PA
#ifdef PA_NODE_ID_P
#undef PA_NODE_ID_P
#endif
#endif

#ifndef PA_NODE_ID_P
static const char *pa_hex_dump(uint8_t *ptr, size_t len, char *s) {
	char n;
	s[2*len] = '\0';
	for(;len;len--) {
		n = (ptr[len] & 0xf0) >> 4;
		s[2*len - 2] = (n > 9)?('a'+(n-10)):('0'+n);
		n = (ptr[len] & 0x0f);
		s[2*len - 1] = (n > 9)?('a'+(n-10)):('0'+n);
	}
	return s;
}

#define PA_NODE_ID_P   "[%s]"
#define PA_NODE_ID_PA(node_id) pa_hex_dump((uint8_t *)node_id, sizeof(PA_NODE_ID_TYPE)*PA_NODE_ID_LEN, alloca(sizeof(PA_NODE_ID_TYPE)*PA_NODE_ID_LEN*2+1))
#endif

#ifndef PA_DEFAULT_FLOODING_DELAY
#define PA_DEFAULT_FLOODING_DELAY 10000
#endif

#ifndef PA_RUN_DELAY
#define PA_RUN_DELAY 20
#endif

/***************************
 *       Generic API       *
 ***************************/

/* Structure containing state specific to the overall algorithm. */
struct pa_core {
	struct btrie prefixes;           /* btrie containing all Assigned and Advertised Prefixes */
	PA_NODE_ID_TYPE node_id[PA_NODE_ID_LEN]; /* The Node ID of the local node. Initial value is 0. */
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
void pa_core_set_node_id(struct pa_core *core, const PA_NODE_ID_TYPE[PA_NODE_ID_LEN]);

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
	struct list_head ldps;/* List of Link/DP pairs associated with this Link. */
	const char *name;     /* Name, displayed in logs. */
#ifdef PA_LINK_TYPE
	uint8_t type;           /* Link type identifier provided by user. */
#endif
};

/* Link print format and arguments. */
#define PA_LINK_P "%s"
#define PA_LINK_PA(pa_link) (pa_link)?(pa_link)->name?(pa_link)->name:"no-name":"no-link"

/* Adds and deletes a Link for prefix assignment */
int pa_link_add(struct pa_core *, struct pa_link *);
void pa_link_del(struct pa_link *);

/*
 * Structure used to identify a Delegated Prefix.
 */
struct pa_dp {
	struct list_head le;    /* Linked in pa_core. */
	struct list_head ldps;  /* List of Link/DP pairs associated with this Delegated Prefix. */
	struct in6_addr prefix; /* The delegated prefix value. */
	uint8_t plen;           /* The prefix length. */
#ifdef PA_DP_TYPE
	uint8_t type;           /* Delegated Prefix type identifier provided by user. */
#endif
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
#define PAT_ASSIGNED   0x01
#define PAT_ADVERTISED 0x02
};

/*
 * Structure used to identify a Link/Delegated Prefix pair.
 * It may or may not contain an Assigned Prefix.
 */
struct pa_ldp {
	struct pa_pentry in_core;       /* Used to link Assigned and Advertised Prefixes in the same btrie */
	struct list_head in_link;       /* Linked in the Link structure. */
	struct list_head in_dp;         /* Linked in the Delegated Prefix structure. */
	struct pa_link *link;           /* The Link. */
	struct pa_dp *dp;               /* The Delegated Prefix. */
	struct pa_core *core;           /* Back-pointer to the associated pa_core struct */
	uint8_t assigned  : 1;          /* There is an associated Assigned Prefix. */
	uint8_t published : 1;          /* The Assigned Prefix is published. */
	uint8_t applied   : 1;          /* The Assigned Prefix is applied. */
	uint8_t adopting  : 1;          /* The Assigned Prefix will be adopted (Only set during backoff). */
	uint8_t valid     : 1;          /* (in routine) Whether the routine will destroy the Assigned Prefix. */
	uint8_t backoff   : 1;          /* (in routine) The routine is executed following backoff timeout. */
	struct in6_addr prefix;         /* (if assigned) The Assigned Prefix. */
	uint8_t plen;                   /* (if assigned) The Assigned Prefix length. */
	pa_priority priority;           /* (if published) The Advertised Prefix Priority. */
	pa_rule_priority rule_priority; /* (if published) The internal rule priority. */
	struct pa_rule *rule;           /* (if published) The rule used to publish this prefix. */
	struct uloop_timeout routine_to;/* Timer used to schedule the routine. */
	struct uloop_timeout backoff_to;/* Timer used to backoff prefix generation, adoption or apply. */
	struct pa_advp *best_assignment;/* (in routine) Best on-link assognment, ours included. */
#if PA_LDP_USERS != 0
	void *userdata[PA_LDP_USERS];   /* Generic pointers, initialized to NULL, for use by users. */
#endif
};

/* Assigned Prefix print format and arguments */
#define PA_LDP_P "%s%%"PA_LINK_P" from "PA_DP_P" flags (%s %s %s)"
#define PA_LDP_PA(pa_ldp) ((pa_ldp)->assigned)?PREFIX_REPR(&(pa_ldp)->prefix, (pa_ldp)->plen):"no-prefix", \
	PA_LINK_PA((pa_ldp)->link), PA_DP_PA((pa_ldp)->dp), \
	((pa_ldp)->published)?"Published":"-", ((pa_ldp)->applied)?"Applied":"-", ((pa_ldp)->adopting)?"Adopting":"-"

/*
 * Structure used to identify an Advertised Prefix.
 */
struct pa_advp {
	struct pa_pentry in_core;     /* Used to link Assigned and Advertised Prefixes in the same btrie */
	PA_NODE_ID_TYPE node_id[PA_NODE_ID_LEN]; /* The node ID of the node advertising the prefix. */
	struct in6_addr prefix;       /* The Advertised Prefix). */
	uint8_t plen;                 /* The Advertised Prefix length. */
	pa_priority priority;         /* The Advertised Prefix Priority. */
	struct pa_link *link;         /* Advertised Prefix associated Shared Link (or null). */
};

/* Advertised Prefix print format and arguments */
#define PA_ADVP_P "%s%%"PA_LINK_P"@"PA_NODE_ID_P":(%d)"
#define PA_ADVP_PA(pa_advp) PREFIX_REPR(&(pa_advp)->prefix, (pa_advp)->plen), \
	PA_LINK_PA((pa_advp)->link), PA_NODE_ID_PA((pa_advp)->node_id), (pa_advp)->priority

/* Adds a new Advertised Prefix. */
int pa_advp_add(struct pa_core *, struct pa_advp *);

/* Removes an Advertised Prefix which was previously added. */
void pa_advp_del(struct pa_core *, struct pa_advp *);

/* Notify that the content of the Advertised Prefix was changes. */
void pa_advp_update(struct pa_core *, struct pa_advp *);


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
	void (*assigned)(struct pa_user *, struct pa_ldp *);
	void (*published)(struct pa_user *, struct pa_ldp *);
	void (*applied)(struct pa_user *, struct pa_ldp *);
};

/* pa_user print format and argument */
#define PA_USER_P "%p - %d:%d:%d"
#define PA_USER_PA(pa_user) pa_user, (pa_user)->assigned, (pa_user)->published, (pa_user)->applied

/* Adds a user which will receive events callback.
 * When added, the user does not receive callbacks for existing prefixes. */
#define pa_user_register(core, user) list_add(&(user)->le, &(core)->users)

/* Unregister a user. */
#define pa_user_unregister(user) list_del(&(user)->le)

#define pa_for_each_link(pa_core, pa_link) list_for_each_entry(pa_link, &(pa_core)->links, le)

#define pa_for_each_ldp_in_link(pa_link, pa_ldp) list_for_each_entry(pa_ldp, &(pa_link)->ldps, in_link)

#define pa_for_each_dp(pa_core, pa_dp) list_for_each_entry(pa_dp, &(pa_core)->dps, le)

#define pa_for_each_ldp_in_dp(pa_dp, pa_ldp) list_for_each_entry(pa_ldp, &(pa_dp)->ldps, in_dp)


/***************************
 *   Configuration API     *
 ***************************/

/* This API is an advanced rule-based configuration API.
 *
 * ! Warning !
 * Rules are supposed to behave in conformance with the prefix assignment algorithm specifications.
 * One should understand the algorithm behavior before trying to implement a rule.
 * pa_core does not check for rules well-behavior. An incorrect rule may result in faults.
 * Specific, more user-friendly rules are defined in pa_rules.h. */

/* The rule target indicates the desired behavior of a rule on a given ldp. */
enum pa_rule_target {
	/* The rule does not match.
	 * Always valid. */
	PA_RULE_NO_MATCH = 0,

	 /* The rule desires to adopt the orphan prefix.
	  * Valid when: (assigned && !published && !best_assignment)*/
	PA_RULE_ADOPT,

	/* The rule desires to make an assignment later.
	 * Valid when: (!assigned) */
	PA_RULE_BACKOFF,

	/* The rule desires to assign and publish a prefix.
	 * Always valid (with a high enough rule_priority and priority) */
	PA_RULE_PUBLISH,

	/* The rule desires to unassign the prefix.
	 * Valid when: (published || adopting)*/
	PA_RULE_DESTROY,
};

/* The argument given to rule's match function in order to get
 * more information about the desired behavior. */
struct pa_rule_arg {
	/* The rule priority indicates with which priority the action
	 * must be applied. The decision will be kept and will not be overriden
	 * unless with an higher priority. */
	pa_rule_priority rule_priority;

	/* These must be filled by the match function when it returns PA_RULE_PUBLISH.
	 * It indicates which prefix to publish. */
	struct in6_addr prefix;
	uint8_t plen;

	/* The pa priority must be specified by the match function when it returns
	 * PA_RULE_PUBLISH or PA_RULE_ADOPT. It is the priority with which the prefix
	 * will be advertised.
	 */
	pa_priority priority;
};

/* This structure is a raw structure for prefix selection.
 *  */
struct pa_rule {
	struct list_head le; /* Linked in pa_core, the Link, or the DP. */

	const char *name; /* Rule name, displayed in logs. */

	/*** get_priority ***
	 * Returns the maximal rule priority the rule may use when
	 * 'match' is called with the same pa_ldp. It is
	 * used to determine the order rules 'match' functions
	 * will be called later. If not specified, the max_priority
	 * value is used instead.
	 *
	 * The special value 0 is returned when the rule cannot match.
	 * In such case, or when the returned max_priority is
	 * smaller than another matching rule, 'match' will not be called.
	 *
	 * In Arguments:
	 * pa_rule: The rule from which the function is called.
	 * pa_ldp:  The considered Link/DP pair.
	 *
	 */
	pa_rule_priority (*get_max_priority)(struct pa_rule *, struct pa_ldp *);

	/* If get_max_priority is NULL, this value is used instead. */
	pa_rule_priority max_priority;

	/*** match ***
	 * Returns the target specified by the rule.
	 *
	 * pa_rule: The rule from which the function is called.
	 * pa_ldp:  The considered Link/DP pair.
	 * best_match_priority: The priority of the preferred matching rule.
	 *      'match' shall only match when returning an higher rule priority.
	 * pa_arg:  Arguments to be filled by the function.
	 *
	 *
	 */
	 enum pa_rule_target (*match)(struct pa_rule *, struct pa_ldp *,
			pa_rule_priority best_match_priority,
			struct pa_rule_arg *pa_arg);

	 /* PRIVATE - Used by pa_core. */
	 pa_rule_priority _max_priority;
	 struct list_head _le;
};

/* pa_rule print format and argument */
#define PA_RULE_P "'%s'@%p"
#define PA_RULE_PA(rule) (rule)->name?(rule)->name:"no-name", rule

void pa_rule_add(struct pa_core *, struct pa_rule *);
void pa_rule_del(struct pa_core *, struct pa_rule *);

#endif /* PA_CORE_H_ */
