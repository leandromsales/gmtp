#ifndef _GMTP_H
#define _GMTP_H
/*
 *  net/gmtp/gmtp.h
 *
 *  An implementation of the GMTP protocol
 *  Copyright (c) 2005 Arnaldo Carvalho de Melo <acme@conectiva.com.br>
 *  Copyright (c) 2005-6 Ian McDonald <ian.mcdonald@jandi.co.nz>
 *
 *	This program is free software; you can redistribute it and/or modify it
 *	under the terms of the GNU General Public License version 2 as
 *	published by the Free Software Foundation.
 */

#include <linux/gmtp.h>
#include <linux/ktime.h>
#include <net/snmp.h>
#include <net/sock.h>
#include <net/tcp.h>
#include "ackvec.h"

#define IPPROTO_GMTP 254


/*
 * 	GMTP - specific warning and debugging macros.
 */
#define GMTP_WARN(fmt, a...) LIMIT_NETDEBUG(KERN_WARNING "%s: " fmt,       \
							__func__, ##a)
#define GMTP_CRIT(fmt, a...) printk(KERN_CRIT fmt " at %s:%d/%s()\n", ##a, \
					 __FILE__, __LINE__, __func__)
#define GMTP_BUG(a...)       do { GMTP_CRIT("BUG: " a); dump_stack(); } while(0)
#define GMTP_BUG_ON(cond)    do { if (unlikely((cond) != 0))		   \
				     GMTP_BUG("\"%s\" holds (exception!)", \
					      __stringify(cond));          \
			     } while (0)

#define GMTP_PRINTK(enable, fmt, args...)	do { if (enable)	     \
							printk(fmt, ##args); \
						} while(0)
#define GMTP_PR_DEBUG(enable, fmt, a...)	GMTP_PRINTK(enable, KERN_DEBUG \
						  "%s: " fmt, __func__, ##a)

#ifdef CONFIG_IP_GMTP_DEBUG
extern bool gmtp_debug;
#define gmtp_pr_debug(format, a...)	  GMTP_PR_DEBUG(gmtp_debug, format, ##a)
#define gmtp_pr_debug_cat(format, a...)   GMTP_PRINTK(gmtp_debug, format, ##a)
#define gmtp_debug(fmt, a...)		  gmtp_pr_debug_cat(KERN_DEBUG fmt, ##a)
#else
#define gmtp_pr_debug(format, a...)
#define gmtp_pr_debug_cat(format, a...)
#define gmtp_debug(format, a...)
#endif

extern struct inet_hashinfo gmtp_hashinfo;

extern struct percpu_counter gmtp_orphan_count;

extern void gmtp_time_wait(struct sock *sk, int state, int timeo);

/*
 *  Set safe upper bounds for header and option length. Since Data Offset is 8
 *  bits (RFC 4340, sec. 5.1), the total header length can never be more than
 *  4 * 255 = 1020 bytes. The largest possible header length is 28 bytes (X=1):
 *    - GMTP-Response with ACK Subheader and 4 bytes of Service code      OR
 *    - GMTP-Reset    with ACK Subheader and 4 bytes of Reset Code fields
 *  Hence a safe upper bound for the maximum option length is 1020-28 = 992
 */
#define MAX_GMTP_SPECIFIC_HEADER (255 * sizeof(uint32_t))
#define GMTP_MAX_PACKET_HDR 28
#define GMTP_MAX_OPT_LEN (MAX_GMTP_SPECIFIC_HEADER - GMTP_MAX_PACKET_HDR)
#define MAX_GMTP_HEADER (MAX_GMTP_SPECIFIC_HEADER + MAX_HEADER)

/* Upper bound for initial feature-negotiation overhead (padded to 32 bits) */
#define GMTP_FEATNEG_OVERHEAD	 (32 * sizeof(uint32_t))

#define GMTP_TIMEWAIT_LEN (60 * HZ) /* how long to wait to destroy TIME-WAIT
				     * state, about 60 seconds */

/* RFC 1122, 4.2.3.1 initial RTO value */
#define GMTP_TIMEOUT_INIT ((unsigned int)(3 * HZ))

/*
 * The maximum back-off value for retransmissions. This is needed for
 *  - retransmitting client-Requests (sec. 8.1.1),
 *  - retransmitting Close/CloseReq when closing (sec. 8.3),
 *  - feature-negotiation retransmission (sec. 6.6.3),
 *  - Acks in client-PARTOPEN state (sec. 8.1.5).
 */
#define GMTP_RTO_MAX ((unsigned int)(64 * HZ))

/*
 * RTT sampling: sanity bounds and fallback RTT value from RFC 4340, section 3.4
 */
#define GMTP_SANE_RTT_MIN	100
#define GMTP_FALLBACK_RTT	(USEC_PER_SEC / 5)
#define GMTP_SANE_RTT_MAX	(3 * USEC_PER_SEC)

/* sysctl variables for GMTP */
extern int  sysctl_gmtp_request_retries;
extern int  sysctl_gmtp_retries1;
extern int  sysctl_gmtp_retries2;
extern int  sysctl_gmtp_tx_qlen;
extern int  sysctl_gmtp_sync_ratelimit;

/*
 *	48-bit sequence number arithmetic (signed and unsigned)
 */
#define INT48_MIN	  0x800000000000LL		/* 2^47	    */
#define UINT48_MAX	  0xFFFFFFFFFFFFLL		/* 2^48 - 1 */
#define COMPLEMENT48(x)	 (0x1000000000000LL - (x))	/* 2^48 - x */
#define TO_SIGNED48(x)	 (((x) < INT48_MIN)? (x) : -COMPLEMENT48( (x)))
#define TO_UNSIGNED48(x) (((x) >= 0)?	     (x) :  COMPLEMENT48(-(x)))
#define ADD48(a, b)	 (((a) + (b)) & UINT48_MAX)
#define SUB48(a, b)	 ADD48((a), COMPLEMENT48(b))

static inline void gmtp_set_seqno(u64 *seqno, u64 value)
{
	*seqno = value & UINT48_MAX;
}

static inline void gmtp_inc_seqno(u64 *seqno)
{
	*seqno = ADD48(*seqno, 1);
}

/* signed mod-2^48 distance: pos. if seqno1 < seqno2, neg. if seqno1 > seqno2 */
static inline s64 gmtp_delta_seqno(const u64 seqno1, const u64 seqno2)
{
	u64 delta = SUB48(seqno2, seqno1);

	return TO_SIGNED48(delta);
}

/* is seq1 < seq2 ? */
static inline int before48(const u64 seq1, const u64 seq2)
{
	return (s64)((seq2 << 16) - (seq1 << 16)) > 0;
}

/* is seq1 > seq2 ? */
#define after48(seq1, seq2)	before48(seq2, seq1)

/* is seq2 <= seq1 <= seq3 ? */
static inline int between48(const u64 seq1, const u64 seq2, const u64 seq3)
{
	return (seq3 << 16) - (seq2 << 16) >= (seq1 << 16) - (seq2 << 16);
}

static inline u64 max48(const u64 seq1, const u64 seq2)
{
	return after48(seq1, seq2) ? seq1 : seq2;
}

/**
 * gmtp_loss_count - Approximate the number of lost data packets in a burst loss
 * @s1:  last known sequence number before the loss ('hole')
 * @s2:  first sequence number seen after the 'hole'
 * @ndp: NDP count on packet with sequence number @s2
 */
static inline u64 gmtp_loss_count(const u64 s1, const u64 s2, const u64 ndp)
{
	s64 delta = gmtp_delta_seqno(s1, s2);

	WARN_ON(delta < 0);
	delta -= ndp + 1;

	return delta > 0 ? delta : 0;
}

/**
 * gmtp_loss_free - Evaluate condition for data loss from RFC 4340, 7.7.1
 */
static inline bool gmtp_loss_free(const u64 s1, const u64 s2, const u64 ndp)
{
	return gmtp_loss_count(s1, s2, ndp) == 0;
}

enum {
	GMTP_MIB_NUM = 0,
	GMTP_MIB_ACTIVEOPENS,			/* ActiveOpens */
	GMTP_MIB_ESTABRESETS,			/* EstabResets */
	GMTP_MIB_CURRESTAB,			/* CurrEstab */
	GMTP_MIB_OUTSEGS,			/* OutSegs */
	GMTP_MIB_OUTRSTS,
	GMTP_MIB_ABORTONTIMEOUT,
	GMTP_MIB_TIMEOUTS,
	GMTP_MIB_ABORTFAILED,
	GMTP_MIB_PASSIVEOPENS,
	GMTP_MIB_ATTEMPTFAILS,
	GMTP_MIB_OUTDATAGRAMS,
	GMTP_MIB_INERRS,
	GMTP_MIB_OPTMANDATORYERROR,
	GMTP_MIB_INVALIDOPT,
	__GMTP_MIB_MAX
};

#define GMTP_MIB_MAX	__GMTP_MIB_MAX
struct gmtp_mib {
	unsigned long	mibs[GMTP_MIB_MAX];
};

DECLARE_SNMP_STAT(struct gmtp_mib, gmtp_statistics);
#define GMTP_INC_STATS(field)	    SNMP_INC_STATS(gmtp_statistics, field)
#define GMTP_INC_STATS_BH(field)    SNMP_INC_STATS_BH(gmtp_statistics, field)
#define GMTP_DEC_STATS(field)	    SNMP_DEC_STATS(gmtp_statistics, field)

/*
 * 	Checksumming routines
 */
static inline unsigned int gmtp_csum_coverage(const struct sk_buff *skb)
{
	const struct gmtp_hdr* dh = gmtp_hdr(skb);

	if (dh->gmtph_cscov == 0)
		return skb->len;
	return (dh->gmtph_doff + dh->gmtph_cscov - 1) * sizeof(u32);
}

static inline void gmtp_csum_outgoing(struct sk_buff *skb)
{
	unsigned int cov = gmtp_csum_coverage(skb);

	if (cov >= skb->len)
		gmtp_hdr(skb)->gmtph_cscov = 0;

	skb->csum = skb_checksum(skb, 0, (cov > skb->len)? skb->len : cov, 0);
}

extern void gmtp_v4_send_check(struct sock *sk, struct sk_buff *skb);

extern int  gmtp_retransmit_skb(struct sock *sk);

extern void gmtp_send_ack(struct sock *sk);
extern void gmtp_reqsk_send_ack(struct sock *sk, struct sk_buff *skb,
				struct request_sock *rsk);

extern void gmtp_send_sync(struct sock *sk, const u64 seq,
			   const enum gmtp_pkt_type pkt_type);

/*
 * TX Packet Dequeueing Interface
 */
extern void		gmtp_qpolicy_push(struct sock *sk, struct sk_buff *skb);
extern bool		gmtp_qpolicy_full(struct sock *sk);
extern void		gmtp_qpolicy_drop(struct sock *sk, struct sk_buff *skb);
extern struct sk_buff	*gmtp_qpolicy_top(struct sock *sk);
extern struct sk_buff	*gmtp_qpolicy_pop(struct sock *sk);
extern bool		gmtp_qpolicy_param_ok(struct sock *sk, __be32 param);

/*
 * TX Packet Output and TX Timers
 */
extern void   gmtp_write_xmit(struct sock *sk);
extern void   gmtp_write_space(struct sock *sk);
extern void   gmtp_flush_write_queue(struct sock *sk, long *time_budget);

extern void gmtp_init_xmit_timers(struct sock *sk);
static inline void gmtp_clear_xmit_timers(struct sock *sk)
{
	inet_csk_clear_xmit_timers(sk);
}

extern unsigned int gmtp_sync_mss(struct sock *sk, u32 pmtu);

extern const char *gmtp_packet_name(const int type);

extern void gmtp_set_state(struct sock *sk, const int state);
extern void gmtp_done(struct sock *sk);

extern int  gmtp_reqsk_init(struct request_sock *rq, struct gmtp_sock const *dp,
			    struct sk_buff const *skb);

extern int gmtp_v4_conn_request(struct sock *sk, struct sk_buff *skb);

extern struct sock *gmtp_create_openreq_child(struct sock *sk,
					      const struct request_sock *req,
					      const struct sk_buff *skb);

extern int gmtp_v4_do_rcv(struct sock *sk, struct sk_buff *skb);

extern struct sock *gmtp_v4_request_recv_sock(struct sock *sk,
					      struct sk_buff *skb,
					      struct request_sock *req,
					      struct dst_entry *dst);
extern struct sock *gmtp_check_req(struct sock *sk, struct sk_buff *skb,
				   struct request_sock *req,
				   struct request_sock **prev);

extern int gmtp_child_process(struct sock *parent, struct sock *child,
			      struct sk_buff *skb);
extern int gmtp_rcv_state_process(struct sock *sk, struct sk_buff *skb,
				  struct gmtp_hdr *dh, unsigned int len);
extern int gmtp_rcv_established(struct sock *sk, struct sk_buff *skb,
				const struct gmtp_hdr *dh, const unsigned int len);

extern int gmtp_init_sock(struct sock *sk, const __u8 ctl_sock_initialized);
extern void gmtp_destroy_sock(struct sock *sk);

extern void		gmtp_close(struct sock *sk, long timeout);
extern struct sk_buff	*gmtp_make_response(struct sock *sk,
					    struct dst_entry *dst,
					    struct request_sock *req);

extern int	   gmtp_connect(struct sock *sk);
extern int	   gmtp_disconnect(struct sock *sk, int flags);
extern int	   gmtp_getsockopt(struct sock *sk, int level, int optname,
				   char __user *optval, int __user *optlen);
extern int	   gmtp_setsockopt(struct sock *sk, int level, int optname,
				   char __user *optval, unsigned int optlen);
#ifdef CONFIG_COMPAT
extern int	   compat_gmtp_getsockopt(struct sock *sk,
				int level, int optname,
				char __user *optval, int __user *optlen);
extern int	   compat_gmtp_setsockopt(struct sock *sk,
				int level, int optname,
				char __user *optval, unsigned int optlen);
#endif
extern int	   gmtp_ioctl(struct sock *sk, int cmd, unsigned long arg);
extern int	   gmtp_sendmsg(struct kiocb *iocb, struct sock *sk,
				struct msghdr *msg, size_t size);
extern int	   gmtp_recvmsg(struct kiocb *iocb, struct sock *sk,
				struct msghdr *msg, size_t len, int nonblock,
				int flags, int *addr_len);
extern void	   gmtp_shutdown(struct sock *sk, int how);
extern int	   inet_gmtp_listen(struct socket *sock, int backlog);
extern unsigned int gmtp_poll(struct file *file, struct socket *sock,
			     poll_table *wait);
extern int	   gmtp_v4_connect(struct sock *sk, struct sockaddr *uaddr,
				   int addr_len);

extern struct sk_buff *gmtp_ctl_make_reset(struct sock *sk,
					   struct sk_buff *skb);
extern int	   gmtp_send_reset(struct sock *sk, enum gmtp_reset_codes code);
extern void	   gmtp_send_close(struct sock *sk, const int active);
extern int	   gmtp_invalid_packet(struct sk_buff *skb);
extern u32	   gmtp_sample_rtt(struct sock *sk, long delta);

static inline int gmtp_bad_service_code(const struct sock *sk,
					const __be32 service)
{
	const struct gmtp_sock *dp = gmtp_sk(sk);

	if (dp->gmtps_service == service)
		return 0;
	return !gmtp_list_has_service(dp->gmtps_service_list, service);
}

/**
 * gmtp_skb_cb  -  GMTP per-packet control information
 * @gmtpd_type: one of %gmtp_pkt_type (or unknown)
 * @gmtpd_ccval: CCVal field (5.1), see e.g. RFC 4342, 8.1
 * @gmtpd_reset_code: one of %gmtp_reset_codes
 * @gmtpd_reset_data: Data1..3 fields (depend on @gmtpd_reset_code)
 * @gmtpd_opt_len: total length of all options (5.8) in the packet
 * @gmtpd_seq: sequence number
 * @gmtpd_ack_seq: acknowledgment number subheader field value
 *
 * This is used for transmission as well as for reception.
 */
struct gmtp_skb_cb {
	union {
		struct inet_skb_parm	h4;
#if IS_ENABLED(CONFIG_IPV6)
		struct inet6_skb_parm	h6;
#endif
	} header;
	__u8  gmtpd_type:4;
	__u8  gmtpd_ccval:4;
	__u8  gmtpd_reset_code,
	      gmtpd_reset_data[3];
	__u16 gmtpd_opt_len;
	__u64 gmtpd_seq;
	__u64 gmtpd_ack_seq;
};

#define GMTP_SKB_CB(__skb) ((struct gmtp_skb_cb *)&((__skb)->cb[0]))

/* RFC 4340, sec. 7.7 */
static inline int gmtp_non_data_packet(const struct sk_buff *skb)
{
	const __u8 type = GMTP_SKB_CB(skb)->gmtpd_type;

	return type == GMTP_PKT_ACK	 ||
	       type == GMTP_PKT_CLOSE	 ||
	       type == GMTP_PKT_CLOSEREQ ||
	       type == GMTP_PKT_RESET	 ||
	       type == GMTP_PKT_SYNC	 ||
	       type == GMTP_PKT_SYNCACK;
}

/* RFC 4340, sec. 7.7 */
static inline int gmtp_data_packet(const struct sk_buff *skb)
{
	const __u8 type = GMTP_SKB_CB(skb)->gmtpd_type;

	return type == GMTP_PKT_DATA	 ||
	       type == GMTP_PKT_DATAACK  ||
	       type == GMTP_PKT_REQUEST  ||
	       type == GMTP_PKT_RESPONSE;
}

static inline int gmtp_packet_without_ack(const struct sk_buff *skb)
{
	const __u8 type = GMTP_SKB_CB(skb)->gmtpd_type;

	return type == GMTP_PKT_DATA || type == GMTP_PKT_REQUEST;
}

#define GMTP_PKT_WITHOUT_ACK_SEQ (UINT48_MAX << 2)

static inline void gmtp_hdr_set_seq(struct gmtp_hdr *dh, const u64 gss)
{
	struct gmtp_hdr_ext *dhx = (struct gmtp_hdr_ext *)((void *)dh +
							   sizeof(*dh));
	dh->gmtph_seq2 = 0;
	dh->gmtph_seq = htons((gss >> 32) & 0xfffff);
	dhx->gmtph_seq_low = htonl(gss & 0xffffffff);
}

static inline void gmtp_hdr_set_ack(struct gmtp_hdr_ack_bits *dhack,
				    const u64 gsr)
{
	dhack->gmtph_reserved1 = 0;
	dhack->gmtph_ack_nr_high = htons(gsr >> 32);
	dhack->gmtph_ack_nr_low  = htonl(gsr & 0xffffffff);
}

static inline void gmtp_update_gsr(struct sock *sk, u64 seq)
{
	struct gmtp_sock *dp = gmtp_sk(sk);

	if (after48(seq, dp->gmtps_gsr))
		dp->gmtps_gsr = seq;
	/* Sequence validity window depends on remote Sequence Window (7.5.1) */
	dp->gmtps_swl = SUB48(ADD48(dp->gmtps_gsr, 1), dp->gmtps_r_seq_win / 4);
	/*
	 * Adjust SWL so that it is not below ISR. In contrast to RFC 4340,
	 * 7.5.1 we perform this check beyond the initial handshake: W/W' are
	 * always > 32, so for the first W/W' packets in the lifetime of a
	 * connection we always have to adjust SWL.
	 * A second reason why we are doing this is that the window depends on
	 * the feature-remote value of Sequence Window: nothing stops the peer
	 * from updating this value while we are busy adjusting SWL for the
	 * first W packets (we would have to count from scratch again then).
	 * Therefore it is safer to always make sure that the Sequence Window
	 * is not artificially extended by a peer who grows SWL downwards by
	 * continually updating the feature-remote Sequence-Window.
	 * If sequence numbers wrap it is bad luck. But that will take a while
	 * (48 bit), and this measure prevents Sequence-number attacks.
	 */
	if (before48(dp->gmtps_swl, dp->gmtps_isr))
		dp->gmtps_swl = dp->gmtps_isr;
	dp->gmtps_swh = ADD48(dp->gmtps_gsr, (3 * dp->gmtps_r_seq_win) / 4);
}

static inline void gmtp_update_gss(struct sock *sk, u64 seq)
{
	struct gmtp_sock *dp = gmtp_sk(sk);

	dp->gmtps_gss = seq;
	/* Ack validity window depends on local Sequence Window value (7.5.1) */
	dp->gmtps_awl = SUB48(ADD48(dp->gmtps_gss, 1), dp->gmtps_l_seq_win);
	/* Adjust AWL so that it is not below ISS - see comment above for SWL */
	if (before48(dp->gmtps_awl, dp->gmtps_iss))
		dp->gmtps_awl = dp->gmtps_iss;
	dp->gmtps_awh = dp->gmtps_gss;
}

static inline int gmtp_ackvec_pending(const struct sock *sk)
{
	return gmtp_sk(sk)->gmtps_hc_rx_ackvec != NULL &&
	       !gmtp_ackvec_is_empty(gmtp_sk(sk)->gmtps_hc_rx_ackvec);
}

static inline int gmtp_ack_pending(const struct sock *sk)
{
	return gmtp_ackvec_pending(sk) || inet_csk_ack_scheduled(sk);
}

extern int  gmtp_feat_signal_nn_change(struct sock *sk, u8 feat, u64 nn_val);
extern int  gmtp_feat_finalise_settings(struct gmtp_sock *dp);
extern int  gmtp_feat_server_ccid_dependencies(struct gmtp_request_sock *dreq);
extern int  gmtp_feat_insert_opts(struct gmtp_sock*, struct gmtp_request_sock*,
				  struct sk_buff *skb);
extern int  gmtp_feat_activate_values(struct sock *sk, struct list_head *fn);
extern void gmtp_feat_list_purge(struct list_head *fn_list);

extern int gmtp_insert_options(struct sock *sk, struct sk_buff *skb);
extern int gmtp_insert_options_rsk(struct gmtp_request_sock*, struct sk_buff*);
extern int gmtp_insert_option_elapsed_time(struct sk_buff *skb, u32 elapsed);
extern u32 gmtp_timestamp(void);
extern void gmtp_timestamping_init(void);
extern int gmtp_insert_option(struct sk_buff *skb, unsigned char option,
			      const void *value, unsigned char len);

#ifdef CONFIG_SYSCTL
extern int gmtp_sysctl_init(void);
extern void gmtp_sysctl_exit(void);
#else
static inline int gmtp_sysctl_init(void)
{
	return 0;
}

static inline void gmtp_sysctl_exit(void)
{
}
#endif

#endif /* _GMTP_H */
