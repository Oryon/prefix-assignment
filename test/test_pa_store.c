#include <stdio.h>
#define PA_WARNING(format, ...) printf("PA Warning : "format"\n", ##__VA_ARGS__)
#define PA_INFO(format, ...)    printf("PA Info    : "format"\n", ##__VA_ARGS__)
#define PA_DEBUG(format, ...)   printf("PA Debug   : "format"\n", ##__VA_ARGS__)

#define TEST_DEBUG(format, ...) printf("TEST Debug   : "format"\n", ##__VA_ARGS__)

#include "pa_store.c"

#include "sput.h"

static struct in6_addr p = {{{0x20, 0x01, 0, 0, 0, 0, 0x01, 0x00}}};
static struct in6_addr v4 = {{{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0xff}}};
#define PV(i) (p.s6_addr[7] = i, p)
#define PP(i) (p.s6_addr[7] = i, &p)
#define PP4(i, u) (v4.s6_addr[12] = i, v4.s6_addr[13] = u, &v4)

//Test prefix parsing
void pa_store_cache_parsing()
{
	char str[PA_PREFIX_STRLEN];
	struct in6_addr a;
	uint8_t plen, b;
	pa_prefix_tostring(str, PP(0), (plen = 64));
	sput_fail_if(strcmp(str, "2001:0:0:100::/64"), "Ok string");
	sput_fail_unless(pa_prefix_fromstring(str, &a, &b), "Ok parse");
	sput_fail_if(pa_prefix_cmp(&p, plen, &a, b), "Ok prefix");

	pa_prefix_tostring(str, PP(1), (plen = 64));
	sput_fail_if(strcmp(str, "2001:0:0:101::/64"), "Ok string");
	sput_fail_unless(pa_prefix_fromstring(str, &a, &b), "Ok parse");
	sput_fail_if(pa_prefix_cmp(&p, plen, &a, b), "Ok prefix");

	pa_prefix_tostring(str, PP(1), (plen = 63));
	sput_fail_if(strcmp(str, "2001:0:0:100::/63"), "Ok string");
	sput_fail_unless(pa_prefix_fromstring(str, &a, &b), "Ok parse");
	sput_fail_if(pa_prefix_cmp(&p, plen, &a, b), "Ok prefix");

	pa_prefix_tostring(str, PP(2), (plen = 63));
	sput_fail_if(strcmp(str, "2001:0:0:102::/63"), "Ok string");
	sput_fail_unless(pa_prefix_fromstring(str, &a, &b), "Ok parse");
	sput_fail_if(pa_prefix_cmp(&p, plen, &a, b), "Ok prefix");

	pa_prefix_tostring(str, PP(1), (plen = 8));
	sput_fail_if(strcmp(str, "2000::/8"), "Ok string");
	sput_fail_unless(pa_prefix_fromstring(str, &a, &b), "Ok parse");
	sput_fail_if(pa_prefix_cmp(&p, plen, &a, b), "Ok prefix");

	pa_prefix_tostring(str, PP4(10, 10), (plen = 104));
	sput_fail_if(strcmp(str, "10.0.0.0/8"), "Ok string");
	sput_fail_unless(pa_prefix_fromstring(str, &a, &b), "Ok parse");
	sput_fail_if(pa_prefix_cmp(&v4, plen, &a, b), "Ok prefix");
	sput_fail_unless(pa_prefix_fromstring("::ffff:a00:0/104", &a, &b)==1, "Ok parse");
	sput_fail_if(pa_prefix_cmp(&v4, plen, &a, b), "Ok prefix");

	pa_prefix_tostring(str, PP4(10, 1), (plen = 111));
	sput_fail_if(strcmp(str, "10.0.0.0/15"), "Ok string");
	sput_fail_unless(pa_prefix_fromstring(str, &a, &b), "Ok parse");
	sput_fail_if(pa_prefix_cmp(&v4, plen, &a, b), "Ok prefix");

	pa_prefix_tostring(str, PP4(10, 1), (plen = 112));
	sput_fail_if(strcmp(str, "10.1.0.0/16"), "Ok string");
	sput_fail_unless(pa_prefix_fromstring(str, &a, &b), "Ok parse");
	sput_fail_if(pa_prefix_cmp(&v4, plen, &a, b), "Ok prefix");

	pa_prefix_tostring(str, PP(1), (plen = 128));
	sput_fail_if(strcmp(str, "2001:0:0:101::/128"), "Ok string");
	sput_fail_unless(pa_prefix_fromstring(str, &a, &b), "Ok parse");
	sput_fail_if(pa_prefix_cmp(&p, plen, &a, b), "Ok prefix");
	sput_fail_unless(pa_prefix_fromstring("2001:0:0:101::", &a, &b), "Ok parse");
	sput_fail_if(pa_prefix_cmp(&p, plen, &a, b), "Ok prefix");

	pa_prefix_tostring(str, PP4(10, 1), (plen = 96));
	sput_fail_if(strcmp(str, "0.0.0.0/0"), "Ok string");
	sput_fail_unless(pa_prefix_fromstring(str, &a, &b), "Ok parse");
	sput_fail_if(pa_prefix_cmp(&v4, plen, &a, b), "Ok prefix");

	pa_prefix_tostring(str, PP(1), (plen = 0));
	sput_fail_if(strcmp(str, "::/0"), "Ok string");
	sput_fail_unless(pa_prefix_fromstring(str, &a, &b), "Ok parse");
	sput_fail_if(pa_prefix_cmp(&p, plen, &a, b), "Ok prefix");
}


void pa_store_cache_test()
{
	struct pa_core core;
	INIT_LIST_HEAD(&core.users);

	struct pa_store store;
	struct pa_store_prefix *prefix, *prefix2;
	pa_store_init(&store, &core, 3);
	sput_fail_if(store.user.assigned, "No assigned function");
	sput_fail_if(store.user.published, "No published function");
	sput_fail_unless(store.user.applied, "Applied function");
	sput_fail_unless(list_empty(&store.links), "No links");

	struct pa_link l;
	struct pa_dp d;
	struct pa_ldp ldp;
	ldp.link = &l;
	ldp.dp = &d;
	ldp.prefix = PV(0);
	ldp.plen = 64;
	ldp.assigned = 0;
	ldp.applied = 0;

	store.user.applied(&store.user, &ldp);
	sput_fail_unless(list_empty(&store.links), "No links");
	sput_fail_unless(store.n_prefixes == 0, "No prefixes");

	ldp.assigned = 1;
	ldp.applied = 1;
	store.user.applied(&store.user, &ldp);
	sput_fail_unless(list_empty(&store.links), "No links");
	sput_fail_unless(store.n_prefixes == 0, "No prefixes");

	struct pa_store_link link;
	pa_store_link_init(&link, &l, "L1", 2);
	pa_store_link_add(&store, &link);
	sput_fail_if(list_empty(&store.links), "One link in the list");
	sput_fail_unless(store.n_prefixes == 0, "No prefixes");
	sput_fail_unless(list_empty(&link.prefixes), "No stored prefix");
	sput_fail_unless(link.n_prefixes == 0, "No stored prefix");

	ldp.applied = 0;
	store.user.applied(&store.user, &ldp);
	sput_fail_if(list_empty(&store.links), "One link in the list");
	sput_fail_unless(store.n_prefixes == 0, "No prefixes");
	sput_fail_unless(list_empty(&link.prefixes), "No stored prefix");
	sput_fail_unless(link.n_prefixes == 0, "No stored prefix");

	ldp.applied = 1;
	store.user.applied(&store.user, &ldp);
	sput_fail_unless(store.n_prefixes == 1, "One cached prefix");
	sput_fail_unless(link.n_prefixes == 1, "One cached prefix");
	sput_fail_if(list_empty(&store.prefixes), "One prefix in list");
	prefix = list_entry(store.prefixes.next, struct pa_store_prefix, in_store);
	sput_fail_if(pa_prefix_cmp(&prefix->prefix, prefix->plen, PP(0), 64), "Correct prefix");
	prefix2 = list_entry(link.prefixes.next, struct pa_store_prefix, in_link);
	sput_fail_unless(prefix == prefix2, "Same prefix in both lists");

	//Same prefix again
	store.user.applied(&store.user, &ldp);
	sput_fail_unless(store.n_prefixes == 1, "One cached prefix");
	sput_fail_unless(link.n_prefixes == 1, "One cached prefix");

	struct pa_link l2;
	struct pa_store_link link2;
	pa_store_link_init(&link2, &l2, "L2", 2);
	pa_store_link_add(&store, &link2);
	sput_fail_unless(list_empty(&link2.prefixes), "No stored prefix");
	sput_fail_unless(link2.n_prefixes == 0, "No stored prefix");

	//Another prefix for first link
	link.max_prefixes = 1;
	ldp.prefix = PV(2);
	store.user.applied(&store.user, &ldp);
	sput_fail_unless(store.n_prefixes == 1, "One cached prefix");
	sput_fail_unless(link.n_prefixes == 1, "One cached prefix");
	sput_fail_if(list_empty(&store.prefixes), "One prefix in list");
	prefix = list_entry(store.prefixes.next, struct pa_store_prefix, in_store);
	sput_fail_if(pa_prefix_cmp(&prefix->prefix, prefix->plen, PP(2), 64), "Correct prefix");
	prefix2 = list_entry(link.prefixes.next, struct pa_store_prefix, in_link);
	sput_fail_unless(prefix == prefix2, "Same prefix in both lists");

	//Another prefix removed by storage limitation
	link.max_prefixes = 2;
	store.max_prefixes = 1;
	ldp.prefix = PV(3);
	store.user.applied(&store.user, &ldp);
	sput_fail_unless(store.n_prefixes == 1, "One cached prefix");
	sput_fail_unless(link.n_prefixes == 1, "One cached prefix");
	sput_fail_if(list_empty(&store.prefixes), "One prefix in list");
	prefix = list_entry(store.prefixes.next, struct pa_store_prefix, in_store);
	sput_fail_if(pa_prefix_cmp(&prefix->prefix, prefix->plen, PP(3), 64), "Correct prefix");
	prefix2 = list_entry(link.prefixes.next, struct pa_store_prefix, in_link);
	sput_fail_unless(prefix == prefix2, "Same prefix in both lists");

	//Add the same to the other link
	ldp.link = &l2;
	store.user.applied(&store.user, &ldp);
	sput_fail_unless(store.n_prefixes == 1, "One cached prefix");
	sput_fail_unless(link.n_prefixes == 0, "No cached prefix");
	sput_fail_unless(link2.n_prefixes == 1, "One cached prefix");
	sput_fail_if(list_empty(&store.prefixes), "One prefix in list");
	prefix = list_entry(store.prefixes.next, struct pa_store_prefix, in_store);
	sput_fail_if(pa_prefix_cmp(&prefix->prefix, prefix->plen, PP(3), 64), "Correct prefix");
	prefix2 = list_entry(link2.prefixes.next, struct pa_store_prefix, in_link);
	sput_fail_unless(prefix == prefix2, "Same prefix in both lists");

	//Check the order
	store.max_prefixes = 2;
	link2.max_prefixes = 2;
	ldp.prefix = PV(4);
	store.user.applied(&store.user, &ldp);
	sput_fail_unless(store.n_prefixes == 2, "Two cached prefix");
	sput_fail_unless(link.n_prefixes == 0, "No cached prefix");
	sput_fail_unless(link2.n_prefixes == 2, "Two cached prefix");

	prefix = list_entry(store.prefixes.next, struct pa_store_prefix, in_store);
	sput_fail_if(pa_prefix_cmp(&prefix->prefix, prefix->plen, PP(4), 64), "Correct prefix");
	prefix2 = list_entry(link2.prefixes.next, struct pa_store_prefix, in_link);
	sput_fail_unless(prefix == prefix2, "Same prefix in both lists");

	prefix = list_entry(store.prefixes.prev, struct pa_store_prefix, in_store);
	sput_fail_if(pa_prefix_cmp(&prefix->prefix, prefix->plen, PP(3), 64), "Correct prefix");
	prefix2 = list_entry(link2.prefixes.prev, struct pa_store_prefix, in_link);
	sput_fail_unless(prefix == prefix2, "Same prefix in both lists");


	pa_store_link_remove(&store, &link);
	pa_store_link_remove(&store, &link2); //This should create a private link
	sput_fail_unless(store.n_prefixes == 2, "Two cached prefix");

	//Now let's add link one with link2 name, it should take precedent link2
	strcpy(link.name, "L2");
	link.max_prefixes = 2;
	pa_store_link_add(&store, &link);
	sput_fail_unless(store.n_prefixes == 2, "Two cached prefix");
	sput_fail_unless(link.n_prefixes == 2, "Two cached prefix");

	prefix = list_entry(store.prefixes.next, struct pa_store_prefix, in_store);
	sput_fail_if(pa_prefix_cmp(&prefix->prefix, prefix->plen, PP(4), 64), "Correct prefix");
	prefix2 = list_entry(link2.prefixes.next, struct pa_store_prefix, in_link);
	sput_fail_unless(prefix == prefix2, "Same prefix in both lists");

	prefix = list_entry(store.prefixes.prev, struct pa_store_prefix, in_store);
	sput_fail_if(pa_prefix_cmp(&prefix->prefix, prefix->plen, PP(3), 64), "Correct prefix");
	prefix2 = list_entry(link2.prefixes.prev, struct pa_store_prefix, in_link);
	sput_fail_unless(prefix == prefix2, "Same prefix in both lists");

	//Remove again, and add with less max prefixes
	pa_store_link_remove(&store, &link);
	link.max_prefixes = 1;
	pa_store_link_add(&store, &link);

	sput_fail_unless(store.n_prefixes == 1, "1 cached prefix");
	sput_fail_unless(link.n_prefixes == 1, "1 cached prefix");

	prefix = list_entry(store.prefixes.next, struct pa_store_prefix, in_store);
	sput_fail_if(pa_prefix_cmp(&prefix->prefix, prefix->plen, PP(4), 64), "Correct prefix");
	prefix2 = list_entry(link.prefixes.next, struct pa_store_prefix, in_link);
	sput_fail_unless(prefix == prefix2, "Same prefix in both lists");

	//Just to remove a prefix from private entry
	pa_store_link_remove(&store, &link);
	store.max_prefixes = 1;
	link.max_prefixes = 1;
	strcpy(link.name, "L22");
	pa_store_link_add(&store, &link);
	ldp.prefix = PV(5);
	ldp.link = link.link;
	store.user.applied(&store.user, &ldp);
	sput_fail_unless(store.n_prefixes == 1, "1 cached prefix");
	sput_fail_unless(link.n_prefixes == 1, "1 cached prefix");

	prefix = list_entry(store.prefixes.next, struct pa_store_prefix, in_store);
	sput_fail_if(pa_prefix_cmp(&prefix->prefix, prefix->plen, PP(5), 64), "Correct prefix");
	prefix2 = list_entry(link.prefixes.next, struct pa_store_prefix, in_link);
	sput_fail_unless(prefix == prefix2, "Same prefix in both lists");

	pa_store_link_remove(&store, &link);
	pa_store_term(&store);
}

int main() {
	sput_start_testing();
	sput_enter_suite("Prefix Assignment Storage tests"); /* optional */
	sput_run_test(pa_store_cache_parsing);
	sput_run_test(pa_store_cache_test);
	sput_leave_suite(); /* optional */
	sput_finish_testing();
	return sput_get_return_value();
}
