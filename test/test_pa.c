/* Basic tests. */

#include <stdio.h>
#include <stdlib.h>

#include "fake_uloop.h"

/* Make calloc fail */
static bool calloc_fail = false;
static void *f_calloc (size_t __nmemb, size_t __size) {
	if(calloc_fail)
		return NULL;
	return calloc(__nmemb, __size);
}

#define calloc f_calloc

/* Make btrie_add fail */
#include "btrie.h"
static bool btrie_fail = false;
static int f_btrie_add(struct btrie *root, struct btrie_element *new, const btrie_key_t *key, btrie_plen_t len) {
	if(btrie_fail)
		return -1;
	return btrie_add(root, new, key, len);
}

#define btrie_add f_btrie_add



#include <stdio.h>
#define PA_WARNING(format, ...) printf("PA Warning : "format"\n", ##__VA_ARGS__)
#define PA_INFO(format, ...)    printf("PA Info    : "format"\n", ##__VA_ARGS__)
#define PA_DEBUG(format, ...)   printf("PA Debug   : "format"\n", ##__VA_ARGS__)

#define TEST_DEBUG(format, ...) printf("TEST Debug   : "format"\n", ##__VA_ARGS__)

#include "pa_core.c"

#define __unused __attribute__ ((unused))

static struct pa_dp
	d1 = {.plen = 56, .prefix = {{{0x20, 0x01, 0, 0, 0, 0, 0x01}}}},
	d2 = {.plen = 56, .prefix = {{{0x20, 0x01, 0, 0, 0, 0, 0x02}}}};

static struct pa_link
		l1 = {.name = "L1"},
		l2 = {.name = "L2"};

static uint32_t id0 = 0,
		id1 = 0x111111,
		id2 = 0x222222,
		id3 = 0x333333;

static struct pa_advp
		advp1_01 = {.plen = 64, .prefix = {{{0x20, 0x01, 0, 0, 0, 0, 0x01, 0x01}}}},
		advp1_02 = {.plen = 64, .prefix = {{{0x20, 0x01, 0, 0, 0, 0, 0x01, 0x11}}}},
		advp2_01 = {.plen = 64, .prefix = {{{0x20, 0x01, 0, 0, 0, 0, 0x02, 0x01}}}};

struct test_user {
	struct pa_user user;
	struct pa_ldp *assigned_ldp, *published_ldp, *applied_ldp;
};

#define check_user(tuser, assigned, published, applied) \
	sput_fail_unless((tuser)->assigned_ldp == assigned, "Correct user assigned"); \
	sput_fail_unless((tuser)->published_ldp == published, "Correct user published"); \
	sput_fail_unless((tuser)->applied_ldp == applied, "Correct user applied"); \
	(tuser)->assigned_ldp = (tuser)->published_ldp = (tuser)->applied_ldp = NULL;

#define check_ldp_flags(ldp, ass, pub, app, adopt) \
		sput_fail_unless((ldp)->assigned == ass, "Correct ldp assigned"); \
		sput_fail_unless((ldp)->published == pub, "Correct ldp published"); \
		sput_fail_unless((ldp)->applied == app, "Correct ldp applied"); \
		sput_fail_unless((ldp)->adopting == adopt, "Correct ldp adopting");

#define check_ldp_prefix(ldp, p, pl) \
		sput_fail_unless(prefix_equals(&(ldp)->prefix, (ldp)->plen, p, pl), "Correct prefix");

/* Custom user */

static void user_assigned(struct pa_user *user, struct pa_ldp *ldp) {
	TEST_DEBUG("Called user_assigned");
	struct test_user *tuser = container_of(user, struct test_user, user);
	tuser->assigned_ldp = ldp;
}

static void user_published(struct pa_user *user, struct pa_ldp *ldp) {
	TEST_DEBUG("Called user_published");
	struct test_user *tuser = container_of(user, struct test_user, user);
	tuser->published_ldp = ldp;
}

static void user_applied(struct pa_user *user, struct pa_ldp *ldp) {
	TEST_DEBUG("Called user_applied");
	struct test_user *tuser = container_of(user, struct test_user, user);
	tuser->applied_ldp = ldp;
}

static struct test_user tuser = {
		.user = {.assigned = user_assigned,
		.published = user_published,
		.applied = user_applied }
};

/* Custom rules */

struct no_match_rule {
	struct pa_rule rule;
	pa_rule_priority priority;
};

pa_rule_priority no_match_rule_prio(__unused struct pa_rule *rule, __unused struct pa_ldp *ldp)
{
	return container_of(rule, struct no_match_rule, rule)->priority;
}

enum pa_rule_target no_match_rule_match(__unused struct pa_rule *rule, __unused struct pa_ldp *ldp,
		__unused pa_rule_priority best_match_priority,
		__unused struct pa_rule_arg *pa_arg)
{
	return PA_RULE_NO_MATCH;
}

void no_match_rule_init(struct no_match_rule *r, pa_rule_priority p) {
	r->priority = p;
	r->rule.get_max_priority = no_match_rule_prio;
	r->rule.match = no_match_rule_match;
}

pa_rule_priority no_priority_rule_f(struct pa_rule *rule, struct pa_ldp *ldp)
{
	return 0;
}

void no_priority_rule_init(struct pa_rule *rule)
{
	rule->get_max_priority = no_priority_rule_f;
}

void pa_core_norule() {
	struct pa_core core;
	struct pa_ldp *ldp, *ldp2;

	//Nothing pending
	sput_fail_if(fu_next(), "No pending timeout");

	pa_core_init(&core);

	pa_link_add(&core, &l1);
	pa_dp_add(&core, &d1);

	pa_for_each_ldp_in_dp(&d1, ldp2){
		ldp = ldp2; //Get the unique ldp
	}

	//Test scheduling
	sput_fail_unless(ldp, "ldp present");
	sput_fail_unless(ldp->routine_to.pending, "Routine pending");
	sput_fail_unless(uloop_timeout_remaining(&ldp->routine_to) == PA_RUN_DELAY, "Correct delay");
	sput_fail_unless(fu_next() == &ldp->routine_to, "Correct timeout");

	set_time(get_time() + 1);
	pa_core_set_node_id(&core, &id1); //Reschedule
	sput_fail_unless(ldp->routine_to.pending, "Routine pending");
	sput_fail_unless(uloop_timeout_remaining(&ldp->routine_to) == PA_RUN_DELAY - 1, "Correct delay");

	//Adding user
	pa_user_register(&core, &tuser.user);

	//Execute routine with nothing
	fu_loop(1);
	check_user(&tuser, NULL, NULL, NULL);
	check_ldp_flags(ldp, false, false, false, false);

	//Add an adv prefix outside the dp on a null link
	//Scheduling only happens when overlap with a dp
	advp2_01.link = NULL;
	advp2_01.priority = 2;
	pa_advp_add(&core, &advp2_01);
	pa_advp_update(&core, &advp2_01);
	sput_fail_if(ldp->routine_to.pending, "Not routine pending");
	sput_fail_if(fu_next(), "No pending timeout");

	//Add an adv prefix inside the dp on a null link
	advp1_01.link = NULL;
	advp1_01.priority = 2;
	pa_advp_add(&core, &advp1_01);
	sput_fail_unless(ldp->routine_to.pending, "Routine pending");
	sput_fail_unless(uloop_timeout_remaining(&ldp->routine_to) == PA_RUN_DELAY, "Correct delay");
	fu_loop(1);
	check_user(&tuser, NULL, NULL, NULL);
	check_ldp_flags(ldp, false, false, false, false);

	//Set the adv prefix as onlink
	//Accept a prefix
	advp1_01.link = &l1;
	pa_advp_update(&core, &advp1_01);
	sput_fail_unless(ldp->routine_to.pending, "Routine pending");
	sput_fail_unless(uloop_timeout_remaining(&ldp->routine_to) == PA_RUN_DELAY, "Correct delay");
	fu_loop(1);
	check_user(&tuser, ldp, NULL, NULL);
	check_ldp_flags(ldp, 1, 0, 0, 0);
	check_ldp_prefix(ldp, &advp1_01.prefix, advp1_01.plen);

	//Apply running
	sput_fail_unless(ldp->backoff_to.pending, "Apply timeout pending");
	sput_fail_unless(uloop_timeout_remaining(&ldp->backoff_to) == (int)(2 * core.flooding_delay), "Correct apply delay");
	sput_fail_unless(fu_next() == &ldp->backoff_to, "Correct timeout");

	//Remove adv2_01
	pa_advp_del(&core, &advp2_01);
	sput_fail_if(ldp->routine_to.pending, "Not routine pending");

	//Remove and add adv1_01 again
	pa_advp_del(&core, &advp1_01);
	sput_fail_unless(ldp->routine_to.pending, "Routine pending");
	check_user(&tuser, NULL, NULL, NULL);
	check_ldp_flags(ldp, 1, 0, 0, 0);

	set_time(get_time() + 1);
	pa_advp_add(&core, &advp1_01);
	sput_fail_unless(ldp->routine_to.pending, "Routine pending");
	sput_fail_unless(uloop_timeout_remaining(&ldp->routine_to) == PA_RUN_DELAY - 1, "Correct delay");
	fu_loop(1);
	check_user(&tuser, NULL, NULL, NULL);
	check_ldp_flags(ldp, 1, 0, 0, 0);

	//Apply running
	sput_fail_unless(ldp->backoff_to.pending, "Apply timeout pending");
	sput_fail_unless(uloop_timeout_remaining(&ldp->backoff_to) == (int)(2 * core.flooding_delay) - PA_RUN_DELAY, "Correct apply delay");
	sput_fail_unless(fu_next() == &ldp->backoff_to, "Correct timeout");

	//Remove and execute routine
	pa_advp_del(&core, &advp1_01);
	fu_loop(1);
	check_user(&tuser, ldp, NULL, NULL);
	check_ldp_flags(ldp, 0, 0, 0, 0);

	//Apply canceled
	sput_fail_if(ldp->backoff_to.pending, "Apply to not pending");
	sput_fail_if(fu_next(), "Not timeout");

	//Add and execute routine
	pa_advp_add(&core, &advp1_01);
	fu_loop(1);
	check_user(&tuser, ldp, NULL, NULL);
	check_ldp_flags(ldp, 1, 0, 0, 0);

	//Now let's apply the prefix
	fu_loop(1);
	check_user(&tuser, NULL, NULL, ldp);
	check_ldp_flags(ldp, 1, 0, 1, 0);

	//Add another prefix on a different link
	advp1_02.link = NULL;
	advp1_02.priority = 10;
	pa_advp_add(&core, &advp1_02);
	fu_loop(1);
	check_user(&tuser, NULL, NULL, NULL);
	check_ldp_flags(ldp, 1, 0, 1, 0);

	//Lower priority
	advp1_02.priority = 0;
	pa_advp_update(&core, &advp1_02);
	fu_loop(1);
	check_user(&tuser, NULL, NULL, NULL);
	check_ldp_flags(ldp, 1, 0, 1, 0);

	//On the link
	advp1_02.link = &l1;
	pa_advp_update(&core, &advp1_02);
	fu_loop(1);
	check_user(&tuser, NULL, NULL, NULL);
	check_ldp_flags(ldp, 1, 0, 1, 0);

	//With a higher priority
	advp1_02.priority = 3;
	pa_advp_update(&core, &advp1_02);
	fu_loop(1);
	check_user(&tuser, ldp, NULL, ldp);
	check_ldp_flags(ldp, 1, 0, 0, 0);
	check_ldp_prefix(ldp, &advp1_02.prefix, advp1_02.plen);

	//Check apply timer
	sput_fail_unless(ldp->backoff_to.pending, "Apply timeout pending");
	sput_fail_unless(uloop_timeout_remaining(&ldp->backoff_to) == (int)(2 * core.flooding_delay), "Correct apply delay");

	//First one use a lower rid
	advp1_02.node_id[0] = id3;
	advp1_01.node_id[0] = id2; //Lower router id
	advp1_01.priority = 3; //Same prio
	pa_advp_update(&core, &advp1_01);
	fu_loop(1);
	check_user(&tuser, NULL, NULL, NULL);
	check_ldp_flags(ldp, 1, 0, 0, 0);
	check_ldp_prefix(ldp, &advp1_02.prefix, advp1_02.plen);

	//Now use an higher router ID
	advp1_02.node_id[0] = id2;
	advp1_01.node_id[0] = id3; //Higher router id
	pa_advp_update(&core, &advp1_01);
	fu_loop(1);
	check_user(&tuser, ldp, NULL, NULL);
	check_ldp_flags(ldp, 1, 0, 0, 0);
	check_ldp_prefix(ldp, &advp1_01.prefix, advp1_01.plen);

	//Remove the link from core
	pa_link_del(&l1);
	sput_fail_if(ldp->routine_to.pending, "Not routine pending");
	sput_fail_if(fu_next(), "No pending timeout");
	check_user(&tuser, ldp, NULL, NULL);

	//Add link again
	pa_link_add(&core, &l1);
	pa_for_each_ldp_in_dp(&d1, ldp2){
		ldp = ldp2; //Get the unique ldp
	}
	fu_loop(1);
	check_user(&tuser, ldp, NULL, NULL);
	check_ldp_flags(ldp, 1, 0, 0, 0);
	check_ldp_prefix(ldp, &advp1_01.prefix, advp1_01.plen);

	//Remove both advertised prefixes
	pa_advp_del(&core, &advp1_01);
	pa_advp_del(&core, &advp1_02);
	fu_loop(1);
	check_user(&tuser, ldp, NULL, NULL);
	check_ldp_flags(ldp, 0, 0, 0, 0);

	pa_link_del(&l1);
	pa_dp_del(&d1);

	sput_fail_if(fu_next(), "No scheduled timer.");
}

void pa_core_data() {
	struct pa_core core;
	sput_fail_if(fu_next(), "No pending timeout");

	pa_core_init(&core);
	sput_fail_unless(core.flooding_delay == PA_DEFAULT_FLOODING_DELAY, "Default flooding delay");
	sput_fail_if(memcmp(core.node_id, &id0, PA_NODE_ID_LEN), "Default Node ID");

	pa_core_set_node_id(&core, &id1);
	pa_core_set_flooding_delay(&core, 20000);
	pa_core_set_flooding_delay(&core, 5000);
	sput_fail_if(pa_link_add(&core, &l1), "Add L1");
	sput_fail_if(pa_dp_add(&core, &d1), "Add DP1");
	sput_fail_if(pa_link_add(&core, &l2), "Add L2");
	sput_fail_if(pa_dp_add(&core, &d2), "Add DP2");
	pa_core_set_node_id(&core, &id2);
	pa_core_set_flooding_delay(&core, 10000);
	pa_core_set_flooding_delay(&core, 5000);

	pa_link_del(&l1);
	pa_dp_del(&d1);

	sput_fail_if(pa_link_add(&core, &l1), "Add L1");
	sput_fail_if(pa_dp_add(&core, &d1), "Add DP1");

	/* Test adding PPs */
	advp1_01.link = &l1;
	memcpy(advp1_01.node_id, &id1, PA_NODE_ID_LEN);

	advp2_01.link = &l2;
	memcpy(advp2_01.node_id, &id3, PA_NODE_ID_LEN);
	btrie_fail = true;
	sput_fail_unless(pa_advp_add(&core, &advp1_01), "Can't add advp1_01");
	btrie_fail = false;
	sput_fail_if(pa_advp_add(&core, &advp1_01), "Add advp1_01");
	btrie_fail = true;
	sput_fail_unless(pa_advp_add(&core, &advp2_01), "Can't add advp2_01");
	btrie_fail = false;
	sput_fail_if(pa_advp_add(&core, &advp2_01), "Add advp2_01");

	pa_advp_update(&core, &advp1_01);
	pa_advp_update(&core, &advp2_01);
	pa_advp_del(&core, &advp1_01);
	pa_advp_del(&core, &advp2_01);

	/* Adding rules */
	struct pa_rule r1,r2;
	pa_rule_add(&core, &r1);
	pa_rule_add(&core, &r2);
	pa_rule_del(&core, &r1);
	pa_rule_del(&core, &r2);

	/* Remove all */
	pa_dp_del(&d1);
	pa_link_del(&l1);
	pa_dp_del(&d2);
	pa_link_del(&l2);

	/* Test when calloc fails */
	calloc_fail = true;
	sput_fail_if(pa_link_add(&core, &l1), "Add L1");
	sput_fail_unless(pa_dp_add(&core, &d1), "Fail adding DP1");
	pa_link_del(&l1);

	sput_fail_if(pa_dp_add(&core, &d1), "Add DP1");
	sput_fail_unless(pa_link_add(&core, &l1), "Fail adding L1");
	pa_dp_del(&d1);
	calloc_fail = false;

	/* There should be nothing scheduled here */
	sput_fail_if(fu_next(), "No scheduled timer.");
}

int main() {
	fu_init();
	sput_start_testing();
	sput_enter_suite("Prefix assignment tests"); /* optional */
	sput_run_test(pa_core_data);
	sput_run_test(pa_core_norule);
	sput_leave_suite(); /* optional */
	sput_finish_testing();
	return sput_get_return_value();
}
