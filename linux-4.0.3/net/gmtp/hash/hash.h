/*
 * hash.h
 *
 *  Created on: 07/05/2015
 *      Author: wendell
 */

#ifndef HASH_INTRA_H_
#define HASH_INTRA_H_

#define GMTP_HASH_KEY_LEN  16

struct gmtp_hashtable;

/**
 * struct gmtp_hash_ops - The GMTP hash table operations
 */
struct gmtp_hash_ops {
	unsigned int (*hash)(struct gmtp_hashtable *table, const __u8 *key);
	struct gmtp_hash_entry *(*lookup)(struct gmtp_hashtable *table,
			const __u8 *key);
	int (*add_entry)(struct gmtp_hashtable *table,
			struct gmtp_hash_entry *entry);
	void (*del_entry)(struct gmtp_hashtable *table, const __u8 *key);
	void (*destroy)(struct gmtp_hashtable *table);
};

extern const struct gmtp_hash_ops gmtp_client_hash_ops;
extern const struct gmtp_hash_ops gmtp_server_hash_ops;

/**
 * struct gmtp_hash_entry - The GMTP hash table entry
 *
 * @flowname:	the key of hash table entry
 * @next: 	the next entry with the same key (hash)
 */
struct gmtp_hash_entry {
	__u8 				key[GMTP_HASH_KEY_LEN];
	struct gmtp_hash_entry		*next;
};

/**
 * struct gmtp_hashtable - The GMTP hash table
 *
 * @size: 		the max number of entries in hash table (fixed)
 * @gmtp_hash_ops: 	the operations of hashtable
 * @table:		the array of table entries
 * 			(it can be a client or a server entry)
 */
struct gmtp_hashtable {
	int 				size;

	struct gmtp_hash_entry		**entry;
	struct gmtp_hash_ops		hash_ops;
};

/** hash.c */
struct gmtp_hashtable *gmtp_build_hashtable(unsigned int size,
		struct gmtp_hash_ops hash_ops);
unsigned int gmtp_hash(struct gmtp_hashtable *table, const __u8 *key);
struct gmtp_hash_entry *gmtp_lookup_entry(struct gmtp_hashtable *table,
		const __u8 *key);
int gmtp_add_entry(struct gmtp_hashtable *table, struct gmtp_hash_entry *entry);
void destroy_gmtp_hashtable(struct gmtp_hashtable *table);
void kfree_gmtp_hashtable(struct gmtp_hashtable *table);

/**
 * struct gmtp_client_entry - An entry in client hash table
 *
 * clients: clients connected to flowname
 * channel_addr: multicast channel to receive media
 * channel_port: multicast port to receive media
 */
struct gmtp_client_entry {
	/* gmtp_hash_entry has to be the first member of gmtp_client_entry */
	struct gmtp_hash_entry		entry;

	struct gmtp_client 		*clients;
	__be32 				channel_addr;
	__be16 				channel_port;
};


int gmtp_add_client_entry(struct gmtp_hashtable *table,
		const __u8 *flowname, __be32 local_addr, __be16 local_port,
		__be32 channel_addr, __be16 channel_port);
struct gmtp_client_entry *gmtp_lookup_client(struct gmtp_hashtable *table,
		const __u8 *key);
void gmtp_del_client_entry(struct gmtp_hashtable *table, const __u8 *key);

/** Servers **/

/**
 * struct gmtp_server_entry - An entry in server hash table
 *
 * @srelay: 	source of route (route[route->nrelays]).
 * 			the primary key at table is 'srelay->relayid'
 * @route:	the route stored in table
 * @next: 	the next entry with the same key (hash)
 */
struct gmtp_server_entry {
	/* gmtp_hash_entry has to be the first member of gmtp_client_entry */
	struct gmtp_hash_entry		entry;

	struct gmtp_hashtable 		*relay_hashtable;
};

int gmtp_add_server_entry(struct gmtp_hashtable *table, const __u8 *flowname,
		struct gmtp_hdr_route *route);

/**
 * struct gmtp_relay_table_entry - An entry in relays hash table (in server)
 * @next: 	the next entry with the same key (hash)
 *
 * @relay: the relay info
 * @nextRelay: 	the next relay in path
 * @relay_id: the relay id (key)
 * @relay_ip: the relay ip
 *
 * @list:  the list head
 */
struct gmtp_relay_entry {
	struct gmtp_hash_entry		entry;

	struct gmtp_relay 		relay;
	struct gmtp_relay_entry		*nextRelay;
	struct gmtp_relay_entry		*prevRelayList;

	struct list_head 		list;
};




#endif /* HASH_INTRA_H_ */
