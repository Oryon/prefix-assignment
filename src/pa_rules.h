/*
 * Author: Pierre Pfister <pierre pfister@darou.fr>
 *
 * Copyright (c) 2014 Cisco Systems, Inc.
 *
 * This file provides some pre-defined rules
 * to be used with pa_core.
 *
 */


#ifndef PA_RULES_H_
#define PA_RULES_H_

#include "pa_core.h"

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


/* This rule is used to reflect the desire to assign a given prefix.
 * It may override existing assignment depending on overriding priorities.
 */
struct pa_rule_static {
	struct pa_rule rule; /* The PA rule */
	pa_prefix prefix;    /* The prefix assigned */
	pa_plen plen;        /* The prefix length */

	/* Priority and rule priority used by this rule. */
	pa_priority priority;
	pa_rule_priority rule_priority;

	/* The prefix may override any Advertised Prefix
	 * advertised by another node with an Advertised
	 * Prefix Priority strictly lower than the
	 * override_priority.
	 * And any Assigned Prefix which is Published
	 * by the local node with a rule_priority
	 * strictly lower than the override_rule_priority. */
	pa_priority override_priority;
	pa_rule_priority override_rule_priority;

	/* When enabled, do not override a Published Prefix
	 * unless the Advertised Prefix Priority is lower or equal
	 * to override_priority.
	 * When disabled, assignment loop may happen with other nodes. */
	uint8_t safety;
};

pa_rule_priority pa_rule_static_get_max_priority(struct pa_rule *rule, struct pa_ldp *ldp);
enum pa_rule_target pa_rule_static_match(struct pa_rule *rule, struct pa_ldp *ldp,
			pa_rule_priority, struct pa_rule_arg *);

#define pa_rule_static_init(rule_static) pa_rule_init(&(rule_static)->rule,  \
		pa_rule_static_get_max_priority, 0, pa_rule_static_match)

#endif /* PA_RULES_H_ */
