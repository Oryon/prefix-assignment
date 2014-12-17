/*
 * Author: Pierre Pfister <pierre pfister@darou.fr>
 *
 * Copyright (c) 2014 Cisco Systems, Inc.
 *
 * This document provides some pre-defined rules
 * and filters to be used with pa_core.
 *
 */


#ifndef PA_RULES_H_
#define PA_RULES_H_

#include "pa_core.h"


/***************
 *   Filters   *
 ***************/

/*
 * Filters are used to pa_core in order to allow a single rule
 * to match or not match given different contexts. It allows
 * filtering and action separation, thus reducing the amount
 * of necessary code.
 */

struct pa_filter;
typedef int (*pa_filter_f)(struct pa_rule *, struct pa_ldp *, struct pa_filter *filter);

/*
 * Single filter structure used by all filters defined in this file.
 */
struct pa_filter {
	pa_filter_f accept;
	struct list_head le;
};

/* Configure a rule to use the specified filter. */
#define pa_rule_set_filter(rule, filter) do { \
		(rule)->filter_accept = (filter)->accept; \
		(rule)->filter_privare = filter; \
	} while(0)

/* Remove the filter from a given rule. */
#define pa_rule_unset_filter(rule) (rule)->filter_accept = NULL



/*
 * Multiple filters can be combined together in order
 * to form more complex combination.
 * AND, OR, NAND and NOR are supported.
 */
struct pa_filters;
struct pa_filters {
	struct pa_filter filter;
	struct list_head filters;
	uint8_t negate; //When set, the result is inverted
};

int pa_filters_or(struct pa_rule *rule, struct pa_ldp *ldp, struct pa_filter *filter);
int pa_filters_and(struct pa_rule *rule, struct pa_ldp *ldp, struct pa_filter *filter);

#define pa_filters_init(fs, accept_f, negate) do{ \
	(fs)->filter.accept = accept_f; \
	(fs)->negate = negate;\
	INIT_LIST_HEAD(&(fs)->filters); \
} while(0)

#define pa_filters_add(fs, f) list_add((f)->le ,&(fs)->filters)
#define pa_filters_del(f) list_del((f)->le)


/*
 * Simple filter used to filter for a given link, dp, or both.
 */
struct pa_filter_basic {
	struct pa_filter filter;
	struct pa_link *link;
	struct pa_dp *dp;
};

int pa_filter_basic(struct pa_rule *, struct pa_ldp *, struct pa_filter *);

#define pa_filter_basic_init(fb, link, dp) \
	((fb)->filter.accept = pa_filter_basic, (fb)->link = link, (fb)->dp = dp)

/*
 * Filter which only matches for a given dp or link type.
 */
struct pa_filter_type {
	struct pa_filter filter;
	uint8_t type;
};

#ifdef PA_DP_TYPE
int pa_filter_type_dp(struct pa_rule *rule, struct pa_ldp *ldp, struct pa_filter *filter);
#define pa_filter_type_dp_init(ft, type) \
	((ft)->filter.accept = pa_filter_type_dp, (fb)->type = type)

#endif

#ifdef PA_LINK_TYPE
int pa_filter_type_link(struct pa_rule *rule, struct pa_ldp *ldp, struct pa_filter *filter);
#define pa_filter_type_link_init(ft, type) \
	((ft)->filter.accept = pa_filter_type_dp, (fb)->type = type)
#endif



/***************
 *    Rules    *
 ***************/

#define pa_rule_init(rule, get_prio, max_prio, match_f) do{ \
	(rule)->get_max_priority = get_prio; \
	(rule)->max_priority = max_prio; \
	(rule)->match = match_f; } while(0)

/* When a prefix is assigned and valid, but advertised by no-one,
 * it may be adopted after some random delay.
 * The adopt rule will always adopt a prefix when possible, using the
 * specified rule_priority and advertising the adopted prefix
 * with the specified priority. */
struct pa_rule_adopt {
	struct pa_rule rule;
	pa_rule_priority rule_priority;
	pa_priority priority;
};

pa_rule_priority pa_rule_adopt_get_max_priority(struct pa_rule *rule, struct pa_ldp *ldp);
enum pa_rule_target pa_rule_adopt_match(struct pa_rule *rule, struct pa_ldp *ldp,
			pa_rule_priority, struct pa_rule_arg *);

#define pa_rule_adopt_init(rule_adopt) pa_rule_init(&(rule_adopt)->rule, \
						pa_rule_adopt_get_max_priority, 0, pa_rule_adopt_match)

/* When no prefix is assigned on a given Link,
 * a new prefix may be picked randomly.
 * This rule implements the prefix selection algorithm
 * detailed in the prefix assignment specifications.
 */
struct pa_rule_random {
	struct pa_rule rule;

	pa_rule_priority rule_priority; /* The rule priority */
	pa_priority priority;           /* Advertised Prefix Priority */
	pa_plen desired_plen;           /* The desired prefix length */

	/* Pseudo-random and random prefixes are picked
	 * in a given set or candidates. */
	uint16_t random_set_size;

	/* The algorithm first makes pseudo_random_tentatives
	 * pseudo-random tentatives. */
	uint16_t pseudo_random_tentatives;
	uint8_t *pseudo_random_seed;
	uint16_t pseudo_random_seedlen;
};

pa_rule_priority pa_rule_random_get_max_priority(struct pa_rule *rule, struct pa_ldp *ldp);
enum pa_rule_target pa_rule_random_match(struct pa_rule *rule, struct pa_ldp *ldp,
			pa_rule_priority, struct pa_rule_arg *);

#define pa_rule_random_init(rule_random) pa_rule_init(&(rule_random)->rule,  \
			pa_rule_random_get_max_priority, 0, pa_rule_random_match)

#endif /* PA_RULES_H_ */
