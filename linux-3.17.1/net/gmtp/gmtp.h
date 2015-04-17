/*
 * gmtp.h
 *
 *  Created on: 18/06/2014
 *      Author: Wendell Silva Soares <wendell@compelab.org>
 */

#ifndef GMTP_H_
#define GMTP_H_

#include <net/inet_timewait_sock.h>
#include <net/inet_hashtables.h>
#include <uapi/asm-generic/errno.h>

#include <net/netns/gmtp.h>

#include <linux/types.h>
#include <linux/skbuff.h>
#include <uapi/linux/ip.h>

#include <linux/gmtp.h>
#include <uapi/linux/gmtp.h>

extern struct inet_hashinfo gmtp_inet_hashinfo;
extern struct percpu_counter gmtp_orphan_count;

#define GMTP_HASH_SIZE  16

/** GMTP Debugs */
#define GMTP_INFO "[GMTP] %s:%d - "
#define GMTP_DEBUG GMTP_INFO
#define GMTP_WARNING "[GMTP_WARNING]  %s:%d at %s - "
#define GMTP_ERROR "[GMTP_ERROR] %s:%d at %s - "

/* TODO Improve debug func names */
#define gmtp_print_debug(fmt, args...) pr_info(GMTP_DEBUG fmt \
		"\n", __FUNCTION__, __LINE__, ##args)
#define gmtp_print_warning(fmt, args...) pr_warning(GMTP_WARNING fmt \
		"\n", __FUNCTION__, __LINE__, __FILE__, ##args)
#define gmtp_print_error(fmt, args...) pr_err(GMTP_ERROR fmt \
		"\n", __FUNCTION__, __LINE__, __FILE__, ##args)
#define gmtp_print_function() pr_info("-------- %s --------\n" , __FUNCTION__)

/* Better print names */
#define gmtp_pr_func() pr_info("-------- %s --------\n" , __FUNCTION__)
#define gmtp_pr_info(fmt, args...) pr_info(GMTP_DEBUG fmt \
		"\n", __FUNCTION__, __LINE__, ##args)
#define gmtp_pr_debug(fmt, args...) gmtp_pr_info(fmt, ##args);
#define gmtp_pr_warning(fmt, args...) pr_warning(GMTP_WARNING fmt \
		"\n", __FUNCTION__, __LINE__, __FILE__, ##args)
#define gmtp_pr_error(fmt, args...) pr_err(GMTP_ERROR fmt \
		"\n", __FUNCTION__, __LINE__, __FILE__, ##args)

/** ---- */
#define GMTP_MAX_HDR_LEN 2047  /* 2^11 - 1 */
#define GMTP_FIXED_HDR_LEN 36  /* theoretically: 32 bytes, In fact: 36 bytes */
#define GMTP_MAX_VARIABLE_HDR_LEN (GMTP_MAX_HDR_LEN - GMTP_FIXED_HDR_LEN)

#define GMTP_MIN_SAMPLE_LEN 100 /* Minimal sample to measure 'instant' Tx rate*/
#define GMTP_DEFAULT_SAMPLE_LEN 1000 /* Sample to measure 'instant' Tx rate */
#define GMTP_MAX_SAMPLE_LEN 10000 /* Max sample to measure 'instant' Tx rate */


/**
 * Based on TCP Default Maximum Segment Size calculation
 * The solution to these two competing issues was to establish a default MSS
 * for TCP that was as large as possible while avoiding fragmentation for most
 * transmitted segments.
 * This was computed by starting with the minimum MTU for IP networks of 576.
 * All networks are required to be able to handle an IP datagram of this size
 * without fragmenting.
 * From this number, we subtract 36 bytes for the GMTP header and 20 for the
 * IP header, leaving 536 bytes. This is the standard MSS for GMTP.
 */
#define GMTP_DEFAULT_MSS (576 - GMTP_FIXED_HDR_LEN - 20)

/*
 * RTT sampling: sanity bounds and fallback RTT value from RFC 4340, section 3.4
 */
#define GMTP_SANE_RTT_MIN	100 /* microsseconds */
#define GMTP_FALLBACK_RTT	(USEC_PER_SEC / 5)
#define GMTP_SANE_RTT_MAX	(3 * USEC_PER_SEC)

#define GMTP_DEFAULT_RTT 64  /* milisseconds */


/* initial RTO value */
#define GMTP_TIMEOUT_INIT ((unsigned int)(3 * HZ))
/*
 * The maximum back-off value for retransmissions. This is needed for
 *  - retransmitting client-Requests
 *  - retransmitting Close/CloseReq when closing
 */
#define GMTP_RTO_MAX ((unsigned int)(64 * HZ))
#define GMTP_TIMEWAIT_LEN (60 * HZ)

/**
 * struct gmtp_client_entry - An entry in client hash table
 *
 * flowname[GMTP_FLOWNAME_LEN];
 * local_addr:	local client IP address
 * local_port:	local client port
 * channel_addr: multicast channel to receive media
 * channel_port: multicast port to receive media
 */
/** FIXME If a client requests a media in localhost, we get an error! */
struct gmtp_client_entry {
	struct gmtp_client_entry *next; /** It must be the first */

	__u8 flowname[GMTP_FLOWNAME_LEN];
	__be32 local_addr;
	__be16 local_port;
	__be32 channel_addr;
	__be16 channel_port;
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
struct gmtp_hashtable {
	int size;
	union gmtp_entry {
		struct gmtp_client_entry *client_entry;
		struct gmtp_server_entry *server_entry;
	} *table;
};

extern struct gmtp_hashtable* gmtp_hashtable;

void gmtp_init_xmit_timers(struct sock *sk);
static inline void gmtp_clear_xmit_timers(struct sock *sk)
{
	inet_csk_clear_xmit_timers(sk);
}


/** proto.c */
int gmtp_init_sock(struct sock *sk);
void gmtp_done(struct sock *sk);
void gmtp_close(struct sock *sk, long timeout);
int gmtp_connect(struct sock *sk);
int gmtp_disconnect(struct sock *sk, int flags);
int gmtp_ioctl(struct sock *sk, int cmd, unsigned long arg);
int gmtp_sendmsg(struct kiocb *iocb, struct sock *sk, struct msghdr *msg,
		size_t size);
int gmtp_recvmsg(struct kiocb *iocb, struct sock *sk, struct msghdr *msg,
		size_t len, int nonblock, int flags, int *addr_len);
void gmtp_shutdown(struct sock *sk, int how);
void gmtp_destroy_sock(struct sock *sk);
void gmtp_set_state(struct sock*, const int);
int inet_gmtp_listen(struct socket *sock, int backlog);
const char *gmtp_packet_name(const int);
const char *gmtp_state_name(const int);
void flowname_str(__u8* str, const __u8 *flowname);
void print_route(struct gmtp_hdr_route *route);

/** sockopt.c */
int gmtp_getsockopt(struct sock *sk, int level, int optname,
		char __user *optval, int __user *optlen);
int gmtp_setsockopt(struct sock *sk, int level, int optname,
		char __user *optval, unsigned int optlen);

/** input.c */
int gmtp_rcv_established(struct sock *sk, struct sk_buff *skb,
		const struct gmtp_hdr *dh, const unsigned int len);
int gmtp_rcv_state_process(struct sock *sk, struct sk_buff *skb,
		struct gmtp_hdr *gh, unsigned int len);

/** output.c */
void gmtp_send_ack(struct sock *sk,  __u8 ackcode);
void gmtp_send_close(struct sock *sk, const int active);
int gmtp_send_reset(struct sock *sk, enum gmtp_reset_codes code);
void gmtp_write_xmit(struct sock *sk, struct sk_buff *skb);
unsigned int gmtp_sync_mss(struct sock *sk, u32 pmtu);
struct sk_buff *gmtp_make_register_reply(struct sock *sk, struct dst_entry *dst,
		struct request_sock *req);
struct sk_buff *gmtp_ctl_make_reset(struct sock *sk,
		struct sk_buff *rcv_skb);
/** output.c - Packet Output and Timers  */
void gmtp_write_space(struct sock *sk);
int gmtp_retransmit_skb(struct sock *sk);

/** minisocks.c */
void gmtp_time_wait(struct sock *sk, int state, int timeo);
int gmtp_reqsk_init(struct request_sock *rq, struct gmtp_sock const *gp,
		struct sk_buff const *skb);
struct sock *gmtp_create_openreq_child(struct sock *sk,
				       const struct request_sock *req,
				       const struct sk_buff *skb);
int gmtp_child_process(struct sock *parent, struct sock *child,
		       struct sk_buff *skb);
struct sock *gmtp_check_req(struct sock *sk, struct sk_buff *skb,
			    struct request_sock *req,
			    struct request_sock **prev);
void gmtp_reqsk_send_ack(struct sock *sk, struct sk_buff *skb,
			 struct request_sock *rsk);

/** ipv4.c */
int gmtp_v4_connect(struct sock *sk, struct sockaddr *uaddr, int addr_len);

/** hash.c */
struct gmtp_hashtable *gmtp_create_hashtable(unsigned int size);
struct gmtp_client_entry *gmtp_lookup_media(
		struct gmtp_hashtable *hashtable, const __u8 *media);
int gmtp_add_client_entry(struct gmtp_hashtable *hashtable, __u8 *flowname,
		__be32 local_addr, __be16 local_port,
		__be32 channel_addr, __be16 channel_port);
void gmtp_del_client_entry(struct gmtp_hashtable *hashtable, __u8 *media);

struct gmtp_server_entry *gmtp_lookup_route(
		struct gmtp_hashtable *hashtable, const __u8 *relayid);
int gmtp_add_server_entry(struct gmtp_hashtable *hashtable, __u8 *relayid,
		__u8 *flowname, struct gmtp_hdr_route *route);

void kfree_gmtp_hashtable(struct gmtp_hashtable *hashtable);

/**
 * This is the control buffer. It is free to use by any layer.
 * This is an opaque area used to store private information.
 *
 * struct sk_buff {
 * (...)
 * 		char cb[48]
 * }
 *
 * gmtp_skb_cb  -  GMTP per-packet control information
 *
 * @type: one of %gmtp_pkt_type (or unknown)
 * @ackcode: ack code. One of sock, one of %gmtp_ack_codes
 * @reset_code: one of %gmtp_reset_codes
 * @reset_data: Data1..3 fields (depend on @gmtpd_reset_code)
 * @seq: sequence number
 *
 * This is used for transmission as well as for reception.
 */
struct gmtp_skb_cb {
	__u8 type :5;
	__u8 ackcode;
	__u8 reset_code,
		reset_data[3];
	__be32 seq;
};

#define GMTP_SKB_CB(__skb) ((struct gmtp_skb_cb *)&((__skb)->cb[0]))

/**
 * gmtp_loss_count - Approximate the number of lost data packets in a burst loss
 * @s1:  last known sequence number before the loss ('hole')
 * @s2:  first sequence number seen after the 'hole'
 */
static inline __be32 gmtp_loss_count(const __be32 s1, const __be32 s2,
		const __be32 ndp)
{
	__be32 delta = s2 - s1;

	WARN_ON(delta < 0);
	delta -= ndp + 1;

	return delta > 0 ? delta : 0;
}

/**
 * gmtp_loss_free - Evaluate condition for data loss from RFC 4340, 7.7.1
 */
static inline bool gmtp_loss_free(const __be32 s1, const __be32 s2,
		const __be32 ndp)
{
	return gmtp_loss_count(s1, s2, ndp) == 0;
}

static inline int gmtp_data_packet(const struct sk_buff *skb)
{
	const __u8 type = GMTP_SKB_CB(skb)->type;

	return type == GMTP_PKT_DATA	 ||
	       type == GMTP_PKT_DATAACK;
}

#endif /* GMTP_H_ */

