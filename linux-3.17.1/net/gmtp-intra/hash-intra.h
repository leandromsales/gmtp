/*
 * hash-intra.h
 *
 *  Created on: 03/05/2015
 *      Author: wendell
 */

#ifndef HASH_INTRA_H_
#define HASH_INTRA_H_

#define GMTP_HASH_SIZE  16

/**
 * struct gmtp_relay_entry: entry in GMTP-Intra Relay Table
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
 * @next: pointer to next gmtp_relay_entry
 */
struct gmtp_relay_entry {
	__u8 flowname[GMTP_FLOWNAME_LEN];
	__be32 server_addr;
	__be32 *relay;
	__be16 media_port;
	__be32 channel_addr;
	__be16 channel_port;
	__u8 state :3;
	struct gmtp_flow_info *info;

	struct gmtp_relay_entry *next;
};

/**
 * struct gmtp_flow_info - Control information for media transmission
 *
 * @iseq: initial sequence number of received packets
 * @seq: sequence number of last received packet
 * @nbytes: amount of received bytes
 * @current_tx: Current max tx (via GMTP-MCC). 0 means unlimited.
 * @last_rx_tstamp: time stamp of last received data packet
 * @data_pkt_tx: number of data packets transmitted
 * @buffer: buffer of GMTP-Data packets
 * @buffer_size: size (in bytes) of GMTP-Data buffer.
 * @buffer_len: number of packets in GMTP-Data buffer]
 * @buffer_size: max number of packets in buffer
 */
struct gmtp_flow_info {
	unsigned int 		iseq;
	unsigned int 		seq;
	unsigned int 		nbytes;

	u64 			current_tx;
	ktime_t 		last_rx_tstamp;
	unsigned int 		data_pkt_tx;

	struct gmtp_client	*clients;
	unsigned int 		nclients;

	struct sk_buff_head 	*buffer;
	unsigned int 		buffer_size;
	unsigned int 		buffer_max;
#define buffer_len 		buffer->qlen
};

/**
 * State of a flow
 */
enum {
	GMTP_INTRA_WAITING_REGISTER_REPLY,
	GMTP_INTRA_REGISTER_REPLY_RECEIVED,
	GMTP_INTRA_TRANSMITTING,
	GMTP_INTRA_CLOSE_RECEIVED,
	GMTP_INTRA_CLOSED
};

/*
 * struct gmtp_intra_hashtable: GMTP-Intra Relay Table
 */
struct gmtp_intra_hashtable {
	int size;
	struct gmtp_relay_entry **table;
};

/** hash.c */
struct gmtp_intra_hashtable *gmtp_intra_create_hashtable(unsigned int size);
struct gmtp_relay_entry *gmtp_intra_lookup_media(
		struct gmtp_intra_hashtable *hashtable, const __u8 *media);
int gmtp_intra_add_entry(struct gmtp_intra_hashtable *hashtable, __u8 *flowname,
		__be32 server_addr, __be32 *relay, __be16 media_port,
		__be32 channel_addr, __be16 channel_port);
struct gmtp_relay_entry *gmtp_intra_del_entry(
		struct gmtp_intra_hashtable *hashtable, __u8 *media);

void kfree_gmtp_intra_hashtable(struct gmtp_intra_hashtable *hashtable);

#endif /* HASH_INTRA_H_ */
