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

int gmtp_add_client_entry(struct gmtp_hashtable *table,
		const __u8 *flowname, __be32 local_addr, __be16 local_port,
		__be32 channel_addr, __be16 channel_port)
{
	struct gmtp_client_entry *new_entry;

	new_entry = kmalloc(sizeof(struct gmtp_client_entry), GFP_ATOMIC);
	if(new_entry == NULL)
		return 1;

	memcpy(new_entry->entry.key, flowname, GMTP_FLOWNAME_LEN);
	new_entry->clients = kmalloc(sizeof(struct gmtp_client), GFP_ATOMIC);
	if(new_entry->clients == NULL)
		return 2;
	INIT_LIST_HEAD(&new_entry->clients->list);

	new_entry->channel_addr = channel_addr;
	new_entry->channel_port = channel_port;

	return table->hash_ops.add_entry(table,
			(struct gmtp_hash_entry*) new_entry);
}
EXPORT_SYMBOL_GPL(gmtp_add_client_entry);

struct gmtp_client_entry *gmtp_lookup_client(struct gmtp_hashtable *table,
		const __u8 *key)
{
	return (struct gmtp_client_entry *) table->hash_ops.lookup(table, key);
}
EXPORT_SYMBOL_GPL(gmtp_lookup_client);

void gmtp_del_client_entry(struct gmtp_hashtable *table, const __u8 *key)
{
	table->hash_ops.del_entry(table, key);
}
EXPORT_SYMBOL_GPL(gmtp_del_client_entry);

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

void gmtp_del_client_hash_entry(struct gmtp_hashtable *table, const __u8 *key)
{
	struct gmtp_client_entry *entry;
	int hashval;

	gmtp_print_function();

	hashval = table->hash_ops.hash(table, key);
	if(hashval < 0)
		return;

	entry = (struct gmtp_client_entry*) table->entry[hashval];
	if(entry != NULL) {
		gmtp_del_client_list(entry);
		kfree(entry);
	}
}



void destroy_gmtp_client_hashtable(struct gmtp_hashtable *table)
{
	int i;
	struct gmtp_hash_entry *entry, *tmp;

	gmtp_print_function();

	if(table == NULL)
		return;

	for(i = 0; i < table->size; ++i) {
		entry = table->entry[i];
		while(entry != NULL) {
			tmp = entry;
			gmtp_del_client_list((struct gmtp_client_entry *) tmp);
			entry = entry->next;
			kfree(tmp);
		}
	}
	kfree(table->entry);
	kfree(table);
}

const struct gmtp_hash_ops gmtp_client_hash_ops = {
		.hash = gmtp_hash,
		.lookup = gmtp_lookup_entry,
		.add_entry = gmtp_add_entry,
		.del_entry = gmtp_del_client_hash_entry,
		.destroy = destroy_gmtp_client_hashtable,
};


