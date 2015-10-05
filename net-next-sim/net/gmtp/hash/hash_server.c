/*
 * hash.c
 *
 *  Created on: 27/02/2015
 *      Author: mario
 */

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/string.h>

#include <uapi/linux/gmtp.h>
#include "../gmtp.h"
#include "hash.h"

struct gmtp_hashtable* server_hashtable;
EXPORT_SYMBOL_GPL(server_hashtable);

void gmtp_del_relay_hash_entry(struct gmtp_hashtable *table, const __u8 *key);

const struct gmtp_hash_ops gmtp_relay_hash_ops = {
		.hashval = gmtp_hashval,
		.add_entry = gmtp_add_entry,
		.del_entry = gmtp_del_relay_hash_entry,
		.destroy = destroy_gmtp_hashtable,
};

static void gmtp_print_server_entry(struct gmtp_server_entry *entry)
{
	struct gmtp_relay_entry *relay = &entry->relay_list;
	int i;

	pr_info("Relays in hash: %u\n", entry->relay_hashtable->len);
	pr_info("Relays in list: %u\n", entry->len);

	/*for(i = 0; i < entry->len; relay = relay->list.next, ++i) {
		print_gmtp_hdr_relay(&relay->relay);
	}*/

	pr_info("-----\n");
	list_for_each_entry(relay, &entry->relay_list.list, list)
	{
		print_gmtp_hdr_relay(&relay->relay);
		if(relay->list.next == &relay->list)
			break;
	}
}

static struct gmtp_relay_entry *gmtp_build_relay_entry(
		const struct gmtp_hdr_relay *relay)
{
	struct gmtp_relay_entry *relay_entry;

	relay_entry = kmalloc(sizeof(struct gmtp_relay_entry), GFP_KERNEL);
	if(relay_entry == NULL)
		return NULL;

	memcpy(relay_entry->entry.key, relay->relay_id, GMTP_HASH_KEY_LEN);
	memcpy(&relay_entry->relay, relay, sizeof(struct gmtp_hdr_relay));

	return relay_entry;
}

static struct gmtp_relay_entry *gmtp_get_relay_entry(
		struct gmtp_hashtable *relay_table, const __u8 *key)
{
	struct gmtp_hash_entry *entry;
	struct gmtp_relay_entry *relay;

	unsigned int hashval;

	gmtp_pr_func();

	hashval = relay_table->hashval(relay_table, key);
	/*relay_entry = (struct gmtp_relay_entry*) relay_table->entry[hashval];*/

	pr_info("hashval: %u\n", hashval);
	entry = relay_table->entry[hashval];
	pr_info("entry: %p\n", entry);
	relay = (struct gmtp_relay_entry*) entry;
	pr_info("relay_entry: %p\n", relay);

	entry = gmtp_lookup_entry(relay_table, key);
	pr_info("entry: %p\n", entry);
	relay = (struct gmtp_relay_entry*) entry;
	pr_info("relay_entry: %p\n", relay);

	return relay;
}

static int gmtp_add_new_route(struct gmtp_server_entry* server_entry,
		struct sock *sk, struct sk_buff *skb)
{
	struct gmtp_hashtable *relay_table = server_entry->relay_hashtable;
	struct gmtp_hdr_relay *relay_list = gmtp_hdr_relay(skb);
	struct list_head *head;
	int i, err = 0;
	__u8 nrelays = gmtp_hdr_route(skb)->nrelays;

	gmtp_pr_func();

	for(i = nrelays-1; i >= 0; --i) {
		struct gmtp_relay_entry *relay_entry;
		relay_entry = gmtp_build_relay_entry(&relay_list[i]);
		pr_info("New relay: %p\n", relay_entry);

		if(relay_entry == NULL)
			return 2;

		if(i == (nrelays-1)) {
			INIT_LIST_HEAD(&relay_entry->list);
			head = &relay_entry->list;
			relay_entry->sk = sk;
			relay_entry->nrelays++;

			/* add it on the list of relays at server */
			pr_info("Adding relay on list...\n");
			list_add_tail(&relay_entry->list, &server_entry->relay_list.list);
			server_entry->len++; /* Number of routes */
		}
		list_add_tail(&relay_entry->list, head);

		err = relay_table->add_entry(relay_table, &relay_entry->entry);
	}

	return err;
}

static int gmtp_update_route(struct gmtp_server_entry* entry,
		struct gmtp_relay_entry *relay,
		struct gmtp_hdr_relay *relay_list, __u8 nrelays)
{
	/** TODO Implement update route */

	pr_info("Number of relays: %u -> %u\n", relay->nrelays, nrelays);
	if(nrelays != relay->nrelays)
		pr_info("TODO update relay added/removed!\n");
	else if(memcmp(&relay->relay, relay_list, sizeof(relay->relay)))
		pr_info("Content change!");
	else
		pr_info("Equal!\n");

	return 1;
}

int gmtp_add_route(struct gmtp_server_entry* entry, struct sock *sk,
		struct sk_buff *skb)
{
	struct gmtp_hashtable *relay_table = entry->relay_hashtable;
	struct gmtp_hdr_relay *relay_list = gmtp_hdr_relay(skb);
	__u8  nrelays = gmtp_hdr_route(skb)->nrelays;

	if(nrelays > 0) {
		struct gmtp_relay_entry *relay;
		relay = gmtp_get_relay_entry(relay_table, relay_list[(nrelays-1)].relay_id);
		pr_info("Last relay: %p\n", relay);
		if(relay == NULL)
			return gmtp_add_new_route(entry, sk, skb);
		else {
			pr_info("Relay already exists:   ");
			print_gmtp_hdr_relay(&relay->relay);
			return gmtp_update_route(entry, relay, relay_list, nrelays);
		}
	}

	return 2;
}

int gmtp_add_server_entry(struct gmtp_hashtable *table, struct sock *sk,
		struct sk_buff *skb)
{
	struct gmtp_hdr_route *route = gmtp_hdr_route(skb);
	struct gmtp_sock *gp = gmtp_sk(sk);
	struct gmtp_server_entry *entry;

	gmtp_pr_func();

	entry = (struct gmtp_server_entry*) gmtp_lookup_entry(table, gp->flowname);
	pr_info("entry: %p\n", entry);

	if(entry == NULL) {
		entry = kmalloc(sizeof(struct gmtp_server_entry), GFP_KERNEL);
		if(entry == NULL)
			return 1;

		entry->len = 0;
		INIT_LIST_HEAD(&entry->relay_list.list);
		memcpy(entry->entry.key, gp->flowname, GMTP_HASH_KEY_LEN);
		entry->relay_hashtable = gmtp_build_hashtable(U8_MAX,
				gmtp_relay_hash_ops);
	}

	pr_info("Table len: %u\n", table->len);
	pr_info("All relays:\n");
	gmtp_print_server_entry(entry);

	if(gmtp_add_route(entry, sk, skb))
		return 1;

	return table->add_entry(table, (struct gmtp_hash_entry*)entry);
}
EXPORT_SYMBOL_GPL(gmtp_add_server_entry);

void gmtp_del_relay_list(struct gmtp_relay_entry *entry)
{
	struct gmtp_relay_entry *relay, *temp;
	gmtp_pr_func();
	list_for_each_entry_safe(relay, temp, &entry->list, list)
	{
		list_del(&relay->list);
		kfree(relay);
	}
}

void gmtp_del_relay_hash_entry(struct gmtp_hashtable *table, const __u8 *key)
{
	struct gmtp_relay_entry *entry;
	int hashval;

	gmtp_print_function();

	hashval = table->hashval(table, key);
	if(hashval < 0)
		return;

	entry = (struct gmtp_relay_entry*) table->entry[hashval];
	if(entry != NULL) {
		gmtp_del_relay_list(entry);
		table->len--;
		kfree(entry);
	}
}

void gmtp_del_server_hash_entry(struct gmtp_hashtable *table, const __u8 *key)
{
	struct gmtp_server_entry *entry;
	int hashval;

	gmtp_print_function();

	hashval = table->hashval(table, key);
	if(hashval < 0)
		return;

	entry = (struct gmtp_server_entry*) table->entry[hashval];
	if(entry != NULL) {
		/*entry->relay_hashtable->destroy(entry->relay_hashtable);*/
		table->len--;
		kfree(entry);
	}
}

const struct gmtp_hash_ops gmtp_server_hash_ops = {
		.hashval = gmtp_hashval,
		.add_entry = gmtp_add_entry,
		.del_entry = gmtp_del_server_hash_entry,
		.destroy = destroy_gmtp_hashtable,
};
