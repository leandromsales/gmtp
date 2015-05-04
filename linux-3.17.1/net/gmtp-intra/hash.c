/*
 * hash.c
 *
 *  Created on: 27/02/2015
 *      Author: wendell
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/string.h>

#include "gmtp-intra.h"
#include "hash.h"

struct gmtp_intra_hashtable *gmtp_intra_create_hashtable(unsigned int size)
{
	int i;
	struct gmtp_intra_hashtable *ht;

	gmtp_pr_func();
	gmtp_pr_info("Size of gmtp_intra_hashtable = %d", size);

	if(size < 1)
		return NULL;

	ht = kmalloc(sizeof(struct gmtp_intra_hashtable), GFP_KERNEL);
	if(ht == NULL)
		return NULL;

	ht->table = kmalloc(sizeof(struct gmtp_relay_entry*) *size, GFP_KERNEL);
	if(ht->table == NULL)
		return NULL;

	for(i = 0; i < size; ++i)
		ht->table[i] = NULL;

	ht->size = size;

	return ht;
}

unsigned int gmtp_intra_hash(struct gmtp_intra_hashtable *hashtable,
		const __u8 *flowname)
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

struct gmtp_relay_entry *gmtp_intra_lookup_media(
		struct gmtp_intra_hashtable *hashtable, const __u8 *media)
{
	struct gmtp_relay_entry *entry;
	unsigned int hashval;

	hashval = gmtp_intra_hash(hashtable, media);

	/* Error */
	if(hashval < 0)
		return NULL;

	for(entry = hashtable->table[hashval]; entry != NULL;
			entry = entry->next)
		if(memcmp(media, entry->flowname, GMTP_FLOWNAME_LEN) == 0)
			return entry;

	return NULL;
}

int gmtp_intra_add_entry(struct gmtp_intra_hashtable *hashtable, __u8 *flowname,
		__be32 server_addr, __be32 *relay, __be16 media_port,
		__be32 channel_addr, __be16 channel_port)
{
	struct gmtp_relay_entry *new_entry;
	struct gmtp_relay_entry *current_entry;
	unsigned int hashval;

	gmtp_print_function();

	hashval = gmtp_intra_hash(hashtable, flowname);

	/* Error */
	if(hashval < 0)
		return hashval;

	new_entry = kmalloc(sizeof(struct gmtp_relay_entry), GFP_KERNEL);
	if(new_entry == NULL)
		return 1;

	current_entry = gmtp_intra_lookup_media(hashtable, flowname);
	if(current_entry != NULL)
		return 2; /* TODO Media already being transmitted by other
								server? */

	new_entry->info = kmalloc(sizeof(struct gmtp_flow_info), GFP_ATOMIC);
	if(new_entry->info == NULL)
		return 1;

	new_entry->info->iseq = 0;
	new_entry->info->seq = 0;
	new_entry->info->nbytes = 0;
	new_entry->info->data_pkt_tx = 0;
	new_entry->info->buffer = kmalloc(sizeof(struct sk_buff_head), GFP_ATOMIC);
	skb_queue_head_init(new_entry->info->buffer);
	new_entry->info->buffer_size = 0;
	new_entry->info->buffer_max = 5;
	new_entry->info->current_tx = 0; /* unlimited tx */

	memcpy(new_entry->flowname, flowname, GMTP_FLOWNAME_LEN);
	new_entry->server_addr = server_addr;
	new_entry->relay = relay; /* FIXME Add list */
	new_entry->media_port = media_port;
	new_entry->channel_addr = channel_addr;
	new_entry->channel_port = channel_port;
	new_entry->state = GMTP_INTRA_WAITING_REGISTER_REPLY;
	new_entry->next = hashtable->table[hashval];

	hashtable->table[hashval] = new_entry;

	return 0;
}
EXPORT_SYMBOL_GPL(gmtp_intra_add_entry);

struct gmtp_relay_entry *gmtp_intra_del_entry(
		struct gmtp_intra_hashtable *hashtable, __u8 *media)
{
	struct gmtp_relay_entry *previous_entry;
	struct gmtp_relay_entry *current_entry;
	int hashval;

	gmtp_print_function();

	hashval = gmtp_intra_hash(hashtable, media);
	if(hashval < 0)
		return NULL;

	previous_entry = NULL;
	current_entry = hashtable->table[hashval];

	while(current_entry != NULL
			&& memcmp(media, current_entry->flowname,
					GMTP_FLOWNAME_LEN) != 0) {
		previous_entry = current_entry;
		current_entry = current_entry->next;
	}

	if(current_entry == NULL) {
		gmtp_print_debug("Media entry not found at %d", hashval);
		return hashtable->table[hashval];
	}

	/* Remove the last entry of list */
	if(previous_entry == NULL) {
		hashtable->table[hashval] = current_entry->next;
	} else { /* The list keeps another media with same hash value */
		previous_entry->next = current_entry->next;
	}

	skb_queue_purge(current_entry->info->buffer);
	kfree(current_entry->info);
	kfree(current_entry);

	gmtp_print_debug("Media entry removed successfully!");
	return hashtable->table[hashval];
}

void kfree_gmtp_intra_hashtable(struct gmtp_intra_hashtable *hashtable)
{
	int i;
	struct gmtp_relay_entry *list, *temp;

	gmtp_print_function();

	if(hashtable == NULL)
		return;

	for(i = 0; i < hashtable->size; ++i) {
		list = hashtable->table[i];
		while(list != NULL) {
			temp = list;
			list = list->next;
			kfree(temp);
		}
	}

	kfree(hashtable->table);
	kfree(hashtable);
}
