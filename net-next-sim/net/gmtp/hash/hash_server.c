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

	pr_info("All relays on hash table: %u\n", entry->relay_hashtable->len);
	pr_info("All routes in the server: %u\n", entry->nroutes);

	list_for_each_entry(relay, &entry->relay_list.list, list)
	{
		print_route_from_list(relay, &relay->list);
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
	return ((struct gmtp_relay_entry*) gmtp_lookup_entry(relay_table, key));
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
			server_entry->nroutes++; /* Number of routes */
		}
		list_add_tail(&relay_entry->list, head);

		err = relay_table->add_entry(relay_table, &relay_entry->entry);
	}

	return err;
}

static void gmtp_update_route_add_tail(struct gmtp_hashtable *relay_table,
		struct gmtp_hdr_relay *relay_list, __u8 nrelays,
		struct list_head *list)
{
	int i;
	for(i = nrelays - 2; i >= 0; --i) {
		struct gmtp_relay_entry *new_relay_entry;
		new_relay_entry = gmtp_build_relay_entry(&relay_list[i]);
		list_add_tail(&new_relay_entry->list, list);
		relay_table->add_entry(relay_table, &new_relay_entry->entry);
	}
}

int gmtp_update_route(struct gmtp_server_entry *server_entry,
	struct gmtp_relay_entry *relay_entry, struct sock *sk,
	struct sk_buff *skb)
{
	struct gmtp_hashtable *relay_table = server_entry->relay_hashtable;
	struct gmtp_hdr_relay *relay_list = gmtp_hdr_relay(skb);
	__u8  nrelays = gmtp_hdr_route(skb)->nrelays;

	pr_info("Number of relays: %u -> %u\n", relay_entry->nrelays, nrelays);
	if(nrelays > relay_entry->nrelays && nrelays >= 2) {

		/* FIXME Update routes correctly... */
		/*gmtp_update_route_add_tail(relay_table, relay_list, nrelays,
				&relay_entry->list);*/

	} else if(memcmp(&relay_entry->relay, relay_list,
			sizeof(relay_entry->relay))) {
		pr_info("Content changed!");
	} else {
		pr_info("Equal!\n");
	}

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
		if(relay == NULL)
			return gmtp_add_new_route(entry, sk, skb);
		else {
			pr_info("Relay already exists:   ");
			print_gmtp_hdr_relay(&relay->relay);
			/*return do_gmtp_update_route(entry, relay, relay_list, nrelays);*/
			return gmtp_update_route(entry, relay, sk, skb);
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

		entry->nroutes = 0;
		INIT_LIST_HEAD(&entry->relay_list.list);
		memcpy(entry->entry.key, gp->flowname, GMTP_HASH_KEY_LEN);
		entry->relay_hashtable = gmtp_build_hashtable(U8_MAX,
				gmtp_relay_hash_ops);
	}

	gmtp_print_server_entry(entry);

	if(gmtp_add_route(entry, sk, skb))
		return 1;

	return table->add_entry(table, (struct gmtp_hash_entry*)entry);
}
EXPORT_SYMBOL_GPL(gmtp_add_server_entry);

/* FIXME This function crashes all... */
void gmtp_del_relay_list(struct list_head *list_head)
{
	struct gmtp_relay_entry *relay, *temp;
	gmtp_pr_func();
	list_for_each_entry_safe(relay, temp, list_head, list)
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
		gmtp_del_relay_list(&entry->list);
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
