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
#define pa_precedes(advp, ldp) \
	((!ldp->published) || ((advp)->priority > (ldp)->priority) || \
	(((advp)->priority == (ldp)->priority) && memcmp((advp)->node_id, (ldp)->core->node_id, PA_NODE_ID_LEN)))

#define pa_for_each_ldp_in_dp_safe(pa_dp, pa_ldp, pa_ldp2) list_for_each_entry_safe(pa_ldp, pa_ldp2, &(pa_dp)->ldps, in_dp)
#define pa_for_each_ldp_in_link_safe(pa_link, pa_ldp, pa_ldp2) list_for_each_entry_safe(pa_ldp, pa_ldp2, &(pa_link)->ldps, in_link)
#define pa_for_each_user(pa_core, pa_user) list_for_each_entry(pa_user, &(pa_core)->users, le)

#define pa_user_notify(pa_ldp, function) \
	do { \
		struct pa_user *_pa_user_notify_user; \
		pa_for_each_user((pa_ldp)->core, _pa_user_notify_user) { \
			if(_pa_user_notify_user->function) \
				_pa_user_notify_user->function(_pa_user_notify_user, ldp);\
		} \
	} while(0)

#define pa_ldp_set_published(pa_ldp, p) do {\
	if((pa_ldp)->published != p) { \
		PA_DEBUG("%s "PA_LDP_P, (p)?"Publishing":"Un-Publishing ",PA_LDP_PA(ldp)); \
		(pa_ldp)->published = p; \
		pa_user_notify(pa_ldp, published); \
	} }while(0)

#define pa_ldp_set_applied(pa_ldp, p) do {\
	if((pa_ldp)->applied != p) { \
		PA_DEBUG("%s "PA_LDP_P, (p)?"Applying":"Un-Applying ",PA_LDP_PA(ldp)); \
		(pa_ldp)->applied = p; \
		pa_user_notify(pa_ldp, applied); \
	} }while(0)

#define pa_routine_schedule(ldp) do { \
	if(!(ldp)->routine_to.pending) \
		uloop_timeout_set(&(ldp)->routine_to, PA_RUN_DELAY); }while(0)

static void pa_ldp_unassign(struct pa_ldp *ldp)
{
	struct pa_ldp *ldp2;
	if(!ldp->assigned)
		return;

	PA_INFO("Unassign prefix: "PA_LDP_P, PA_LDP_PA(ldp));
	pa_ldp_set_applied(ldp, false);
	pa_ldp_set_published(ldp, false);
	uloop_timeout_cancel(&ldp->backoff_to);
	ldp->adopting = false;

	btrie_remove(&ldp->in_core.be);
	ldp->assigned = 0;
	pa_user_notify(ldp, applied); /* Tell users about that */

	/* Destroying the Assigned Prefix possibly freed space that other interfaces may use.
	 * Schedule links for the same dp, if there is no current prefix.
	 * This can be ignored when no prefix is ever created by the local node. */
	pa_for_each_ldp_in_dp(ldp->dp, ldp2) {
		if(!ldp2->assigned)
			pa_routine_schedule(ldp2);
	}
}

static int pa_ldp_assign(struct pa_ldp *ldp, const struct in6_addr *prefix, uint8_t plen)
{
	if(ldp->assigned) {
		PA_WARNING("Could not assign %s to "PA_LDP_P, PREFIX_REPR(prefix, plen), PA_LDP_PA(ldp));
		return -2;
	}

	memcpy(&ldp->prefix, prefix, sizeof(struct in6_addr));
	ldp->plen = plen;
	if(btrie_add(&ldp->core->prefixes, &ldp->in_core.be, (const btrie_key_t *)prefix, plen)) {
		PA_WARNING("Could not assign %s to "PA_LINK_P, PREFIX_REPR(prefix, plen), PA_LINK_PA(ldp->link));
		return -1;
	}

	ldp->assigned = 1;
	PA_INFO("Assigned prefix: "PA_LDP_P, PA_LDP_PA(ldp));
	pa_user_notify(ldp, assigned); /* Tell users about that*/
	return 0;
}

static bool pa_ldp_global_valid(struct pa_ldp *ldp)
{
	/* There can't be any ldp except the one which is checked.
	 * If there are overlapping DPs, this assumption may be wrong and
	 * this code would bug. */
	struct pa_advp *advp;
	btrie_for_each_updown_entry(advp, &ldp->core->prefixes, (btrie_key_t *)&ldp->prefix, ldp->plen, in_core.be) {
		if(&advp->in_core != &ldp->in_core && pa_precedes(advp, ldp))
			return false;
	}
	return true;
}

/*
 * Prefix Assignment Routine.
 */
static void pa_routine(struct pa_ldp *ldp, bool backoff)
{
	PA_DEBUG("Executing PA %sRoutine for"PA_LDP_P, backoff?"backoff ":"", PA_LDP_PA(ldp));

	/*
	 * The algorithm is slightly modified in order to provide support for
	 * custom behavior.
	 * 1. The Best Assignment is fetched and checked.
	 * 2. The validity of the Current Assignment is checked.
	 * 3. Rules may be applied to create/adopt/delete assignments.
	 * 4. The prefix is removed if still invalid, and the routine
	 * is executed assuming existing assignment validity (That is, we assume
	 * rules provide valid assignments).
	 */

	/* 1. Look for the Best Assignment */
	struct pa_advp *advp;
	struct pa_pentry *pentry;
	ldp->best_assignment = NULL;
	btrie_for_each_updown_entry(pentry, &ldp->core->prefixes, (btrie_key_t *)&ldp->dp->prefix, ldp->dp->plen, be) {
		if(pentry->type == PAT_ADVERTISED) {
			advp = container_of(pentry, struct pa_advp, in_core);
			if(advp->link == ldp->link &&
					(!ldp->best_assignment || advp->priority > ldp->best_assignment->priority ||
					((advp->priority == ldp->best_assignment->priority) && (PA_NODE_ID_CMP(advp->node_id, ldp->best_assignment->node_id) > 0))))
				ldp->best_assignment = advp;
		}
	}

	/* 2. Check assignment validity */
	if(!ldp->best_assignment || !pa_precedes(ldp->best_assignment, ldp))
		ldp->best_assignment = NULL; //We don't really care about invalid best assignments.

	if(ldp->assigned) { //Check whether the algorithm would keep that prefix or destroy it.
		if(ldp->best_assignment)
			ldp->valid = pa_ldp_global_valid(ldp); //Globally valid
		else
			ldp->valid = prefix_equals(&ldp->prefix, ldp->plen, //Different from Best Assignment
					&ldp->best_assignment->prefix, ldp->best_assignment->plen);
	}

	/* 3. Execute rules. */
	//TODO:

	/* 4. End the routine with no adoption or creation */
	if(!ldp->valid) {
		pa_ldp_unassign(ldp);
		uloop_timeout_cancel(&ldp->backoff_to);
	}

	if(ldp->assigned) {
		//Assigned and valid

		if(!ldp->published && !ldp->adopting) {
			//It would require an adoption, which we don't do here.
			pa_ldp_unassign(ldp);
			uloop_timeout_cancel(&ldp->backoff_to);
		}

		if(ldp->best_assignment) {
			//We give up the publishing to the other node.
			pa_ldp_set_published(ldp, 0);
		}

	} else if (ldp->best_assignment) {
		//Should accept the best_assignment
		pa_ldp_unassign(ldp);
		pa_ldp_assign(ldp, &ldp->best_assignment->prefix, ldp->best_assignment->plen);
		uloop_timeout_set(&ldp->backoff_to, 2 * ldp->core->flooding_delay);
	}
}

static void pa_backoff_to(struct uloop_timeout *to)
{
	struct pa_ldp *ldp = container_of(to, struct pa_ldp, backoff_to);
	if(ldp->adopting) { //Adopt timeout
		pa_ldp_set_published(ldp, 1);
		if(!ldp->applied)
			uloop_timeout_set(&ldp->backoff_to, 2*ldp->core->flooding_delay);
	} else if(ldp->assigned) { //Apply timeout
		pa_ldp_set_applied(ldp, 1);
	} else { //Backoff delay
		pa_routine(ldp, true);
	}
}

static void pa_routine_to(struct uloop_timeout *to)
{
	struct pa_ldp *ldp = container_of(to, struct pa_ldp, routine_to);
	pa_routine(ldp, false);
}

/*
 * Create a new empty link/dp pairing.
 */
static int pa_ldp_create(struct pa_core *core, struct pa_link *link, struct pa_dp *dp)
{
	struct pa_ldp *ldp;
	if(!(ldp = calloc(1, sizeof(*ldp)))) {
		PA_WARNING("FAILED to create state for "PA_LINK_P"/"PA_DP_P, PA_LINK_PA(link), PA_DP_PA(dp));
		return -1;
	}

	ldp->backoff_to.cb = pa_backoff_to;
	ldp->routine_to.cb = pa_routine_to;
	ldp->in_core.type = PAT_ASSIGNED;
	ldp->core = core;
	ldp->link = link;
	list_add(&ldp->in_link, &link->ldps);
	ldp->dp = dp;
	list_add(&ldp->in_dp, &dp->ldps);
	PA_DEBUG("Creating Link/Delegated Prefix pair: "PA_LDP_P, PA_LDP_PA(ldp));
	return 0;
}

/*
 * Destroy an ldp. All states must be to 0.
 */
static void pa_ldp_destroy(struct pa_ldp *ldp)
{
	PA_DEBUG("Destroying Link/Delegated Prefix pair: "PA_LDP_P, PA_LDP_PA(ldp));
	list_del(&ldp->in_link);
	list_del(&ldp->in_dp);
	uloop_timeout_cancel(&ldp->backoff_to);
	uloop_timeout_cancel(&ldp->routine_to);
	free(ldp);
}

static void _pa_dp_del(struct pa_dp *dp)
{
	struct pa_ldp *ldp, *ldp2;
	//Public part
	pa_for_each_ldp_in_dp(dp, ldp) {
		pa_ldp_set_published(ldp, 0);
		pa_ldp_set_applied(ldp, 0);
		pa_ldp_unassign(ldp);
	}

	//Private part (so the whole deletion is atomic for users)
	pa_for_each_ldp_in_dp_safe(dp, ldp, ldp2)
	pa_ldp_destroy(ldp);
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
	INIT_LIST_HEAD(&dp->ldps);
	list_add(&dp->le, &core->dps);
	struct pa_link *link;
	pa_for_each_link(core, link) {
		if(pa_ldp_create(core, link, dp)) {
			PA_WARNING("FAILED to add Delegated Prefix "PA_DP_P, PA_DP_PA(dp));
			_pa_dp_del(dp);
			return -1;
		}
	}
	return 0;
}

static void _pa_link_del(struct pa_link *link)
{
	struct pa_ldp *ldp, *ldp2;
	//Public part
	pa_for_each_ldp_in_link(link, ldp) {
		pa_ldp_set_published(ldp, 0);
		pa_ldp_set_applied(ldp, 0);
		pa_ldp_unassign(ldp);
	}

	//Private part (so the whole deletion is atomic for users)
	pa_for_each_ldp_in_link_safe(link, ldp, ldp2)
	pa_ldp_destroy(ldp);

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
	INIT_LIST_HEAD(&link->ldps);
	list_add(&link->le, &core->links);
	struct pa_dp *dp;
	pa_for_each_dp(core, dp) {
		if(pa_ldp_create(core, link, dp)) {
			PA_WARNING("FAILED to add Link "PA_LINK_P, PA_LINK_PA(link));
			_pa_link_del(link);
			return -1;
		}
	}
	return 0;
}

static void _pa_advp_update(struct pa_core *core, struct pa_advp *advp)
{
	struct pa_dp *dp;
	struct pa_ldp *ldp;
	pa_for_each_dp(core, dp) {
		/* Schedule all for dps overlapping with the advp. */
		//TODO: Maybe not necessary to schedule if we have Current and advp is not overlapping with it.
		if(prefix_overlap(&dp->prefix, dp->plen, &advp->prefix, advp->plen)) {
			pa_for_each_ldp_in_dp(dp, ldp)
					pa_routine_schedule(ldp);
		}
	}
}

/* Tell the content of the Advertised Prefix was changes. */
void pa_advp_update(struct pa_core *core, struct pa_advp *advp)
{
	PA_DEBUG("Updating Advertised Prefix "PA_ADVP_P, PA_ADVP_PA(advp));
	_pa_advp_update(core, advp);
}

/* Adds a new Advertised Prefix. */
int pa_advp_add(struct pa_core *core, struct pa_advp *advp)
{
	PA_DEBUG("Adding Advertised Prefix "PA_ADVP_P, PA_ADVP_PA(advp));
	advp->in_core.type = PAT_ADVERTISED;
	if(btrie_add(&core->prefixes, &advp->in_core.be, (btrie_key_t *)&advp->prefix, advp->plen)) {
		PA_WARNING("Could not add Advertised Prefix "PA_ADVP_P, PA_ADVP_PA(advp));
		return -1;
	}

	_pa_advp_update(core, advp);
	return 0;
}

/* Removes an added Advertised Prefix. */
void pa_advp_del(struct pa_core *core, struct pa_advp *advp)
{
	PA_DEBUG("Deleting Advertised Prefix "PA_ADVP_P, PA_ADVP_PA(advp));
	btrie_remove(&advp->in_core.be);
	_pa_advp_update(core, advp);
}

void pa_rule_add(struct pa_core *core, struct pa_rule *rule)
{
	PA_DEBUG("Adding rule "PA_RULE_P, PA_RULE_PA(rule));
	list_add(&rule->le, &core->rules);
	/* Schedule all routines */
	struct pa_link *link;
	struct pa_ldp *ldp;
	pa_for_each_link(core, link)
		pa_for_each_ldp_in_link(link, ldp)
			pa_routine_schedule(ldp);
}

void pa_rule_del(struct pa_core *core, struct pa_rule *rule)
{
	PA_DEBUG("Deleting rule "PA_RULE_P, PA_RULE_PA(rule));
	list_del(&rule->le);
	struct pa_link *link;
	struct pa_ldp *ldp;
	pa_for_each_link(core, link)
		pa_for_each_ldp_in_link(link, ldp) {
			pa_routine_schedule(ldp);
			if(ldp->rule == rule)
				ldp->rule = NULL; //Make it orphan
		}
}

void pa_core_set_flooding_delay(struct pa_core *core, uint32_t flooding_delay)
{
	PA_INFO("Set Flooding Delay to %"PRIu32, flooding_delay);
	struct pa_link *link;
	struct pa_ldp *ldp;
	if(flooding_delay > core->flooding_delay) {
		pa_for_each_link(core, link)
			pa_for_each_ldp_in_link(link, ldp)
				if(ldp->published && ldp->backoff_to.pending)
					uloop_timeout_set(&ldp->backoff_to, uloop_timeout_remaining(&ldp->backoff_to) + 2*(flooding_delay - core->flooding_delay));
	} else if (flooding_delay < core->flooding_delay) {
		pa_for_each_link(core, link)
			pa_for_each_ldp_in_link(link, ldp)
				if(ldp->published && ldp->backoff_to.pending && ((uint32_t)uloop_timeout_remaining(&ldp->backoff_to) > 2*flooding_delay))
					uloop_timeout_set(&ldp->backoff_to, 2*flooding_delay);
	}
	core->flooding_delay = flooding_delay;
}

void pa_core_set_node_id(struct pa_core *core, const PA_NODE_ID_TYPE node_id[])
{
	PA_INFO("Set Node ID to "PA_NODE_ID_P, PA_NODE_ID_PA(node_id));
	struct pa_link *link;
	struct pa_ldp *ldp;
	if(memcmp(node_id, core->node_id, PA_NODE_ID_LEN)) {
		memcpy(core->node_id, node_id, PA_NODE_ID_LEN);
		/* Schedule routine for all pairs */
		pa_for_each_link(core, link)
			pa_for_each_ldp_in_link(link, ldp)
				pa_routine_schedule(ldp);
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
