/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef HASH_INTRA_H_
#define HASH_INTRA_H_
/*
 *  net/gmtp/hash/hash.h
 *
 *  Hash table for the GMTP protocol
 *  Copyright (c) 2015 Wendell Silva Soares <wendell@ic.ufal.br>
 */

#define GMTP_HASH_KEY_LEN  16

struct gmtp_hashtable;
struct gmtp_hash_entry;

/**
 * struct gmtp_hash_ops - The GMTP hash table operations
 */
struct gmtp_hash_ops {
	unsigned int (*hashval)(struct gmtp_hashtable *table,
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
 * @len: 		the current number of entries in hash table
 * @gmtp_hash_ops: 	the operations of hashtable
 * @table:		the array of table entries
 * 			(it can be a client or a server entry)
 */
struct gmtp_hashtable {
	unsigned int 			size;
	unsigned int			len;

	struct gmtp_hash_entry		**entry;
	struct gmtp_hash_ops		hash_ops;

	unsigned int (*hashval)(struct gmtp_hashtable *table,
			const __u8 *key);
	int (*add_entry)(struct gmtp_hashtable *table,
			struct gmtp_hash_entry *entry);
	void (*del_entry)(struct gmtp_hashtable *table, const __u8 *key);
	void (*destroy)(struct gmtp_hashtable *table);
};

/** hash.c */
int gmtp_build_hashtable(struct gmtp_hashtable *htable,
		unsigned int size, struct gmtp_hash_ops hash_ops);
unsigned int gmtp_hashval(struct gmtp_hashtable *table, const __u8 *key);
struct gmtp_hash_entry *gmtp_lookup_entry(struct gmtp_hashtable *table,
		const __u8 *key);
int gmtp_add_entry(struct gmtp_hashtable *table, struct gmtp_hash_entry *entry);
void destroy_gmtp_hashtable(struct gmtp_hashtable *table);
void kfree_gmtp_hashtable(struct gmtp_hashtable *table);

/**
 * struct gmtp_client_entry - An entry in client hash table
 * @entry: hash entry
 *
 * @clients: list of clients connected to media P
 * @channel_addr: multicast channel to receive media
 * @channel_port: multicast port to receive media
 */
struct gmtp_client_entry {
	/* gmtp_hash_entry has to be the first member of gmtp_client_entry */
	struct gmtp_hash_entry		entry;

	struct gmtp_client 		*clients;
	__be32 				channel_addr;
	__be16 				channel_port;
};


struct gmtp_client_entry *gmtp_add_client_entry(struct gmtp_hashtable *table,
		const __u8 *flowname, __be32 local_addr, __be16 local_port,
		__be32 channel_addr, __be16 channel_port);
struct gmtp_client_entry *gmtp_lookup_client(struct gmtp_hashtable *table,
		const __u8 *key);
void gmtp_del_client_entry(struct gmtp_hashtable *table, const __u8 *key);



/** Servers **/

/**
 * struct gmtp_relay_table_entry - An entry in relays hash table (in server)
 *
 * @entry: hash entry
 *
 * @relay_list:  the list head of
 *
 * @relay: ID and IP of relay
 * @nrelays: number of relays of route (if this is the last relay of route)
 * @sk: socket to last relay of route
 */
struct gmtp_relay_entry {
	struct gmtp_hash_entry		entry;

	struct list_head 		relay_list;
	struct list_head 		path_list;

	struct gmtp_hdr_relay 		relay;
	__u8 				nrelays;
	struct sock 			*sk;
};

/**
 * struct gmtp_server_entry - An entry in server hash table
 *
 * @entry: hash entry
 * @relay_hashtable: a hash table of all relays in network
 * @relay_list: a list of all relays in network (relay tree)
 * @nroutes: the number of routes stored in server for media P
 */
struct gmtp_server_entry {
	struct gmtp_hash_entry		entry;
	struct gmtp_hashtable 		*relay_hashtable;
	struct gmtp_relay_entry		relays; /* list of routes */
	unsigned int 			nrelays;
	struct sock 			*first_sk;
};

int gmtp_add_server_entry(struct gmtp_hashtable *table, struct sock *sk,
		struct sk_buff *skb);



/** GMTP Clients **/

/**
 * struct gmtp_clients - A list of GMTP Clients
 *
 * @list: The list_head
 * @id: a number to identify and count clients
 * @addr: IP address of client
 * @port: reception port of client
 * @max_clients: for reporters, the max amount of clients.
 * 			0 means that clients is not a reporter
 * @nclients: number of occupied slots at a reporter.
 * 			It must be less or equal %max_clients
 *
 * @ack_rx_tstamp: time stamp of last received ack (or feedback)
 *
 * @clients: clients of a reporter.
 * @reporter: reporter of a client
 * @rsock: socket to reporter
 * @mysock: socket of the client
 * @mac_addr: MAC addr of client
 */
struct gmtp_client {
	struct list_head 	list;
	u32			id;
	__be32 			addr;
	__be16 			port;
	__u8			max_nclients;
	__u8			nclients;
	__u32			ack_rx_tstamp;

	struct gmtp_client	*clients;
	struct gmtp_client	*reporter;
	struct sock 		*rsock;
	struct sock 		*mysock;
	unsigned char 		mac_addr[6];
};

struct gmtp_client *gmtp_create_client(__be32 addr, __be16 port, __u8 max_nclients);
struct gmtp_client *gmtp_list_add_client(u32 id, __be32 addr, __be16 port,
		__u8 max_nclients, struct list_head *head);

struct gmtp_client* gmtp_get_first_client(struct list_head *head);
struct gmtp_client* gmtp_get_client(struct list_head *head, __be32 addr, __be16 port);
struct gmtp_client* gmtp_get_client_by_id(struct list_head *head, u32 id);

int gmtp_delete_clients(struct list_head *list, __be32 addr, __be16 port);
void print_gmtp_client(struct gmtp_client *c);


#endif /* HASH_H_ */
