#include <linux/types.h>

#include "gmtp.h"

void gmtp_set_state(struct sock *sk, const int state)
{
//	const int oldstate = sk->sk_state;

	switch (state) {
	//TODO make treatment on set new state
	}

	/* Change state AFTER socket is unhashed to avoid closed
	 * socket sitting in hash tables.
	 */
	sk->sk_state = state;
}


//TODO Implement gmtp callbacks
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
//
//		/* Special case. */
		inet_csk_listen_stop(sk);
//
		goto adjudge_to_death;
	}

	/*
	 * We need to flush the recv. buffs.  We do this only on the
	 * descriptor close, not protocol-sourced closes, because the
	 * reader process may not have drained the data yet!
	 */
	while ((skb = __skb_dequeue(&sk->sk_receive_queue)) != NULL) {
		data_was_unread += skb->len;
		__kfree_skb(skb);
	}

	if (data_was_unread) {
		/* Unread data was tossed, send an appropriate Reset Code */
		printk(KERN_WARNING "ABORT with %u bytes unread\n", data_was_unread);
//		dccp_send_reset(sk, DCCP_RESET_CODE_ABORTED);
		gmtp_set_state(sk, GMTP_CLOSED);
	} else if (sock_flag(sk, SOCK_LINGER) && !sk->sk_lingertime) {
		/* Check zero linger _after_ checking for unread data. */
		sk->sk_prot->disconnect(sk, 0);
	} else if (sk->sk_state != GMTP_CLOSED) {
		/*
		 * Normal connection termination. May need to wait if there are
		 * still packets in the TX queue that are delayed by the CCID.
		 */
		//TODO Verify if this is necessary
//		dccp_flush_write_queue(sk, &timeout);
//		dccp_terminate_connection(sk);
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


int gmtp_connect(struct sock *sk, struct sockaddr *uaddr, int addr_len)
{
	return 0;
}
EXPORT_SYMBOL_GPL(gmtp_connect);

int gmtp_disconnect(struct sock *sk, int flags)
{
	return 0;
}
EXPORT_SYMBOL_GPL(gmtp_disconnect);

int gmtp_ioctl(struct sock *sk, int cmd, unsigned long arg)
{
	return 0;
}
EXPORT_SYMBOL_GPL(gmtp_ioctl);

int gmtp_getsockopt(struct sock *sk, int level, int optname,
		    char __user *optval, int __user *optlen)
{
	return 0;
}
EXPORT_SYMBOL_GPL(gmtp_getsockopt);

int gmtp_sendmsg(struct kiocb *iocb, struct sock *sk, struct msghdr *msg,
		 size_t size)
{
	return 0;
}
EXPORT_SYMBOL_GPL(gmtp_sendmsg);

int gmtp_setsockopt(struct sock *sk, int level, int optname,
		    char __user *optval, unsigned int optlen)
{
	return 0;
}
EXPORT_SYMBOL_GPL(gmtp_setsockopt);

int gmtp_recvmsg(struct kiocb *iocb, struct sock *sk,
		 struct msghdr *msg, size_t len, int nonblock, int flags,
		 int *addr_len)
{
	return 0;
}
EXPORT_SYMBOL_GPL(gmtp_recvmsg);

int gmtp_do_rcv(struct sock *sk, struct sk_buff *skb)
{
	return 0;
}
EXPORT_SYMBOL_GPL(gmtp_do_rcv);

void gmtp_shutdown(struct sock *sk, int how)
{
}
EXPORT_SYMBOL_GPL(gmtp_shutdown);

void gmtp_destroy_sock(struct sock *sk)
{

}
EXPORT_SYMBOL_GPL(gmtp_destroy_sock);

int inet_gmtp_listen(struct socket *sock, int backlog)
{
	return 0;
}
EXPORT_SYMBOL_GPL(inet_gmtp_listen);

