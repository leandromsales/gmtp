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
	struct gmtp_relay_entry *relay;
	int i;

	pr_info("Server entry: \n");
	pr_info("\t%u relays on hash table\n", entry->relay_hashtable->len);
	pr_info("\t%u routes in the server\n", entry->nrelays);
	list_for_each_entry(relay, &entry->relays.relay_list, relay_list)
	{
		print_route_from_list(relay);
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
	struct list_head *path_list_head = NULL;
	int i, err = 0;
	__u8 nrelays = gmtp_hdr_route(skb)->nrelays;

	gmtp_pr_func();

	for(i = nrelays-1; i >= 0; --i) {
		struct gmtp_relay_entry *relay_entry;
		relay_entry = gmtp_build_relay_entry(&relay_list[i]);

		if(relay_entry == NULL)
			return 2;

		if(i == (nrelays-1)) {
			INIT_LIST_HEAD(&relay_entry->path_list);
			path_list_head = &relay_entry->path_list;
			relay_entry->sk = sk;
			relay_entry->nrelays++;

			/* add it on the list of relays at server */
			pr_info("Adding relay %pI4 on server list...\n",
					&relay_entry->relay.relay_ip);
			list_add(&relay_entry->relay_list,
					&server_entry->relays.relay_list);
			server_entry->nrelays++; /* Number of routes */
		}
		err = relay_table->add_entry(relay_table, &relay_entry->entry);
		if(!err && path_list_head != NULL)
			list_add(&relay_entry->path_list, path_list_head);
	}

	return err;
}

static int gmtp_update_route_add_tail(struct gmtp_hashtable *relay_table,
		struct gmtp_hdr_relay *relay_list, __u8 nrelays,
		struct list_head *list)
{
	int i, err = 0;
	for(i = nrelays - 2; i >= 0; --i) {
		struct gmtp_relay_entry *new_relay_entry;
		new_relay_entry = gmtp_build_relay_entry(&relay_list[i]);
		err = relay_table->add_entry(relay_table, &new_relay_entry->entry);
		list_add_tail(&new_relay_entry->path_list, list);
	}
	return err;
}

int gmtp_update_route(struct gmtp_server_entry *server_entry,
	struct gmtp_relay_entry *relay_entry, struct sock *sk,
	struct sk_buff *skb)
{
	struct gmtp_hashtable *relay_table = server_entry->relay_hashtable;
	struct gmtp_hdr_relay *relay_list = gmtp_hdr_relay(skb);
	__u8  nrelays = gmtp_hdr_route(skb)->nrelays;

	pr_info("Relays in path: from %u to %u\n", relay_entry->nrelays, nrelays);
	if(nrelays > relay_entry->nrelays && nrelays >= 2) {

		struct gmtp_relay_entry *new_relay;
		struct sk_buff *delegate_skb;

		gmtp_update_route_add_tail(relay_table, relay_list, nrelays,
				&relay_entry->path_list);
		relay_entry->nrelays = nrelays;

		pr_info("Sending a GMTP-Delegate to new relay...\n");
		new_relay = gmtp_get_relay_entry(relay_table, relay_list[nrelays-2].relay_id);
		if(new_relay != NULL) {
			delegate_skb = gmtp_make_delegate(new_relay->sk, skb,
					relay_entry->relay.relay_id);
			print_gmtp_packet(ip_hdr(delegate_skb), gmtp_hdr(delegate_skb));
			gmtp_transmit_built_skb(new_relay->sk, delegate_skb);
		}

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
			pr_info("Relay %pI4 already exists\n", &relay->relay.relay_ip);
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

	if(entry == NULL) {
		pr_info("New server entry\n");
		entry = kmalloc(sizeof(struct gmtp_server_entry), GFP_KERNEL);
		if(entry == NULL)
			return 1;

		entry->nrelays = 0;
		INIT_LIST_HEAD(&entry->relays.relay_list);
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
	list_for_each_entry_safe(relay, temp, list_head, path_list)
	{
		list_del(&relay->path_list);
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
		gmtp_del_relay_list(&entry->path_list);
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
