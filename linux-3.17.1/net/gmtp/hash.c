/*
 * hash.c
 *
 *  Created on: 27/02/2015
 *      Author: mario
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/string.h>
#include "gmtp.h"

struct gmtp_hashtable *gmtp_create_hashtable(unsigned int size)
{
	int i;
	struct gmtp_hashtable *nt;

	gmtp_print_function();
	gmtp_print_debug("Size of gmtp_hashtable: %d", size);

	if(size < 1)
		return NULL;

	nt = kmalloc(sizeof(struct gmtp_hashtable), GFP_KERNEL);
	if(nt == NULL)
		return NULL;

	nt->table = kmalloc(sizeof(union gmtp_entry) * size, GFP_KERNEL);
	if(nt->table == NULL)
		return NULL;

	for(i = 0; i < size; ++i)
		nt->table[i].client_entry = NULL; /* serve_entry <- null */

	nt->size = size;
	return nt;
}

unsigned int gmtp_hash(struct gmtp_hashtable *hashtable, const __u8 *flowname)
{
	unsigned int hashval;
	int i;

	if(hashtable == NULL)
		return -EINVAL;

	if(flowname == NULL)
		return -ENOKEY;

	hashval = 0;
	for(i=0; i<GMTP_FLOWNAME_LEN; ++i)
		hashval = flowname[i] + (hashval << 5) - hashval;

	return hashval % hashtable->size;
}

struct gmtp_client_entry *gmtp_lookup_media(
		struct gmtp_hashtable *hashtable, const __u8 *media)
{
	struct gmtp_client_entry *entry;
	unsigned int hashval;

	hashval = gmtp_hash(hashtable, media);

	/* Error */
	if(hashval < 0)
		return NULL;

	entry = hashtable->table[hashval].client_entry;
	for(; entry != NULL; entry = entry->next)
		if(memcmp(media, entry->flowname, GMTP_FLOWNAME_LEN) == 0)
			return entry;
	return NULL;
}
EXPORT_SYMBOL_GPL(gmtp_lookup_media);

int gmtp_add_client_entry(struct gmtp_hashtable *hashtable, __u8 *flowname,
		__be32 local_addr, __be16 local_port,
		__be32 channel_addr, __be16 channel_port)
{
	struct gmtp_client_entry *new_entry;
	struct gmtp_client_entry *cur_entry;
	unsigned int hashval;

	gmtp_print_function();

	/** Primary key at client hashtable is flowname */
	hashval = gmtp_hash(hashtable, flowname);

	/* Error */
	if(hashval < 0)
		return hashval;

	new_entry = kmalloc(sizeof(struct gmtp_client_entry), GFP_KERNEL);
	if(new_entry == NULL)
		return 1;

	cur_entry = gmtp_lookup_media(hashtable, flowname);
	if(cur_entry != NULL)
		return 2; /* TODO Media already being transmitted by other
								server? */

	memcpy(new_entry->flowname, flowname, GMTP_FLOWNAME_LEN);
	new_entry->local_addr = local_addr;
	new_entry->local_port = local_port;
	new_entry->channel_addr = channel_addr;
	new_entry->channel_port = channel_port;

	new_entry->next = hashtable->table[hashval].client_entry;
	hashtable->table[hashval].client_entry = new_entry;

	return 0;
}
EXPORT_SYMBOL_GPL(gmtp_add_client_entry);

void gmtp_del_client_entry(struct gmtp_hashtable *hashtable, __u8 *media)
{
	struct gmtp_client_entry *entry;
	int hashval;

	gmtp_print_function();

	hashval = gmtp_hash(hashtable, media);

	if(hashval < 0)
		return;

	entry = hashtable->table[hashval].client_entry;
	if(entry != NULL)
		kfree(entry);
}

struct gmtp_server_entry *gmtp_lookup_route(
		struct gmtp_hashtable *hashtable, const __u8 *relayid)
{
	struct gmtp_server_entry *entry;
	unsigned int hashval;

	hashval = gmtp_hash(hashtable, relayid);

	/* Error */
	if(hashval < 0)
		return NULL;

	entry = hashtable->table[hashval].server_entry;
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

	new_entry->next = hashtable->table[hashval].server_entry;
	hashtable->table[hashval].server_entry = new_entry;

	return 0;
}
EXPORT_SYMBOL_GPL(gmtp_add_server_entry);

void kfree_gmtp_hashtable(struct gmtp_hashtable *hashtable)
{
	int i;
	struct gmtp_client_entry *entry, *tmp;

	gmtp_print_function();

	if(hashtable == NULL)
		return;

	for(i = 0; i < hashtable->size; ++i) {
		entry = hashtable->table[i].client_entry;
		while(entry != NULL) {
			tmp = entry;
			entry = entry->next;
			kfree(tmp);
		}
	}

	kfree(hashtable->table);
	kfree(hashtable);
}
