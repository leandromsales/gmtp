#include <asm/ioctls.h>

#include <linux/init.h>
#include <linux/module.h>
#include <linux/types.h>

#include <net/inet_hashtables.h>
#include <net/sock.h>
#include <net/tcp.h>

#include <uapi/linux/gmtp.h>
#include <linux/gmtp.h>
#include "gmtp.h"
#include "mcc.h"

struct percpu_counter gmtp_orphan_count;
EXPORT_SYMBOL_GPL(gmtp_orphan_count);

struct inet_hashinfo gmtp_inet_hashinfo;
EXPORT_SYMBOL_GPL(gmtp_inet_hashinfo);

struct gmtp_hashtable* gmtp_hashtable;
EXPORT_SYMBOL_GPL(gmtp_hashtable);

struct gmtp_info* gmtp_info;
EXPORT_SYMBOL_GPL(gmtp_info);

const char *gmtp_packet_name(const int type)
{
	static const char *const gmtp_packet_names[] = {
		[GMTP_PKT_REQUEST]  = "REQUEST",
		[GMTP_PKT_REQUESTNOTIFY]  = "REQUESTNOTIFY",
		[GMTP_PKT_RESPONSE] = "RESPONSE",
		[GMTP_PKT_REGISTER] = "REGISTER",
		[GMTP_PKT_REGISTER_REPLY] = "REGISTER_REPLY",
		[GMTP_PKT_ROUTE_NOTIFY] = "ROUTE_NOTIFY",
		[GMTP_PKT_RELAYQUERY] = "RELAYQUERY",
		[GMTP_PKT_RELAYQUERY_REPLY] = "RELAYQUERY_REPLY",
		[GMTP_PKT_DATA]	    = "DATA",
		[GMTP_PKT_ACK]	    = "ACK",
		[GMTP_PKT_DATAACK]  = "DATAACK",
		[GMTP_PKT_MEDIADESC]  = "MEDIADESC",
		[GMTP_PKT_DATAPULL_REQUEST]  = "DATAPULL_REQUEST",
		[GMTP_PKT_DATAPULL_RESPONSE]  = "DATAPULL_RESPONSE",
		[GMTP_PKT_ELECT_REQUEST]  = "ELECT_REQUEST",
		[GMTP_PKT_ELECT_RESPONSE]  = "ELECT_RESPONSE",
		[GMTP_PKT_CLOSE]    = "CLOSE",
		[GMTP_PKT_RESET]    = "RESET",
		[GMTP_PKT_FEEDBACK]    = "REPORTER_FEEDBACK",
	};

	if (type >= GMTP_NR_PKT_TYPES)
		return "INVALID";
	else
		return gmtp_packet_names[type];
}
EXPORT_SYMBOL_GPL(gmtp_packet_name);

void gmtp_done(struct sock *sk)
{
	gmtp_print_function();

	gmtp_set_state(sk, GMTP_CLOSED);
	gmtp_clear_xmit_timers(sk);

	sk->sk_shutdown = SHUTDOWN_MASK;

	if(!sock_flag(sk, SOCK_DEAD))
		sk->sk_state_change(sk);
	else
		inet_csk_destroy_sock(sk);
}
EXPORT_SYMBOL_GPL(gmtp_done);

const char *gmtp_state_name(const int state)
{
	static const char *const gmtp_state_names[] = {
	[GMTP_OPEN]		= "OPEN",
	[GMTP_REQUESTING]	= "REQUESTING",
	[GMTP_LISTEN]		= "LISTEN",
	[GMTP_REQUEST_RECV]		= "REQUEST/REGISTER_RECEIVED",
	[GMTP_ACTIVE_CLOSEREQ]	= "CLOSEREQ",
	[GMTP_PASSIVE_CLOSE]	= "PASSIVE_CLOSE",
	[GMTP_CLOSING]		= "CLOSING",
	[GMTP_TIME_WAIT]	= "TIME_WAIT",
	[GMTP_CLOSED]		= "CLOSED",
	};

	if (state >= GMTP_PKT_INVALID)
		return "INVALID STATE!";
	else
		return gmtp_state_names[state];
}
EXPORT_SYMBOL_GPL(gmtp_state_name);

/**
 * @str size MUST HAVE len >= GMTP_FLOWNAME_STR_LEN
 */
void flowname_str(__u8* str, const __u8 *flowname)
{
	int i;
	for(i = 0; i < GMTP_FLOWNAME_LEN; ++i)
		sprintf(&str[i*2], "%02x", flowname[i]);
}
EXPORT_SYMBOL_GPL(flowname_str);

/**
 * @str size MUST HAVE len >= GMTP_FLOWNAME_STR_LEN
 */
void relayid_str(__u8* str, const __u8 *relayid)
{
	flowname_str(str, relayid);
}

void print_route(struct gmtp_hdr_route *route)
{
	int i;
	unsigned char relayid[GMTP_RELAY_ID_LEN];
	const struct gmtp_relay *gr;

	if(route->nrelays <= 0)
		return;

	gr = &route->relay_list[route->nrelays-1];
	relayid_str(relayid, gr->relay_id);

	for(i=0; i < route->nrelays; ++i)
		pr_info("Route[%d]: %s :: %pI4\n", i, relayid, &gr->relay_ip);
}
EXPORT_SYMBOL_GPL(print_route);

void gmtp_set_state(struct sock *sk, const int state)
{
	const int oldstate = sk->sk_state;

	gmtp_print_function();
	gmtp_print_debug("%s --> %s\n", gmtp_state_name(oldstate),
			gmtp_state_name(state));

	if(state == oldstate)
		gmtp_print_warning("new state == old state!");

	switch(state) {
	case GMTP_CLOSED:
		/* TODO Implement protocol stats
		if(oldstate == GMTP_OPEN || oldstate == GMTP_CLOSING)
			DCCP_INC_STATS(DCCP_MIB_ESTABRESETS); */

		sk->sk_prot->unhash(sk);
		if(inet_csk(sk)->icsk_bind_hash != NULL
				&& !(sk->sk_userlocks & SOCK_BINDPORT_LOCK))
			inet_put_port(sk);
		/* fall through */
	}

	/* Change state AFTER socket is unhashed to avoid closed
	 * socket sitting in hash tables.
	 */
	sk->sk_state = state;
}
EXPORT_SYMBOL_GPL(gmtp_set_state);

static void gmtp_finish_passive_close(struct sock *sk)
{
	gmtp_print_function();
	if(sk->sk_state == GMTP_PASSIVE_CLOSE) {
		/* Node (client or server) has received Close packet. */
		gmtp_send_reset(sk, GMTP_RESET_CODE_CLOSED);
		gmtp_set_state(sk, GMTP_CLOSED);
	}
}

int gmtp_init_sock(struct sock *sk)
{
	struct gmtp_sock *gp = gmtp_sk(sk);
	struct inet_connection_sock *icsk = inet_csk(sk);
	int ret = 0;

	gmtp_print_function();

	gmtp_init_xmit_timers(sk);
	icsk->icsk_rto		= GMTP_TIMEOUT_INIT;
	icsk->icsk_syn_retries	= TCP_SYN_RETRIES;
	sk->sk_state		= GMTP_CLOSED;
	sk->sk_write_space	= gmtp_write_space;
	icsk->icsk_sync_mss	= gmtp_sync_mss;

	gp->mss			= GMTP_DEFAULT_MSS;
	gp->role		= GMTP_ROLE_UNDEFINED;

	gp->req_stamp		= 0;
	gp->ack_rcv_tstamp	= 0;
	gp->tx_rtt		= GMTP_DEFAULT_RTT;
	gp->relay_rtt		= 0;

	gp->rx_max_rate 	= 0;

	gp->tx_dpkts_sent 	= 0;
	gp->tx_data_sent 	= 0;
	gp->tx_bytes_sent 	= 0;

	gp->tx_sample_len 	= GMTP_DEFAULT_SAMPLE_LEN;
	gp->tx_time_sample 	= 0;
	gp->tx_byte_sample 	= 0;

	gp->tx_sample_rate 	= 0;
	gp->tx_total_rate 	= 0;

	gp->tx_first_stamp	= 0UL;
	gp->tx_last_stamp	= 0UL;
	gp->tx_max_rate		= 0UL; /* Unlimited */
	gp->tx_byte_budget	= INT_MIN;
	gp->tx_adj_budget	= 0;

	memset(gp->flowname, 0, GMTP_FLOWNAME_LEN);

	return ret;
}
EXPORT_SYMBOL_GPL(gmtp_init_sock);

void gmtp_destroy_sock(struct sock *sk)
{
	gmtp_pr_func();

	if(gmtp_sk(sk)->role == GMTP_ROLE_REPORTER)
		mcc_rx_exit(sk);

	if (sk->sk_send_head != NULL) {
		kfree_skb(sk->sk_send_head);
		sk->sk_send_head = NULL;
	}

	/* Clean up a referenced GMTP bind bucket. */
	if (inet_csk(sk)->icsk_bind_hash != NULL)
		inet_put_port(sk);

}
EXPORT_SYMBOL_GPL(gmtp_destroy_sock);

static void gmtp_terminate_connection(struct sock *sk)
{
	u8 next_state = GMTP_CLOSED;

	gmtp_print_function();

	switch (sk->sk_state) {
	case GMTP_PASSIVE_CLOSE:
		gmtp_finish_passive_close(sk);
		break;
	case GMTP_OPEN:
		gmtp_send_close(sk, 1);

		if (gmtp_sk(sk)->role == GMTP_ROLE_SERVER &&
		    !gmtp_sk(sk)->server_timewait)
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
	/*struct gmtp_sock *gp = gmtp_sk(sk);*/
	struct sk_buff *skb;
	u32 data_was_unread = 0;
	int state;

	gmtp_pr_func();
	gmtp_pr_info("Closing connection...");
	gmtp_pr_info("Timeout: %ld", timeout);
	gmtp_pr_info("sk->st_state: %s", gmtp_state_name(sk->sk_state));

	lock_sock(sk);

	sk->sk_shutdown = SHUTDOWN_MASK;
	if(sk->sk_state == GMTP_LISTEN) {
		gmtp_set_state(sk, GMTP_CLOSED);

		/* Special case. */
		inet_csk_listen_stop(sk);

		goto adjudge_to_death;
	}

	/*
	 * We need to flush the recv. buffs.  We do this only on the
	 * descriptor close, not protocol-sourced closes, because the
	 * reader process may not have drained the data yet!
	 */
	while((skb = __skb_dequeue(&sk->sk_receive_queue)) != NULL) {
		data_was_unread += skb->len;
		__kfree_skb(skb);
	}

	if(data_was_unread) {
		/* Unread data was tossed, send an appropriate Reset Code */
		gmtp_print_warning("ABORT with %u bytes unread\n",
				data_was_unread);
		gmtp_send_reset(sk, GMTP_RESET_CODE_ABORTED);
		gmtp_set_state(sk, GMTP_CLOSED);
	} else if(sock_flag(sk, SOCK_LINGER) && !sk->sk_lingertime) {
		/* Check zero linger _after_ checking for unread data. */
		sk->sk_prot->disconnect(sk, 0);
	} else if(sk->sk_state != GMTP_CLOSED) {
		/*
		 * TODO Normal connection termination.
		 * May need to wait if there are still packets in the
		 * TX queue that are delayed by the CCID.
		 */
		gmtp_print_debug("TODO: Normal connection termination.");
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
	if(state != GMTP_CLOSED && sk->sk_state == GMTP_CLOSED) {
		gmtp_print_debug("we already been destroyed by a "
				"softirq or backlog");
		goto out;
	}

	if(sk->sk_state == GMTP_CLOSED)
		inet_csk_destroy_sock(sk);

	/* Otherwise, socket is reprieved until protocol close. */
out:
	bh_unlock_sock(sk);
	local_bh_enable();
	sock_put(sk);
}
EXPORT_SYMBOL_GPL(gmtp_close);

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

	gmtp_print_function();

	if(old_state != GMTP_CLOSED)
		gmtp_set_state(sk, GMTP_CLOSED);


	/* This corresponds to the ABORT function of RFC793, sec. 3.8
	 * TCP uses a RST segment, DCCP a Reset packet with Code 2, "Aborted".
	 */
	if(old_state == GMTP_LISTEN) {
		inet_csk_listen_stop(sk);
	} else if(gmtp_need_reset(old_state)) {
		gmtp_send_reset(sk, GMTP_RESET_CODE_ABORTED);
		sk->sk_err = ECONNRESET;
	} else if(old_state == GMTP_REQUESTING)
		sk->sk_err = ECONNRESET;

	gmtp_clear_xmit_timers(sk);

	__skb_queue_purge(&sk->sk_receive_queue);
	__skb_queue_purge(&sk->sk_write_queue);
	if(sk->sk_send_head != NULL) {
		__kfree_skb(sk->sk_send_head);
		sk->sk_send_head = NULL;
	}

	inet->inet_dport = 0;

	if(!(sk->sk_userlocks & SOCK_BINDADDR_LOCK))
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

int gmtp_ioctl(struct sock *sk, int cmd, unsigned long arg)
{
	int rc = -ENOTCONN;

	gmtp_print_function();
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

int gmtp_recvmsg(struct kiocb *iocb, struct sock *sk, struct msghdr *msg,
		 size_t len, int nonblock, int flags, int *addr_len)
{
	const struct gmtp_hdr *gh;
	long timeo;

	lock_sock(sk);

	if(sk->sk_state == GMTP_LISTEN) {
		len = -ENOTCONN;
		goto out;
	}

	timeo = sock_rcvtimeo(sk, nonblock);

	do {
		struct sk_buff *skb = skb_peek(&sk->sk_receive_queue);
		if(skb == NULL)
			goto verify_sock_status;

		gh = gmtp_hdr(skb);

		switch(gh->type) {
		case GMTP_PKT_DATA:
		case GMTP_PKT_DATAACK:
			goto found_ok_skb;
		case GMTP_PKT_CLOSE:
			if(!(flags & MSG_PEEK))
				gmtp_finish_passive_close(sk);
			/* fall through */
		case GMTP_PKT_RESET:
			gmtp_print_debug("found fin (%s) ok!\n",
					gmtp_packet_name(gh->type));
			len = 0;
			goto found_fin_ok;
		default:
			gmtp_print_debug("packet_type=%s\n",
					gmtp_packet_name(gh->type));
			sk_eat_skb(sk, skb);
		}
verify_sock_status:
		if(sock_flag(sk, SOCK_DONE)) {
			len = 0;
			break;
		}

		if(sk->sk_err) {
			len = sock_error(sk);
			break;
		}

		if(sk->sk_shutdown & RCV_SHUTDOWN) {
			len = 0;
			break;
		}

		if(sk->sk_state == GMTP_CLOSED) {
			if(!sock_flag(sk, SOCK_DONE)) {
				/* This occurs when user tries to read
				 * from never connected socket.
				 */
				len = -ENOTCONN;
				break;
			}
			len = 0;
			break;
		}

		if(!timeo) {
			len = -EAGAIN;
			break;
		}

		if(signal_pending(current)) {
			len = sock_intr_errno(timeo);
			break;
		}

		sk_wait_data(sk, &timeo);
		continue;
found_ok_skb:
		if(len > skb->len)
			len = skb->len;
		else if(len < skb->len)
			msg->msg_flags |= MSG_TRUNC;

		if(skb_copy_datagram_msg(skb, 0, msg, len)) {
			/* Exception. Bailout! */
			len = -EFAULT;
			break;
		}
		if(flags & MSG_TRUNC)
			len = skb->len;
found_fin_ok:
		if(!(flags & MSG_PEEK))
			sk_eat_skb(sk, skb);
		break;
	} while(1);
out:
	release_sock(sk);
	return len;
}

EXPORT_SYMBOL_GPL(gmtp_recvmsg);

int gmtp_sendmsg(struct kiocb *iocb, struct sock *sk, struct msghdr *msg,
		size_t len)
{
	const struct gmtp_sock *gp = gmtp_sk(sk);
	const int flags = msg->msg_flags;
	const int noblock = flags & MSG_DONTWAIT;
	struct sk_buff *skb;
	int rc, size;
	long timeo;

	if (len > gp->mss)
		return -EMSGSIZE;

	lock_sock(sk);

	timeo = sock_sndtimeo(sk, noblock);

	/*
	 * We have to use sk_stream_wait_connect here to set sk_write_pending,
	 * so that the trick in gmtp_rcv_request_sent_state_process.
	 */
	/* Wait for a connection to finish. */
	if ((1 << sk->sk_state) & ~(GMTPF_OPEN | GMTPF_PASSIVE_CLOSE))
		if ((rc = sk_stream_wait_connect(sk, &timeo)) != 0)
			goto out_release;

	size = sk->sk_prot->max_header + len;
	release_sock(sk);

	skb = sock_alloc_send_skb(sk, size, noblock, &rc);

	lock_sock(sk);
	if (skb == NULL)
		goto out_release;

	skb_reserve(skb, sk->sk_prot->max_header);
	rc = memcpy_from_msg(skb_put(skb, len), msg, len);
	if (rc != 0)
		goto out_discard;

	/**
	 * FIXME Use a timer to rate-based congestion control protocols.
	 * The timer will expire when congestion control permits to release
	 * further packets into the network
	 */
	/*if (!timer_pending(&gp->xmit_timer))*/
	gmtp_write_xmit(sk, skb);

out_release:
	release_sock(sk);
	return rc ? : len;
out_discard:
	kfree_skb(skb);
	goto out_release;

}
EXPORT_SYMBOL_GPL(gmtp_sendmsg);

int inet_gmtp_listen(struct socket *sock, int backlog)
{
	struct sock *sk = sock->sk;
	struct gmtp_sock *gs = gmtp_sk(sk);
	unsigned char old_state;
	int err;

	gmtp_print_function();

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

		gs->role = GMTP_ROLE_LISTEN;

		err = inet_csk_listen_start(sk, backlog);
		gmtp_print_debug("inet_csk_listen_start(sk, %d) "
				"returns: %d", backlog, err);
		if (err)
			goto out;
	}
	err = 0;
out:
	release_sock(sk);
	return err;
}
EXPORT_SYMBOL_GPL(inet_gmtp_listen);

void gmtp_shutdown(struct sock *sk, int how)
{
	gmtp_print_function();
	gmtp_print_debug("called shutdown(%x)", how);
}
EXPORT_SYMBOL_GPL(gmtp_shutdown);

/* TODO Study thash_entries... This is from DCCP thash_entries */
static int thash_entries;
module_param(thash_entries, int, 0444);
MODULE_PARM_DESC(thash_entries, "Number of ehash buckets");

/**
 * Unfortunately, we can't use the alloc_large_system_hash method...
 * So, this method is an adaptation from dccp hashinfo initialization
 */
static int gmtp_create_inet_hashinfo(void)
{
	unsigned long goal;
	int ehash_order, bhash_order, i;
	int rc;

	gmtp_print_function();

	BUILD_BUG_ON(sizeof(struct gmtp_skb_cb) >
	FIELD_SIZEOF(struct sk_buff, cb));

	rc = -ENOBUFS;

	inet_hashinfo_init(&gmtp_inet_hashinfo);
	gmtp_inet_hashinfo.bind_bucket_cachep =
			kmem_cache_create("gmtp_bind_bucket",
					sizeof(struct inet_bind_bucket), 0,
					SLAB_HWCACHE_ALIGN, NULL);
	if (!gmtp_inet_hashinfo.bind_bucket_cachep)
		goto out_fail;

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
		gmtp_inet_hashinfo.ehash_mask = hash_size - 1;
		gmtp_inet_hashinfo.ehash = (struct inet_ehash_bucket *)
				__get_free_pages(GFP_ATOMIC|__GFP_NOWARN,
						ehash_order);

	} while (!gmtp_inet_hashinfo.ehash && --ehash_order > 0);

	if (!gmtp_inet_hashinfo.ehash) {
		gmtp_print_error("Failed to allocate GMTP established "
				"hash table");
		goto out_free_bind_bucket_cachep;
	}

	for (i = 0; i <= gmtp_inet_hashinfo.ehash_mask; i++)
		INIT_HLIST_NULLS_HEAD(&gmtp_inet_hashinfo.ehash[i].chain, i);

	if (inet_ehash_locks_alloc(&gmtp_inet_hashinfo))
		goto out_free_gmtp_ehash;

	bhash_order = ehash_order;

	do {
		gmtp_inet_hashinfo.bhash_size = (1UL << bhash_order) * PAGE_SIZE /
				sizeof(struct inet_bind_hashbucket);
		if ((gmtp_inet_hashinfo.bhash_size > (64 * 1024)) &&
				bhash_order > 0)
			continue;
		gmtp_inet_hashinfo.bhash = (struct inet_bind_hashbucket *)
				__get_free_pages(GFP_ATOMIC|__GFP_NOWARN,
						bhash_order);

	} while (!gmtp_inet_hashinfo.bhash && --bhash_order >= 0);

	if (!gmtp_inet_hashinfo.bhash) {
		gmtp_print_error("Failed to allocate GMTP bind hash table");
		goto out_free_gmtp_locks;
	}

	for (i = 0; i < gmtp_inet_hashinfo.bhash_size; i++) {
		spin_lock_init(&gmtp_inet_hashinfo.bhash[i].lock);
		INIT_HLIST_HEAD(&gmtp_inet_hashinfo.bhash[i].chain);
	}

	return 0;

out_free_gmtp_locks:
	inet_ehash_locks_free(&gmtp_inet_hashinfo);
out_free_gmtp_ehash:
	free_pages((unsigned long)gmtp_inet_hashinfo.ehash, ehash_order);
out_free_bind_bucket_cachep:
	kmem_cache_destroy(gmtp_inet_hashinfo.bind_bucket_cachep);
out_fail:
	gmtp_print_error("gmtp_init_hashinfo: FAIL");
	gmtp_inet_hashinfo.bhash = NULL;
	gmtp_inet_hashinfo.ehash = NULL;
	gmtp_inet_hashinfo.bind_bucket_cachep = NULL;

	return rc;
}

static int ghash_entries = 256;
module_param(ghash_entries, int, 0444);
MODULE_PARM_DESC(ghash_entries, "Number of GMTP hash entries");

/*************************************************/
static int __init gmtp_init(void)
{
	int rc = 0;
	gmtp_print_debug("GMTP init!");
	gmtp_print_function();

	rc = mcc_lib_init();
	if(rc)
		goto out;

	rc = percpu_counter_init(&gmtp_orphan_count, 0, GFP_KERNEL);
	if(rc) {
		percpu_counter_destroy(&gmtp_orphan_count);
		goto out;
	}

	gmtp_hashtable = gmtp_create_hashtable(ghash_entries);
	if(gmtp_hashtable == NULL) {
		rc = -ENOBUFS;
		goto out;
	}

	gmtp_info = kmalloc(sizeof(struct gmtp_info), GFP_KERNEL);
	if(gmtp_info == NULL) {
		rc = -ENOBUFS;
		goto out;
	}
	gmtp_info->relay_enabled = 0;

	rc = gmtp_create_inet_hashinfo();
	if(rc)
		goto out;

out:
	return rc;
}

static void __exit gmtp_exit(void)
{
	gmtp_print_function();
	gmtp_print_debug("GMTP exit!");

	free_pages((unsigned long)gmtp_inet_hashinfo.bhash,
			get_order(gmtp_inet_hashinfo.bhash_size *
					sizeof(struct inet_bind_hashbucket)));
	free_pages((unsigned long)gmtp_inet_hashinfo.ehash,
			get_order((gmtp_inet_hashinfo.ehash_mask + 1) *
					sizeof(struct inet_ehash_bucket)));
	inet_ehash_locks_free(&gmtp_inet_hashinfo);
	kmem_cache_destroy(gmtp_inet_hashinfo.bind_bucket_cachep);

	kfree(gmtp_info);
	kfree_gmtp_hashtable(gmtp_hashtable);
	percpu_counter_destroy(&gmtp_orphan_count);
	mcc_lib_exit();
}

module_init(gmtp_init);
module_exit(gmtp_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Joilnen Leite <joilnen@gmail.com>");
MODULE_AUTHOR("Mário André Menezes <mariomenezescosta@gmail.com>");
MODULE_AUTHOR("Wendell Silva Soares <wss@ic.ufal.br>");
MODULE_DESCRIPTION("GMTP - Global Media Transmission Protocol");

