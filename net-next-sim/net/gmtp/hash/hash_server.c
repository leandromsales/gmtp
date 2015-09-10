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
		.hashval = gmtp_hashval,
		.add_entry = gmtp_add_entry,
		.del_entry = gmtp_del_relay_hash_entry,
		.destroy = destroy_gmtp_hashtable,
};

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

int gmtp_add_route(struct gmtp_server_entry* server, struct sock *sk,
		struct gmtp_hdr_route *route_hdr)
{
	struct gmtp_hashtable *relay_table = server->relay_hashtable;
	struct list_head *head;
	int i, err = 0;

	gmtp_pr_func();

	for(i = 0; i < route_hdr->nrelays; ++i) {
		struct gmtp_relay_entry *relay;
		relay = gmtp_build_relay_entry(&route_hdr->relay_list[i]);
		if(relay == NULL)
			return 1;

		if(i == 0) {
			INIT_LIST_HEAD(&relay->list);
			head = &relay->list;
			relay->sk = sk;
			/* add it on the list of relays at server */
			list_add_tail(&relay->list, &server->relay_list.list);
			++server->len;
		}
		list_add_tail(&relay->list, head);

		err = relay_table->add_entry(relay_table,
				(struct gmtp_hash_entry*) relay);
	}

	return err;
}

int gmtp_add_server_entry(struct gmtp_hashtable *table, struct sock *sk,
		struct gmtp_hdr_route *route)
{
	struct gmtp_sock *gp = gmtp_sk(sk);
	struct gmtp_server_entry *server;

	gmtp_pr_func();

	server = (struct gmtp_server_entry*) gmtp_lookup_entry(table, gp->flowname);

	if(server == NULL) {
		server = kmalloc(sizeof(struct gmtp_server_entry), GFP_KERNEL);
		if(server == NULL)
			return 1;

		server->len = 0;
		INIT_LIST_HEAD(&server->relay_list.list);
		memcpy(server->entry.key, gp->flowname, GMTP_HASH_KEY_LEN);
		server->relay_hashtable = gmtp_build_hashtable(U8_MAX,
				gmtp_relay_hash_ops);
	}

	if(gmtp_add_route(server, sk, route))
		return 1;

	return table->add_entry(table, (struct gmtp_hash_entry*)server);
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

	hashval = table->hashval(table, key);
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

	hashval = table->hashval(table, key);
	if(hashval < 0)
		return;

	entry = (struct gmtp_server_entry*) table->entry[hashval];
	if(entry != NULL) {
		/*entry->relay_hashtable->destroy(entry->relay_hashtable);*/
		kfree(entry);
	}
}

const struct gmtp_hash_ops gmtp_server_hash_ops = {
		.hashval = gmtp_hashval,
		.add_entry = gmtp_add_entry,
		.del_entry = gmtp_del_server_hash_entry,
		.destroy = destroy_gmtp_hashtable,
};
