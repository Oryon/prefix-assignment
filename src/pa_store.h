/*
 * Author: Pierre Pfister <pierre pfister@darou.fr>
 *
 * Copyright (c) 2014 Cisco Systems, Inc.
 *
 * Prefix storing and caching module for the prefix assignment
 * algorithm.
 *
 * The API provides global and per-link prefix storage limit
 * configuration. The number of cached prefixes must be greater than
 * the number of stored prefixes. Otherwise, prefixes may be forgotten.
 */

#ifndef PA_STORE_H_
#define PA_STORE_H_

#include <libubox/avl.h>
#include <libubox/uloop.h>
#include <string.h>

#include "pa_core.h"

/* Maximum length of the link name, identifying the link
 * in the stable storage file.
 * DHCPv6 DUID is at most 20bytes (40 hex characters)
 * On Linux IFNAMESIZ is 16 characters
 */
#define PA_STORE_NAMELEN 50

/* Prefix parsing function used by pa_store.
 * A written or read prefix must not include
 * any space ' ', tab '\t' , newline '\n' or '#' characters.
 * Function prototype is:
 *    int pa_prefix_fromstring(const char *src, pa_prefix *addr, pa_plen *plen)
 * The prefix must fit in a PA_PREFIX_STRLEN
 * long buffer (null character included).
 *    (Mandatory when pa_store is enabled)
 */
#define pa_prefix_fromstring(buff, p, plen) \
		prefix_pton(buff, p, plen)

/* Each stored object has a type */
#define PA_STORE_PREFIX "prefix"
#define PA_STORE_ADDR   "address"
#define PA_STORE_DELAY  "delay"
#define PA_STORE_ULA    "ula"

#define PA_STORE_BANNER \
	"# Prefix Assignment Algorithm Storage Module File.\n"\
	"# This file was generated automatically.\n"\
	"# Do not modify unless you know what you are doing.\n"\
	"# Do not modify while the process is running as\n"\
	"# modifications could be overridden.\n\n"\

#define PA_STORE_SAVE_DELAY 10000

struct pa_store {
	struct list_head links;    /* Tree containing pa_store Links */
	struct pa_core *core;     /* The PA core the module is operating on. */
	struct pa_user user;      /* PA user used to receive apply notifications. */
	const char *filepath;         /* Path of the file to be used for storage. */
	struct list_head prefixes;/* All cached prefixes */
	uint32_t max_prefixes;    /* Maximum number of remembered prefixes. */
	uint32_t n_prefixes;      /* Number of cached prefixes. */
	struct uloop_timeout timer; /* Delay cache write into the disk. */
};

/* Structure representing a given link used by pa_store.
 * It is provided by the user, or created by pa_store when
 * encountering unknown links while reading the file.
 */
struct pa_store_link {
	/* The associated Link. */
	struct pa_link *link;

	/* A name without space, to be used in the storage.
	 * When no name is specified, prefixes are cached
	 * but not stored. */
	char name[PA_STORE_NAMELEN];

	/* Maximum number of remembered prefixes for this Link. */
	uint32_t max_prefixes;

	/* PRIVATE to pa_store */
	struct list_head le;      /* Linked in pa_store. */
	struct list_head prefixes;/* List of pa_store entries. */
	uint32_t n_prefixes;       /* Number of entries currently stored for this Link. */
};

/* Initializes the pa storage structure.
 * pa_core must be initialized.
 */
void pa_store_init(struct pa_store *, struct pa_core *, uint32_t max_prefixes);

/* Sets the file.
 * Read and write rights are checked.
 * Returns -1 in case of error and errno is set.
 * Returns 0 otherwise.
 */
int pa_store_set_file(struct pa_store *, const char *filepath);

/* Loads the file into the cache. The content is considered
 * more recent than the cached information.
 * Returns -1 in case of error. 0 otherwise. */
int pa_store_load(struct pa_store *);

/* Manually trigger cache saving into the file.
 * Returns -1 in case of error. 0 otherwise. */
int pa_store_save(struct pa_store *);

/* Free all memory and cache entries.
 * But do not flush that state to the file. */
void pa_store_term(struct pa_store *);

#define pa_store_link_init(store_link, pa_link, linkname, max_px) do { \
		(store_link)->link = pa_link; \
		(store_link)->max_prefixes = max_px; \
		strcpy((store_link)->name, linkname); \
	} while(0)

void pa_store_link_add(struct pa_store *, struct pa_store_link *);
void pa_store_link_remove(struct pa_store *, struct pa_store_link *);

/* Rule to be used in pa_core which will propose
 * cached prefixes from more recent to less recent
 * when a new prefix can be created for a given link.
 */
struct pa_store_rule {
	struct pa_rule rule;
	struct pa_store *store;
	pa_rule_priority rule_priority;
	pa_priority priority;
};

void pa_store_rule_init(struct pa_store_rule *rule);




#endif /* PA_STORE_H_ */
