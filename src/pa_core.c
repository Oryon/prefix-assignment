/*
 * Author: Pierre Pfister <pierre pfister@darou.fr>
 *
 * Copyright (c) 2014 Cisco Systems, Inc.
 */


#include "pa_core.h"

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include "prefix.h"

#ifndef container_of
#define container_of(ptr, type, member) (           \
    (type *)( (char *)ptr - offsetof(type,member) ))
#endif

/* Returns whether the Advertised Prefix takes precedence over the Assigned Prefix. */
#define pa_precedes(pp, ap) \
	((!ap->published) || ((pp)->priority > (ap)->priority) || \
	(((pp)->priority == (ap)->priority) && memcmp((pp)->node_id, (ap)->core->node_id, PA_NODE_ID_LEN)))

#define pa_for_each_ap_in_dp_safe(pa_dp, pa_ap, pa_ap2) list_for_each_entry_safe(pa_ap, pa_ap2, &(pa_dp)->aps, in_dp)
#define pa_for_each_ap_in_link_safe(pa_link, pa_ap, pa_ap2) list_for_each_entry_safe(pa_ap, pa_ap2, &(pa_link)->aps, in_link)
#define pa_for_each_user(pa_core, pa_user) list_for_each_entry(pa_user, &(pa_core)->users, le)

#define pa_user_notify(pa_ap, function) \
	do { \
		struct pa_user *_pa_user_notify_user; \
		pa_for_each_user((pa_ap)->core, _pa_user_notify_user) { \
			if(_pa_user_notify_user->function) \
				_pa_user_notify_user->function(_pa_user_notify_user, ap);\
		} \
	} while(0)

#define pa_ap_set_published(pa_ap, p) do {\
	if((pa_ap)->published != p) { \
		PA_DEBUG("%s "PA_AP_P, (p)?"Publishing":"Un-Publishing ",PA_AP_PA(ap)); \
		(pa_ap)->published = p; \
		pa_user_notify(pa_ap, published); \
	} }while(0)

#define pa_ap_set_applied(pa_ap, p) do {\
	if((pa_ap)->applied != p) { \
		PA_DEBUG("%s "PA_AP_P, (p)?"Applying":"Un-Applying ",PA_AP_PA(ap)); \
		(pa_ap)->applied = p; \
		pa_user_notify(pa_ap, applied); \
	} }while(0)

#define pa_routine_schedule(ap) do { \
	if(!(ap)->routine_to.pending) \
		uloop_timeout_set(&(ap)->routine_to, PA_RUN_DELAY); }while(0)

static void pa_ap_unassign(struct pa_ap *ap)
{
	struct pa_ap *ap2;
	if(!ap->assigned)
		return;

	PA_INFO("Unassign prefix: "PA_AP_P, PA_AP_PA(ap));
	btrie_remove(&ap->in_core.be);
	ap->assigned = 0;
	pa_user_notify(ap, applied); /* Tell users about that */

	/* Destroying the Assigned Prefix possibly freed space that other interfaces may use.
	 * Schedule links for the same dp, if there is no current prefix.
	 * This can be ignored when no prefix is ever created by the local node. */
	pa_for_each_ap_in_dp(ap->dp, ap2) {
		if(!ap2->assigned)
			pa_routine_schedule(ap2);
	}
}

static int pa_ap_assign(struct pa_ap *ap, const struct in6_addr *prefix, uint8_t plen)
{
	if(ap->assigned) {
		PA_WARNING("Could not assign %s to "PA_AP_P, PREFIX_REPR(prefix, plen), PA_AP_PA(ap));
		return -2;
	}

	memcpy(&ap->prefix, prefix, sizeof(struct in6_addr));
	ap->plen = plen;
	if(btrie_add(&ap->core->prefixes, &ap->in_core.be, (const btrie_key_t *)prefix, plen)) {
		PA_WARNING("Could not assign %s to "PA_LINK_P, PREFIX_REPR(prefix, plen), PA_LINK_PA(ap->link));
		return -1;
	}

	ap->assigned = 1;
	PA_INFO("Assigned prefix: "PA_AP_P, PA_AP_PA(ap));
	pa_user_notify(ap, assigned); /* Tell users about that*/
	return 0;
}

/*
 * Prefix Assignment Routine.
 */
static void pa_routine(struct pa_ap *ap, bool backoff)
{
	PA_DEBUG("Executing PA Routine %sfor "PA_AP_P, backoff?"with backoff ":"", PA_AP_PA(ap));
}

static void pa_backoff_to(struct uloop_timeout *to)
{
	struct pa_ap *ap = container_of(to, struct pa_ap, backoff_to);
	if(ap->adopting) { //Adopt timeout
		pa_ap_set_published(ap, 1);
		if(!ap->applied)
			uloop_timeout_set(&ap->backoff_to, 2*ap->core->flooding_delay);
	} else if(ap->assigned) { //Apply timeout
		pa_ap_set_applied(ap, 1);
	} else { //Backoff delay
		pa_routine(ap, true);
	}
}

static void pa_routine_to(struct uloop_timeout *to)
{
	struct pa_ap *ap = container_of(to, struct pa_ap, routine_to);
	pa_routine(ap, false);
}

/*
 * Create a new empty link/dp pairing.
 */
static int pa_ap_create(struct pa_core *core, struct pa_link *link, struct pa_dp *dp)
{
	struct pa_ap *ap;
	if(!(ap = calloc(1, sizeof(*ap)))) {
		PA_WARNING("FAILED to create state for "PA_LINK_P"/"PA_DP_P, PA_LINK_PA(link), PA_DP_PA(dp));
		return -1;
	}

	ap->backoff_to.cb = pa_backoff_to;
	ap->routine_to.cb = pa_routine_to;
	ap->in_core.type = PAT_AP;
	ap->core = core;
	ap->link = link;
	list_add(&ap->in_link, &link->aps);
	ap->dp = dp;
	list_add(&ap->in_dp, &dp->aps);
	PA_DEBUG("Creating Link/Delegated Prefix pair: "PA_AP_P, PA_AP_PA(ap));
	return 0;
}

/*
 * Destroy an ap. All states must be to 0.
 */
static void pa_ap_destroy(struct pa_ap *ap)
{
	PA_DEBUG("Destroying Link/Delegated Prefix pair: "PA_AP_P, PA_AP_PA(ap));
	list_del(&ap->in_link);
	list_del(&ap->in_dp);
	uloop_timeout_cancel(&ap->backoff_to);
	uloop_timeout_cancel(&ap->routine_to);
	free(ap);
}

static void _pa_dp_del(struct pa_dp *dp)
{
	struct pa_ap *ap, *ap2;
	//Public part
	pa_for_each_ap_in_dp(dp, ap) {
		pa_ap_set_published(ap, 0);
		pa_ap_set_applied(ap, 0);
		pa_ap_unassign(ap);
	}

	//Private part (so the whole deletion is atomic for users)
	pa_for_each_ap_in_dp_safe(dp, ap, ap2)
	pa_ap_destroy(ap);
	list_del(&dp->le);
}

void pa_dp_del(struct pa_dp *dp)
{
	PA_INFO("Removing Delegated Prefix "PA_DP_P, PA_DP_PA(dp));
	_pa_dp_del(dp);
}

int pa_dp_add(struct pa_core *core, struct pa_dp *dp)
{
	PA_INFO("Adding Delegated Prefix "PA_DP_P, PA_DP_PA(dp));
	INIT_LIST_HEAD(&dp->aps);
	list_add(&dp->le, &core->dps);
	struct pa_link *link;
	pa_for_each_link(core, link) {
		if(pa_ap_create(core, link, dp)) {
			PA_WARNING("FAILED to add Delegated Prefix "PA_DP_P, PA_DP_PA(dp));
			_pa_dp_del(dp);
			return -1;
		}
	}
	return 0;
}

static void _pa_link_del(struct pa_link *link)
{
	struct pa_ap *ap, *ap2;
	//Public part
	pa_for_each_ap_in_link(link, ap) {
		pa_ap_set_published(ap, 0);
		pa_ap_set_applied(ap, 0);
		pa_ap_unassign(ap);
	}

	//Private part (so the whole deletion is atomic for users)
	pa_for_each_ap_in_link_safe(link, ap, ap2)
	pa_ap_destroy(ap);

	list_del(&link->le);
}

void pa_link_del(struct pa_link *link)
{
	PA_INFO("Removing Link "PA_LINK_P, PA_LINK_PA(link));
	_pa_link_del(link);
}

int pa_link_add(struct pa_core *core, struct pa_link *link)
{
	PA_INFO("Adding Link "PA_LINK_P, PA_LINK_PA(link));
	INIT_LIST_HEAD(&link->aps);
	list_add(&link->le, &core->links);
	struct pa_dp *dp;
	pa_for_each_dp(core, dp) {
		if(pa_ap_create(core, link, dp)) {
			PA_WARNING("FAILED to add Link "PA_LINK_P, PA_LINK_PA(link));
			_pa_link_del(link);
			return -1;
		}
	}
	return 0;
}

static void _pa_pp_update(struct pa_core *core, struct pa_pp *pp)
{
	struct pa_dp *dp;
	struct pa_ap *ap;
	pa_for_each_dp(core, dp) {
		/* Schedule all for dps overlapping with the pp. */
		//TODO: Maybe not necessary to schedule if we have Current and pp is not overlapping with it.
		if(prefix_overlap(&dp->prefix, dp->plen, &pp->prefix, pp->plen)) {
			pa_for_each_ap_in_dp(dp, ap)
					pa_routine_schedule(ap);
		}
	}
}

/* Tell the content of the Advertised Prefix was changes. */
void pa_pp_update(struct pa_core *core, struct pa_pp *pp)
{
	PA_DEBUG("Updating Advertised Prefix "PA_PP_P, PA_PP_PA(pp));
	_pa_pp_update(core, pp);
}

/* Adds a new Advertised Prefix. */
int pa_pp_add(struct pa_core *core, struct pa_pp *pp)
{
	PA_DEBUG("Adding Advertised Prefix "PA_PP_P, PA_PP_PA(pp));
	pp->in_core.type = PAT_PP;
	if(btrie_add(&core->prefixes, &pp->in_core.be, (btrie_key_t *)&pp->prefix, pp->plen)) {
		PA_WARNING("Could not add Advertised Prefix "PA_PP_P, PA_PP_PA(pp));
		return -1;
	}

	_pa_pp_update(core, pp);
	return 0;
}

/* Removes an added Advertised Prefix. */
void pa_pp_del(struct pa_core *core, struct pa_pp *pp)
{
	PA_DEBUG("Deleting Advertised Prefix "PA_PP_P, PA_PP_PA(pp));
	btrie_remove(&pp->in_core.be);
	_pa_pp_update(core, pp);
}

void pa_rule_add(struct pa_core *core, struct pa_rule *rule)
{
	PA_DEBUG("Adding rule "PA_RULE_P, PA_RULE_PA(rule));
	list_add(&rule->le, &core->rules);
	/* Schedule all routines */
	struct pa_link *link;
	struct pa_ap *ap;
	pa_for_each_link(core, link)
		pa_for_each_ap_in_link(link, ap)
			pa_routine_schedule(ap);
}

void pa_rule_del(struct pa_core *core, struct pa_rule *rule)
{
	PA_DEBUG("Deleting rule "PA_RULE_P, PA_RULE_PA(rule));
	list_del(&rule->le);
	struct pa_link *link;
	struct pa_ap *ap;
	pa_for_each_link(core, link)
		pa_for_each_ap_in_link(link, ap) {
			pa_routine_schedule(ap);
			if(ap->rule == rule)
				ap->rule = NULL; //Make it orphan
		}
}

void pa_core_set_flooding_delay(struct pa_core *core, uint32_t flooding_delay)
{
	PA_INFO("Set Flooding Delay to %"PRIu32, flooding_delay);
	struct pa_link *link;
	struct pa_ap *ap;
	if(flooding_delay > core->flooding_delay) {
		pa_for_each_link(core, link)
			pa_for_each_ap_in_link(link, ap)
				if(ap->published && ap->backoff_to.pending)
					uloop_timeout_set(&ap->backoff_to, uloop_timeout_remaining(&ap->backoff_to) + 2*(flooding_delay - core->flooding_delay));
	} else if (flooding_delay < core->flooding_delay) {
		pa_for_each_link(core, link)
			pa_for_each_ap_in_link(link, ap)
				if(ap->published && ap->backoff_to.pending && ((uint32_t)uloop_timeout_remaining(&ap->backoff_to) > 2*flooding_delay))
					uloop_timeout_set(&ap->backoff_to, 2*flooding_delay);
	}
	core->flooding_delay = flooding_delay;
}

void pa_core_set_node_id(struct pa_core *core, const uint8_t node_id[])
{
	PA_INFO("Set Node ID to "PA_NODE_ID_P, PA_NODE_ID_PA(node_id));
	struct pa_link *link;
	struct pa_ap *ap;
	if(memcmp(node_id, core->node_id, PA_NODE_ID_LEN)) {
		memcpy(core->node_id, node_id, PA_NODE_ID_LEN);
		/* Schedule routine for all pairs */
		pa_for_each_link(core, link)
			pa_for_each_ap_in_link(link, ap)
				pa_routine_schedule(ap);
	}
}

void pa_core_init(struct pa_core *core)
{
	PA_INFO("Initialize Prefix Assignment Algorithm Core");
	INIT_LIST_HEAD(&core->dps);
	INIT_LIST_HEAD(&core->links);
	INIT_LIST_HEAD(&core->users);
	INIT_LIST_HEAD(&core->rules);
	btrie_init(&core->prefixes);
	memset(core->node_id, 0, PA_NODE_ID_LEN);
	core->flooding_delay = PA_DEFAULT_FLOODING_DELAY;
}
