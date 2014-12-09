/* Basic tests. */

#include <stdio.h>
#include <stdlib.h>

#include "../src/pa_core.h"

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
}

int main(int argc, const char *argv[]) {
	uloop_init();
	pa_core_data();

	return 0;
}
