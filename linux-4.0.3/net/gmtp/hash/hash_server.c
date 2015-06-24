/*
 * hash.c
 *
 *  Created on: 27/02/2015
 *      Author: mario
 */

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/string.h>

#include "hash.h"

struct gmtp_server_entry *gmtp_lookup_route(
		struct gmtp_hashtable *hashtable, const __u8 *relayid)
{
	struct gmtp_server_entry *entry;
	unsigned int hashval;

	hashval = gmtp_hash(hashtable, relayid);

	/* Error */
	if(hashval < 0)
		return NULL;

	entry = hashtable->server_table[hashval];
	for(; entry != NULL; entry = entry->next)
		if(!memcmp(relayid, entry->srelay->relay_id, GMTP_RELAY_ID_LEN))
			return entry;
	return NULL;
}
EXPORT_SYMBOL_GPL(gmtp_lookup_route);

int gmtp_add_server_entry(struct gmtp_hashtable *hashtable, __u8 *relayid,
		__u8 *flowname, struct gmtp_hdr_route *route)
{
	struct gmtp_server_entry *new_entry;
	struct gmtp_server_entry *cur_entry;
	unsigned int hashval;
	__u8 nrelays = route->nrelays;

	gmtp_print_function();

	hashval = gmtp_hash(hashtable, relayid);

	/* Error */
	if(hashval < 0)
		return hashval;

	new_entry = kmalloc(sizeof(struct gmtp_server_entry), GFP_KERNEL);
	if(new_entry == NULL)
		return 1;

	/* TODO Relay already registered */
	cur_entry = gmtp_lookup_route(hashtable, relayid);
	if(cur_entry != NULL)
		return 2;

	memcpy(new_entry->flowname, flowname, GMTP_FLOWNAME_LEN);
	memcpy(&new_entry->route, route, sizeof(*route));
	new_entry->srelay = &new_entry->route.relay_list[nrelays];

	new_entry->next = hashtable->server_table[hashval];
	hashtable->server_table[hashval] = new_entry;

	return 0;
}
EXPORT_SYMBOL_GPL(gmtp_add_server_entry);
