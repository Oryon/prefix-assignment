/* Basic tests. */

#include <stdio.h>
#include <stdlib.h>

#include "fake_uloop.h"

#include <stdio.h>
#define PA_WARNING(format, ...) printf("PA Warning : "format"\n", ##__VA_ARGS__)
#define PA_INFO(format, ...)    printf("PA Info    : "format"\n", ##__VA_ARGS__)
#define PA_DEBUG(format, ...)   printf("PA Debug   : "format"\n", ##__VA_ARGS__)

#include "../src/pa_core.c"

void pa_core_data() {
	struct pa_core core;
	uint32_t id1 = 0x111111, id2 = 0x222222, id3 = 0x333333;
	struct pa_link l1,l2;
	struct pa_dp d1 = {.plen = 56, .prefix = {{{0x20, 0x01, 0, 0, 0, 0, 0x01}}}}
	,d2 = {.plen = 56, .prefix = {{{0x20, 0x01, 0, 0, 0, 0, 0x01}}}};

	l1.name = "L1";
	l2.name = "L2";

	pa_core_init(&core);
	pa_link_add(&core, &l1);
	pa_dp_add(&core, &d1);
	pa_link_add(&core, &l2);
	pa_dp_add(&core, &d2);
	pa_core_set_node_id(&core, (uint8_t *)&id1);
	pa_core_set_node_id(&core, (uint8_t *)&id2);

	pa_link_del(&l1);
	pa_dp_del(&d1);

	pa_link_add(&core, &l1);
	pa_dp_add(&core, &d1);

	pa_dp_del(&d1);
	pa_link_del(&l1);
	pa_dp_del(&d2);
	pa_link_del(&l2);


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
