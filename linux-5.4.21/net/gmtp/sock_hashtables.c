/*
 * sock_hashtables.c
 *
 *  Created on: 5 de mar de 2020
 *      Author: wendell
 */

#include <linux/slab.h>
#include <linux/types.h>
#include <linux/list.h>
#include <linux/err.h>

#include "gmtp.h"
#include "sock_hashtables.h"

int gmtp_build_listen_hashtable(struct gmtp_listen_hashtable *table,
		size_t max_size)
{
	gmtp_pr_func();
	gmtp_pr_debug("Building a gmtp_lhashtable with %lu max size", max_size);

	if (max_size < 1)
		goto failure;

	if (table == NULL)
		goto failure;

	table->entry = kmalloc(sizeof(struct gmtp_listen_hashitem*), GFP_KERNEL);
	if (table->entry == NULL)
		goto failure;

	INIT_LIST_HEAD(&table->entry->list);
	table->max_size = max_size;
	table->len = 0;

	return 0;

failure:
	gmtp_pr_error("gmtp_listen_hashtable wasn't built!");
	return -ENOMEM;
}
EXPORT_SYMBOL_GPL(gmtp_build_listen_hashtable);



int __gmtp_add_listen_item(struct gmtp_listen_hashtable *table,
		struct gmtp_listen_hashitem *item)
{
	gmtp_pr_func();

	if (table->len + 1 >= table->max_size) {
		gmtp_pr_info("GMTP Listen Hashtable is full! (max_size: %u)",
				table->max_size);
		return -ENOBUFS;
	}

	if (table->entry == NULL)
		return -ENOKEY;

	INIT_LIST_HEAD(&item->list);
	list_add_tail(&item->list, &table->entry->list);
	table->len++;

	return 0;
}

struct gmtp_listen_hashitem * __gmtp_lookup_listener(
		struct gmtp_listen_hashtable *table,
		const __be32 listen_addr, const __be16 listen_port)
{
	struct gmtp_listen_hashitem *pos;
	struct inet_sock *inet;

	gmtp_pr_func();

	gmtp_pr_info("Searching for: %pI4@%-5d", &listen_addr, ntohs(listen_port));

	if(table->entry == NULL)
		return NULL;

	list_for_each_entry(pos, &table->entry->list, list)
	{
		if(pos == NULL)
			continue;
		if (pos->sk == NULL)
			continue;

		inet = inet_sk(pos->sk);
		gmtp_pr_info("Item: %pI4@%-5d", &inet->inet_rcv_saddr,
				ntohs(inet->inet_sport));
		if (inet->inet_rcv_saddr == listen_addr &&
				inet->inet_sport == listen_port) {
			return pos;
		}
	}
	return NULL;
}

int gmtp_sk_listen_start(struct gmtp_listen_hashtable *table, struct sock *sk)
{
	struct gmtp_listen_hashitem *new_item;
	struct inet_sock *inet = inet_sk(sk);

	gmtp_pr_func();

	new_item = __gmtp_lookup_listener(table,
			inet->inet_rcv_saddr, inet->inet_sport);

	if(new_item)
		return 1; /* Entry already exists */

	new_item = kmalloc(sizeof(struct gmtp_listen_hashitem), GFP_ATOMIC);

	if (new_item == NULL)
		return -ENOMEM;

	new_item->sk = sk;
	return __gmtp_add_listen_item(table, new_item);
}
EXPORT_SYMBOL_GPL(gmtp_sk_listen_start);

/**
 * Lookup listener sk in GMTP Listen Hashtable
 */
struct sock *gmtp_lookup_listener(struct gmtp_listen_hashtable *table,
		 const __be32 server_addr, const __be16 server_port)
{
	struct gmtp_listen_hashitem *result;

	gmtp_pr_func();

	result = __gmtp_lookup_listener(table, server_addr, server_port);
	if(result)
		return result->sk;

	return NULL;
}
EXPORT_SYMBOL_GPL(gmtp_lookup_listener);

