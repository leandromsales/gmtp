/*
 * hash-inter.h
 *
 *  Created on: 03/05/2015
 *      Author: wendell
 */

#ifndef HASH_INTER_H_
#define HASH_INTER_H_

#define GMTP_HASH_KEY_LEN  16

/**
 * struct gmtp_inter_entry: entry in GMTP-inter Relay Table
 *
 * @flowname: Name of dataflow
 * @server_addr: IP address of media server
 * @relay: list of relays
 * @media_port: port of media at server
 * @channel_addr: IP address of media multicast channel
 * @channel_port: Port of media multicast channel
 * @state: state of media transmission
 * @info: control information for media transmission
 *
 * @next: pointer to next gmtp_inter_entry
 */
struct gmtp_inter_entry {
	__u8 flowname[GMTP_FLOWNAME_LEN];
	__be32 server_addr;
	unsigned char server_mac_addr[6];
	__be32 *relay;
	__be16 media_port;
	__be32 channel_addr;
	__be16 channel_port;
	__u8 state :3;

	struct timer_list ack_timer_entry;

	struct gmtp_flow_info *info;
	struct gmtp_inter_entry *next;
};

/**
 * struct gmtp_flow_info - Control information for media transmission
 *
 * @seq: sequence number of last received packet
 * @total_bytes: amount of received bytes
 * @last_rx_tstamp: time stamp of last received data packet (milliseconds)
 * @last_data_tstamp: time stamp stored in last received data packet.
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
 * @clients: list of reporters connected to relay
 * @nclients: number of clients connected to relay
 * @cur_reporter: current reporter (to connect new clients)
 *
 * @buffer: buffer of GMTP-Data packets
 * @buffer_size: size (in bytes) of GMTP-Data buffer.
 * @buffer_len: number of packets in GMTP-Data buffer]
 * @buffer_size: max number of packets in buffer
 * @buffer_len: buffer length in bytes
 */
struct gmtp_flow_info {
	__be32			my_addr;
	__be16			my_port;

	unsigned int		seq;
	unsigned int 		total_bytes;
	unsigned long  		last_rx_tstamp; /* milliseconds */
	__be32 			last_data_tstamp;
	__be32 			rcv_tx_rate;

	/* GMTP-MCC */
	unsigned int		nfeedbacks;
	unsigned int		sum_feedbacks;
	unsigned int        	recent_bytes;
	unsigned long  		recent_rx_tstamp;
	unsigned int 		current_rx;
	unsigned int 		required_tx;
	unsigned int 		data_pkt_out;
	unsigned int 		flow_rtt;
	unsigned int 		flow_avg_rtt;
	struct timer_list 	mcc_timer;

	struct gmtp_client	*clients;
	unsigned int 		nclients;
	struct gmtp_client 	*cur_reporter;

	struct sk_buff_head 	*buffer;
	unsigned int 		buffer_min;
	unsigned int 		buffer_max;  /* buffer_min * 3 */
	unsigned int 		buffer_len; /* in bytes */
};

static inline void gmtp_set_buffer_limits(struct gmtp_flow_info *info,
		unsigned int buffer_min)
{
	info->buffer_min = buffer_min;
	info->buffer_max = info->buffer_min * 3;
}

/**
 * State of a flow
 */
enum {
	GMTP_INTER_WAITING_REGISTER_REPLY,
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
void ack_timer_callback(struct gmtp_inter_entry *entry);
int gmtp_inter_add_entry(struct gmtp_inter_hashtable *hashtable, __u8 *flowname,
		__be32 server_addr, __be32 *relay, __be16 media_port,
		__be32 channel_addr, __be16 channel_port);
struct gmtp_inter_entry *gmtp_inter_del_entry(
		struct gmtp_inter_hashtable *hashtable, __u8 *media);

void kfree_gmtp_inter_hashtable(struct gmtp_inter_hashtable *hashtable);

#endif /* HASH_INTER_H_ */
