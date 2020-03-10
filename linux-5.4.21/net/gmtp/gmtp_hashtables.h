/*
 * sock_hashtables.h
 *
 *  Created on: 5 de mar de 2020
 *      Author: wendell
 */

#ifndef GMTP_HASHTABLES_H_
#define GMTP_HASHTABLES_H_

#include <linux/list.h>
#include <linux/types.h>

#include <net/sock.h>

/*
 * For now, it's a linked list... it's not a hashtable...
 */
struct gmtp_sk_hashitem {
	unsigned int count;
	struct list_head 	list;
	struct sock 		*sk;
};


struct gmtp_sk_hashtable {
	struct gmtp_sk_hashitem *hbind;
	struct gmtp_sk_hashitem *hlisten;
};

int gmtp_build_sk_hashtable(struct gmtp_sk_hashtable *sk_table);

int gmtp_sk_listen_start(struct gmtp_sk_hashtable *sk_table, struct sock *sk);

struct sock *gmtp_lookup_listener(struct gmtp_sk_hashtable *sk_table,
		 const __be32 addr, const __be16 port);

int gmtp_sk_hash_connect(struct gmtp_sk_hashtable *sk_table, struct sock *sk);

struct sock *gmtp_lookup_established(struct gmtp_sk_hashtable *sk_table,
		 const __be32 saddr, const __be16 sport,
		 const __be32 daddr, const __be16 dport);

#endif /* GMTP_HASHTABLES_H_ */
