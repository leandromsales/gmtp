/*
 * hash.c
 *
 *  Created on: 27/02/2015
 *      Author: mario
 */

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/string.h>

#include "../gmtp.h"
#include "hash.h"

struct gmtp_hashtable* server_hashtable;
EXPORT_SYMBOL_GPL(server_hashtable);

void gmtp_del_relay_hash_entry(struct gmtp_hashtable *table, const __u8 *key);

const struct gmtp_hash_ops gmtp_relay_hash_ops = {
		.hash = gmtp_hash,
		.lookup = gmtp_lookup_entry,
		.add_entry = gmtp_add_entry,
		.del_entry = gmtp_del_relay_hash_entry,
		.destroy = destroy_gmtp_hashtable,
};

static struct gmtp_relay_entry *gmtp_build_relay_entry(
		const struct gmtp_relay *relay)
{
	struct gmtp_relay_entry *entry;
	entry = kmalloc(sizeof(struct gmtp_relay_entry), GFP_KERNEL);
	if(entry == NULL)
		return NULL;

	entry->nextRelay = NULL;
	entry->prevRelayList = NULL;

	memcpy(entry->entry.key, relay->relay_id, GMTP_HASH_KEY_LEN);
	memcpy(&entry->relay, relay, sizeof(struct gmtp_relay));

	return entry;
}

int gmtp_add_relay_entries(struct gmtp_hashtable *rtable,
		struct gmtp_hdr_route *route)
{
	int i, err = 0;
	struct gmtp_relay_entry *nextRelay = NULL;
	struct gmtp_relay_entry *prevRelay = NULL;

	gmtp_pr_func();

	for(i = route->nrelays - 1; (i >= 0) && (!err); --i) {
		struct gmtp_relay_entry *entry;
		
		entry = rtable->hash_ops.lookup(rtable,
				route->relay_list[i].relay_id);

		if(entry == NULL) {
			entry = gmtp_build_relay_entry(&route->relay_list[i]);
			if(entry == NULL)
				return 1;
		}

		entry->nextRelay = nextRelay;
		if(entry->nextRelay != NULL) {
			INIT_LIST_HEAD(&entry->nextRelay->prevRelayList->list);
			list_add_tail(&entry->nextRelay->prevRelayList->list,
					entry);
		}
		nextRelay = entry;

		err = rtable->hash_ops.add_entry(rtable,
				(struct gmtp_hash_entry*)entry);

		print_gmtp_relay(&entry->relay);
		pr_info("entry->nextRelay: %p\n", entry->nextRelay);
		pr_info("entry->prevRelayList: %p\n", entry->nextRelay);
	}

	return err;
}

int gmtp_add_server_entry(struct gmtp_hashtable *table, const __u8 *flowname,
		struct gmtp_hdr_route *route)
{
	struct gmtp_server_entry *entry;

	gmtp_pr_func();

	entry = table->hash_ops.lookup(table, flowname);

	if(entry == NULL) {

		entry = kmalloc(sizeof(struct gmtp_server_entry), GFP_KERNEL);
		if(entry == NULL)
			return 1;

		memcpy(entry->entry.key, flowname, GMTP_HASH_KEY_LEN);
		entry->relay_hashtable = gmtp_build_hashtable(U8_MAX,
				gmtp_relay_hash_ops);
	}

	if(gmtp_add_relay_entries(entry->relay_hashtable, route))
		return 1;

	return table->hash_ops.add_entry(table, (struct gmtp_hash_entry*)entry);
}
EXPORT_SYMBOL_GPL(gmtp_add_server_entry);

void gmtp_del_relay_list(struct gmtp_relay_entry *entry)
{
	struct gmtp_relay_entry *relay, *temp;
	gmtp_pr_func();
	list_for_each_entry_safe(relay, temp, &entry->list, list)
	{
		list_del(&relay->list);
	}
}

void gmtp_del_relay_hash_entry(struct gmtp_hashtable *table, const __u8 *key)
{
	struct gmtp_relay_entry *entry;
	int hashval;

	gmtp_print_function();

	hashval = table->hash_ops.hash(table, key);
	if(hashval < 0)
		return;

	entry = (struct gmtp_relay_entry*) table->entry[hashval];
	if(entry != NULL) {
		gmtp_del_relay_list(entry);
		kfree(entry);
	}
}

void gmtp_del_server_hash_entry(struct gmtp_hashtable *table, const __u8 *key)
{
	struct gmtp_server_entry *entry;
	int hashval;

	gmtp_print_function();

	hashval = table->hash_ops.hash(table, key);
	if(hashval < 0)
		return;

	entry = (struct gmtp_server_entry*) table->entry[hashval];
	if(entry != NULL) {
		/*entry->relay_hashtable->hash_ops.destroy(entry->relay_hashtable);*/
		kfree(entry);
	}
}

const struct gmtp_hash_ops gmtp_server_hash_ops = {
		.hash = gmtp_hash,
		.lookup = gmtp_lookup_entry,
		.add_entry = gmtp_add_entry,
		.del_entry = gmtp_del_server_hash_entry,
		.destroy = destroy_gmtp_hashtable,
};
