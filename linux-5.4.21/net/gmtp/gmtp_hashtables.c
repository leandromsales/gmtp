#include "gmtp_hashtables.h"

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

int gmtp_build_sk_hashtable(struct gmtp_sk_hashtable *sk_table)
{
	gmtp_pr_func();

	if (sk_table == NULL)
		goto failure;

	sk_table->hbind = kmalloc(sizeof(struct gmtp_sk_hashitem), GFP_KERNEL);
	if (sk_table->hbind == NULL)
		goto failure;

	sk_table->hlisten = kmalloc(sizeof(struct gmtp_sk_hashitem), GFP_KERNEL);
	if (sk_table->hlisten == NULL)
		goto failure_listen;

	INIT_LIST_HEAD(&sk_table->hbind->list);
	INIT_LIST_HEAD(&sk_table->hlisten->list);

	return 0;

failure_listen:
	kfree(sk_table->hbind);

failure:
	gmtp_pr_error("gmtp_listen_hashtable wasn't built!");
	return -ENOMEM;
}
EXPORT_SYMBOL_GPL(gmtp_build_sk_hashtable);

int __gmtp_add_listen_item(struct gmtp_sk_hashtable *sk_table,
		struct gmtp_sk_hashitem *item)
{
	gmtp_pr_func();

	if (sk_table->hlisten == NULL)
		return -ENOKEY;

	INIT_LIST_HEAD(&item->list);
	list_add_tail(&item->list, &sk_table->hlisten->list);
	item->count++;

	return 0;
}

struct gmtp_sk_hashitem * __gmtp_lookup_listener(
		struct gmtp_sk_hashtable *sk_table,
		const __be32 addr, const __be16 port)
{
	struct gmtp_sk_hashitem *pos;
	struct inet_sock *inet;

	gmtp_pr_func();

	gmtp_pr_info("Searching for: %pI4@%-5d", &addr, ntohs(port));

	if(sk_table->hlisten == NULL)
		return NULL;

	list_for_each_entry(pos, &sk_table->hlisten->list, list)
	{
		if(pos == NULL)
			continue;
		if (pos->sk == NULL)
			continue;

		inet = inet_sk(pos->sk);
		gmtp_pr_info("Item: [%pI4@%-5d]",
						&inet->inet_rcv_saddr, ntohs(inet->inet_sport));
		if (inet->inet_rcv_saddr == addr && inet->inet_sport == port)
		{
			gmtp_pr_info("Found!");
			return pos;
		}
	}
	return NULL;
}

int gmtp_sk_listen_start(struct gmtp_sk_hashtable *sk_table, struct sock *sk)
{
	struct gmtp_sk_hashitem *new_item;
	struct inet_sock *inet = inet_sk(sk);

	gmtp_pr_func();

	new_item = __gmtp_lookup_listener(sk_table,
			inet->inet_rcv_saddr, inet->inet_sport);

	if(new_item)
		return 1; /* Entry already exists */

	new_item = kmalloc(sizeof(struct gmtp_sk_hashitem), GFP_ATOMIC);

	if (new_item == NULL)
		return -ENOMEM;

	new_item->sk = sk;
	gmtp_pr_info("Adding to GMTP_LHASH: %pI4@%-5d",
			&inet->inet_rcv_saddr, ntohs(inet->inet_sport));
	return __gmtp_add_listen_item(sk_table, new_item);
}
EXPORT_SYMBOL_GPL(gmtp_sk_listen_start);

/**
 * Lookup listener sk in GMTP Listen Hashtable
 */
struct sock *gmtp_lookup_listener(struct gmtp_sk_hashtable *sk_table,
		 const __be32 addr, const __be16 port)
{
	struct gmtp_sk_hashitem *result;

	gmtp_pr_func();

	result = __gmtp_lookup_listener(sk_table, addr, port);
	if(result)
		return result->sk;

	return NULL;
}
EXPORT_SYMBOL_GPL(gmtp_lookup_listener);

struct gmtp_sk_hashitem * __gmtp_lookup_established(
		struct gmtp_sk_hashtable *sk_table,
		 const __be32 saddr, const __be16 sport,
		 const __be32 daddr, const __be16 dport)
{
	struct gmtp_sk_hashitem *pos;
	struct inet_sock *inet;

	gmtp_pr_func();

	gmtp_pr_info("Searching for: [%pI4@%-5d <=> %pI4@%-5d]",
			&daddr, ntohs(dport), &saddr, ntohs(sport));

	if(sk_table->hbind == NULL)
		return NULL;

	list_for_each_entry(pos, &sk_table->hbind->list, list)
	{
		if(pos == NULL)
			continue;
		if (pos->sk == NULL)
			continue;

		inet = inet_sk(pos->sk);
		gmtp_pr_info("Item: [%pI4@%-5d <=> %pI4@%-5d]",
				&inet->inet_rcv_saddr, ntohs(inet->inet_sport),
				&inet->inet_daddr, ntohs(inet->inet_dport));

		/**
		 * Lookup for received packets...
		 * - saddr is remote addr (sk->daddr)
		 * - sport is remote port (sk->dport)
		 * - daddr is local addr (sk->saddr)
		 * - dport is local port (sk->sport)
		 */
		if (inet->inet_rcv_saddr == daddr && inet->inet_sport == dport &&
				inet->inet_daddr == saddr && inet->inet_dport == sport)
		{
			gmtp_pr_info("Found!");
			return pos;
		}
	}
	return NULL;
}

int __gmtp_add_established_item(struct gmtp_sk_hashtable *sk_table,
		struct gmtp_sk_hashitem *item)
{
	gmtp_pr_func();

	if (sk_table->hbind == NULL)
		return -ENOKEY;

	INIT_LIST_HEAD(&item->list);
	list_add_tail(&item->list, &sk_table->hbind->list);
	item->count++;

	return 0;
}

int gmtp_sk_hash_connect(struct gmtp_sk_hashtable *sk_table, struct sock *sk)
{
	struct gmtp_sk_hashitem *new_item;
	struct inet_sock *inet = inet_sk(sk);

	gmtp_pr_func();

/*	new_item = __gmtp_lookup_listener(sk_table,
			inet->inet_rcv_saddr, inet->inet_sport);

	if(new_item)
		return 1;*/ /* Entry already exists */

	new_item = kmalloc(sizeof(struct gmtp_sk_hashitem), GFP_ATOMIC);

	if (new_item == NULL)
		return -ENOMEM;

	new_item->sk = sk;
	gmtp_pr_info("Adding to GMTP_BHASH: [%pI4@%-5d <=> %pI4@%-5d]",
					&inet->inet_rcv_saddr, ntohs(inet->inet_sport),
					&inet->inet_daddr, ntohs(inet->inet_dport));
	return __gmtp_add_established_item(sk_table, new_item);
}
EXPORT_SYMBOL_GPL(gmtp_sk_hash_connect);

/**
 * Lookup listener sk in GMTP Bind Hashtable
 */
struct sock *gmtp_lookup_established(struct gmtp_sk_hashtable *sk_table,
		 const __be32 saddr, const __be16 sport,
		 const __be32 daddr, const __be16 dport)
{
	struct gmtp_sk_hashitem *result;

	gmtp_pr_func();

	result = __gmtp_lookup_established(sk_table, saddr, sport, daddr, dport);
	if(result)
		return result->sk;

	return NULL;
}
EXPORT_SYMBOL_GPL(gmtp_lookup_established);

