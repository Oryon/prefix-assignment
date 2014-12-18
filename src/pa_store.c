/*
 * Author: Pierre Pfister <pierre pfister@darou.fr>
 *
 * Copyright (c) 2014 Cisco Systems, Inc.
 *
 */

#include "pa_store.h"

#include <string.h>

struct pa_store_prefix {
	struct list_head in_store;
	struct list_head in_link;
	pa_prefix prefix;
	pa_plen plen;
};

static void pa_store_updated(struct pa_store *store)
{

}

/* Only an empty private link can be destroyed */
static void pa_store_private_link_destroy(struct pa_store_link *l)
{
	list_del(&l->le);
	free(l);
}

static struct pa_store_link *pa_store_private_link_create(struct pa_store *store, const char *name)
{
	struct pa_store_link *l;
	if(!(l = malloc(sizeof(*l))))
		return NULL;

	strcpy(l->name, name);
	INIT_LIST_HEAD(&l->prefixes);
	l->n_prefixes = 0;
	l->link = NULL;
	l->max_prefixes = 0;
	list_add(&store->links, &l->le);
	return l;
}

/* Warning: Only the last prefix stored in a link can be uncached. */
static void pa_store_uncache(struct pa_store *store, struct pa_store_prefix *p)
{
	struct pa_store_link *l = list_entry(&p->in_link.next, struct pa_store_link, prefixes);
	list_del(&p->in_link);
	l->n_prefixes--;
	list_del(&p->in_store);
	store->n_prefixes--;
	if(!l->n_prefixes && !l->link)
		pa_store_private_link_destroy(l);

	free(p);
	pa_store_updated(store);
}

static int pa_store_cache(struct pa_store *store, struct pa_store_link *link, pa_prefix *prefix, pa_plen plen)
{
	struct pa_store_prefix *p;
	list_for_each_entry(p, &link->prefixes, in_link) {
		if(pa_prefix_equals(prefix, plen, &p->prefix, p->plen)) {
			//Put existing prefix at head
			list_move(&p->in_store, &store->prefixes);
			list_move(&p->in_store, &store->prefixes);
			pa_store_updated(store);
			return 0;
		}
	}
	if(!(p = malloc(sizeof(*p))))
		return -1;

	//Add the new prefix
	pa_prefix_cpy(&prefix, plen, &p->prefix, p->plen);
	list_add(&p->in_link, &link->prefixes);
	link->n_prefixes++;
	list_add(&p->in_store, &link->prefixes);
	store->n_prefixes++;

	//If too many prefixes in the link, remove the last one
	if(link->max_prefixes && link->n_prefixes > link->max_prefixes) {
		p = list_entry(&link->prefixes.prev, struct pa_store_prefix, in_link);
		pa_store_uncache(store, p);
	}

	//If too many prefixes in storage, remove the last one
	if(store->max_prefixes && store->n_prefixes > store->max_prefixes) {
		p = list_entry(&store->prefixes.prev, struct pa_store_prefix, in_store);
		pa_store_uncache(store, p);
	}

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

int pa_store_set_file(struct pa_store *store, const char *filepath)
{
	store->filepath = filepath;
	//TODO: read the file
	return 0;
}

void pa_store_link_add(struct pa_store *store, struct pa_store_link *link)
{
	struct pa_store_link *l;
	INIT_LIST_HEAD(&link->prefixes);
	link->n_prefixes = 0;
	list_for_each_entry(l, &store->links, le) {
		if(!strcmp(l->name, link->name)) {
			list_splice(&l->prefixes, &link->prefixes);
			link->n_prefixes = l->n_prefixes;
			pa_store_private_link_destroy(l);
			break;
		}
	}
	list_add(&link->le, &store->links);
	return;
}

void pa_store_link_remove(struct pa_store *store, struct pa_store_link *link)
{
	struct pa_store_link *l;
	list_del(&link->le);
	if(link->n_prefixes && (l = pa_store_private_link_create(store, link->name))) {
		list_splice(&link->prefixes, &l->prefixes); //Save prefixes in a private list
		l->n_prefixes = link->n_prefixes;
	}
	return;
}

void pa_store_init(struct pa_store *store, struct pa_core *core, uint32_t max_prefixes)
{
	store->core = core;
	store->max_prefixes = max_prefixes;
	INIT_LIST_HEAD(&store->links);
	INIT_LIST_HEAD(&store->prefixes);
	store->user.applied = pa_store_applied_cb;
	store->filepath = NULL;
	pa_user_register(core, &store->user);
}
