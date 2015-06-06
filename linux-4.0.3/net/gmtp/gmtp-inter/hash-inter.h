/*
 * hash-inter.h
 *
 *  Created on: 03/05/2015
 *      Author: wendell
 */

#ifndef HASH_INTER_H_
#define HASH_INTER_H_

#define GMTP_HASH_SIZE  16

/**
 * struct gmtp_relay_entry: entry in GMTP-inter Relay Table
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
	__be32			my_addr;
	__be16			my_port;

	unsigned int 		iseq;
	unsigned int 		seq;
	unsigned int 		nbytes;
    unsigned int        total_bytes;
	u64 			current_tx;
	ktime_t 		last_rx_tstamp;
	unsigned int 		data_pkt_tx;

	struct gmtp_client	*clients;
	unsigned int 		nclients;

	struct sk_buff_head 	*buffer;
	unsigned int 		buffer_size;
	unsigned int 		buffer_min;
	unsigned int 		buffer_max;  /* buffer_min * 3 */
#define buffer_len 		buffer->qlen
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
	struct gmtp_relay_entry **table;
};

/** hash.c */
struct gmtp_inter_hashtable *gmtp_inter_create_hashtable(unsigned int size);
struct gmtp_relay_entry *gmtp_inter_lookup_media(
		struct gmtp_inter_hashtable *hashtable, const __u8 *media);
int gmtp_inter_add_entry(struct gmtp_inter_hashtable *hashtable, __u8 *flowname,
		__be32 server_addr, __be32 *relay, __be16 media_port,
		__be32 channel_addr, __be16 channel_port);
struct gmtp_relay_entry *gmtp_inter_del_entry(
		struct gmtp_inter_hashtable *hashtable, __u8 *media);

void kfree_gmtp_inter_hashtable(struct gmtp_inter_hashtable *hashtable);

#endif /* HASH_INTER_H_ */
