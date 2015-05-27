 /* gmtp-inter.h
 *
 *  Created on: 17/02/2015
 *      Author: wendell
 */

#ifndef GMTP_INTER_H_
#define GMTP_INTER_H_

#include <linux/types.h>
#include <linux/skbuff.h>
#include <linux/spinlock.h>
#include <uapi/linux/ip.h>

#include <linux/gmtp.h>
#include <uapi/linux/gmtp.h>
#include "../gmtp.h"

#include "hash-inter.h"
#include "ucc.h"

#define H_USER 	1024
#define MD5_LEN GMTP_RELAY_ID_LEN

extern const char *gmtp_packet_name(const int);
extern const char *gmtp_state_name(const int);
extern void flowname_str(__u8* str, const __u8* flowname);
extern void gmtp_list_add_client(unsigned int id, __be32 addr,
		__be16 port, __u8 reporter, struct list_head *head);

/**
 * TODO Negotiate buffer size with server
 * struct gmtp_inter - GMTP-inter state variables
 *
 * @total_bytes_rx: total data bytes received
 * @total_rx: Current RX rate
 * @mcst: control of granted multicast addresses
 * @nclients: number of connected clients
 *
 * @hashtable: GMTP-inter relay table
 */
struct gmtp_inter {
	unsigned char		relay_id[GMTP_RELAY_ID_LEN];

	unsigned int 		total_bytes_rx;
	unsigned int 		total_rx;
	unsigned char		mcst[4];

	struct gmtp_inter_hashtable *hashtable;
};

extern struct gmtp_inter gmtp_inter;

/** gmtp-inter.c */
__be32 get_mcst_v4_addr(void);
void gmtp_buffer_add(struct gmtp_flow_info *info, struct sk_buff *newsk);
struct sk_buff *gmtp_buffer_dequeue(struct gmtp_flow_info *info);
struct gmtp_flow_info *gmtp_inter_get_info(
		struct gmtp_inter_hashtable *hashtable, const __u8 *media);
__be32 gmtp_inter_device_ip(struct net_device *dev);
unsigned char *gmtp_build_md5(unsigned char *buf);

/** input.c */
int gmtp_inter_request_rcv(struct sk_buff *skb);
int gmtp_inter_register_reply_rcv(struct sk_buff *skb);
int gmtp_inter_ack_rcv(struct sk_buff *skb);
int gmtp_inter_data_rcv(struct sk_buff *skb);
int gmtp_inter_feedback_rcv(struct sk_buff *skb);
int gmtp_inter_close_rcv(struct sk_buff *skb);

/** Output.c */
void gmtp_inter_add_relayid(struct sk_buff *skb);
struct gmtp_hdr *gmtp_inter_make_route_hdr(struct sk_buff *skb);
struct gmtp_hdr *gmtp_inter_make_request_notify_hdr(struct sk_buff *skb,
		struct gmtp_relay_entry *media_info, __be16 new_sport,
		__be16 new_dport, __u8 error_code);
int gmtp_inter_make_request_notify(struct sk_buff *skb, __be32 new_saddr,
		__be16 new_sport, __be32 new_daddr, __be16 new_dport,
		__u8 error_code);

void gmtp_inter_build_and_send_pkt(struct sk_buff *skb_src, __be32 saddr,
		__be32 daddr, struct gmtp_hdr *gh_ref, bool backward);
void gmtp_inter_build_and_send_skb(struct sk_buff *skb);

int gmtp_inter_register_out(struct sk_buff *skb);
int gmtp_inter_request_notify_out(struct sk_buff *skb);
int gmtp_inter_data_out(struct sk_buff *skb);
int gmtp_inter_close_out(struct sk_buff *skb);


/**
 * A very ugly delayer, to GMTP-inter...
 *
 * If we use schedule(), we get this error:  'BUG: scheduling while atomic'
 *
 * We cannot use schedule(), because hook functions are atomic,
 * and sleeping in kernel code is not allowed in atomic context.
 *
 * Calling cond_resched(), kernel call schedule() where it's possible...
 */
static inline void gmtp_inter_wait_us(s64 delay)
{
	ktime_t timeout = ktime_add_us(ktime_get_real(), delay);
	while(ktime_before(ktime_get_real(), timeout)) {
		cond_resched(); /* Do nothing, just wait... */
	}
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

static inline int bytes_added(int sprintf_return)
{
	return (sprintf_return > 0) ? sprintf_return : 0;
}

static inline void flowname_strn(__u8* str, const __u8 *buffer, int length)
{

	int i;
	for(i = 0; i < length; ++i) {
		sprintf(&str[i*2], "%02x", buffer[i]);
		/*printk("testando = %02x\n", buffer[i]); */
	}
}

#endif /* GMTP_INTER_H_ */
