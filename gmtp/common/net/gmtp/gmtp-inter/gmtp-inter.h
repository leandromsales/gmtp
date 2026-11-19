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

#include "ucc.h"
#include "hash-inter.h"

#define H_USER 	1024

#define CAPACITY_DEFAULT 125000000 /* B/s => 10 Mbps */

extern const char *gmtp_packet_name(const __u8);
extern const char *gmtp_state_name(const int);
extern void flowname_str(__u8* str, const __u8* flowname);
extern void print_gmtp_packet(const struct iphdr *iph, const struct gmtp_hdr *gh);
extern void print_ipv4_packet(struct sk_buff *skb, bool in);
extern bool gmtp_build_md5(unsigned char *result, unsigned char* data, size_t len);
extern unsigned char *gmtp_build_relay_id(void);
extern __be32 gmtp_dev_ip(struct net_device *dev);
extern bool gmtp_local_ip(__be32 ip);
extern void gmtp_add_relayid(struct sk_buff *skb);
extern struct gmtp_hashtable server_hashtable;

/**
 * TODO Negotiate buffer size with server
 * TODO Make kreporter configurable
 *
 * struct gmtp_inter - GMTP-inter state variables
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
	int 			capacity;
	int			buffer_len;

	unsigned int 		total_bytes_rx;

	unsigned int 		total_rx;
	int 			ucc_rx;
	unsigned int        	ucc_bytes;
	unsigned long  		ucc_rx_tstamp;
	unsigned int 		rx_rate_wnd;
	int			h_user;
	unsigned int		worst_rtt;

	unsigned char		mcst[4];

	unsigned char		kreporter;

	struct timer_list 	gmtp_ucc_timer;

	struct gmtp_inter_hashtable *hashtable;
};

extern struct gmtp_inter gmtp_inter;

#define skblen(skb) (((*skb).len) + ETH_HLEN)

enum gmtp_inter_direction {
	GMTP_INTER_FORWARD = 0,
	GMTP_INTER_BACKWARD,
	GMTP_INTER_LOCAL
};

/** gmtp-inter.c */
__be32 get_mcst_v4_addr(void);
void gmtp_buffer_add(struct gmtp_inter_entry *info, struct sk_buff *newsk);
struct sk_buff *gmtp_buffer_dequeue(struct gmtp_inter_entry *info);
void gmtp_ucc_buffer_add(struct sk_buff *newskb);
void gmtp_ucc_buffer_dequeue(struct sk_buff *newskb);
__be32 gmtp_inter_device_ip(struct net_device *dev);
void gmtp_timer_callback(void);
bool gmtp_local_ip(__be32 ip);

/** input.c */
int gmtp_inter_register_rcv(struct sk_buff *skb);
struct gmtp_client *jump_over_gmtp_intra(struct sk_buff *skb,
		struct list_head *list_head);
int gmtp_inter_request_rcv(struct sk_buff *skb);
int gmtp_inter_register_local_in(struct sk_buff *skb,
		struct gmtp_inter_entry *entry);
int gmtp_inter_register_reply_rcv(struct sk_buff *skb,
		struct gmtp_inter_entry *entry,
		enum gmtp_inter_direction direction);
int gmtp_inter_ack_rcv(struct sk_buff *skb, struct gmtp_inter_entry *entry);
int gmtp_inter_route_rcv(struct sk_buff *skb, struct gmtp_inter_entry *entry);
int gmtp_inter_data_rcv(struct sk_buff *skb, struct gmtp_inter_entry *entry);
int gmtp_inter_feedback_rcv(struct sk_buff *skb, struct gmtp_inter_entry *entry);
int gmtp_inter_delegate_rcv(struct sk_buff *skb, struct gmtp_inter_entry *entry);
int gmtp_inter_elect_resp_rcv(struct sk_buff *skb,
		struct gmtp_inter_entry *entry);
int gmtp_inter_close_rcv(struct sk_buff *skb, struct gmtp_inter_entry *entry,
		bool in);

/** Output.c */
int gmtp_inter_register_out(struct sk_buff *skb, struct gmtp_inter_entry *entry);
int gmtp_inter_register_reply_out(struct sk_buff *skb,
		struct gmtp_inter_entry *entry);
int gmtp_inter_request_notify_out(struct sk_buff *skb,
		struct gmtp_inter_entry *entry);
int gmtp_inter_ack_out(struct sk_buff *skb, struct gmtp_inter_entry *entry);
int gmtp_inter_data_out(struct sk_buff *skb, struct gmtp_inter_entry *entry);
int gmtp_inter_close_out(struct sk_buff *skb, struct gmtp_inter_entry *entry);

/** build.c */
struct sk_buff *gmtp_inter_build_pkt_len(struct sk_buff *skb_src, __be32 saddr,
		__be32 daddr, struct gmtp_hdr *gh_ref, int data_len,
		enum gmtp_inter_direction direction);
struct sk_buff *gmtp_inter_build_pkt(struct sk_buff *skb_src, __be32 saddr,
		__be32 daddr, struct gmtp_hdr *gh_ref,
		enum gmtp_inter_direction direction);
void gmtp_inter_send_pkt(struct sk_buff *skb);
struct gmtp_hdr *gmtp_inter_make_route_hdr(struct sk_buff *skb);

int gmtp_inter_make_register(struct sk_buff *skb);
struct gmtp_hdr *gmtp_inter_make_request_notify_hdr(struct sk_buff *skb,
		struct gmtp_inter_entry *media_info, __be16 new_sport,
		__be16 new_dport, struct gmtp_client *reporter,
		__u8 max_nclients, __u8 error_code);

int gmtp_inter_make_request_notify(struct sk_buff *skb, __be32 new_saddr,
		__be16 new_sport, __be32 new_daddr, __be16 new_dport,
		struct gmtp_client *reporter, __u8 max_nclients,
		__u8 error_code);
struct gmtp_hdr *gmtp_inter_make_register_reply_hdr(struct sk_buff *skb,
		struct gmtp_inter_entry *entry, __be16 new_sport,
		__be16 new_dport);

int gmtp_inter_make_delegate_reply(struct sk_buff *skb, struct gmtp_relay *relay,
		struct gmtp_inter_entry *entry);

struct gmtp_hdr *gmtp_inter_make_reset_hdr(struct sk_buff *skb, __u8 code);
int gmtp_inter_make_reset(struct sk_buff *skb, struct gmtp_hdr *gh_reset);
struct gmtp_hdr *gmtp_inter_make_close_hdr(struct sk_buff *skb);
void gmtp_inter_build_and_send_pkt(struct sk_buff *skb_src, __be32 saddr,
		__be32 daddr, struct gmtp_hdr *gh_ref,
		enum gmtp_inter_direction direction);
void gmtp_inter_build_and_send_pkt_len(struct sk_buff *skb_src, __be32 saddr,
		__be32 daddr, struct gmtp_hdr *gh_ref, int data_len,
		enum gmtp_inter_direction direction);
void gmtp_inter_build_and_send_skb(struct sk_buff *skb,
		enum gmtp_inter_direction direction);
void gmtp_copy_hdr(struct sk_buff *skb, struct sk_buff *src_skb);
struct gmtp_hdr *gmtp_inter_make_ack_hdr(struct sk_buff *skb,
		struct gmtp_inter_entry *entry, __be32 tstamp);
int gmtp_inter_make_ack_from_feedback(struct sk_buff *skb,
		struct gmtp_inter_entry *entry);
struct sk_buff *gmtp_inter_build_register(struct gmtp_inter_entry *entry);
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
 * In NS-3 we call schedule_timeout(1L) (1 jiffy)
 */
static inline void gmtp_inter_wait_ms(unsigned int delay)
{
	unsigned int timeout = jiffies_to_msecs(jiffies) + delay;
	while(jiffies_to_msecs(jiffies) < timeout) {
		/*cond_resched(); *//* Do nothing, just wait... */
		schedule_timeout(1L);
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

#endif /* GMTP_INTER_H_ */
