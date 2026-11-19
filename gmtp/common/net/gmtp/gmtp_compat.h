/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Kernel API shims shared by the two GMTP trees.
 *
 * GMTP builds in two kernels: the host tree (netdev net-next, Linux 7.3)
 * and the DCE/NUSE library (Linux 5.10.47). Files under gmtp/common use
 * these helpers instead of APIs that changed between those versions.
 */
#ifndef _GMTP_COMPAT_H
#define _GMTP_COMPAT_H

#include <linux/version.h>
#include <linux/timer.h>
#include <net/sock.h>
#include <net/inet_connection_sock.h>
#include <net/request_sock.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 12, 0)
#include <linux/unaligned.h>
#else
#include <asm/unaligned.h>
#endif

/* from_timer() became timer_container_of() in 6.16. */
#ifndef timer_container_of
#define timer_container_of(var, callback_timer, timer_fieldname) \
	from_timer(var, callback_timer, timer_fieldname)
#endif

/* del_timer*() became timer_delete*() in 6.2; the old names left in 6.15. */
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 2, 0)
#define timer_delete(t)		del_timer(t)
#define timer_delete_sync(t)	del_timer_sync(t)
#endif

/* TCP_SYNQ_INTERVAL (HZ/5) was removed from include/net/tcp.h. */
#define GMTP_SYNQ_INTERVAL	(HZ / 5)

/*
 * Linux 6.15 moved the retransmit timer from inet_connection_sock into
 * struct sock (tcp_retransmit_timer), dropped icsk_timeout and
 * icsk_ack.timeout in favour of timer->expires, and gave keepalive its own
 * icsk_keepalive_timer instead of sk->sk_timer.
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 15, 0)
#define GMTP_NEW_ICSK_TIMERS 1
#endif

static inline struct timer_list *gmtp_rtx_timer(struct sock *sk)
{
#ifdef GMTP_NEW_ICSK_TIMERS
	return &sk->tcp_retransmit_timer;
#else
	return &inet_csk(sk)->icsk_retransmit_timer;
#endif
}

static inline struct sock *gmtp_sk_from_rtx_timer(struct timer_list *t)
{
#ifdef GMTP_NEW_ICSK_TIMERS
	return container_of(t, struct sock, tcp_retransmit_timer);
#else
	struct inet_connection_sock *icsk =
		container_of(t, struct inet_connection_sock,
			     icsk_retransmit_timer);
	return &icsk->icsk_inet.sk;
#endif
}

static inline unsigned long gmtp_rtx_timeout(const struct sock *sk)
{
#ifdef GMTP_NEW_ICSK_TIMERS
	return READ_ONCE(sk->tcp_retransmit_timer.expires);
#else
	return inet_csk(sk)->icsk_timeout;
#endif
}

static inline unsigned long gmtp_delack_timeout(const struct sock *sk)
{
#ifdef GMTP_NEW_ICSK_TIMERS
	return READ_ONCE(inet_csk(sk)->icsk_delack_timer.expires);
#else
	return inet_csk(sk)->icsk_ack.timeout;
#endif
}

static inline struct timer_list *gmtp_keepalive_timer_of(struct sock *sk)
{
#ifdef GMTP_NEW_ICSK_TIMERS
	return &inet_csk(sk)->icsk_keepalive_timer;
#else
	return &sk->sk_timer;
#endif
}

static inline struct sock *gmtp_sk_from_keepalive_timer(struct timer_list *t)
{
#ifdef GMTP_NEW_ICSK_TIMERS
	struct inet_connection_sock *icsk =
		container_of(t, struct inet_connection_sock,
			     icsk_keepalive_timer);
	return &icsk->icsk_inet.sk;
#else
	return container_of(t, struct sock, sk_timer);
#endif
}

/* inet_csk_reset_keepalive_timer() was removed upstream; GMTP keeps it. */
static inline void gmtp_reset_keepalive_timer(struct sock *sk,
					      unsigned long len)
{
	sk_reset_timer(sk, gmtp_keepalive_timer_of(sk), jiffies + len);
}

/*
 * Request sockets. Linux 6.10 added a reset reason to send_reset(), and
 * net-next removed rtx_syn_ack/syn_ack_timeout from request_sock_ops
 * (inet_rtx_syn_ack() is gone) and added opt_child_init to
 * syn_recv_sock(). The version cut-offs below are chosen for the two
 * supported trees (5.10.47 and 7.3); kernels in between are not tested.
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 15, 0)
int gmtp_v4_rtx_register_reply(const struct sock *sk,
			       struct request_sock *req);

static inline int gmtp_rtx_syn_ack(const struct sock *sk,
				   struct request_sock *req)
{
	return gmtp_v4_rtx_register_reply(sk, req);
}

static inline void gmtp_reqsk_send_reset(struct request_sock *req,
					 const struct sock *sk,
					 struct sk_buff *skb)
{
	req->rsk_ops->send_reset(sk, skb, SK_RST_REASON_NOT_SPECIFIED);
}

static inline struct sock *gmtp_syn_recv_sock(const struct sock *sk,
					      struct sk_buff *skb,
					      struct request_sock *req,
					      struct dst_entry *dst,
					      struct request_sock *req_unhash,
					      bool *own_req)
{
	return inet_csk(sk)->icsk_af_ops->syn_recv_sock(sk, skb, req, dst,
							req_unhash, own_req,
							NULL);
}
#else
static inline int gmtp_rtx_syn_ack(const struct sock *sk,
				   struct request_sock *req)
{
	return inet_rtx_syn_ack(sk, req);
}

static inline void gmtp_reqsk_send_reset(struct request_sock *req,
					 const struct sock *sk,
					 struct sk_buff *skb)
{
	req->rsk_ops->send_reset(sk, skb);
}

static inline struct sock *gmtp_syn_recv_sock(const struct sock *sk,
					      struct sk_buff *skb,
					      struct request_sock *req,
					      struct dst_entry *dst,
					      struct request_sock *req_unhash,
					      bool *own_req)
{
	return inet_csk(sk)->icsk_af_ops->syn_recv_sock(sk, skb, req, dst,
							req_unhash, own_req);
}
#endif

#endif /* _GMTP_COMPAT_H */
