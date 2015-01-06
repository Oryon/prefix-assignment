#include <stdio.h>
#define PA_WARNING(format, ...) printf("PA Warning : "format"\n", ##__VA_ARGS__)
#define PA_INFO(format, ...)    printf("PA Info    : "format"\n", ##__VA_ARGS__)
#define PA_DEBUG(format, ...)   printf("PA Debug   : "format"\n", ##__VA_ARGS__)

#define TEST_DEBUG(format, ...) printf("TEST Debug   : "format"\n", ##__VA_ARGS__)



#include "fake_uloop.h"

/* Fake file handling */
int fake_files = 0;

FILE *test_fopen_ret = NULL;
const char *test_fopen_path = NULL;
const char *test_fopen_mode = NULL;
FILE *test_fopen(const char *path, const char *mode)
{
	test_fopen_mode = mode;
	test_fopen_path = path;
	return fake_files?test_fopen_ret:fopen(path, mode);
}

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

const char *test_open_pathname = NULL;
int test_open_flags = 0;
int test_open_mode = 0;
int test_open_ret = -1;
int test_open(const char *pathname, int flags, mode_t mode)
{
	test_open_pathname = pathname;
	test_open_flags = flags;
	test_open_mode = mode;
	return fake_files?test_open_ret:open(pathname,flags, mode);
}

char test_getline_lines[30][200];
int test_getline_n = 0;
int test_getline_max = 0;
ssize_t test_getline(char **lineptr, size_t *n, FILE *stream)
{
	if(!fake_files)
		return getline(lineptr, n, stream);

	if(test_getline_n >= test_getline_max) {
		*lineptr = NULL;
		return -1;
	}
	*lineptr = test_getline_lines[test_getline_n++];
	*n = strlen(*lineptr)+1;
	return *n - 1;
}

FILE *test_close_fp = NULL;
int test_fclose(FILE *fp)
{
	test_close_fp = fp;
	return fake_files?0:fclose(fp);
}

int test_close(int fd) {
	return fake_files?0:close(fd);
}

#define fopen test_fopen
#define fclose test_fclose
#define open test_open
#define getline test_getline
#define close test_close

#include "pa_store.c"

#include "sput.h"

static struct in6_addr p = {{{0x20, 0x01, 0, 0, 0, 0, 0x01, 0x00}}};
static struct in6_addr v4 = {{{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0xff}}};
#define PV(i) (p.s6_addr[7] = i, p)
#define PP(i) (p.s6_addr[7] = i, &p)
#define PP4(i, u) (v4.s6_addr[12] = i, v4.s6_addr[13] = u, &v4)

void pa_store_delays_test()
{
	fu_init();
	struct pa_core core;
	INIT_LIST_HEAD(&core.users);

	struct pa_store store;
	pa_store_init(&store, &core, 4);

	fake_files = 0;
	const char *filepath = "/tmp/test_pa_core.store";
	unlink(filepath);
	sput_fail_if(pa_store_set_file(&store, filepath, 2000, 10000), "Could open file");
	sput_fail_unless(store.save_delay = 2000, "Correct save delay");
	sput_fail_unless(store.token_delay = 10000, "Correct token delay");
	sput_fail_unless(store.token_count = PA_STORE_WTOKENS_DEFAULT, "Default number of tokens");
	sput_fail_unless(uloop_timeout_remaining(&store.token_timer) == 10000, "Token timer pending");
	sput_fail_unless(!store.save_timer.pending, "Store timer not pending");

	fu_loop(1);
	sput_fail_unless(store.token_count == PA_STORE_WTOKENS_DEFAULT+1, "One more token");
	store.token_count = 0;

	struct pa_link l;
	struct pa_dp d;
	struct pa_ldp ldp;
	ldp.link = &l;
	ldp.dp = &d;
	ldp.prefix = PV(0);
	ldp.plen = 64;
	ldp.assigned = 0;
	ldp.applied = 1;

	struct pa_store_link link;
	pa_store_link_init(&link, &l, "L1", 2);
	pa_store_link_add(&store, &link);

	store.user.applied(&store.user, &ldp);
	sput_fail_unless(store.n_prefixes == 1, "One prefixes");

	sput_fail_unless(uloop_timeout_remaining(&store.token_timer) == 10000, "Token timer pending");
	sput_fail_unless(!store.save_timer.pending, "Store timer not pending");

	fu_loop(1);
	sput_fail_unless(store.token_count == 1, "One token");
	sput_fail_unless(uloop_timeout_remaining(&store.save_timer) == 2000, "Store timer pending");

	fu_loop(1);
	sput_fail_unless(store.token_count == 0, "No token left");
	sput_fail_unless(uloop_timeout_remaining(&store.token_timer) == 8000, "Token timer pending");

	store.user.applied(&store.user, &ldp); //Update, but no token

	fu_loop(1);
	sput_fail_unless(uloop_timeout_remaining(&store.token_timer) == 10000, "Token timer pending");
	sput_fail_unless(uloop_timeout_remaining(&store.save_timer) == 2000, "Store timer pending");

	fu_loop(1);
	sput_fail_unless(store.token_count == 0, "No token left");

	store.token_count = 2;
	sput_fail_if(pa_store_set_file(&store, filepath, 3000, 11000), "Could open file");
	sput_fail_unless(store.token_count == 0, "No more tokens");

	fu_loop(3);//3 more tokens
	sput_fail_unless(store.token_count == 3, "3 tokens");

	store.user.applied(&store.user, &ldp);
	sput_fail_unless(uloop_timeout_remaining(&store.save_timer) == 3000, "Store timer pending");
	fu_loop(1); //Write to file

	store.token_count = 10;
	sput_fail_if(pa_store_set_file(&store, filepath, 2000, 10000), "Could open file");
	sput_fail_unless(store.token_count == 2, "2 tokens");

	unlink(filepath);
	pa_store_term(&store);
}

void pa_store_saveload_test()
{
	fu_init();
	struct pa_core core;
	INIT_LIST_HEAD(&core.users);

	struct pa_store store;
	pa_store_init(&store, &core, 4);
	const char *filepath = "/tmp/test_pa_core.store";
	struct pa_store_prefix *prefix;
	struct pa_store_link *link;
	unlink(filepath);

	fake_files = 1;

	test_open_ret = 1;
	test_fopen_ret = (FILE *)1;
	test_getline_n = 0;
	test_getline_max = 0;
	pa_store_set_file(&store, filepath, 1000, 1000);
	sput_fail_if(strcmp(store.filepath, filepath), "Correct file path");
	strcpy(test_getline_lines[0], "prefix link0 2001:0:0:100::/62");
	strcpy(test_getline_lines[1], "prefix link1 2001:0:0:101::/63");
	strcpy(test_getline_lines[2], "prefix link0 2001:0:0:102::/64");
	strcpy(test_getline_lines[3], "prefix link1 2001:0:0:103::/65");
	test_getline_n = 0;
	test_getline_max = 4;
	sput_fail_unless(pa_store_load(&store, filepath) == 0, "Can load virtual file");

	fake_files = 0;

	sput_fail_if(pa_store_save(&store), "Store in file");
	pa_store_term(&store); //Empty this store

	pa_store_init(&store, &core, 3);

	chmod(filepath, 0);
	sput_fail_unless(pa_store_set_file(&store, filepath, 1000, 1000), "Can't open file");

	chmod(filepath, S_IRUSR);
	sput_fail_unless(pa_store_set_file(&store, filepath, 1000, 1000), "Can't open file");

	chmod(filepath, S_IRUSR | S_IWUSR);
	sput_fail_if(pa_store_set_file(&store, filepath, 1000, 1000), "Can open file");

	sput_fail_if(strcmp(store.filepath, filepath), "Correct file path");
	sput_fail_unless(pa_store_load(&store, filepath) == 0, "Can load virtual file");
	sput_fail_unless(store.n_prefixes == 3, "3 cached entries");


	//Check values
	link = list_entry(store.links.next, struct pa_store_link, le);
	sput_fail_if(strcmp(link->name, "link1"), "Correct link name");
	sput_fail_unless(link->n_prefixes == 2,"Two cached prefix");

	prefix = list_entry(link->prefixes.next, struct pa_store_prefix, in_link);
	sput_fail_if(pa_prefix_cmp(PP(3), 65, &prefix->prefix, prefix->plen), "Correct prefix");

	prefix = list_entry(link->prefixes.prev, struct pa_store_prefix, in_link);
	sput_fail_if(pa_prefix_cmp(PP(1), 63, &prefix->prefix, prefix->plen), "Correct prefix");

	link = list_entry(store.links.prev, struct pa_store_link, le);
	sput_fail_if(strcmp(link->name, "link0"), "Correct link name");
	sput_fail_unless(link->n_prefixes == 1,"One cached prefix");

	prefix = list_entry(link->prefixes.next, struct pa_store_prefix, in_link);
	sput_fail_if(pa_prefix_cmp(PP(2), 64, &prefix->prefix, prefix->plen), "Correct prefix");

	//Load and save
	sput_fail_if(pa_store_save(&store), "Store in file");
	sput_fail_unless(pa_store_load(&store, filepath) == 0, "Can load virtual file");
	sput_fail_unless(store.n_prefixes == 3, "3 cached entries");

	//Remove file and load again
	unlink(filepath);
	sput_fail_unless(pa_store_load(&store, filepath) == -1, "Cannot load virtual file");

	//Save again
	sput_fail_if(pa_store_save(&store), "Store in file");

	unlink(filepath);
	pa_store_term(&store);
}

void pa_store_load_test()
{
	fu_init();
	struct pa_core core;
	INIT_LIST_HEAD(&core.users);

	struct pa_store store;
	pa_store_init(&store, &core, 10);

	fake_files = 1;
	const char *filepath = "/file/path";

	sput_fail_unless(pa_store_load(&store, filepath) == -1, "No specified file");

	test_open_ret = -1;
	sput_fail_unless(pa_store_set_file(&store, filepath, 1000, 1000), "Can't set file");
	sput_fail_if(store.filepath != NULL, "No file path");

	test_open_ret = 0;
	test_fopen_ret = (FILE *)1;
	test_getline_n = 0;
	test_getline_max = 0;
	sput_fail_if(pa_store_set_file(&store, filepath, 1000, 1000), "Set file OK");
	sput_fail_if(strcmp(store.filepath, filepath), "Correct file path");

	test_fopen_ret = NULL;
	sput_fail_unless(pa_store_load(&store, filepath) == -1, "Cannot load file");

	test_fopen_ret = (FILE *)1;
	test_getline_n = 0;
	test_getline_max = 0;
	sput_fail_unless(pa_store_load(&store, filepath) == 0, "Can load file");
	sput_fail_unless(!strcmp(test_fopen_path, filepath), "Correct filepath");

	strcpy(test_getline_lines[0], "#\n");
	strcpy(test_getline_lines[1], " #\n");
	strcpy(test_getline_lines[2], "\t#");
	strcpy(test_getline_lines[3], "#\n");
	test_getline_n = 0;
	test_getline_max = 4;
	sput_fail_unless(pa_store_load(&store, filepath) == 0, "Can load file");
	sput_fail_unless(test_getline_n == 4, "5 getline calls");

#define pa_store_load_line(line, ret) do { \
		test_getline_n = 0;\
		test_getline_max = 1;\
		strcpy(test_getline_lines[0], line);\
		sput_fail_unless(pa_store_load(&store, filepath) == ret, "Load single line");\
		sput_fail_unless(test_getline_n == 1, "2 getline calls"); \
	} while(0)

	pa_store_load_line("  \n", 0);
	pa_store_load_line(" #\n", 0);
	pa_store_load_line("#### #### ### ### ###\n", 0);
	pa_store_load_line(" \t\t\n", 0);
	pa_store_load_line("\t", 0);
	pa_store_load_line("", 0);

	pa_store_load_line("aa", -1);
	pa_store_load_line(" aa aa  aa\t a", -1);
	pa_store_load_line("prefix ", -1);
	pa_store_load_line("prefix nya notaprefix", -1);
	pa_store_load_line("prefix nya ::/0 tomanyargs", -1);

	struct pa_store_prefix *prefix;
	struct pa_store_link *link;
	pa_store_load_line("prefix link0 2001:0:0:100::/64", 0);
	sput_fail_unless(store.n_prefixes == 1, "Correct number of prefixes");
	prefix = list_entry(store.prefixes.next, struct pa_store_prefix, in_store);
	sput_fail_if(pa_prefix_cmp(&prefix->prefix, prefix->plen, PP(0), 64), "Correct prefix");
	link = list_entry(store.links.next, struct pa_store_link, le);
	sput_fail_if(strcmp("link0", link->name), "Link name");
	sput_fail_if(link->link, "Private link");
	sput_fail_unless(link->n_prefixes == 1, "One single prefix");
	sput_fail_unless(store.links.next->next == &store.links, "One single link");

	//Load same prefix again
	pa_store_load_line("prefix link0 2001:0:0:100::/64", 0);
	sput_fail_unless(store.n_prefixes == 1, "Correct number of prefixes");
	prefix = list_entry(store.prefixes.next, struct pa_store_prefix, in_store);
	sput_fail_if(pa_prefix_cmp(&prefix->prefix, prefix->plen, PP(0), 64), "Correct prefix");
	link = list_entry(store.links.next, struct pa_store_link, le);
	sput_fail_if(strcmp("link0", link->name), "Link name");
	sput_fail_if(link->link, "Private link");
	sput_fail_unless(link->n_prefixes == 1, "One single prefix");
	sput_fail_unless(store.links.next->next == &store.links, "One single link");

	//Uncache
	pa_store_uncache(&store, link, prefix);
	sput_fail_unless(list_empty(&store.links), "No link");

	//Load multiple prefixes
	strcpy(test_getline_lines[0], "#\n");
	strcpy(test_getline_lines[1], "prefix link0 2001:0:0:100::/64\n");
	strcpy(test_getline_lines[2], "prefix link0 2001:0:0:101::/64\n");
	strcpy(test_getline_lines[3], "prefix link1 2001:0:0:101::/64\n");
	test_getline_n = 0;
	test_getline_max = 4;
	sput_fail_unless(pa_store_load(&store, filepath) == 0, "Can load file");
	sput_fail_unless(store.n_prefixes == 3, "3 prefixes");
	prefix = list_entry(store.prefixes.next, struct pa_store_prefix, in_store);
	sput_fail_if(pa_prefix_cmp(&prefix->prefix, prefix->plen, PP(1), 64), "Correct prefix");
	link = list_entry(store.links.next, struct pa_store_link, le);
	sput_fail_if(strcmp("link1", link->name), "Link name");
	sput_fail_unless(link->n_prefixes == 1, "One prefix in link1");

	pa_store_term(&store);
	fake_files = 0;
}

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
	sput_run_test(pa_store_load_test);
	sput_run_test(pa_store_saveload_test);
	sput_run_test(pa_store_delays_test);
	sput_leave_suite(); /* optional */
	sput_finish_testing();
	return sput_get_return_value();
}
