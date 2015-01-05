/*
 * Author: Pierre Pfister <pierre pfister@darou.fr>
 *
 * Copyright (c) 2014 Cisco Systems, Inc.
 *
 */

#include "pa_store.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

struct pa_store_prefix {
	struct list_head in_store;
	struct list_head in_link;
	pa_prefix prefix;
	pa_plen plen;
};

static int pa_store_cache(struct pa_store *store, struct pa_store_link *link, pa_prefix *prefix, pa_plen plen);

static struct pa_store_link *pa_store_link_goc(struct pa_store *store, const char *name, int create)
{
	struct pa_store_link *l;
	list_for_each_entry(l, &store->links, le) {
		if(!strcmp(l->name, name)) {
			return l;
		}
	}
	if(!create || !(l = malloc(sizeof(*l))))
		return NULL;

	strcpy(l->name, name);
	INIT_LIST_HEAD(&l->prefixes);
	l->n_prefixes = 0;
	l->link = NULL;
	l->max_prefixes = 0;
	list_add(&l->le, &store->links);
	return l;
}

#define PAS_PE(test, errmsg, ...) \
		if(test) { \
			if(!err) {\
				PA_WARNING("Parsing error in file %s", store->filepath);\
				err = -1;\
			}\
			PA_WARNING(" - "errmsg" at line %d: %s", ##__VA_ARGS__, (int)linecnt, line); \
		}\
		continue;

int pa_store_load(struct pa_store *store)
{
	FILE *f;
	if(!store->filepath) {
		PA_WARNING("No specified file.");
		return -1;
	}

	if(!(f = fopen(store->filepath, "r"))) {
		PA_WARNING("Cannot open file %s (read mode) - %s", store->filepath, strerror(errno));
		return -1;
	}

	char *line = NULL;
	ssize_t read;
	size_t len;
	size_t linecnt = 0;
	int err = 0;
	while ((read = getline(&line, &len, f)) != -1) {
		linecnt++;
		char *tokens[3] = {NULL, NULL, NULL};
		int word = -1, reading = 0;
		ssize_t pos;
		for(pos = 0; pos < read; pos++) {
			switch (line[pos]) {
				case ' ':
				case '\n':
				case '\t':
				case '\0':
					if(reading) {
						line[pos] = '\0';
						reading = 0;
					}
					break;
				default:
					if(!reading) {
						word++;
						reading = 1;
						if(word < 3)
							tokens[word] = &line[pos];
					}
					break;
			}
		}

		if(!tokens[0] || tokens[0][0] == '#')
			continue;

		if(!strcmp(tokens[0], PA_STORE_PREFIX)) {
			pa_prefix px;
			pa_plen plen;
			struct pa_store_link *l;
			PAS_PE(!tokens[1] || !tokens[2], "Missing arguments");
			PAS_PE(word >= 3, "Too many arguments");
			PAS_PE(pa_prefix_fromstring(tokens[2], &px, &plen), "Invalid prefix");
			PAS_PE(strlen(tokens[1]) >= PA_STORE_NAMELEN, "Link name '%s' is too long", tokens[1]);
			PAS_PE(!(l = pa_store_link_goc(store, tokens[1], 1)), "Internal error");
			pa_store_cache(store, l, &px, plen);
		} else {
			PAS_PE(1,"Unknown type %s", tokens[0]);
		}
	}

	free(line);
	fclose(f);
	return err;
}

int pa_store_save(struct pa_store *store)
{
	FILE *f;
	if(!store->filepath) {
		PA_WARNING("No specified file.");
		return -1;
	}

	if(!(f = fopen(store->filepath, "w"))) {
		PA_WARNING("Cannot open file %s (write mode) - %s", store->filepath, strerror(errno));
		return -1;
	}

	if(fprintf(f, PA_STORE_BANNER) <= 0) {
		PA_WARNING("Error occurred while writing cache into %s: %s", store->filepath, strerror(errno));
		return -1;
	}

	struct pa_store_prefix *p;
	struct pa_store_link *link;
	char px[PA_PREFIX_STRLEN];
	int err = 0;
	list_for_each_entry_reverse(p, &store->prefixes, in_store) {
		link = list_entry(p->in_link.next, struct pa_store_link, prefixes);
		if(!strlen(link->name))
			continue;

		if(!err) {
			if(fprintf(f, PA_STORE_PREFIX" %s %s\n",
					link->name,
					pa_prefix_tostring(px, &p->prefix, p->plen)) < 0)
				err = 1;
		}
		list_move(&p->in_link, &link->prefixes);
	}
	if(err)
		PA_WARNING("Error occurred while writing cache into %s: %s", store->filepath, strerror(errno));

	fclose(f);
	return err;
}

static void pa_store_to(struct uloop_timeout *to)
{
	struct pa_store *store = container_of(to, struct pa_store, timer);
	pa_store_save(store);
}

static void pa_store_updated(struct pa_store *store)
{
	if(!store->timer.pending)
		uloop_timeout_set(&store->timer, PA_STORE_SAVE_DELAY);
}

/* Only an empty private link can be destroyed */
static void pa_store_private_link_destroy(struct pa_store_link *l)
{
	list_del(&l->le);
	free(l);
}

static void pa_store_uncache(struct pa_store *store, struct pa_store_link *l, struct pa_store_prefix *p)
{
	list_del(&p->in_link);
	l->n_prefixes--;
	list_del(&p->in_store);
	store->n_prefixes--;
	if(!l->n_prefixes && !l->link)
		pa_store_private_link_destroy(l);

	free(p);
	pa_store_updated(store);
}

#define pa_store_uncache_last_from_link(store, l) \
			pa_store_uncache(store, l, list_entry((l)->prefixes.prev, struct pa_store_prefix, in_link))

static void pa_store_uncache_last_from_store(struct pa_store *store)
{
	struct pa_store_prefix *p = list_entry((store)->prefixes.prev, struct pa_store_prefix, in_store);
	struct pa_store_link *l =  list_entry(p->in_link.next, struct pa_store_link, prefixes);
	pa_store_uncache(store, l, p);
}

static int pa_store_cache(struct pa_store *store, struct pa_store_link *link, pa_prefix *prefix, pa_plen plen)
{
	PA_DEBUG("Caching %s %s", link->name, pa_prefix_repr(prefix, plen));
	struct pa_store_prefix *p;
	list_for_each_entry(p, &link->prefixes, in_link) {
		if(pa_prefix_equals(prefix, plen, &p->prefix, p->plen)) {
			//Put existing prefix at head
			list_move(&p->in_store, &store->prefixes);
			list_move(&p->in_link, &link->prefixes);
			pa_store_updated(store);
			return 0;
		}
	}
	if(!(p = malloc(sizeof(*p))))
		return -1;
	//Add the new prefix
	pa_prefix_cpy(prefix, plen, &p->prefix, p->plen);
	list_add(&p->in_link, &link->prefixes);
	link->n_prefixes++;
	list_add(&p->in_store, &store->prefixes);
	store->n_prefixes++;

	//If too many prefixes in the link, remove the last one
	if(link->max_prefixes && link->n_prefixes > link->max_prefixes)
		pa_store_uncache_last_from_link(store, link);

	//If too many prefixes in storage, remove the last one
	if(store->max_prefixes && store->n_prefixes > store->max_prefixes)
		pa_store_uncache_last_from_store(store);

	pa_store_updated(store);
	return 0;
}

static void pa_store_applied_cb(struct pa_user *user, struct pa_ldp *ldp)
{
	struct pa_store *store = container_of(user, struct pa_store, user);
	if(!ldp->applied)
		return;

	struct pa_store_link *link;
	list_for_each_entry(link, &store->links, le) {
		if(link->link == ldp->link) {
			pa_store_cache(store, link, &ldp->prefix, ldp->plen);
			return;
		}
	}
}

void pa_store_link_add(struct pa_store *store, struct pa_store_link *link)
{
	struct pa_store_link *l;
	INIT_LIST_HEAD(&link->prefixes);
	link->n_prefixes = 0;
	if((l = pa_store_link_goc(store, link->name, 0))) {
		list_splice(&l->prefixes, &link->prefixes);
		link->n_prefixes = l->n_prefixes;
		if(!l->link)
			pa_store_private_link_destroy(l);

		if(link->max_prefixes)
			while(link->n_prefixes > link->max_prefixes)
				pa_store_uncache_last_from_link(store, link);
	}
	list_add(&link->le, &store->links);
	return;
}

void pa_store_link_remove(struct pa_store *store, struct pa_store_link *link)
{
	struct pa_store_link *l;
	list_del(&link->le);
	if(link->n_prefixes && (l = pa_store_link_goc(store, link->name, 1))) {
		list_splice(&link->prefixes, &l->prefixes); //Save prefixes in a private list
		l->n_prefixes = link->n_prefixes;

		if(l->max_prefixes)
			while(l->n_prefixes > l->max_prefixes)
				pa_store_uncache_last_from_link(store, l);
	}
	return;
}

void pa_store_term(struct pa_store *store)
{
	struct pa_store_prefix *p, *p2;
	list_for_each_entry_safe(p, p2, &store->prefixes, in_store) {
		free(p);
	}

	struct pa_store_link *l, *l2;
	list_for_each_entry_safe(l, l2, &store->links, le) {
		if(!l->link)
			free(l);
	}

	pa_user_unregister(&store->user);
	uloop_timeout_cancel(&store->timer);
}

int pa_store_set_file(struct pa_store *store, const char *filepath)
{
	int fd;
	if((fd = open(filepath, O_RDWR | O_CREAT, 0x00664)) == -1) {
		PA_WARNING("Could not open file (Or incorrect authorizations) %s: %s", filepath, strerror(errno));
		return -1;
	}
	store->filepath = filepath;
	return 0;
}

void pa_store_init(struct pa_store *store, struct pa_core *core, uint32_t max_prefixes)
{
	store->core = core;
	store->max_prefixes = max_prefixes;
	INIT_LIST_HEAD(&store->links);
	INIT_LIST_HEAD(&store->prefixes);
	store->user.applied = pa_store_applied_cb;
	store->user.assigned = NULL;
	store->user.published = NULL;
	store->filepath = NULL;
	store->n_prefixes = 0;
	store->timer.pending = 0;
	store->timer.cb = pa_store_to;
	pa_user_register(core, &store->user);
}
