/*
 * hash-inter.h
 *
 *  Created on: 03/05/2015
 *      Author: wendell
 */

#ifndef HASH_INTER_H_
#define HASH_INTER_H_

#define GMTP_HASH_KEY_LEN  16

#include <linux/netdevice.h>
#include "../hash.h"
#include "gmtp-inter.h"

/**
 * struct gmtp_inter_entry: entry in GMTP-inter Relay Table
 *
 * @flowname: Name of dataflow
 * @server_addr: IP address of media server
 * @relays: list of relays connected to this relay
 * @nrelays: number of relays connected to this relay
 * @media_port: port of media at server
 * @channel_addr: IP address of media multicast channel
 * @channel_port: Port of media multicast channel
 * @state: state of media transmission
 *
 *  Control information for media transmission:
 *
 * @seq: sequence number of last received packet
 * @total_bytes: amount of received bytes
 * @last_rx_tstamp: time stamp of last received data packet (milliseconds)
 * @last_data_tstamp: time stamp stored in last received data packet.
 * @transm_r: default tx rate of server
 *
 * FIXME now, rcv_tx_rate is updated via acks (r -> s). his is wrong...
 * @rcv_tx_rate: tx rate received from relays (or server) in path s->r
 *
 * @nfeedbacks: number of received feedbacks at last window
 * @sum_feedbacks: sum of all feedbacks tx rates received at last window
 * @recent_bytes: amount of received bytes in last window for calcs of RX
 * @recent_rx_tstamp: time stamp of received data packet in calcs of RX
 * @current_rx: current rx_rate calculated
 * @required_tx: Max tx (via GMTP-MCC). 0 means unlimited.
 * @data_pkt_out: number of data packets transmitted
 * @RTT: RTT from server to client (available in data packets)
 * @mcc_timer: Timer to control mcc tx reduction
 *
 * @clients: list of reporters connected to this relay
 * @nclients: number of clients connected to this relay
 * @cur_reporter: current reporter (to connect new clients)
 *
 * @buffer: buffer of GMTP-Data packets
 * @buffer_size: size (in bytes) of GMTP-Data buffer.
 * @buffer_len: number of packets in GMTP-Data buffer]
 * @buffer_size: max number of packets in buffer
 * @buffer_len: buffer length in bytes
 *
 * @next: pointer to next gmtp_inter_entry
 */
struct gmtp_inter_entry {
	__u8 flowname[GMTP_FLOWNAME_LEN];
	__be32 server_addr;
	struct gmtp_relay *relays;
	unsigned int nrelays;
	__be16 media_port;
	__be32 channel_addr;
	__be16 channel_port;
	__u8 state :3;

	struct timer_list ack_timer;
	struct timer_list register_timer;

	unsigned char server_mac_addr[6];
	unsigned char request_mac_addr[6];

	/* Information of transmission state */
	__be32 my_addr;
	__be16 my_port;

	unsigned int seq;
	unsigned int total_bytes;
	unsigned long last_rx_tstamp; /* milliseconds */
	__be32 last_data_tstamp;
	__be32 transm_r;
	__be32 rcv_tx_rate;
	__u8 ucc_type;
	struct gmtp_inter_ucc_protocol ucc;

	/* GMTP-MCC */
	unsigned int nfeedbacks;
	unsigned int sum_feedbacks;
	unsigned int recent_bytes;
	unsigned long recent_rx_tstamp;
	unsigned int current_rx;
	unsigned int required_tx;
	unsigned int data_pkt_in;
	unsigned int data_pkt_out;
	unsigned int server_rtt;
	unsigned int clients_rtt;
	struct timer_list mcc_timer;

	struct gmtp_client *clients;
	unsigned int nclients;
	struct gmtp_client *cur_reporter;

	struct sk_buff_head *buffer;
	unsigned int buffer_min;
	unsigned int buffer_max; /* buffer_min * 3 */
	unsigned int buffer_len; /* in bytes */

	struct net_device *dev_in;
	struct net_device *dev_out;
	bool route_pending;

	struct gmtp_inter_entry *next;
};

static inline void gmtp_set_buffer_limits(struct gmtp_inter_entry *info,
		unsigned int buffer_min)
{
	info->buffer_min = buffer_min;
	info->buffer_max = info->buffer_min * 3;
}

/**
 * State of a flow
 */
enum {
	GMTP_INTER_WAITING_REGISTER_REPLY=0,
	GMTP_INTER_REGISTER_REPLY_RECEIVED,
	GMTP_INTER_TRANSMITTING,
	GMTP_INTER_CLOSE_RECEIVED,
	GMTP_INTER_CLOSED
};

/*
 * struct gmtp_inter_hashtable: GMTP-inter Relay Table
 */
struct gmtp_inter_hashtable {
	int size;
	struct gmtp_inter_entry **table;
};

/** hash.c */
struct gmtp_inter_hashtable *gmtp_inter_create_hashtable(unsigned int size);
struct gmtp_inter_entry *gmtp_inter_lookup_media(
		struct gmtp_inter_hashtable *hashtable, const __u8 *media);
int gmtp_inter_add_entry(struct gmtp_inter_hashtable *hashtable, __u8 *flowname,
		__be32 server_addr, __be32 *relay, __be16 media_port,
		__be32 channel_addr, __be16 channel_port);
struct gmtp_inter_entry *gmtp_inter_del_entry(
		struct gmtp_inter_hashtable *hashtable, __u8 *media);

void kfree_gmtp_inter_hashtable(struct gmtp_inter_hashtable *hashtable);

/**
 * struct gmtp_relay - A list of GMTP Relays
 *
 * @state: state of relay
 * @dev: struct net_device of incoming request from client
 *
 * @tx_rate: max tx rate to relay
 * @tx_byte_budget: the amount of bytes that can be sent immediately.
 */
struct gmtp_relay {
	struct list_head list;
	__be32 addr;
	__be16 port;
	unsigned char mac_addr[6];

	enum gmtp_state state;
	struct net_device *dev;

	/** GMTP-UCC */
	unsigned long tx_rate;
	int tx_byte_budget;
	struct timer_list xmit_timer;
	int losses;
};

/** Relay.c **/

struct gmtp_relay *gmtp_create_relay(__be32 addr, __be16 port);
struct gmtp_relay *gmtp_list_add_relay(__be32 addr, __be16 port,
		struct list_head *head);
struct gmtp_relay *gmtp_inter_create_relay(struct sk_buff *skb,
		struct gmtp_inter_entry *entry);
struct gmtp_relay* gmtp_get_relay(struct list_head *head,
		__be32 addr, __be16 port);
int gmtp_delete_relays(struct list_head *list, __be32 addr, __be16 port);
void print_gmtp_relay(struct gmtp_relay *r);


#endif /* HASH_INTER_H_ */
