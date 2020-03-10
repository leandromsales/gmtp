/*
 * sock_hashtables.h
 *
 *  Created on: 5 de mar de 2020
 *      Author: wendell
 */

#ifndef GMTP_HASHTABLES_H_
#define GMTP_HASHTABLES_H_

#include <linux/list.h>

#include <net/sock.h>

/*
 * For now, it's a linked list... it's not a hashtable...
 */
struct gmtp_listen_hashitem {
	struct list_head 	list;
	struct sock 		*sk;
};


struct gmtp_listen_hashtable {
	unsigned int 		max_size;
	unsigned int		len;

	struct gmtp_listen_hashitem *entry;
};

int gmtp_build_listen_hashtable(struct gmtp_listen_hashtable *table,
		size_t max_size);
int gmtp_sk_listen_start(struct gmtp_listen_hashtable *table, struct sock *sk);

struct sock *gmtp_lookup_listener(struct gmtp_listen_hashtable *table,
		 const __be32 server_addr, const __be16 server_port);

#endif /* GMTP_HASHTABLES_H_ */
