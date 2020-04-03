/*
 * hash-inter.c
 *
 *  Created on: 27/02/2015
 *      Author: wendell
 */

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/list.h>
#include <linux/timer.h>

#include "gmtp-inter.h"
#include "hash-inter.h"
#include "mcc-inter.h"
#include "ucc.h"

struct gmtp_inter_hashtable *gmtp_inter_create_hashtable(unsigned int size)
{
	int i;
	struct gmtp_inter_hashtable *ht;

	gmtp_pr_func();
	gmtp_pr_info("Size of gmtp_inter_hashtable = %d", size);

	if(size < 1)
		return NULL;

	ht = kmalloc(sizeof(struct gmtp_inter_hashtable), GFP_KERNEL);
	if(ht == NULL)
		return NULL;

	ht->table = kmalloc(sizeof(struct gmtp_inter_entry*) *size, GFP_KERNEL);
	if(ht->table == NULL)
		return NULL;

	for(i = 0; i < size; ++i)
		ht->table[i] = NULL;

	ht->size = size;

	return ht;
}

unsigned int gmtp_inter_hash(struct gmtp_inter_hashtable *hashtable,
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

struct gmtp_inter_entry *gmtp_inter_lookup_media(
		struct gmtp_inter_hashtable *hashtable, const __u8 *media)
{
	struct gmtp_inter_entry *entry;
	unsigned int hashval;

	hashval = gmtp_inter_hash(hashtable, media);

	/* Error */
	if(hashval < 0)
		return NULL;

	for(entry = hashtable->table[hashval]; entry != NULL;
			entry = entry->next)
		if(memcmp(media, entry->flowname, GMTP_FLOWNAME_LEN) == 0)
			return entry;

	return NULL;
}

void __gmtp_inter_build_info(struct gmtp_inter_entry *info)
{
	if(unlikely(info == NULL))
		return;

	info->total_bytes = 0;
	info->last_rx_tstamp = 0;
	info->rcv_tx_rate = UINT_MAX;

	info->nfeedbacks = 0;
	info->sum_feedbacks = 0;
	info->recent_bytes = UINT_MAX;
	info->recent_rx_tstamp = 0;
	info->current_rx = 0;
	info->required_tx = 0;
	info->data_pkt_out = 0;
	info->server_rtt = 64;
	info->transm_r = UINT_MAX;

	info->clients = kmalloc(sizeof(struct gmtp_client), GFP_KERNEL);
	INIT_LIST_HEAD(&info->clients->list);
	info->nclients = 0;

	info->buffer = kmalloc(sizeof(struct sk_buff_head), GFP_KERNEL);
	skb_queue_head_init(info->buffer);
	info->buffer_len = 0;
	/*gmtp_set_buffer_limits(info, 40);*/
	gmtp_set_buffer_limits(info, 40);

	setup_timer(&info->mcc_timer, mcc_timer_callback, (unsigned long) info);
}

void gmtp_inter_build_info(struct gmtp_inter_entry *info, unsigned int bmin)
{
	if(likely(info != NULL)) {
		gmtp_set_buffer_limits(info, bmin);
		__gmtp_inter_build_info(info);
	}
}

int gmtp_inter_add_entry(struct gmtp_inter_hashtable *hashtable, __u8 *flowname,
		__be32 server_addr, __be32 *relay, __be16 media_port,
		__be32 channel_addr, __be16 channel_port)
{
	struct gmtp_inter_entry *new_entry;
	struct gmtp_inter_entry *current_entry;
	unsigned int hashval;

	gmtp_print_function();

	hashval = gmtp_inter_hash(hashtable, flowname);

	/* Error */
	if(hashval < 0)
		return hashval;

	new_entry = kmalloc(sizeof(struct gmtp_inter_entry), GFP_KERNEL);
	if(new_entry == NULL)
		return 1;

	current_entry = gmtp_inter_lookup_media(hashtable, flowname);
	if(current_entry != NULL)
		return 2; /* TODO Media already being transmitted by other
								server? */
	gmtp_inter_build_info(new_entry, 5);

	memcpy(new_entry->flowname, flowname, GMTP_FLOWNAME_LEN);
	new_entry->server_addr = server_addr;

	new_entry->relays = kmalloc(sizeof(struct gmtp_relay), GFP_KERNEL);
	INIT_LIST_HEAD(&new_entry->relays->list);
	new_entry->nrelays = 0;

	new_entry->media_port = media_port;
	new_entry->channel_addr = channel_addr;
	new_entry->channel_port = channel_port;
	new_entry->state = GMTP_INTER_WAITING_REGISTER_REPLY;
	new_entry->next = hashtable->table[hashval];
	hashtable->table[hashval] = new_entry;
	setup_timer(&new_entry->ack_timer, gmtp_inter_ack_timer_callback,
			(unsigned long ) new_entry);
	setup_timer(&new_entry->register_timer,
			gmtp_inter_register_timer_callback,
			(unsigned long ) new_entry);

	return 0;
}
EXPORT_SYMBOL_GPL(gmtp_inter_add_entry);

void gmtp_inter_del_clients(struct gmtp_inter_entry *entry)
{
	struct gmtp_client *client, *temp;

	gmtp_pr_func();

	list_for_each_entry_safe(client, temp, &entry->clients->list, list)
	{
		list_del(&client->list);
		kfree(client);
	}
}

struct gmtp_inter_entry *gmtp_inter_del_entry(
		struct gmtp_inter_hashtable *hashtable, __u8 *media)
{
	struct gmtp_inter_entry *previous_entry;
	struct gmtp_inter_entry *current_entry;
	int hashval;

	gmtp_print_function();

	hashval = gmtp_inter_hash(hashtable, media);
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
	if(previous_entry == NULL)
		hashtable->table[hashval] = current_entry->next;
	else  /* The list keeps another media with same hash value */
		previous_entry->next = current_entry->next;

	gmtp_inter_del_clients(current_entry);
	skb_queue_purge(current_entry->buffer);
	del_timer_sync(&current_entry->mcc_timer);
	del_timer_sync(&current_entry->ack_timer);
	kfree(current_entry);

	gmtp_print_debug("Media entry removed successfully!");
	return hashtable->table[hashval];
}

void kfree_gmtp_inter_hashtable(struct gmtp_inter_hashtable *hashtable)
{
	int i;
	struct gmtp_inter_entry *list, *temp;

	gmtp_print_function();

	if(hashtable == NULL)
		return;

	for(i = 0; i < hashtable->size; ++i) {
		list = hashtable->table[i];
		while(list != NULL) {
			temp = list;
			list = list->next;
			gmtp_inter_del_entry(hashtable, temp->flowname);
		}
	}

	kfree(hashtable->table);
	kfree(hashtable);
}
