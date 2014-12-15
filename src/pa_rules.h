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

struct pa_filter;
typedef int (*pa_filter_f)(struct pa_rule *, struct pa_ldp *, struct pa_filter *filter);

/*
 * Single filter structure used by all filters.
 */
struct pa_filter {
	pa_filter_f accept;
	struct list_head le;
};

/* Use the filter as rule base filter */
#define pa_rule_set_filter(rule, filter) do { \
		(rule)->filter_accept = (filter)->accept; \
		(rule)->filter_privare = filter; \
	} while(0)

/* Remove the filter as core filter */
#define pa_rule_unset_filter(rule) (rule)->filter_accept = NULL

/*
 * Multiple filters combined together forming a more complex logic.
 */
struct pa_filters;
struct pa_filters {
	struct pa_filter filter;
	struct list_head filters;
};

void pa_filters_init(struct pa_filters *, pa_filter_f accept);
#define pa_filters_add(fs, f) list_add((f)->filter ,&(fs)->filters)
#define pa_filters_del(f) list_del((f)->filter)

/* Use this function for filter disjunction
 * At least one filter must return true
 * for the filter to return true. */
int pa_filters_or(struct pa_rule *rule, struct pa_ldp *ldp, struct pa_filter *filter);

/* Use this function for filter conjunction.
 * All sub-filters must return true
 * for the filter to return true. */
int pa_filters_and(struct pa_rule *rule, struct pa_ldp *ldp, struct pa_filter *filter);


/*
 * Simple filter used to filter for a given link, dp, or both.
 */
struct pa_filter_basic {
	struct pa_filter filter;
	struct pa_link *link;
	struct pa_dp *dp;
};

void pa_filter_basic_init(struct pa_filter_basic *filter, struct pa_link *link, struct pa_dp *dp);

#endif /* PA_RULES_H_ */
