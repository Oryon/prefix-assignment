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

#include "pa_core.c"

static struct pa_dp
	d1 = {.plen = 56, .prefix = {{{0x20, 0x01, 0, 0, 0, 0, 0x01}}}},
	d2 = {.plen = 56, .prefix = {{{0x20, 0x01, 0, 0, 0, 0, 0x01}}}};

static struct pa_link
		l1 = {.name = "L1"},
		l2 = {.name = "L2"};

static uint32_t id0 = 0,
		id1 = 0x111111,
		id2 = 0x222222,
		id3 = 0x333333;

static struct pa_pp
		pp1 = {.plen = 56, .prefix = {{{0x20, 0x01, 0, 0, 0, 0, 0x01, 0x01}}}},
		pp2 = {.plen = 56, .prefix = {{{0x20, 0x01, 0, 0, 0, 0, 0x01, 0x01}}}};

enum pa_rule_target no_match_rule_f(struct pa_rule *rule, struct pa_ap *ap,
			pa_rule_priority *rule_priority)
{
	return PA_RULE_NO_MATCH;
}

static struct pa_rule
		no_match_rule1 = {.name = "Do Nothing 1", .match = no_match_rule_f},
		no_match_rule2 = {.name = "Do Nothing 2", .match = no_match_rule_f};


void pa_core_data() {
	struct pa_core core;

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
	pp1.link = &l1;
	memcpy(pp1.node_id, &id1, PA_NODE_ID_LEN);

	pp2.link = &l2;
	memcpy(pp2.node_id, &id3, PA_NODE_ID_LEN);
	btrie_fail = true;
	sput_fail_unless(pa_pp_add(&core, &pp1), "Can't add pp1");
	btrie_fail = false;
	sput_fail_if(pa_pp_add(&core, &pp1), "Add pp1");
	btrie_fail = true;
	sput_fail_unless(pa_pp_add(&core, &pp2), "Can't add pp2");
	btrie_fail = false;
	sput_fail_if(pa_pp_add(&core, &pp2), "Add pp2");

	pa_pp_update(&core, &pp1);
	pa_pp_update(&core, &pp2);
	pa_pp_del(&core, &pp1);
	pa_pp_del(&core, &pp2);

	/* Adding rules */
	pa_rule_add(&core, &no_match_rule1);
	pa_rule_add(&core, &no_match_rule2);
	pa_rule_del(&core, &no_match_rule1);
	pa_rule_del(&core, &no_match_rule2);

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
	sput_leave_suite(); /* optional */
	sput_finish_testing();
	return sput_get_return_value();
}
