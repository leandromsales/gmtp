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

#define CAPACITY_DEFAULT 1250000 /* bytes/s => 10 Mbit/s */

extern const char *gmtp_packet_name(const __u8);
extern const char *gmtp_state_name(const int);
extern void flowname_str(__u8* str, const __u8* flowname);
extern void print_gmtp_packet(const struct iphdr *iph, const struct gmtp_hdr *gh);
extern struct gmtp_hashtable* server_hashtable;

/**
 * TODO Negotiate buffer size with server
 * TODO Make kreporter configurable
 *
 * struct gmtp_inter - GMTP-inter state variables
 *
 * @relay_id: Relay unique id
 *
 * @buffer_len: relay buffer occupation (total)
 * @capacity: channel capacity of transmission (bytes/s)
 * @total_bytes_rx: total data bytes received
 * @total_rx: Current total RX rate (bytes/s)
 * @ucc_rx: Current per flow max RX (bytes/s)
 * @ucc_bytes: bytes received since last GMTP-UCC execution
 * @ucc_rx_tstamp: time stamp of last GMTP-UCC execution
 * @rx_rate_wnd: size of window to calculate rx rates
 * @h: Current H_0 in RCP equation
 * @last_rtt: Last RTT received in all flows
 *
 * @mcst: control of granted multicast addresses
 * @kreporter: number of clients per reporter.
 *
 * @hashtable: GMTP-inter relay table
 */
struct gmtp_inter {
	unsigned char		relay_id[GMTP_RELAY_ID_LEN];

	unsigned int 		capacity;
	unsigned int		buffer_len;

	unsigned int 		total_bytes_rx;
	unsigned int 		total_rx;
	unsigned int 		ucc_rx;
	unsigned int        	ucc_bytes;
	unsigned long  		ucc_rx_tstamp;
	unsigned int 		rx_rate_wnd;
	unsigned int		h_user;
	unsigned int		worst_rtt;

	unsigned char		mcst[4];

	unsigned char		kreporter;

	struct timer_list 	gmtp_ucc_timer;

	struct gmtp_inter_hashtable *hashtable;
};

extern struct gmtp_inter gmtp_inter;

enum gmtp_inter_direction {
	GMTP_INTER_FORWARD,
	GMTP_INTER_BACKWARD,
	GMTP_INTER_LOCAL
};

/** gmtp-inter.c */
__be32 get_mcst_v4_addr(void);
void gmtp_buffer_add(struct gmtp_inter_entry *info, struct sk_buff *newsk);
struct sk_buff *gmtp_buffer_dequeue(struct gmtp_inter_entry *info);
__be32 gmtp_inter_device_ip(struct net_device *dev);
unsigned char *gmtp_build_md5(unsigned char *buf);
void gmtp_timer_callback(void);

/** input.c */
int gmtp_inter_register_rcv(struct sk_buff *skb);
struct gmtp_client *jump_over_gmtp_intra(struct sk_buff *skb,
		struct list_head *list_head);
int gmtp_inter_request_rcv(struct sk_buff *skb);
int gmtp_inter_register_local_in(struct sk_buff *skb);
int gmtp_inter_register_reply_rcv(struct sk_buff *skb,
		enum gmtp_inter_direction direction);
int gmtp_inter_ack_rcv(struct sk_buff *skb);
int gmtp_inter_data_rcv(struct sk_buff *skb);
int gmtp_inter_feedback_rcv(struct sk_buff *skb);
int gmtp_inter_elect_resp_rcv(struct sk_buff *skb);
int gmtp_inter_close_rcv(struct sk_buff *skb, bool in);

/** Output.c */
int gmtp_inter_register_out(struct sk_buff *skb);
int gmtp_inter_register_reply_out(struct sk_buff *skb);
int gmtp_inter_request_notify_out(struct sk_buff *skb);
int gmtp_inter_data_out(struct sk_buff *skb);
int gmtp_inter_close_out(struct sk_buff *skb);

/** build.c */
struct sk_buff *gmtp_inter_build_multicast_pkt(struct sk_buff *skb_src,
		struct net_device *dev);
struct sk_buff *gmtp_inter_build_pkt(struct sk_buff *skb_src, __be32 saddr,
		__be32 daddr, struct gmtp_hdr *gh_ref,
		enum gmtp_inter_direction direction);
void gmtp_inter_send_pkt(struct sk_buff *skb);
void gmtp_inter_add_relayid(struct sk_buff *skb);
struct gmtp_hdr *gmtp_inter_make_route_hdr(struct sk_buff *skb);

struct gmtp_hdr *gmtp_inter_make_request_notify_hdr(struct sk_buff *skb,
		struct gmtp_inter_entry *media_info, __be16 new_sport,
		__be16 new_dport, struct gmtp_client *reporter,
		__u8 max_nclients, __u8 error_code);

int gmtp_inter_make_request_notify(struct sk_buff *skb, __be32 new_saddr,
		__be16 new_sport, __be32 new_daddr, __be16 new_dport,
		struct gmtp_client *reporter, __u8 max_nclients,
		__u8 error_code);

struct gmtp_hdr *gmtp_inter_make_reset_hdr(struct sk_buff *skb, __u8 code);
int gmtp_inter_make_reset(struct sk_buff *skb, struct gmtp_hdr *gh_reset);
struct gmtp_hdr *gmtp_inter_make_close_hdr(struct sk_buff *skb);
void gmtp_inter_build_and_send_pkt(struct sk_buff *skb_src, __be32 saddr,
		__be32 daddr, struct gmtp_hdr *gh_ref,
		enum gmtp_inter_direction direction);
void gmtp_inter_build_and_send_skb(struct sk_buff *skb,
		enum gmtp_inter_direction direction);
void gmtp_copy_hdr(struct sk_buff *skb, struct sk_buff *src_skb);
struct sk_buff *gmtp_inter_build_ack(struct gmtp_inter_entry *entry);

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

static inline unsigned long ktime_to_jiffies(ktime_t value)
{
	struct timespec ts = ktime_to_timespec(value);

	return timespec_to_jiffies(&ts);
}

static inline void jiffies_to_ktime(const unsigned long jiffies, ktime_t *value)
{
	struct timespec ts;
	jiffies_to_timespec(jiffies, &ts);
	*value = timespec_to_ktime(ts);
}

/*
 * Print IP packet basic information
 */
static inline void print_packet(struct sk_buff *skb, bool in)
{
	struct iphdr *iph = ip_hdr(skb);
	const char *type = in ? "IN" : "OUT";
	pr_info("%s: Src=%pI4 | Dst=%pI4 | TTL=%u | Proto: %d | Len: %d B\n",
			type,
			&iph->saddr, &iph->daddr,
			iph->ttl,
			iph->protocol,
			ntohs(iph->tot_len));
}

/*
 * Print Data of GMTP-Data packets
 */
static inline void print_gmtp_data(struct sk_buff *skb, char* label)
{
	__u8* data = gmtp_data(skb);
	__u32 data_len = gmtp_data_len(skb);
	char *lb = (label != NULL) ? label : "Data";
	if(data_len > 0) {
		unsigned char *data_str = kmalloc(data_len+1, GFP_KERNEL);
		memcpy(data_str, data, data_len);
		data_str[data_len] = '\0';
		pr_info("%s: %s\n", lb, data_str);
	} else {
		pr_info("%s: <empty>\n", lb);
	}
}

static inline int bytes_added(int sprintf_return)
{
	return (sprintf_return > 0) ? sprintf_return : 0;
}

static inline void flowname_strn(__u8* str, const __u8 *buffer, int length)
{
	int i;
	for(i = 0; i < length; ++i)
		sprintf(&str[i*2], "%02x", buffer[i]);
}

#endif /* GMTP_INTER_H_ */
