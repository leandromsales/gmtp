/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef GMTP_H_
#define GMTP_H_
/*
 *  net/gmtp/gmtp.h
 *
 *  An implementation of the GMTP protocol
 *  Copyright (c) 2015 Wendell Silva Soares <wendell@ic.ufal.br>
 */

#include <linux/gmtp.h>
#include <linux/types.h>
#include <linux/skbuff.h>

#include <net/inet_timewait_sock.h>
#include <net/inet_hashtables.h>
#include <net/tcp.h>
#include <net/netns/gmtp.h>

#include <uapi/asm-generic/errno.h>
#include <uapi/linux/ip.h>
#include <uapi/linux/gmtp.h>

#include "hash.h"
#include "gmtp_hashtables.h"

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

#define GMTP_DEFAULT_RTT    64  /* milisseconds */
#define GMTP_MIN_RTT_MS     1  /* milisseconds */
/*
 * RTT sampling: sanity bounds and fallback RTT value from RFC 4340, section 3.4
 * Units in usec (microseconds)
 */
#define GMTP_SANE_RTT_MIN   100         /* 0.1 ms */
#define GMTP_FALLBACK_RTT   ((GMTP_DEFAULT_RTT * USEC_PER_MSEC) / 5)
#define GMTP_SANE_RTT_MAX   (3 * USEC_PER_SEC)  /* 3 s    */
#define GMTP_RTT_WEIGHT     20  /* 0.02 */

/**
 * rtt_ewma  -  Exponentially weighted moving average
 * @weight: Weight to be used as damping factor, in units of 1/1000
 *      factor 1/1000 allows better weight granularity
 */
static inline u32 rtt_ewma(const u32 avg, const u32 newval, const u32 weight)
{
    return avg ? (weight * avg + (1000 - weight) * newval) / 1000 : newval;
}

/* initial RTO value
 * The back-off value for retransmissions. This is needed for
 *  - retransmitting Client-Requests
 *  - retransmitting Close/CloseReq when closing
 */
#define GMTP_TIMEOUT_INIT (TCP_SYNQ_INTERVAL)
#define GMTP_RTO_MAX ((unsigned int)(64 * HZ))
#define GMTP_TIMEWAIT_LEN (60 * HZ)
#define GMTP_REQ_INTERVAL (TCP_SYNQ_INTERVAL)
#define GMTP_SYN_RETRIES (2 * TCP_SYN_RETRIES)

/* For reporters and servers keep_alive */
#define GMTP_ACK_INTERVAL ((unsigned int)(HZ))
#define GMTP_ACK_TIMEOUT  (4 * GMTP_ACK_INTERVAL)

#define MD5_LEN (16)

/* Int to __U12 operations */
#define TO_U12(x)   min((U16_MAX >> 4), (x))
#define SUB_U12(a, b)   min((U16_MAX >> 4), (a-b))
#define ADD_U12(a, b)   min((U16_MAX >> 4), (a+b))

extern struct gmtp_info *gmtp_info;
extern struct inet_hashinfo gmtp_inet_hashinfo;
extern struct gmtp_sk_hashtable gmtp_sk_hash;
extern struct percpu_counter gmtp_orphan_count;
extern struct gmtp_hashtable client_hashtable;
extern struct gmtp_hashtable server_hashtable;

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
/*unsigned char *gmtp_build_md5(unsigned char *buf);*/
bool gmtp_build_md5(unsigned char *result, unsigned char* data, size_t len);
unsigned char *gmtp_inter_build_relay_id(void);
__be32 gmtp_dev_ip(struct net_device *dev);
bool gmtp_local_ip(__be32 ip);
void gmtp_add_relayid(struct sk_buff *skb);
int gmtp_init_sock(struct sock *sk);
void gmtp_done(struct sock *sk);
void gmtp_close(struct sock *sk, long timeout);
int gmtp_connect(struct sock *sk);
int gmtp_disconnect(struct sock *sk, int flags);
__poll_t gmtp_poll(struct file *file, struct socket *sock, poll_table *wait);
int gmtp_ioctl(struct sock *sk, int cmd, unsigned long arg);
int gmtp_sendmsg(struct sock *sk, struct msghdr *msg, size_t size);
int gmtp_recvmsg(struct sock *sk, struct msghdr *msg, size_t len, int nonblock,
        int flags, int *addr_len);
void gmtp_shutdown(struct sock *sk, int how);
void gmtp_destroy_sock(struct sock *sk);
void gmtp_set_state(struct sock*, const int);
int inet_gmtp_listen(struct socket *sock, int backlog);
const char *gmtp_packet_name(const __u8);
const char *gmtp_state_name(const int);
void flowname_str(__u8* str, const __u8 *flowname);
void print_ipv4_packet(struct sk_buff *skb, bool in);
void print_gmtp_packet(const struct iphdr *iph, const struct gmtp_hdr *gh);
void print_gmtp_data(struct sk_buff *skb, char* label);
void print_gmtp_hdr_relay(const struct gmtp_hdr_relay *relay);
void print_route_from_skb(struct sk_buff *skb);
void print_route_from_list(struct gmtp_relay_entry *relay_list);
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
void gmtp_send_route_notify(struct sock *sk, struct sk_buff *rcv_skb);
void gmtp_send_elect_request(struct sock *sk, unsigned long interval);
void gmtp_send_elect_response(struct sock *sk, __u8 code);
void gmtp_send_feedback(struct sock *sk);
void gmtp_send_close(struct sock *sk, const int active);
int gmtp_send_reset(struct sock *sk, enum gmtp_reset_codes code);
void gmtp_write_xmit(struct sock *sk, struct sk_buff *skb);
unsigned int gmtp_sync_mss(struct sock *sk, u32 pmtu);
int gmtp_transmit_built_skb(struct sock *sk, struct sk_buff *skb);
struct sk_buff *gmtp_make_register_reply(struct sock *sk, struct dst_entry *dst,
        struct request_sock *req);
struct sk_buff *gmtp_make_register_reply_open(struct sock *sk,
        struct sk_buff *rcv_skb);
struct sk_buff *gmtp_make_delegate(struct sock *sk, struct sk_buff *rcv_skb,
        __u8 *rid);
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
                struct request_sock *req);
void gmtp_reqsk_send_ack(const struct sock *sk, struct sk_buff *skb,
             struct request_sock *rsk);


/** GMTP structs and etc **/
struct gmtp_info {
    unsigned char       relay_id[GMTP_RELAY_ID_LEN];
    unsigned char       relay_enabled:1;
    int pkt_sent;

    struct sock     *control_sk;
    struct sockaddr_in  *ctrl_addr;
};

static inline void kfree_gmtp_info(struct gmtp_info *gmtp_info)
{
    if(gmtp_info->control_sk != NULL)
        kfree(gmtp_info->control_sk);
    if(gmtp_info->ctrl_addr != NULL)
        kfree(gmtp_info->ctrl_addr);
    if(gmtp_info->relay_id != NULL)
    	kfree(gmtp_info->relay_id);
    kfree(gmtp_info);
}

/**
 * This is the control buffer. It is free to use by any layer.
 * This is an opaque area used to store private information.
 *
 * struct sk_buff {
 * (...)
 *      char cb[48]
 * }
 *
 * gmtp_skb_cb  -  GMTP per-packet control information
 *
 * @type: one of %gmtp_pkt_type (or unknown)
 * @ackcode: ack code. One of sock, one of %gmtp_ack_codes
 * @reset_code: one of %gmtp_reset_codes
 * @reset_data: Data1..3 fields (depend on @gmtpd_reset_code)
 * @seq: sequence number
 * @orig_tstamp: time stamp of last data packet (stamped at origin)
 * @jumped: used in gmtp-inter. It is 1 if "jump_over_gmtp_intra" was called.
 *
 * This is used for transmission as well as for reception.
 */
struct gmtp_skb_cb {
    __u8 type :5;
    __be16  hdrlen:11;
    __u8 reset_code,
        reset_data[3];
    __u8 elect_code:2;
    __u8 retransmits;
    __be32 seq;
    __be32 orig_tstamp;
    __u8 jumped;
};

#define GMTP_SKB_CB(__skb) ((struct gmtp_skb_cb *)&((__skb)->cb[0]))

/**
 * Returns subtraction of two ktimes, in __be32 format (milliseconds)
 */
static inline __be32 ktime_sub_ms_be32(ktime_t last, ktime_t first)
{
    return (__be32) ktime_to_ms(ktime_sub(last, first));
}

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

    return type == GMTP_PKT_DATA     ||
           type == GMTP_PKT_DATAACK;
}

#endif /* GMTP_H_ */

