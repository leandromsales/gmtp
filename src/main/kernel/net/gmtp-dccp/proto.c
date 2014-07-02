/*
 *  net/gmtp/proto.c
 *
 *  An implementation of the GMTP protocol
 *  Arnaldo Carvalho de Melo <acme@conectiva.com.br>
 *
 *	This program is free software; you can redistribute it and/or modify it
 *	under the terms of the GNU General Public License version 2 as
 *	published by the Free Software Foundation.
 */

#include <linux/gmtp.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/in.h>
#include <linux/if_arp.h>
#include <linux/init.h>
#include <linux/random.h>
#include <linux/slab.h>
#include <net/checksum.h>

#include <net/inet_sock.h>
#include <net/sock.h>
#include <net/xfrm.h>

#include <asm/ioctls.h>
#include <linux/spinlock.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <linux/poll.h>

#include "ccid.h"
#include "gmtp.h"
#include "feat.h"

DEFINE_SNMP_STAT(struct gmtp_mib, gmtp_statistics) __read_mostly;

EXPORT_SYMBOL_GPL(gmtp_statistics);

struct percpu_counter gmtp_orphan_count;
EXPORT_SYMBOL_GPL(gmtp_orphan_count);

struct inet_hashinfo gmtp_hashinfo;
EXPORT_SYMBOL_GPL(gmtp_hashinfo);

/* the maximum queue length for tx in packets. 0 is no limit */
int sysctl_gmtp_tx_qlen __read_mostly = 5;

#ifdef CONFIG_IP_GMTP_DEBUG
static const char *gmtp_state_name(const int state)
{
	static const char *const gmtp_state_names[] = {
	[GMTP_OPEN]		= "OPEN",
	[GMTP_REQUESTING]	= "REQUESTING",
	[GMTP_PARTOPEN]		= "PARTOPEN",
	[GMTP_LISTEN]		= "LISTEN",
	[GMTP_RESPOND]		= "RESPOND",
	[GMTP_CLOSING]		= "CLOSING",
	[GMTP_ACTIVE_CLOSEREQ]	= "CLOSEREQ",
	[GMTP_PASSIVE_CLOSE]	= "PASSIVE_CLOSE",
	[GMTP_PASSIVE_CLOSEREQ]	= "PASSIVE_CLOSEREQ",
	[GMTP_TIME_WAIT]	= "TIME_WAIT",
	[GMTP_CLOSED]		= "CLOSED",
	};

	if (state >= GMTP_MAX_STATES)
		return "INVALID STATE!";
	else
		return gmtp_state_names[state];
}
#endif

void gmtp_set_state(struct sock *sk, const int state)
{
	const int oldstate = sk->sk_state;

	gmtp_pr_debug("%s(%p)  %s  -->  %s\n", gmtp_role(sk), sk,
		      gmtp_state_name(oldstate), gmtp_state_name(state));
	WARN_ON(state == oldstate);

	switch (state) {
	case GMTP_OPEN:
		if (oldstate != GMTP_OPEN)
			GMTP_INC_STATS(GMTP_MIB_CURRESTAB);
		/* Client retransmits all Confirm options until entering OPEN */
		if (oldstate == GMTP_PARTOPEN)
			gmtp_feat_list_purge(&gmtp_sk(sk)->gmtps_featneg);
		break;

	case GMTP_CLOSED:
		if (oldstate == GMTP_OPEN || oldstate == GMTP_ACTIVE_CLOSEREQ ||
		    oldstate == GMTP_CLOSING)
			GMTP_INC_STATS(GMTP_MIB_ESTABRESETS);

		sk->sk_prot->unhash(sk);
		if (inet_csk(sk)->icsk_bind_hash != NULL &&
		    !(sk->sk_userlocks & SOCK_BINDPORT_LOCK))
			inet_put_port(sk);
		/* fall through */
	default:
		if (oldstate == GMTP_OPEN)
			GMTP_DEC_STATS(GMTP_MIB_CURRESTAB);
	}

	/* Change state AFTER socket is unhashed to avoid closed
	 * socket sitting in hash tables.
	 */
	sk->sk_state = state;
}

EXPORT_SYMBOL_GPL(gmtp_set_state);

static void gmtp_finish_passive_close(struct sock *sk)
{
	switch (sk->sk_state) {
	case GMTP_PASSIVE_CLOSE:
		/* Node (client or server) has received Close packet. */
		gmtp_send_reset(sk, GMTP_RESET_CODE_CLOSED);
		gmtp_set_state(sk, GMTP_CLOSED);
		break;
	case GMTP_PASSIVE_CLOSEREQ:
		/*
		 * Client received CloseReq. We set the `active' flag so that
		 * gmtp_send_close() retransmits the Close as per RFC 4340, 8.3.
		 */
		gmtp_send_close(sk, 1);
		gmtp_set_state(sk, GMTP_CLOSING);
	}
}

void gmtp_done(struct sock *sk)
{
	gmtp_set_state(sk, GMTP_CLOSED);
	gmtp_clear_xmit_timers(sk);

	sk->sk_shutdown = SHUTDOWN_MASK;

	if (!sock_flag(sk, SOCK_DEAD))
		sk->sk_state_change(sk);
	else
		inet_csk_destroy_sock(sk);
}

EXPORT_SYMBOL_GPL(gmtp_done);

const char *gmtp_packet_name(const int type)
{
	static const char *const gmtp_packet_names[] = {
		[GMTP_PKT_REQUEST]  = "REQUEST",
		[GMTP_PKT_RESPONSE] = "RESPONSE",
		[GMTP_PKT_DATA]	    = "DATA",
		[GMTP_PKT_ACK]	    = "ACK",
		[GMTP_PKT_DATAACK]  = "DATAACK",
		[GMTP_PKT_CLOSEREQ] = "CLOSEREQ",
		[GMTP_PKT_CLOSE]    = "CLOSE",
		[GMTP_PKT_RESET]    = "RESET",
		[GMTP_PKT_SYNC]	    = "SYNC",
		[GMTP_PKT_SYNCACK]  = "SYNCACK",
	};

	if (type >= GMTP_NR_PKT_TYPES)
		return "INVALID";
	else
		return gmtp_packet_names[type];
}

EXPORT_SYMBOL_GPL(gmtp_packet_name);

int gmtp_init_sock(struct sock *sk, const __u8 ctl_sock_initialized)
{
	struct gmtp_sock *dp = gmtp_sk(sk);
	struct inet_connection_sock *icsk = inet_csk(sk);

	icsk->icsk_rto		= GMTP_TIMEOUT_INIT;
	icsk->icsk_syn_retries	= sysctl_gmtp_request_retries;
	sk->sk_state		= GMTP_CLOSED;
	sk->sk_write_space	= gmtp_write_space;
	icsk->icsk_sync_mss	= gmtp_sync_mss;
	dp->gmtps_mss_cache	= 536;
	dp->gmtps_rate_last	= jiffies;
	dp->gmtps_role		= GMTP_ROLE_UNDEFINED;
	dp->gmtps_service	= GMTP_SERVICE_CODE_IS_ABSENT;
	dp->gmtps_tx_qlen	= sysctl_gmtp_tx_qlen;

	gmtp_init_xmit_timers(sk);

	INIT_LIST_HEAD(&dp->gmtps_featneg);
	/* control socket doesn't need feat nego */
	if (likely(ctl_sock_initialized))
		return gmtp_feat_init(sk);
	return 0;
}

EXPORT_SYMBOL_GPL(gmtp_init_sock);

void gmtp_destroy_sock(struct sock *sk)
{
	struct gmtp_sock *dp = gmtp_sk(sk);

	/*
	 * GMTP doesn't use sk_write_queue, just sk_send_head
	 * for retransmissions
	 */
	if (sk->sk_send_head != NULL) {
		kfree_skb(sk->sk_send_head);
		sk->sk_send_head = NULL;
	}

	/* Clean up a referenced GMTP bind bucket. */
	if (inet_csk(sk)->icsk_bind_hash != NULL)
		inet_put_port(sk);

	kfree(dp->gmtps_service_list);
	dp->gmtps_service_list = NULL;

	if (dp->gmtps_hc_rx_ackvec != NULL) {
		gmtp_ackvec_free(dp->gmtps_hc_rx_ackvec);
		dp->gmtps_hc_rx_ackvec = NULL;
	}
	ccid_hc_rx_delete(dp->gmtps_hc_rx_ccid, sk);
	ccid_hc_tx_delete(dp->gmtps_hc_tx_ccid, sk);
	dp->gmtps_hc_rx_ccid = dp->gmtps_hc_tx_ccid = NULL;

	/* clean up feature negotiation state */
	gmtp_feat_list_purge(&dp->gmtps_featneg);
}

EXPORT_SYMBOL_GPL(gmtp_destroy_sock);

static inline int gmtp_listen_start(struct sock *sk, int backlog)
{
	struct gmtp_sock *dp = gmtp_sk(sk);

	dp->gmtps_role = GMTP_ROLE_LISTEN;
	/* do not start to listen if feature negotiation setup fails */
	if (gmtp_feat_finalise_settings(dp))
		return -EPROTO;
	return inet_csk_listen_start(sk, backlog);
}

static inline int gmtp_need_reset(int state)
{
	return state != GMTP_CLOSED && state != GMTP_LISTEN &&
	       state != GMTP_REQUESTING;
}

int gmtp_disconnect(struct sock *sk, int flags)
{
	struct inet_connection_sock *icsk = inet_csk(sk);
	struct inet_sock *inet = inet_sk(sk);
	int err = 0;
	const int old_state = sk->sk_state;

	if (old_state != GMTP_CLOSED)
		gmtp_set_state(sk, GMTP_CLOSED);

	/*
	 * This corresponds to the ABORT function of RFC793, sec. 3.8
	 * TCP uses a RST segment, GMTP a Reset packet with Code 2, "Aborted".
	 */
	if (old_state == GMTP_LISTEN) {
		inet_csk_listen_stop(sk);
	} else if (gmtp_need_reset(old_state)) {
		gmtp_send_reset(sk, GMTP_RESET_CODE_ABORTED);
		sk->sk_err = ECONNRESET;
	} else if (old_state == GMTP_REQUESTING)
		sk->sk_err = ECONNRESET;

	gmtp_clear_xmit_timers(sk);

	__skb_queue_purge(&sk->sk_receive_queue);
	__skb_queue_purge(&sk->sk_write_queue);
	if (sk->sk_send_head != NULL) {
		__kfree_skb(sk->sk_send_head);
		sk->sk_send_head = NULL;
	}

	inet->inet_dport = 0;

	if (!(sk->sk_userlocks & SOCK_BINDADDR_LOCK))
		inet_reset_saddr(sk);

	sk->sk_shutdown = 0;
	sock_reset_flag(sk, SOCK_DONE);

	icsk->icsk_backoff = 0;
	inet_csk_delack_init(sk);
	__sk_dst_reset(sk);

	WARN_ON(inet->inet_num && !icsk->icsk_bind_hash);

	sk->sk_error_report(sk);
	return err;
}

EXPORT_SYMBOL_GPL(gmtp_disconnect);

/*
 *	Wait for a GMTP event.
 *
 *	Note that we don't need to lock the socket, as the upper poll layers
 *	take care of normal races (between the test and the event) and we don't
 *	go look at any of the socket buffers directly.
 */
unsigned int gmtp_poll(struct file *file, struct socket *sock,
		       poll_table *wait)
{
	unsigned int mask;
	struct sock *sk = sock->sk;

	sock_poll_wait(file, sk_sleep(sk), wait);
	if (sk->sk_state == GMTP_LISTEN)
		return inet_csk_listen_poll(sk);

	/* Socket is not locked. We are protected from async events
	   by poll logic and correct handling of state changes
	   made by another threads is impossible in any case.
	 */

	mask = 0;
	if (sk->sk_err)
		mask = POLLERR;

	if (sk->sk_shutdown == SHUTDOWN_MASK || sk->sk_state == GMTP_CLOSED)
		mask |= POLLHUP;
	if (sk->sk_shutdown & RCV_SHUTDOWN)
		mask |= POLLIN | POLLRDNORM | POLLRDHUP;

	/* Connected? */
	if ((1 << sk->sk_state) & ~(GMTPF_REQUESTING | GMTPF_RESPOND)) {
		if (atomic_read(&sk->sk_rmem_alloc) > 0)
			mask |= POLLIN | POLLRDNORM;

		if (!(sk->sk_shutdown & SEND_SHUTDOWN)) {
			if (sk_stream_wspace(sk) >= sk_stream_min_wspace(sk)) {
				mask |= POLLOUT | POLLWRNORM;
			} else {  /* send SIGIO later */
				set_bit(SOCK_ASYNC_NOSPACE,
					&sk->sk_socket->flags);
				set_bit(SOCK_NOSPACE, &sk->sk_socket->flags);

				/* Race breaker. If space is freed after
				 * wspace test but before the flags are set,
				 * IO signal will be lost.
				 */
				if (sk_stream_wspace(sk) >= sk_stream_min_wspace(sk))
					mask |= POLLOUT | POLLWRNORM;
			}
		}
	}
	return mask;
}

EXPORT_SYMBOL_GPL(gmtp_poll);

int gmtp_ioctl(struct sock *sk, int cmd, unsigned long arg)
{
	int rc = -ENOTCONN;

	lock_sock(sk);

	if (sk->sk_state == GMTP_LISTEN)
		goto out;

	switch (cmd) {
	case SIOCINQ: {
		struct sk_buff *skb;
		unsigned long amount = 0;

		skb = skb_peek(&sk->sk_receive_queue);
		if (skb != NULL) {
			/*
			 * We will only return the amount of this packet since
			 * that is all that will be read.
			 */
			amount = skb->len;
		}
		rc = put_user(amount, (int __user *)arg);
	}
		break;
	default:
		rc = -ENOIOCTLCMD;
		break;
	}
out:
	release_sock(sk);
	return rc;
}

EXPORT_SYMBOL_GPL(gmtp_ioctl);

static int gmtp_setsockopt_service(struct sock *sk, const __be32 service,
				   char __user *optval, unsigned int optlen)
{
	struct gmtp_sock *dp = gmtp_sk(sk);
	struct gmtp_service_list *sl = NULL;

	if (service == GMTP_SERVICE_INVALID_VALUE ||
	    optlen > GMTP_SERVICE_LIST_MAX_LEN * sizeof(u32))
		return -EINVAL;

	if (optlen > sizeof(service)) {
		sl = kmalloc(optlen, GFP_KERNEL);
		if (sl == NULL)
			return -ENOMEM;

		sl->gmtpsl_nr = optlen / sizeof(u32) - 1;
		if (copy_from_user(sl->gmtpsl_list,
				   optval + sizeof(service),
				   optlen - sizeof(service)) ||
		    gmtp_list_has_service(sl, GMTP_SERVICE_INVALID_VALUE)) {
			kfree(sl);
			return -EFAULT;
		}
	}

	lock_sock(sk);
	dp->gmtps_service = service;

	kfree(dp->gmtps_service_list);

	dp->gmtps_service_list = sl;
	release_sock(sk);
	return 0;
}

static int gmtp_setsockopt_cscov(struct sock *sk, int cscov, bool rx)
{
	u8 *list, len;
	int i, rc;

	if (cscov < 0 || cscov > 15)
		return -EINVAL;
	/*
	 * Populate a list of permissible values, in the range cscov...15. This
	 * is necessary since feature negotiation of single values only works if
	 * both sides incidentally choose the same value. Since the list starts
	 * lowest-value first, negotiation will pick the smallest shared value.
	 */
	if (cscov == 0)
		return 0;
	len = 16 - cscov;

	list = kmalloc(len, GFP_KERNEL);
	if (list == NULL)
		return -ENOBUFS;

	for (i = 0; i < len; i++)
		list[i] = cscov++;

	rc = gmtp_feat_register_sp(sk, GMTPF_MIN_CSUM_COVER, rx, list, len);

	if (rc == 0) {
		if (rx)
			gmtp_sk(sk)->gmtps_pcrlen = cscov;
		else
			gmtp_sk(sk)->gmtps_pcslen = cscov;
	}
	kfree(list);
	return rc;
}

static int gmtp_setsockopt_ccid(struct sock *sk, int type,
				char __user *optval, unsigned int optlen)
{
	u8 *val;
	int rc = 0;

	if (optlen < 1 || optlen > GMTP_FEAT_MAX_SP_VALS)
		return -EINVAL;

	val = memdup_user(optval, optlen);
	if (IS_ERR(val))
		return PTR_ERR(val);

	lock_sock(sk);
	if (type == GMTP_SOCKOPT_TX_CCID || type == GMTP_SOCKOPT_CCID)
		rc = gmtp_feat_register_sp(sk, GMTPF_CCID, 1, val, optlen);

	if (!rc && (type == GMTP_SOCKOPT_RX_CCID || type == GMTP_SOCKOPT_CCID))
		rc = gmtp_feat_register_sp(sk, GMTPF_CCID, 0, val, optlen);
	release_sock(sk);

	kfree(val);
	return rc;
}

static int do_gmtp_setsockopt(struct sock *sk, int level, int optname,
		char __user *optval, unsigned int optlen)
{
	struct gmtp_sock *dp = gmtp_sk(sk);
	int val, err = 0;

	switch (optname) {
	case GMTP_SOCKOPT_PACKET_SIZE:
		GMTP_WARN("sockopt(PACKET_SIZE) is deprecated: fix your app\n");
		return 0;
	case GMTP_SOCKOPT_CHANGE_L:
	case GMTP_SOCKOPT_CHANGE_R:
		GMTP_WARN("sockopt(CHANGE_L/R) is deprecated: fix your app\n");
		return 0;
	case GMTP_SOCKOPT_CCID:
	case GMTP_SOCKOPT_RX_CCID:
	case GMTP_SOCKOPT_TX_CCID:
		return gmtp_setsockopt_ccid(sk, optname, optval, optlen);
	}

	if (optlen < (int)sizeof(int))
		return -EINVAL;

	if (get_user(val, (int __user *)optval))
		return -EFAULT;

	if (optname == GMTP_SOCKOPT_SERVICE)
		return gmtp_setsockopt_service(sk, val, optval, optlen);

	lock_sock(sk);
	switch (optname) {
	case GMTP_SOCKOPT_SERVER_TIMEWAIT:
		if (dp->gmtps_role != GMTP_ROLE_SERVER)
			err = -EOPNOTSUPP;
		else
			dp->gmtps_server_timewait = (val != 0);
		break;
	case GMTP_SOCKOPT_SEND_CSCOV:
		err = gmtp_setsockopt_cscov(sk, val, false);
		break;
	case GMTP_SOCKOPT_RECV_CSCOV:
		err = gmtp_setsockopt_cscov(sk, val, true);
		break;
	case GMTP_SOCKOPT_QPOLICY_ID:
		if (sk->sk_state != GMTP_CLOSED)
			err = -EISCONN;
		else if (val < 0 || val >= GMTPQ_POLICY_MAX)
			err = -EINVAL;
		else
			dp->gmtps_qpolicy = val;
		break;
	case GMTP_SOCKOPT_QPOLICY_TXQLEN:
		if (val < 0)
			err = -EINVAL;
		else
			dp->gmtps_tx_qlen = val;
		break;
	default:
		err = -ENOPROTOOPT;
		break;
	}
	release_sock(sk);

	return err;
}

int gmtp_setsockopt(struct sock *sk, int level, int optname,
		    char __user *optval, unsigned int optlen)
{
	if (level != SOL_GMTP)
		return inet_csk(sk)->icsk_af_ops->setsockopt(sk, level,
							     optname, optval,
							     optlen);
	return do_gmtp_setsockopt(sk, level, optname, optval, optlen);
}

EXPORT_SYMBOL_GPL(gmtp_setsockopt);

#ifdef CONFIG_COMPAT
int compat_gmtp_setsockopt(struct sock *sk, int level, int optname,
			   char __user *optval, unsigned int optlen)
{
	if (level != SOL_GMTP)
		return inet_csk_compat_setsockopt(sk, level, optname,
						  optval, optlen);
	return do_gmtp_setsockopt(sk, level, optname, optval, optlen);
}

EXPORT_SYMBOL_GPL(compat_gmtp_setsockopt);
#endif

static int gmtp_getsockopt_service(struct sock *sk, int len,
				   __be32 __user *optval,
				   int __user *optlen)
{
	const struct gmtp_sock *dp = gmtp_sk(sk);
	const struct gmtp_service_list *sl;
	int err = -ENOENT, slen = 0, total_len = sizeof(u32);

	lock_sock(sk);
	if ((sl = dp->gmtps_service_list) != NULL) {
		slen = sl->gmtpsl_nr * sizeof(u32);
		total_len += slen;
	}

	err = -EINVAL;
	if (total_len > len)
		goto out;

	err = 0;
	if (put_user(total_len, optlen) ||
	    put_user(dp->gmtps_service, optval) ||
	    (sl != NULL && copy_to_user(optval + 1, sl->gmtpsl_list, slen)))
		err = -EFAULT;
out:
	release_sock(sk);
	return err;
}

static int do_gmtp_getsockopt(struct sock *sk, int level, int optname,
		    char __user *optval, int __user *optlen)
{
	struct gmtp_sock *dp;
	int val, len;

	if (get_user(len, optlen))
		return -EFAULT;

	if (len < (int)sizeof(int))
		return -EINVAL;

	dp = gmtp_sk(sk);

	switch (optname) {
	case GMTP_SOCKOPT_PACKET_SIZE:
		GMTP_WARN("sockopt(PACKET_SIZE) is deprecated: fix your app\n");
		return 0;
	case GMTP_SOCKOPT_SERVICE:
		return gmtp_getsockopt_service(sk, len,
					       (__be32 __user *)optval, optlen);
	case GMTP_SOCKOPT_GET_CUR_MPS:
		val = dp->gmtps_mss_cache;
		break;
	case GMTP_SOCKOPT_AVAILABLE_CCIDS:
		return ccid_getsockopt_builtin_ccids(sk, len, optval, optlen);
	case GMTP_SOCKOPT_TX_CCID:
		val = ccid_get_current_tx_ccid(dp);
		if (val < 0)
			return -ENOPROTOOPT;
		break;
	case GMTP_SOCKOPT_RX_CCID:
		val = ccid_get_current_rx_ccid(dp);
		if (val < 0)
			return -ENOPROTOOPT;
		break;
	case GMTP_SOCKOPT_SERVER_TIMEWAIT:
		val = dp->gmtps_server_timewait;
		break;
	case GMTP_SOCKOPT_SEND_CSCOV:
		val = dp->gmtps_pcslen;
		break;
	case GMTP_SOCKOPT_RECV_CSCOV:
		val = dp->gmtps_pcrlen;
		break;
	case GMTP_SOCKOPT_QPOLICY_ID:
		val = dp->gmtps_qpolicy;
		break;
	case GMTP_SOCKOPT_QPOLICY_TXQLEN:
		val = dp->gmtps_tx_qlen;
		break;
	case 128 ... 191:
		return ccid_hc_rx_getsockopt(dp->gmtps_hc_rx_ccid, sk, optname,
					     len, (u32 __user *)optval, optlen);
	case 192 ... 255:
		return ccid_hc_tx_getsockopt(dp->gmtps_hc_tx_ccid, sk, optname,
					     len, (u32 __user *)optval, optlen);
	default:
		return -ENOPROTOOPT;
	}

	len = sizeof(val);
	if (put_user(len, optlen) || copy_to_user(optval, &val, len))
		return -EFAULT;

	return 0;
}

int gmtp_getsockopt(struct sock *sk, int level, int optname,
		    char __user *optval, int __user *optlen)
{
	if (level != SOL_GMTP)
		return inet_csk(sk)->icsk_af_ops->getsockopt(sk, level,
							     optname, optval,
							     optlen);
	return do_gmtp_getsockopt(sk, level, optname, optval, optlen);
}

EXPORT_SYMBOL_GPL(gmtp_getsockopt);

#ifdef CONFIG_COMPAT
int compat_gmtp_getsockopt(struct sock *sk, int level, int optname,
			   char __user *optval, int __user *optlen)
{
	if (level != SOL_GMTP)
		return inet_csk_compat_getsockopt(sk, level, optname,
						  optval, optlen);
	return do_gmtp_getsockopt(sk, level, optname, optval, optlen);
}

EXPORT_SYMBOL_GPL(compat_gmtp_getsockopt);
#endif

static int gmtp_msghdr_parse(struct msghdr *msg, struct sk_buff *skb)
{
	struct cmsghdr *cmsg = CMSG_FIRSTHDR(msg);

	/*
	 * Assign an (opaque) qpolicy priority value to skb->priority.
	 *
	 * We are overloading this skb field for use with the qpolicy subystem.
	 * The skb->priority is normally used for the SO_PRIORITY option, which
	 * is initialised from sk_priority. Since the assignment of sk_priority
	 * to skb->priority happens later (on layer 3), we overload this field
	 * for use with queueing priorities as long as the skb is on layer 4.
	 * The default priority value (if nothing is set) is 0.
	 */
	skb->priority = 0;

	for (; cmsg != NULL; cmsg = CMSG_NXTHDR(msg, cmsg)) {

		if (!CMSG_OK(msg, cmsg))
			return -EINVAL;

		if (cmsg->cmsg_level != SOL_GMTP)
			continue;

		if (cmsg->cmsg_type <= GMTP_SCM_QPOLICY_MAX &&
		    !gmtp_qpolicy_param_ok(skb->sk, cmsg->cmsg_type))
			return -EINVAL;

		switch (cmsg->cmsg_type) {
		case GMTP_SCM_PRIORITY:
			if (cmsg->cmsg_len != CMSG_LEN(sizeof(__u32)))
				return -EINVAL;
			skb->priority = *(__u32 *)CMSG_DATA(cmsg);
			break;
		default:
			return -EINVAL;
		}
	}
	return 0;
}

int gmtp_sendmsg(struct kiocb *iocb, struct sock *sk, struct msghdr *msg,
		 size_t len)
{
	const struct gmtp_sock *dp = gmtp_sk(sk);
	const int flags = msg->msg_flags;
	const int noblock = flags & MSG_DONTWAIT;
	struct sk_buff *skb;
	int rc, size;
	long timeo;

	if (len > dp->gmtps_mss_cache)
		return -EMSGSIZE;

	lock_sock(sk);

	if (gmtp_qpolicy_full(sk)) {
		rc = -EAGAIN;
		goto out_release;
	}

	timeo = sock_sndtimeo(sk, noblock);

	/*
	 * We have to use sk_stream_wait_connect here to set sk_write_pending,
	 * so that the trick in gmtp_rcv_request_sent_state_process.
	 */
	/* Wait for a connection to finish. */
	if ((1 << sk->sk_state) & ~(GMTPF_OPEN | GMTPF_PARTOPEN))
		if ((rc = sk_stream_wait_connect(sk, &timeo)) != 0)
			goto out_release;

	size = sk->sk_prot->max_header + len;
	release_sock(sk);
	skb = sock_alloc_send_skb(sk, size, noblock, &rc);
	lock_sock(sk);
	if (skb == NULL)
		goto out_release;

	skb_reserve(skb, sk->sk_prot->max_header);
	rc = memcpy_fromiovec(skb_put(skb, len), msg->msg_iov, len);
	if (rc != 0)
		goto out_discard;

	rc = gmtp_msghdr_parse(msg, skb);
	if (rc != 0)
		goto out_discard;

	gmtp_qpolicy_push(sk, skb);
	/*
	 * The xmit_timer is set if the TX CCID is rate-based and will expire
	 * when congestion control permits to release further packets into the
	 * network. Window-based CCIDs do not use this timer.
	 */
	if (!timer_pending(&dp->gmtps_xmit_timer))
		gmtp_write_xmit(sk);
out_release:
	release_sock(sk);
	return rc ? : len;
out_discard:
	kfree_skb(skb);
	goto out_release;
}

EXPORT_SYMBOL_GPL(gmtp_sendmsg);

int gmtp_recvmsg(struct kiocb *iocb, struct sock *sk, struct msghdr *msg,
		 size_t len, int nonblock, int flags, int *addr_len)
{
	const struct gmtp_hdr *dh;
	long timeo;

	lock_sock(sk);

	if (sk->sk_state == GMTP_LISTEN) {
		len = -ENOTCONN;
		goto out;
	}

	timeo = sock_rcvtimeo(sk, nonblock);

	do {
		struct sk_buff *skb = skb_peek(&sk->sk_receive_queue);

		if (skb == NULL)
			goto verify_sock_status;

		dh = gmtp_hdr(skb);

		switch (dh->gmtph_type) {
		case GMTP_PKT_DATA:
		case GMTP_PKT_DATAACK:
			goto found_ok_skb;

		case GMTP_PKT_CLOSE:
		case GMTP_PKT_CLOSEREQ:
			if (!(flags & MSG_PEEK))
				gmtp_finish_passive_close(sk);
			/* fall through */
		case GMTP_PKT_RESET:
			gmtp_pr_debug("found fin (%s) ok!\n",
				      gmtp_packet_name(dh->gmtph_type));
			len = 0;
			goto found_fin_ok;
		default:
			gmtp_pr_debug("packet_type=%s\n",
				      gmtp_packet_name(dh->gmtph_type));
			sk_eat_skb(sk, skb, false);
		}
verify_sock_status:
		if (sock_flag(sk, SOCK_DONE)) {
			len = 0;
			break;
		}

		if (sk->sk_err) {
			len = sock_error(sk);
			break;
		}

		if (sk->sk_shutdown & RCV_SHUTDOWN) {
			len = 0;
			break;
		}

		if (sk->sk_state == GMTP_CLOSED) {
			if (!sock_flag(sk, SOCK_DONE)) {
				/* This occurs when user tries to read
				 * from never connected socket.
				 */
				len = -ENOTCONN;
				break;
			}
			len = 0;
			break;
		}

		if (!timeo) {
			len = -EAGAIN;
			break;
		}

		if (signal_pending(current)) {
			len = sock_intr_errno(timeo);
			break;
		}

		sk_wait_data(sk, &timeo);
		continue;
	found_ok_skb:
		if (len > skb->len)
			len = skb->len;
		else if (len < skb->len)
			msg->msg_flags |= MSG_TRUNC;

		if (skb_copy_datagram_iovec(skb, 0, msg->msg_iov, len)) {
			/* Exception. Bailout! */
			len = -EFAULT;
			break;
		}
		if (flags & MSG_TRUNC)
			len = skb->len;
	found_fin_ok:
		if (!(flags & MSG_PEEK))
			sk_eat_skb(sk, skb, false);
		break;
	} while (1);
out:
	release_sock(sk);
	return len;
}

EXPORT_SYMBOL_GPL(gmtp_recvmsg);

int inet_gmtp_listen(struct socket *sock, int backlog)
{
	struct sock *sk = sock->sk;
	unsigned char old_state;
	int err;

	lock_sock(sk);

	err = -EINVAL;
	if (sock->state != SS_UNCONNECTED || sock->type != SOCK_GMTP)
		goto out;

	old_state = sk->sk_state;
	if (!((1 << old_state) & (GMTPF_CLOSED | GMTPF_LISTEN)))
		goto out;

	/* Really, if the socket is already in listen state
	 * we can only allow the backlog to be adjusted.
	 */
	if (old_state != GMTP_LISTEN) {
		/*
		 * FIXME: here it probably should be sk->sk_prot->listen_start
		 * see tcp_listen_start
		 */
		err = gmtp_listen_start(sk, backlog);
		if (err)
			goto out;
	}
	sk->sk_max_ack_backlog = backlog;
	err = 0;

out:
	release_sock(sk);
	return err;
}

EXPORT_SYMBOL_GPL(inet_gmtp_listen);

static void gmtp_terminate_connection(struct sock *sk)
{
	u8 next_state = GMTP_CLOSED;

	switch (sk->sk_state) {
	case GMTP_PASSIVE_CLOSE:
	case GMTP_PASSIVE_CLOSEREQ:
		gmtp_finish_passive_close(sk);
		break;
	case GMTP_PARTOPEN:
		gmtp_pr_debug("Stop PARTOPEN timer (%p)\n", sk);
		inet_csk_clear_xmit_timer(sk, ICSK_TIME_DACK);
		/* fall through */
	case GMTP_OPEN:
		gmtp_send_close(sk, 1);

		if (gmtp_sk(sk)->gmtps_role == GMTP_ROLE_SERVER &&
		    !gmtp_sk(sk)->gmtps_server_timewait)
			next_state = GMTP_ACTIVE_CLOSEREQ;
		else
			next_state = GMTP_CLOSING;
		/* fall through */
	default:
		gmtp_set_state(sk, next_state);
	}
}

void gmtp_close(struct sock *sk, long timeout)
{
	struct gmtp_sock *dp = gmtp_sk(sk);
	struct sk_buff *skb;
	u32 data_was_unread = 0;
	int state;

	lock_sock(sk);

	sk->sk_shutdown = SHUTDOWN_MASK;

	if (sk->sk_state == GMTP_LISTEN) {
		gmtp_set_state(sk, GMTP_CLOSED);

		/* Special case. */
		inet_csk_listen_stop(sk);

		goto adjudge_to_death;
	}

	sk_stop_timer(sk, &dp->gmtps_xmit_timer);

	/*
	 * We need to flush the recv. buffs.  We do this only on the
	 * descriptor close, not protocol-sourced closes, because the
	  *reader process may not have drained the data yet!
	 */
	while ((skb = __skb_dequeue(&sk->sk_receive_queue)) != NULL) {
		data_was_unread += skb->len;
		__kfree_skb(skb);
	}

	if (data_was_unread) {
		/* Unread data was tossed, send an appropriate Reset Code */
		GMTP_WARN("ABORT with %u bytes unread\n", data_was_unread);
		gmtp_send_reset(sk, GMTP_RESET_CODE_ABORTED);
		gmtp_set_state(sk, GMTP_CLOSED);
	} else if (sock_flag(sk, SOCK_LINGER) && !sk->sk_lingertime) {
		/* Check zero linger _after_ checking for unread data. */
		sk->sk_prot->disconnect(sk, 0);
	} else if (sk->sk_state != GMTP_CLOSED) {
		/*
		 * Normal connection termination. May need to wait if there are
		 * still packets in the TX queue that are delayed by the CCID.
		 */
		gmtp_flush_write_queue(sk, &timeout);
		gmtp_terminate_connection(sk);
	}

	/*
	 * Flush write queue. This may be necessary in several cases:
	 * - we have been closed by the peer but still have application data;
	 * - abortive termination (unread data or zero linger time),
	 * - normal termination but queue could not be flushed within time limit
	 */
	__skb_queue_purge(&sk->sk_write_queue);

	sk_stream_wait_close(sk, timeout);

adjudge_to_death:
	state = sk->sk_state;
	sock_hold(sk);
	sock_orphan(sk);

	/*
	 * It is the last release_sock in its life. It will remove backlog.
	 */
	release_sock(sk);
	/*
	 * Now socket is owned by kernel and we acquire BH lock
	 * to finish close. No need to check for user refs.
	 */
	local_bh_disable();
	bh_lock_sock(sk);
	WARN_ON(sock_owned_by_user(sk));

	percpu_counter_inc(sk->sk_prot->orphan_count);

	/* Have we already been destroyed by a softirq or backlog? */
	if (state != GMTP_CLOSED && sk->sk_state == GMTP_CLOSED)
		goto out;

	if (sk->sk_state == GMTP_CLOSED)
		inet_csk_destroy_sock(sk);

	/* Otherwise, socket is reprieved until protocol close. */

out:
	bh_unlock_sock(sk);
	local_bh_enable();
	sock_put(sk);
}

EXPORT_SYMBOL_GPL(gmtp_close);

void gmtp_shutdown(struct sock *sk, int how)
{
	gmtp_pr_debug("called shutdown(%x)\n", how);
}

EXPORT_SYMBOL_GPL(gmtp_shutdown);

static inline int gmtp_mib_init(void)
{
	return snmp_mib_init((void __percpu **)gmtp_statistics,
			     sizeof(struct gmtp_mib),
			     __alignof__(struct gmtp_mib));
}

static inline void gmtp_mib_exit(void)
{
	snmp_mib_free((void __percpu **)gmtp_statistics);
}

static int thash_entries;
module_param(thash_entries, int, 0444);
MODULE_PARM_DESC(thash_entries, "Number of ehash buckets");

#ifdef CONFIG_IP_GMTP_DEBUG
bool gmtp_debug;
module_param(gmtp_debug, bool, 0644);
MODULE_PARM_DESC(gmtp_debug, "Enable debug messages");

EXPORT_SYMBOL_GPL(gmtp_debug);
#endif

static int __init gmtp_init(void)
{
	unsigned long goal;
	int ehash_order, bhash_order, i;
	int rc;

	BUILD_BUG_ON(sizeof(struct gmtp_skb_cb) >
		     FIELD_SIZEOF(struct sk_buff, cb));
	rc = percpu_counter_init(&gmtp_orphan_count, 0);
	if (rc)
		goto out_fail;
	rc = -ENOBUFS;
	inet_hashinfo_init(&gmtp_hashinfo);
	gmtp_hashinfo.bind_bucket_cachep =
		kmem_cache_create("gmtp_bind_bucket",
				  sizeof(struct inet_bind_bucket), 0,
				  SLAB_HWCACHE_ALIGN, NULL);
	if (!gmtp_hashinfo.bind_bucket_cachep)
		goto out_free_percpu;

	/*
	 * Size and allocate the main established and bind bucket
	 * hash tables.
	 *
	 * The methodology is similar to that of the buffer cache.
	 */
	if (totalram_pages >= (128 * 1024))
		goal = totalram_pages >> (21 - PAGE_SHIFT);
	else
		goal = totalram_pages >> (23 - PAGE_SHIFT);

	if (thash_entries)
		goal = (thash_entries *
			sizeof(struct inet_ehash_bucket)) >> PAGE_SHIFT;
	for (ehash_order = 0; (1UL << ehash_order) < goal; ehash_order++)
		;
	do {
		unsigned long hash_size = (1UL << ehash_order) * PAGE_SIZE /
					sizeof(struct inet_ehash_bucket);

		while (hash_size & (hash_size - 1))
			hash_size--;
		gmtp_hashinfo.ehash_mask = hash_size - 1;
		gmtp_hashinfo.ehash = (struct inet_ehash_bucket *)
			__get_free_pages(GFP_ATOMIC|__GFP_NOWARN, ehash_order);
	} while (!gmtp_hashinfo.ehash && --ehash_order > 0);

	if (!gmtp_hashinfo.ehash) {
		GMTP_CRIT("Failed to allocate GMTP established hash table");
		goto out_free_bind_bucket_cachep;
	}

	for (i = 0; i <= gmtp_hashinfo.ehash_mask; i++) {
		INIT_HLIST_NULLS_HEAD(&gmtp_hashinfo.ehash[i].chain, i);
		INIT_HLIST_NULLS_HEAD(&gmtp_hashinfo.ehash[i].twchain, i);
	}

	if (inet_ehash_locks_alloc(&gmtp_hashinfo))
			goto out_free_gmtp_ehash;

	bhash_order = ehash_order;

	do {
		gmtp_hashinfo.bhash_size = (1UL << bhash_order) * PAGE_SIZE /
					sizeof(struct inet_bind_hashbucket);
		if ((gmtp_hashinfo.bhash_size > (64 * 1024)) &&
		    bhash_order > 0)
			continue;
		gmtp_hashinfo.bhash = (struct inet_bind_hashbucket *)
			__get_free_pages(GFP_ATOMIC|__GFP_NOWARN, bhash_order);
	} while (!gmtp_hashinfo.bhash && --bhash_order >= 0);

	if (!gmtp_hashinfo.bhash) {
		GMTP_CRIT("Failed to allocate GMTP bind hash table");
		goto out_free_gmtp_locks;
	}

	for (i = 0; i < gmtp_hashinfo.bhash_size; i++) {
		spin_lock_init(&gmtp_hashinfo.bhash[i].lock);
		INIT_HLIST_HEAD(&gmtp_hashinfo.bhash[i].chain);
	}

	rc = gmtp_mib_init();
	if (rc)
		goto out_free_gmtp_bhash;

	rc = gmtp_ackvec_init();
	if (rc)
		goto out_free_gmtp_mib;

	rc = gmtp_sysctl_init();
	if (rc)
		goto out_ackvec_exit;

	rc = ccid_initialize_builtins();
	if (rc)
		goto out_sysctl_exit;

	gmtp_timestamping_init();

	return 0;

out_sysctl_exit:
	gmtp_sysctl_exit();
out_ackvec_exit:
	gmtp_ackvec_exit();
out_free_gmtp_mib:
	gmtp_mib_exit();
out_free_gmtp_bhash:
	free_pages((unsigned long)gmtp_hashinfo.bhash, bhash_order);
out_free_gmtp_locks:
	inet_ehash_locks_free(&gmtp_hashinfo);
out_free_gmtp_ehash:
	free_pages((unsigned long)gmtp_hashinfo.ehash, ehash_order);
out_free_bind_bucket_cachep:
	kmem_cache_destroy(gmtp_hashinfo.bind_bucket_cachep);
out_free_percpu:
	percpu_counter_destroy(&gmtp_orphan_count);
out_fail:
	gmtp_hashinfo.bhash = NULL;
	gmtp_hashinfo.ehash = NULL;
	gmtp_hashinfo.bind_bucket_cachep = NULL;
	return rc;
}

static void __exit gmtp_fini(void)
{
	ccid_cleanup_builtins();
	gmtp_mib_exit();
	free_pages((unsigned long)gmtp_hashinfo.bhash,
		   get_order(gmtp_hashinfo.bhash_size *
			     sizeof(struct inet_bind_hashbucket)));
	free_pages((unsigned long)gmtp_hashinfo.ehash,
		   get_order((gmtp_hashinfo.ehash_mask + 1) *
			     sizeof(struct inet_ehash_bucket)));
	inet_ehash_locks_free(&gmtp_hashinfo);
	kmem_cache_destroy(gmtp_hashinfo.bind_bucket_cachep);
	gmtp_ackvec_exit();
	gmtp_sysctl_exit();
	percpu_counter_destroy(&gmtp_orphan_count);
}

module_init(gmtp_init);
module_exit(gmtp_fini);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Arnaldo Carvalho de Melo <acme@conectiva.com.br>");
MODULE_DESCRIPTION("GMTP - Datagram Congestion Controlled Protocol");
