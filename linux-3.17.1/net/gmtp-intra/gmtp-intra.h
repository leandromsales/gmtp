 /* gmtp-intra.h
 *
 *  Created on: 17/02/2015
 *      Author: wendell
 */

#ifndef GMTP_INTRA_H_
#define GMTP_INTRA_H_

#include <linux/types.h>
#include <linux/skbuff.h>
#include <uapi/linux/ip.h>

#include <linux/gmtp.h>
#include <uapi/linux/gmtp.h>
#include "../gmtp/gmtp.h"

struct gmtp_intra {
	unsigned long seq;
	unsigned int npackets;
	unsigned int nbytes;
	unsigned int current_tx;

	unsigned char mcst[4];

	struct sk_buff_head *buffer;
	unsigned int buffer_size;
};

extern struct gmtp_intra gmtp;

#define GMTP_HASH_SIZE  16
#define H_USER 1024

struct gmtp_relay_entry {
	__u8 flowname[GMTP_FLOWNAME_LEN];
	__be32 server_addr;
	__be32 *relay;
	__be16 media_port;
	__be32 channel_addr;
	__be16 channel_port;
	__u8 state :1;
	struct gmtp_relay_entry *next;
};

struct gmtp_intra_hashtable {
	int size;
	struct gmtp_relay_entry **table;
};

enum {
	GMTP_INTRA_WAITING_REGISTER_REPLY,
	GMTP_INTRA_REGISTER_REPLY_RECEIVED
};

extern struct gmtp_intra_hashtable* relay_hashtable;

/** hash.c */
struct gmtp_intra_hashtable *gmtp_intra_create_hashtable(unsigned int size);
struct gmtp_relay_entry *gmtp_intra_lookup_media(
		struct gmtp_intra_hashtable *hashtable, const __u8 *media);
int gmtp_intra_add_entry(struct gmtp_intra_hashtable *hashtable, __u8 *flowname,
		__be32 server_addr, __be32 *relay, __be16 media_port,
		__be32 channel_addr, __be16 channel_port);
struct gmtp_relay_entry *gmtp_intra_del_entry(struct gmtp_intra_hashtable *hashtable, __u8 *media);
void kfree_gmtp_intra_hashtable(struct gmtp_intra_hashtable *hashtable);

/** gmtp-intra.c */
__be32 get_mcst_addr(void);
int gmtp_intra_request_rcv(struct sk_buff *skb);
int gmtp_intra_register_reply_rcv(struct sk_buff *skb);
int gmtp_intra_ack_rcv(struct sk_buff *skb);
int gmtp_intra_data_rcv(struct sk_buff *skb);
void gmtp_intra_relay_get_devices (int option);
const __u8 *gmtp_intra_relay_id (void);
__be32 gmtp_intra_relay_ip (void);

/** Output.c */
void gmtp_intra_add_relayid(struct sk_buff *skb);
struct gmtp_hdr *gmtp_intra_make_route_hdr(struct sk_buff *skb);
struct gmtp_hdr *gmtp_intra_make_request_notify_hdr(struct sk_buff *skb,
		struct gmtp_relay_entry *media_info, __be16 new_sport,
		__be16 new_dport, __u8 error_code);
int gmtp_intra_make_request_notify(struct sk_buff *skb, __be32 new_saddr,
		__be16 new_sport, __be32 new_daddr, __be16 new_dport,
		__u8 error_code);

void gmtp_intra_build_and_send_pkt(struct sk_buff *skb_src, __be32 saddr,
		__be32 daddr, struct gmtp_hdr *gh_ref, bool backward);

/** gmtp-ucc. */
unsigned int gmtp_rtt_average(void);
unsigned int gmtp_rx_rate(void);
unsigned int gmtp_relay_queue_size(void);
unsigned int gmtp_get_current_tx_rate(void);
void gmtp_update_tx_rate(unsigned int h_user);




/** Printers **/
static inline void print_packet(struct iphdr *iph, bool in)
{
	const char *type = in ? "IN" : "OUT";
	pr_info("%s: %lu | Src=%pI4 | Dst=%pI4 | Proto: %d | "
			"Len: %d bytes\n",
			type,
			gmtp.seq,
			&iph->saddr,
			&iph->daddr,
			iph->protocol,
			ntohs(iph->tot_len));
}

static inline void print_gmtp_packet(struct iphdr *iph, struct gmtp_hdr *gh)
{
	__u8 flowname[GMTP_FLOWNAME_STR_LEN];
	flowname_str(flowname, gh->flowname);
	pr_info("%s (%d) src=%pI4@%-5d dst=%pI4@%-5d seq=%u rtt=%u ms "
			" tx=%u flow=%s\n",
				gmtp_packet_name(gh->type),
				gh->type,
				&iph->saddr, ntohs(gh->sport),
				&iph->daddr, ntohs(gh->dport),
				gh->seq,
				gh->server_rtt,
				gh->transm_r,
				flowname);
}

#endif /* GMTP_INTRA_H_ */
