 /* gmtp-intra.h
 *
 *  Created on: 17/02/2015
 *      Author: wendell
 */

#ifndef GMTP_INTRA_H_
#define GMTP_INTRA_H_

#include <linux/types.h>
#include <linux/skbuff.h>
#include <linux/spinlock.h>
#include <uapi/linux/ip.h>

#include <linux/gmtp.h>
#include <uapi/linux/gmtp.h>
#include "../gmtp/gmtp.h"

#include "hash-intra.h"

#define H_USER 1024

/**
 * TODO Negotiate buffer size with server
 * struct gmtp_intra - GMTP-Intra state variables
 *
 * @total_bytes_rx: total data bytes received
 * @total_rx: Current RX rate
 * @mcst: control of granted multicast addresses
 * @nclients: number of connected clients
 *
 * @hashtable: GMTP-Intra relay table
 */
struct gmtp_intra {
	unsigned int 		total_bytes_rx;
	unsigned int 		total_rx;
	unsigned char		mcst[4];

	struct gmtp_intra_hashtable *hashtable;
};

extern struct gmtp_intra gmtp;

/** gmtp-intra.c */
__be32 get_mcst_v4_addr(void);
void gmtp_buffer_add(struct gmtp_flow_info *info, struct sk_buff *newsk);
struct sk_buff *gmtp_buffer_dequeue(struct gmtp_flow_info *info);
struct gmtp_flow_info *gmtp_intra_get_info(
		struct gmtp_intra_hashtable *hashtable, const __u8 *media);

/** input.c */
int gmtp_intra_request_rcv(struct sk_buff *skb);
int gmtp_intra_register_reply_rcv(struct sk_buff *skb);
int gmtp_intra_ack_rcv(struct sk_buff *skb);
int gmtp_intra_data_rcv(struct sk_buff *skb);
int gmtp_intra_feedback_rcv(struct sk_buff *skb);

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
void gmtp_intra_build_and_send_skb(struct sk_buff *skb);

int gmtp_intra_data_out(struct sk_buff *skb);
int gmtp_intra_close_out(struct sk_buff *skb);

/** gmtp-ucc. */
unsigned int gmtp_rtt_average(void);
unsigned int gmtp_rx_rate(void);
unsigned int gmtp_relay_queue_size(void);
unsigned int gmtp_get_current_rx_rate(void);
void gmtp_update_rx_rate(unsigned int h_user);

/**
 * A very ugly delayer, to GMTP-Intra...
 *
 * If we use schedule(), we get this error:  'BUG: scheduling while atomic'
 *
 * We cannot use schedule(), because hook functions are atomic,
 * and sleeping in kernel code is not allowed in atomic context.
 *
 * Calling cond_resched(), kernel call schedule() where it's possible...
 */
static inline void gmtp_intra_wait_us(s64 delay)
{
	ktime_t timeout = ktime_add_us(ktime_get_real(), delay);
	while(ktime_before(ktime_get_real(), timeout)) {
		cond_resched(); /* Do nothing, just wait... */
	}
}

/* FIXME Make the real Relay ID */
static const inline __u8 *gmtp_intra_relay_id(void)
{
	return "777777777777777777777";
}

/* FIXME Make the real Relay IP */
static const inline __be32 gmtp_intra_relay_ip(void)
{
	unsigned char *ip = "\xc0\xa8\x02\x01"; /* 192.168.2.1 */
	return *(unsigned int *)ip;
}

/*
 * Print IP packet basic information
 */
static inline void print_packet(struct sk_buff *skb, bool in)
{
	struct iphdr *iph = ip_hdr(skb);
	const char *type = in ? "IN" : "OUT";
	pr_info("%s (%s): Src=%pI4 | Dst=%pI4 | Proto: %d | Len: %d bytes\n",
			type, skb->dev->name,
			&iph->saddr, &iph->daddr,
			iph->protocol,
			ntohs(iph->tot_len));
}

/*
 * Print GMTP packet basic information
 */
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

/*
 * Print Data of GMTP-Data packets
 */
static inline void print_gmtp_data(struct sk_buff *skb, char* label)
{
	__u8* data = gmtp_data(skb);
	__u32 data_len = gmtp_data_len(skb);

	if(data_len > 0) {
		char *lb = label != NULL ? label : "Data";
		unsigned char *data_str = kmalloc(data_len+1, GFP_KERNEL);
		memcpy(data_str, data, data_len);
		data_str[data_len] = '\0';
		pr_info("%s: %s\n", lb, data_str);
	}
}

#endif /* GMTP_INTRA_H_ */
