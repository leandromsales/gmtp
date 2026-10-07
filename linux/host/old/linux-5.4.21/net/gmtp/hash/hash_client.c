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

struct gmtp_client_entry *gmtp_add_client_entry(struct gmtp_hashtable *table,
		const __u8 *flowname, __be32 local_addr, __be16 local_port,
		__be32 channel_addr, __be16 channel_port)
{
	struct gmtp_client_entry *new_entry;
	int err;

	gmtp_pr_func();

	new_entry = kmalloc(sizeof(struct gmtp_client_entry), GFP_ATOMIC);
	if(new_entry == NULL)
		return NULL;

	memcpy(new_entry->entry.key, flowname, GMTP_HASH_KEY_LEN);
	new_entry->clients = kmalloc(sizeof(struct gmtp_client), GFP_ATOMIC);
	if(new_entry->clients == NULL) {
		kfree(new_entry);
		return NULL;
	}
	INIT_LIST_HEAD(&new_entry->clients->list);

	new_entry->channel_addr = channel_addr;
	new_entry->channel_port = channel_port;

	err = table->add_entry(table, (struct gmtp_hash_entry*) new_entry);
	if (err) {
		kfree(new_entry);
		return NULL;
	}

	return new_entry;
}
EXPORT_SYMBOL_GPL(gmtp_add_client_entry);

struct gmtp_client_entry *gmtp_lookup_client(struct gmtp_hashtable *table,
		const __u8 *key)
{
	return (struct gmtp_client_entry *) gmtp_lookup_entry(table, key);
}
EXPORT_SYMBOL_GPL(gmtp_lookup_client);

void gmtp_del_client_entry(struct gmtp_hashtable *table, const __u8 *key)
{
	gmtp_pr_func();
	table->del_entry(table, key);
}
EXPORT_SYMBOL_GPL(gmtp_del_client_entry);

void gmtp_del_client_list(struct gmtp_client_entry *entry)
{
	struct gmtp_client *client, *temp;
	gmtp_pr_func();
	list_for_each_entry_safe(client, temp, &entry->clients->list, list)
	{
		gmtp_pr_error("FIXME: list_del(&client->list) crashes GMTP");
		/** FIXME Call list_dell here crashes GMTP... */
		/*list_del(&client->list);
		kfree(client);*/
	}

}

void gmtp_del_client_hash_entry(struct gmtp_hashtable *table, const __u8 *key)
{
	struct gmtp_client_entry *entry;

	gmtp_print_function();

	entry = (struct gmtp_client_entry *) gmtp_lookup_entry(table, key);
	if(entry != NULL) {
		gmtp_del_client_list(entry);
		table->len--;
		/*kfree(entry);*/
	}
}

const struct gmtp_hash_ops gmtp_client_hash_ops = {
		.hashval = gmtp_hashval,
		.add_entry = gmtp_add_entry,
		.del_entry = gmtp_del_client_hash_entry,
		.destroy = destroy_gmtp_hashtable,
};


