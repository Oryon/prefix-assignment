/*
 * Author: Pierre Pfister <pierre pfister@darou.fr>
 *
 * Copyright (c) 2014 Cisco Systems, Inc.
 */

#include "pa_rules.h"

int pa_filters_or(struct pa_rule *rule, struct pa_ldp *ldp, struct pa_filter *filter)
{
	struct pa_filters *fs = container_of(filter, struct pa_filters, filter);
	list_for_each_entry(filter, &fs->filters, le) {
		if(filter->accept(rule, ldp, filter))
			return 1;
	}
	return 0;
}

int pa_filters_and(struct pa_rule *rule, struct pa_ldp *ldp, struct pa_filter *filter)
{
	struct pa_filters *fs = container_of(filter, struct pa_filters, filter);
	list_for_each_entry(filter, &fs->filters, le) {
		if(!filter->accept(rule, ldp, filter))
			return 0;
	}
	return 1;
}

void pa_filters_init(struct pa_filters *fs, pa_filter_f accept)
{
	fs->filter.accept = accept;
	INIT_LIST_HEAD(&fs->filters);
}

static int pa_filter_basic_accept(__attribute__ ((unused)) struct pa_rule *rule, struct pa_ldp *ldp, struct pa_filter *filter)
{
	struct pa_filter_basic *fb = container_of(filter, struct pa_filter_basic, filter);
	if(fb->link && fb->link != ldp->link)
		return 0;
	if(fb->dp && fb->dp != ldp->dp)
		return 0;
	return 1;
}

void pa_filter_basic_init(struct pa_filter_basic *filter, struct pa_link *link, struct pa_dp *dp)
{
	filter->filter.accept = pa_filter_basic_accept;
	filter->link = link;
	filter->dp = dp;
}
