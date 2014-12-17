#include "sput.h"

#include "pa_conf.h"

#include <stdio.h>
#define PA_WARNING(format, ...) printf("PA Warning : "format"\n", ##__VA_ARGS__)
#define PA_INFO(format, ...)    printf("PA Info    : "format"\n", ##__VA_ARGS__)
#define PA_DEBUG(format, ...)   printf("PA Debug   : "format"\n", ##__VA_ARGS__)

#define TEST_DEBUG(format, ...) printf("TEST Debug   : "format"\n", ##__VA_ARGS__)

#undef PA_PREFIX_TYPE
#define PA_PREFIX_TYPE uint32_t

#undef pa_prefix_tostring

const char *prefix_tostring(char *buff, uint32_t *prefix, uint8_t plen)
{
	sprintf(buff, "0x%x/%u", *prefix, plen);
	return buff;
}

#define pa_prefix_tostring(p, plen) \
	(plen?prefix_tostring(alloca(40), p, plen):"::/0")

#include "pa_core.h"

void test_advp_add(struct pa_core *core, struct pa_advp *advp)
{
	advp->in_core.type = PAT_ADVERTISED;
	sput_fail_if(btrie_add(&core->prefixes, &advp->in_core.be, (btrie_key_t *)&advp->prefix, advp->plen), "Adding Advertised Prefix");
}

void test_advp_del(struct pa_core *core, struct pa_advp *advp)
{
	btrie_remove(&advp->in_core.be);
}

#include "pa_rules.c"


void pa_rules_adopt()
{
	pa_prefix prefix = 23;
	pa_plen plen = 12;
}

int main() {
	sput_start_testing();
	sput_enter_suite("Prefix Assignment Rules tests"); /* optional */
	sput_run_test(pa_rules_adopt);
	sput_leave_suite(); /* optional */
	sput_finish_testing();
	return sput_get_return_value();
}
