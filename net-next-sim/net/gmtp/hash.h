/*
 * hash.h
 *
 *  Created on: 07/05/2015
 *      Author: wendell
 */

#ifndef HASH_H_
#define HASH_H_

#include "gmtp.h"

#define GMTP_HASH_SIZE  16

/**
 * struct gmtp_client_entry - An entry in client hash table
 *
 * flowname[GMTP_FLOWNAME_LEN];
 * local_addr:	local client IP address
 * local_port:	local client port
 * channel_addr: multicast channel to receive media
 * channel_port: multicast port to receive media
 */
struct gmtp_client_entry {
	struct gmtp_client_entry *next; /** It must be the first */

	__u8 			flowname[GMTP_FLOWNAME_LEN];

	struct gmtp_client 	*clients;
	__be32 			channel_addr;
	__be16 			channel_port;
};


/**
 * struct gmtp_server_entry - An entry in server hash table
 *
 * @next: 	the next entry with the same key (hash)
 * @srelay: 	source of route (route[route->nrelays]).
 * 			the primary key at table is 'srelay->relayid'
 * @route:	the route stored in table
 */
struct gmtp_server_entry {
	struct gmtp_server_entry *next; /** It must be the first */

	struct gmtp_relay *srelay;
	__u8 flowname[GMTP_FLOWNAME_LEN];
	struct gmtp_hdr_route route;
};

/**
 * struct gmtp_hashtable - The GMTP hash table
 *
 * @size: 	the max number of entries in hash table (fixed)
 * @table:	the array of table entries
 * 			(it can be a client or a server entry)
 */
/* TODO Make routes_entry and relays_entry
 * Make a list of relays instead a array... */
struct gmtp_hashtable {
	int size;
	struct gmtp_client_entry **client_table;
	struct gmtp_server_entry **server_table;
};

/** Client */
struct gmtp_hashtable *gmtp_create_hashtable(unsigned int size);
struct gmtp_client_entry *gmtp_lookup_client(
		struct gmtp_hashtable *hashtable, const __u8 *media);
int gmtp_add_client_entry(struct gmtp_hashtable *hashtable, __u8 *flowname,
		__be32 local_addr, __be16 local_port,
		__be32 channel_addr, __be16 channel_port);
void gmtp_del_client_entry(struct gmtp_hashtable *hashtable, __u8 *media);

/** Server */
struct gmtp_server_entry *gmtp_lookup_route(
		struct gmtp_hashtable *hashtable, const __u8 *relayid);
int gmtp_add_server_entry(struct gmtp_hashtable *hashtable, __u8 *relayid,
		__u8 *flowname, struct gmtp_hdr_route *route);

void kfree_gmtp_hashtable(struct gmtp_hashtable *hashtable);

#endif /* HASH_H_ */
