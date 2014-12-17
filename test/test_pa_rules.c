#include "sput.h"

#include "pa_rules.c"

void pa_rules_filters(){

}

int main() {
	sput_start_testing();
	sput_enter_suite("Prefix Assignment Rules tests"); /* optional */
	sput_run_test(pa_rules_filters);
	sput_leave_suite(); /* optional */
	sput_finish_testing();
	return sput_get_return_value();
}
