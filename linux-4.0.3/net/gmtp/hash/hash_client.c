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

struct gmtp_client_entry *gmtp_lookup_client(
		struct gmtp_hashtable *hashtable, const __u8 *flowname)
{
	struct gmtp_client_entry *entry;
	unsigned int hashval;

	hashval = gmtp_hash(hashtable, flowname);

	/* Error */
	if(hashval < 0)
		return NULL;

	entry = hashtable->client_table[hashval];
	for(; entry != NULL; entry = entry->next)
		if(memcmp(flowname, entry->flowname, GMTP_FLOWNAME_LEN) == 0)
			return entry;
	return NULL;
}
EXPORT_SYMBOL_GPL(gmtp_lookup_client);

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

	new_entry = kmalloc(sizeof(struct gmtp_client_entry), GFP_ATOMIC);
	if(new_entry == NULL)
		return 1;

	cur_entry = gmtp_lookup_client(hashtable, flowname);
	if(cur_entry != NULL)
		return 2; /* TODO Media already being transmitted by other
								server? */

	memcpy(new_entry->flowname, flowname, GMTP_FLOWNAME_LEN);

	new_entry->clients = kmalloc(sizeof(struct gmtp_client), GFP_ATOMIC);
	if(new_entry->clients == NULL)
		return 3;
	INIT_LIST_HEAD(&new_entry->clients->list);

	new_entry->channel_addr = channel_addr;
	new_entry->channel_port = channel_port;

	new_entry->next = hashtable->client_table[hashval];
	hashtable->client_table[hashval] = new_entry;

	return 0;
}
EXPORT_SYMBOL_GPL(gmtp_add_client_entry);

void gmtp_del_client_list(struct gmtp_client_entry *entry)
{
	struct gmtp_client *client, *temp;
	gmtp_pr_func();
	list_for_each_entry_safe(client, temp, &(entry->clients->list), list)
	{
		list_del(&client->list);
		kfree(client);
	}
}
EXPORT_SYMBOL_GPL(gmtp_del_client_list);

void gmtp_del_client_entry(struct gmtp_hashtable *hashtable, __u8 *media)
{
	struct gmtp_client_entry *entry;
	int hashval;

	gmtp_print_function();

	hashval = gmtp_hash(hashtable, media);

	if(hashval < 0)
		return;

	entry = hashtable->client_table[hashval];
	if(entry != NULL) {
		gmtp_del_client_list(entry);
		kfree(entry);
	}
}
EXPORT_SYMBOL_GPL(gmtp_del_client_entry);
