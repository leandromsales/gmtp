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
#include "gmtp_hashtables.h"

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

	if(sk_table->hlisten == NULL)
		return NULL;

	list_for_each_entry(pos, &sk_table->hlisten->list, list)
	{
		if(pos == NULL)
			continue;
		if (pos->sk == NULL)
			continue;

		inet = inet_sk(pos->sk);
		
		if(inet->inet_sport == port)
		{
			if(inet->inet_rcv_saddr == 0) /* addr: 0.0.0.0 (using INADDR_ANY causes kernel panic (!?) */
				return pos;

			if (inet->inet_rcv_saddr == addr)
				return pos;
		}
	}
	return NULL;
}

int gmtp_sk_hash_listener(struct gmtp_sk_hashtable *sk_table, struct sock *sk)
{
	struct gmtp_sk_hashitem *new_item;
	struct inet_sock *inet = inet_sk(sk);

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
EXPORT_SYMBOL_GPL(gmtp_sk_hash_listener);

/**
 * Lookup listener sk in GMTP Listen Hashtable
 */
struct sock *gmtp_lookup_listener(struct gmtp_sk_hashtable *sk_table,
		 const __be32 addr, const __be16 port)
{
	struct gmtp_sk_hashitem *result;

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

	if(sk_table->hbind == NULL)
		return NULL;

	list_for_each_entry(pos, &sk_table->hbind->list, list)
	{
		if(pos == NULL)
			continue;
		if (pos->sk == NULL)
			continue;

		inet = inet_sk(pos->sk);

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
			return pos;
		}
	}
	return NULL;
}

int __gmtp_add_established_item(struct gmtp_sk_hashtable *sk_table,
		struct gmtp_sk_hashitem *item)
{
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

	new_item = kmalloc(sizeof(struct gmtp_sk_hashitem), GFP_ATOMIC);

	if (new_item == NULL)
		return -ENOMEM;

	new_item->sk = sk;
	gmtp_pr_info("Adding to GMTP_EHASH: [%pI4@%-5d <=> %pI4@%-5d]",
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

	result = __gmtp_lookup_established(sk_table, saddr, sport, daddr, dport);
	if(result)
		return result->sk;

	return NULL;
}
EXPORT_SYMBOL_GPL(gmtp_lookup_established);

/**
 * @sk_table: hashtable
 * @item_head: pointer to itens
 * @sk: Sock struct
 */
int __gmtp_del_sk_hash(struct gmtp_sk_hashtable *sk_table,
		struct gmtp_sk_hashitem* item_head, struct sock *sk)
{
	struct gmtp_sk_hashitem *pos, *tmp;

	if(item_head == NULL)
		return -ENOENT;

	list_for_each_entry_safe(pos, tmp, &item_head->list, list)
	{
		if(pos == NULL)
			continue;
		if (pos->sk == NULL)
			continue;

		print_gmtp_sock(pos->sk);

		if (pos->sk == sk)
		{
			list_del(&pos->list);
			kfree(pos);
			return 0;
		}
	}
	return -ENOENT;
}

/*
 * Removing non-listening sockets
 */
int gmtp_del_sk_ehash(struct gmtp_sk_hashtable *sk_table, struct sock *sk)
{
	return __gmtp_del_sk_hash(sk_table, sk_table->hbind, sk);
}
EXPORT_SYMBOL_GPL(gmtp_del_sk_ehash);

/*
 * Removing listening sockets
 */
int gmtp_del_sk_lhash(struct gmtp_sk_hashtable *sk_table, struct sock *sk)
{
	return __gmtp_del_sk_hash(sk_table, sk_table->hlisten, sk);
}
EXPORT_SYMBOL_GPL(gmtp_del_sk_lhash);

/**
 * Insert sk into established hash table, removing osk (if osk is not null)
 */
void gmtp_sk_ehash_insert(struct gmtp_sk_hashtable *sk_table,
		struct sock *sk, struct sock *osk)
{
	if(osk)
		gmtp_del_sk_ehash(sk_table, osk);

	if(sk)
		gmtp_sk_hash_connect(sk_table, sk);
}
EXPORT_SYMBOL_GPL(gmtp_sk_ehash_insert);


