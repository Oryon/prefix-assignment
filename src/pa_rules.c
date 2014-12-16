/*
 * Author: Pierre Pfister <pierre pfister@darou.fr>
 *
 * Copyright (c) 2014 Cisco Systems, Inc.
 */

#include "pa_rules.h"

#include <libubox/md5.h>
#include <string.h>

#include "bitops.h"

#ifndef __unused
#define __unused __attribute__ ((unused))
#endif

int pa_filters_or(struct pa_rule *rule, struct pa_ldp *ldp, struct pa_filter *filter)
{
	struct pa_filters *fs = container_of(filter, struct pa_filters, filter);
	list_for_each_entry(filter, &fs->filters, le) {
		if(filter->accept(rule, ldp, filter))
			return 1;
	}
	return 0;
}

int pa_filters_and(struct pa_rule *rule, struct pa_ldp *ldp, struct pa_filter *filter)
{
	struct pa_filters *fs = container_of(filter, struct pa_filters, filter);
	list_for_each_entry(filter, &fs->filters, le) {
		if(!filter->accept(rule, ldp, filter))
			return 0;
	}
	return 1;
}

void pa_filters_init(struct pa_filters *fs, pa_filter_f accept)
{
	fs->filter.accept = accept;
	INIT_LIST_HEAD(&fs->filters);
}

static int pa_filter_basic_accept(__unused struct pa_rule *rule, struct pa_ldp *ldp, struct pa_filter *filter)
{
	struct pa_filter_basic *fb = container_of(filter, struct pa_filter_basic, filter);
	if(fb->link && fb->link != ldp->link)
		return 0;
	if(fb->dp && fb->dp != ldp->dp)
		return 0;
	return 1;
}

void pa_filter_basic_init(struct pa_filter_basic *filter, struct pa_link *link, struct pa_dp *dp)
{
	filter->filter.accept = pa_filter_basic_accept;
	filter->link = link;
	filter->dp = dp;
}

/***** Prefix selection utilities *****/

void pa_rule_prefix_nth(pa_prefix *dst, pa_prefix *container, pa_plen container_len, uint32_t n, pa_plen plen)
{
	uint32_t i = htonl(n);
	pa_plen pp;
	pa_prefix_cpy(container, container_len, dst, pp);
	bmemcpy_shift(dst, container_len, &i, 0, plen - container_len);
}

void pa_rule_prefix_count(struct pa_ldp *ldp, uint16_t *count, pa_plen max_plen) {
	pa_prefix p;
	pa_plen plen;
	struct btrie *n;

	for(plen = 0; plen <= max_plen; plen++)
		count[plen] = 0;

	btrie_for_each_available(&ldp->core->prefixes, n, (btrie_key_t *)&p, (btrie_plen_t *)&plen, (btrie_key_t *)&ldp->dp->prefix, ldp->dp->plen) {
		if(count[plen] != UINT16_MAX)
			count[plen]++;
	}
}

/* Computes the candidate subset. */
uint32_t pa_rule_candidate_subset( //Returns the number of found prefixes
		const uint16_t *count,    //The prefix count returned by pa_rule_prefix_count
		pa_plen desired_plen,     //The desired prefix length
		uint32_t desired_set_size,//Number of desired prefixes in the set
		pa_plen *min_plen,        //The minimal prefix length of containing available prefixes
		uint32_t *overflow_n      //Number of prefixes in set and included in an available prefix of length overflow_plen
		                          //When overflow_n == 0, all prefixes of length desired_plen
								  //included in available prefixes of length >= min_plen are candidate prefixes.
		)
{
	pa_plen plen = desired_plen;
	uint64_t c = 0;
	*overflow_n = 0;
	do {
		if(count[plen]) {
			*min_plen = plen;
			if(desired_plen - plen >= 32 || ((c + count[plen] * ((uint64_t)(1 << (desired_plen - plen)))) > desired_set_size)) {
				*overflow_n = desired_set_size - c; //Number of prefixes contained in this prefix length to reach the desired size
				return desired_set_size;
			}
			c += count[plen] * ((uint64_t)(1 << (desired_plen - plen)));
		}
	} while(plen--); //plen-- returns plen value before decrement

	return (uint32_t)c;
}

/* Returns the nth (starting from 0) candidate prefix of given length,
 * included in an available prefix of length > min_plen and < max_plen */
int pa_rule_candidate_pick(struct pa_ldp *ldp, uint32_t n, pa_prefix *p, pa_plen plen, pa_plen min_plen, pa_plen max_plen)
{
	struct btrie *node;
	pa_plen i;
	pa_prefix iter;
	btrie_for_each_available(&ldp->core->prefixes, node, (btrie_key_t *)&iter, (btrie_plen_t *)&i,
			(btrie_key_t *)&ldp->dp->prefix, ldp->dp->plen) {
		if(i >= min_plen && i <= max_plen) {
			if((plen - i >= 32) || (n < (((uint32_t)1) << (plen - i)))) {
				//The nth prefix is in this available prefix
				pa_rule_prefix_nth(p, &iter, i, n, plen);
				return 0;
			}
			n -= 1 << (plen - i);
		}
	}
	return -1;
}

void pa_rule_prefix_prandom(const uint8_t *seed, size_t seedlen, uint32_t ctr,
		const pa_prefix *container_prefix, pa_plen container_len,
		pa_prefix *dst, pa_plen plen)
{
	uint32_t hash[4];
	md5_ctx_t ctx;

	uint32_t ctr2 = 0;
	uint8_t *buff = (uint8_t *)dst;
	uint32_t bytelen = (((uint32_t)plen) + 7)/8 - 1;
	while(bytelen) {
		uint8_t write = bytelen>16?16:bytelen;
		md5_begin(&ctx);
		md5_hash(seed, seedlen, &ctx);
		md5_hash(&ctr,  sizeof(ctr), &ctx);
		md5_hash(&ctr2, sizeof(ctr), &ctx);
		md5_end(hash, &ctx);
		memcpy(buff, hash, write);
		buff += 16;
		bytelen -= write;
		ctr2++;
	}

	bmemcpy(dst, container_prefix, 0, container_len);
}

/***** Adopt rule ****/

pa_rule_priority pa_rule_adopt_get_max_priority(struct pa_rule *rule, struct pa_ldp *ldp)
{

	if(ldp->valid && !ldp->best_assignment && !ldp->published)
		return rule->max_priority;
	return 0;
}

enum pa_rule_target pa_rule_adopt_match(struct pa_rule *rule, __unused struct pa_ldp *ldp,
			__unused pa_rule_priority best_match_priority,
			struct pa_rule_arg *pa_arg)
{
	struct pa_rule_adopt *rule_a = container_of(rule, struct pa_rule_adopt, rule);
	//No need to check the best_match_priority because the rule uses a unique rule priority
	pa_arg->rule_priority = rule_a->rule_priority;
	pa_arg->priority = rule_a->priority;
	return PA_RULE_ADOPT;
}


/**** Random rule ****/

pa_rule_priority pa_rule_random_get_max_priority(struct pa_rule *rule, struct pa_ldp *ldp)
{
	struct pa_rule_random *rule_r = container_of(rule, struct pa_rule_random, rule);
	if(!ldp->best_assignment && (!ldp->valid || !ldp->published))
		return rule_r->rule_priority;
	return 0;
}

enum pa_rule_target pa_rule_random_match(struct pa_rule *rule, struct pa_ldp *ldp,
			__unused pa_rule_priority best_match_priority, struct pa_rule_arg *pa_arg)
{
	struct pa_rule_random *rule_r = container_of(rule, struct pa_rule_random, rule);
	pa_prefix tentative;

	//No need to check the best_match_priority because the rule uses a unique rule priority
	if(!ldp->backoff)
		return PA_RULE_BACKOFF; //Start or continue backoff timer.

	uint16_t prefix_count[rule_r->desired_plen];
	pa_rule_prefix_count(ldp, prefix_count, rule_r->desired_plen);

	uint32_t found;
	pa_plen min_plen;
	uint32_t overflow_n;
	found = pa_rule_candidate_subset(prefix_count, rule_r->desired_plen, rule_r->random_set_size, &min_plen, &overflow_n);

	if(!found) //No more available prefixes
		return PA_RULE_NO_MATCH;

	if(rule_r->pseudo_random_tentatives) {
		pa_prefix overflow_prefix;
		if(overflow_n)
			pa_rule_candidate_pick(ldp, overflow_n, &overflow_prefix, rule_r->desired_plen, min_plen, min_plen);

		/* Make pseudo-random tentatives. */
		struct btrie *n0, *n;
		btrie_plen_t l0;
		pa_prefix iter_p;
		pa_plen iter_plen;
		uint16_t i;
		for(i=0; i<rule_r->pseudo_random_tentatives; i++) {
			pa_rule_prefix_prandom(rule_r->pseudo_random_seed, rule_r->pseudo_random_seedlen, i, &ldp->dp->prefix, ldp->dp->plen, &tentative, rule_r->desired_plen);
			btrie_for_each_available_loop_stop(&ldp->core->prefixes, n, n0, l0, (btrie_key_t *)&iter_p, &iter_plen, \
					(btrie_key_t *)&tentative, ldp->dp->plen, rule_r->desired_plen)
			{
				if(iter_plen > rule_r->desired_plen || //First available prefix is too small
						!pa_prefix_contains(&iter_p, iter_plen, &tentative) || //First available prefix does not contain the tentative prefix
						iter_plen < min_plen || //Not in the candidate prefix set
						(overflow_n && iter_plen == min_plen && //Minimal length and greater than the overflow prefix
								(bmemcmp(&tentative, &overflow_prefix, rule_r->desired_plen) >= 0))) {
					//Prefix is not in the candidate prefix set
					break;
				}
				goto choose;
			}
		}
	}

	/* Select a random prefix */
	uint32_t id = pa_rand() % found;
	pa_rule_candidate_pick(ldp, id, &tentative, rule_r->desired_plen, min_plen, rule_r->desired_plen);

choose:
	pa_prefix_cpy(&tentative, rule_r->desired_plen, &pa_arg->prefix, pa_arg->plen);
	pa_arg->priority = rule_r->priority;
	pa_arg->rule_priority = rule_r->rule_priority;
	return PA_RULE_PUBLISH;
}
