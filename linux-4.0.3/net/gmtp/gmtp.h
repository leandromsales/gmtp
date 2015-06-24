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
#include <uapi/linux/ip.h>
#include <linux/types.h>
#include <linux/skbuff.h>

#include <net/tcp.h>
#include <net/netns/gmtp.h>
#include <linux/gmtp.h>
#include <uapi/linux/gmtp.h>

#include "hash.h"

/** GMTP Debugs */
#define GMTP_INFO "[GMTP] %s:%d - "
#define GMTP_DEBUG GMTP_INFO
#define GMTP_WARNING "[GMTP_WARNING]  %s:%d - "
#define GMTP_ERROR "[GMTP_ERROR] %s:%d - "

/* TODO Improve debug func names */
#define gmtp_print_debug(fmt, args...) pr_info(GMTP_DEBUG fmt \
		"\n", __FUNCTION__, __LINE__, ##args)
#define gmtp_print_warning(fmt, args...) pr_warning(GMTP_WARNING fmt \
		"\n", __FUNCTION__, __LINE__, ##args)
#define gmtp_print_error(fmt, args...) pr_err(GMTP_ERROR fmt \
		"\n", __FUNCTION__, __LINE__, ##args)
#define gmtp_print_function() pr_info("-------- %s --------\n" , __FUNCTION__)

/* Better print names */
#define gmtp_pr_func() pr_info("-------- %s --------\n" , __FUNCTION__)
#define gmtp_pr_info(fmt, args...) pr_info(GMTP_DEBUG fmt \
		"\n", __FUNCTION__, __LINE__, ##args)
#define gmtp_pr_debug(fmt, args...) gmtp_pr_info(fmt, ##args);
#define gmtp_pr_warning(fmt, args...) pr_warning(GMTP_WARNING fmt \
		"\n", __FUNCTION__, __LINE__, ##args)
#define gmtp_pr_error(fmt, args...) pr_err(GMTP_ERROR fmt \
		"\n", __FUNCTION__, __LINE__, ##args)

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

#define GMTP_DEFAULT_RTT 64  /* milisseconds */
/*
 * RTT sampling: sanity bounds and fallback RTT value from RFC 4340, section 3.4
 * Units in usec (microseconds)
 */
#define GMTP_SANE_RTT_MIN	100		    /* 0.1 ms */
#define GMTP_FALLBACK_RTT	((GMTP_DEFAULT_RTT * USEC_PER_MSEC) / 5)
#define GMTP_SANE_RTT_MAX	(3 * USEC_PER_SEC)  /* 3 s    */

/* initial RTO value
 * The back-off value for retransmissions. This is needed for
 *  - retransmitting Client-Requests
 *  - retransmitting Close/CloseReq when closing
 */
#define GMTP_TIMEOUT_INIT ((unsigned int)(3 * HZ))
#define GMTP_RTO_MAX ((unsigned int)(64 * HZ))
#define GMTP_TIMEWAIT_LEN (60 * HZ)
#define GMTP_REQ_INTERVAL (TCP_SYNQ_INTERVAL)

/* For reporters and servers keep_alive */
#define GMTP_ACK_INTERVAL ((unsigned int)(HZ))
#define GMTP_ACK_TIMEOUT  (4 * GMTP_ACK_INTERVAL)

/* Int to __u8 operations */
#define TO_U8(x) ((x) > UINT_MAX) ? UINT_MAX : (__u8)(x)
#define SUB_U8(a, b) ((a-b) > UINT_MAX) ? UINT_MAX : (a-b)

extern struct gmtp_info *gmtp_info;
extern struct inet_hashinfo gmtp_inet_hashinfo;
extern struct percpu_counter gmtp_orphan_count;
extern struct gmtp_hashtable *gmtp_hashtable;

void gmtp_init_xmit_timers(struct sock *sk);
static inline void gmtp_clear_xmit_timers(struct sock *sk)
{
	inet_csk_clear_xmit_timers(sk);
	if(gmtp_sk(sk)->type == GMTP_SOCK_TYPE_REGULAR
			&& gmtp_sk(sk)->role == GMTP_ROLE_CLIENT) {
		if(gmtp_sk(sk)->myself->rsock != NULL)
			inet_csk_clear_xmit_timers(gmtp_sk(sk)->myself->rsock);
	}
}

static inline u32 gmtp_get_elect_timeout(struct gmtp_sock *gp)
{
	return (gp->rx_rtt > 0 ? gp->rx_rtt : GMTP_DEFAULT_RTT);
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
void print_gmtp_packet(const struct iphdr *iph, const struct gmtp_hdr *gh);
void print_route(struct gmtp_hdr_route *route);
void print_gmtp_sock(struct sock *sk);

/** sockopt.c */
int gmtp_getsockopt(struct sock *sk, int level, int optname,
		char __user *optval, int __user *optlen);
int gmtp_setsockopt(struct sock *sk, int level, int optname,
		char __user *optval, unsigned int optlen);

/** ipv4.c */
int gmtp_v4_connect(struct sock *sk, struct sockaddr *uaddr, int addr_len);

/** input.c */
struct sock* gmtp_multicast_connect(struct sock *sk, enum gmtp_sock_type type,
		__be32 addr, __be16 port);
struct sock* gmtp_sock_connect(struct sock *sk, enum gmtp_sock_type type,
		__be32 addr, __be16 port);
int gmtp_rcv_established(struct sock *sk, struct sk_buff *skb,
		const struct gmtp_hdr *dh, const unsigned int len);
int gmtp_rcv_state_process(struct sock *sk, struct sk_buff *skb,
		struct gmtp_hdr *gh, unsigned int len);

/** output.c */
void gmtp_send_ack(struct sock *sk);
void gmtp_send_elect_request(struct sock *sk, unsigned long interval);
void gmtp_send_elect_response(struct sock *sk, __u8 code);
void gmtp_send_feedback(struct sock *sk, __be32 server_tstamp);
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
struct sk_buff *gmtp_ctl_make_elect_response(struct sock *sk,
		struct sk_buff *rcv_skb);
struct sk_buff *gmtp_ctl_make_ack(struct sock *sk, struct sk_buff *rcv_skb);

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


/** GMTP structs and etc **/
struct gmtp_info {
	unsigned char 		relay_enabled:1;

	struct sock		*control_sk;
	struct sockaddr_in	*ctrl_addr;


};

void kfree_gmtp_info(struct gmtp_info *gmtp);

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
	__u8 reset_code,
		reset_data[3];
	__u8 elect_code:2;
	__be32 seq;
	__be32 server_tstamp;
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

static inline struct gmtp_client *gmtp_create_client(__be32 addr, __be16 port,
		__u8 max_nclients)
{
	struct gmtp_client *new = kmalloc(sizeof(struct gmtp_client),
	GFP_ATOMIC);

	if(new != NULL) {
		new->addr = addr;
		new->port = port;
		new->max_nclients = max_nclients;
		new->nclients = 0;
		new->ack_rx_tstamp = 0;

		new->clients = 0;
		new->reporter = 0;
		new->rsock = 0;
		new->mysock = 0;
	}
	return new;
}

/**
 * Create and add a client in the list of clients
 */
static inline struct gmtp_client *gmtp_list_add_client(unsigned int id,
		__be32 addr, __be16 port, __u8 max_nclients, struct list_head *head)
{
	struct gmtp_client *newc = gmtp_create_client(addr, port, max_nclients);

	if(newc == NULL) {
		gmtp_pr_error("Error while creating new gmtp_client...");
		return NULL;
	}

	newc->id = id;
	gmtp_pr_info("New client (%u): ADDR=%pI4@%-5d",
			newc->id, &addr, ntohs(port));

	INIT_LIST_HEAD(&newc->list);
	list_add_tail(&newc->list, head);
	return newc;
}

static inline struct gmtp_client* gmtp_get_first_client(struct list_head *head)
{
	struct gmtp_client *client;
	list_for_each_entry(client, head, list)
	{
		return client;
	}
	return NULL;
}

static inline struct gmtp_client* gmtp_get_client(struct list_head *head,
		__be32 addr, __be16 port)
{
	struct gmtp_client *client;
	list_for_each_entry(client, head, list)
	{
		if(client->addr == addr && client->port == port)
			return client;
	}
	return NULL;
}

static inline struct gmtp_client* gmtp_get_client_by_id(struct list_head *head,
		unsigned int id)
{
	struct gmtp_client *client;
	list_for_each_entry(client, head, list)
	{
		if(client->id == id)
			return client;
	}
	return NULL;
}

static inline int gmtp_delete_clients(struct list_head *list, __be32 addr, __be16 port)
{
	struct gmtp_client *client, *temp;
	int ret = 0;

	list_for_each_entry_safe(client, temp, list, list)
	{
		if(addr == client->addr && port == client->port) {
			pr_info("Deleting client: %pI4@%-5d\n", &client->addr,
					ntohs(client->port));
			list_del(&client->list);
			kfree(client);
			++ret;
		}
	}
	return ret;
}

#endif /* GMTP_H_ */

