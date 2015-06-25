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

int gmtp_add_server_entry(struct gmtp_hashtable *table, const __u8 *relayid,
		__u8 *flowname, struct gmtp_hdr_route *route)
{
	struct gmtp_server_entry *new_entry;

	new_entry = kmalloc(sizeof(struct gmtp_server_entry), GFP_KERNEL);
	if(new_entry == NULL)
		return 1;

	memcpy(new_entry->entry.key, flowname, GMTP_FLOWNAME_LEN);
	memcpy(&new_entry->route, route, sizeof(*route));
	new_entry->srelay = &new_entry->route.relay_list[route->nrelays];

	return table->hash_ops.add_entry(table,
			(struct gmtp_hash_entry*) new_entry);
}
EXPORT_SYMBOL_GPL(gmtp_add_server_entry);

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
		if(entry->srelay != NULL)
			kfree(entry->srelay);
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
