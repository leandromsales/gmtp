/*
 * timer.c
 *
 *  Created on: 04/02/2015
 *      Author: wendell
 */

#include <linux/skbuff.h>
#include <linux/export.h>

#include <net/tcp.h>

#include <linux/gmtp.h>
#include "gmtp.h"

static void gmtp_write_err(struct sock *sk)
{
	gmtp_print_function();

	sk->sk_err = sk->sk_err_soft ? : ETIMEDOUT;
	sk->sk_error_report(sk);

	gmtp_send_reset(sk, GMTP_RESET_CODE_ABORTED);
	gmtp_done(sk);
}

/* A write timeout has occurred. Process the after effects. */
static int gmtp_write_timeout(struct sock *sk)
{
	const struct inet_connection_sock *icsk = inet_csk(sk);
	int retry_until;
	gmtp_print_function();

	if (sk->sk_state == GMTP_REQUESTING) {
		if (icsk->icsk_retransmits != 0)
			dst_negative_advice(sk);
		retry_until = icsk->icsk_syn_retries ?
			    : TCP_SYN_RETRIES;
	} else {
		if (icsk->icsk_retransmits >= TCP_RETR1) {
		/* NOTE. draft-ietf-tcpimpl-pmtud-01.txt requires pmtu
		   black hole detection. :-(

		   It is place to make it. It is not made. I do not want
		   to make it. It is disguisting. It does not work in any
		   case. Let me to cite the same draft, which requires for
		   us to implement this:

   "The one security concern raised by this memo is that ICMP black holes
   are often caused by over-zealous security administrators who block
   all ICMP messages.  It is vitally important that those who design and
   deploy security systems understand the impact of strict filtering on
   upper-layer protocols.  The safest web site in the world is worthless
   if most TCP implementations cannot transfer data from it.  It would
   be far nicer to have all of the black holes fixed rather than fixing
   all of the TCP implementations."

			   Golden words :-).
		   */

			dst_negative_advice(sk);
		}

		retry_until = TCP_RETR2;
		/*
		 * FIXME: see tcp_write_timout and tcp_out_of_resources
		 */
	}

	if (icsk->icsk_retransmits >= retry_until) {
		/* Has it gone just too far? */
		gmtp_write_err(sk);
		return 1;
	}
	return 0;
}

/*
 *	The GMTP retransmit timer.
 */
static void gmtp_retransmit_timer(struct sock *sk)
{
	struct inet_connection_sock *icsk = inet_csk(sk);
	gmtp_print_function();

	/*
	 * More than than 4MSL (8 minutes) has passed, a RESET(aborted) was
	 * sent, no need to retransmit, this sock is dead.
	 */
	if (gmtp_write_timeout(sk))
		return;

	/*
	 * We want to know the number of packets retransmitted, not the
	 * total number of retransmissions of clones of original packets.
	 */
	if (icsk->icsk_retransmits == 0)
/*		DCCP_INC_STATS_BH(DCCP_MIB_TIMEOUTS);*/
		;

	if (gmtp_retransmit_skb(sk) != 0) {
		/*
		 * Retransmission failed because of local congestion,
		 * do not backoff.
		 */
		if (--icsk->icsk_retransmits == 0)
			icsk->icsk_retransmits = 1;
		inet_csk_reset_xmit_timer(sk, ICSK_TIME_RETRANS,
					  min(icsk->icsk_rto,
					      TCP_RESOURCE_PROBE_INTERVAL),
					  GMTP_RTO_MAX);
		return;
	}

	icsk->icsk_backoff++;

	icsk->icsk_rto = min(icsk->icsk_rto << 1, GMTP_RTO_MAX);
	inet_csk_reset_xmit_timer(sk, ICSK_TIME_RETRANS, icsk->icsk_rto,
				  GMTP_RTO_MAX);
	if (icsk->icsk_retransmits > TCP_RETR1)
		__sk_dst_reset(sk);
}

static void gmtp_write_timer(unsigned long data)
{
	struct sock *sk = (struct sock *)data;
	struct inet_connection_sock *icsk = inet_csk(sk);
	int event = 0;
	gmtp_print_function();

	bh_lock_sock(sk);
	if (sock_owned_by_user(sk)) {
		/* Try again later */
		sk_reset_timer(sk, &icsk->icsk_retransmit_timer,
			       jiffies + (HZ / 20));
		goto out;
	}

	if (sk->sk_state == GMTP_CLOSED || !icsk->icsk_pending)
		goto out;

	if (time_after(icsk->icsk_timeout, jiffies)) {
		sk_reset_timer(sk, &icsk->icsk_retransmit_timer,
			       icsk->icsk_timeout);
		goto out;
	}

	event = icsk->icsk_pending;
	icsk->icsk_pending = 0;

	switch (event) {
        case ICSK_TIME_RETRANS:
            gmtp_retransmit_timer(sk);
            break;
	}
out:
	bh_unlock_sock(sk);
	sock_put(sk);
}

/*
 *	Timer for listening sockets
 */
static void gmtp_response_timer(struct sock *sk)
{
    gmtp_print_function();
	inet_csk_reqsk_queue_prune(sk, TCP_SYNQ_INTERVAL, GMTP_TIMEOUT_INIT,
				   GMTP_RTO_MAX);
}

static void gmtp_keepalive_timer(unsigned long data)
{
	struct sock *sk = (struct sock *)data;

	gmtp_print_function();

	/* Only process if socket is not in use. */
	bh_lock_sock(sk);
	if (sock_owned_by_user(sk)) {
		/* Try again later. */
		inet_csk_reset_keepalive_timer(sk, HZ / 20);
		goto out;
	}

	if (sk->sk_state == GMTP_LISTEN) {
		gmtp_response_timer(sk);
		goto out;
	}
out:
	bh_unlock_sock(sk);
	sock_put(sk);
}

/* This is the same as tcp_delack_timer, sans prequeue & mem_reclaim stuff */
static void gmtp_delack_timer(unsigned long data)
{
	struct sock *sk = (struct sock *)data;
	struct inet_connection_sock *icsk = inet_csk(sk);
	gmtp_print_function();

	bh_lock_sock(sk);
	if (sock_owned_by_user(sk)) {
		/* Try again later. */
		icsk->icsk_ack.blocked = 1;
		NET_INC_STATS_BH(sock_net(sk), LINUX_MIB_DELAYEDACKLOCKED);
		sk_reset_timer(sk, &icsk->icsk_delack_timer,
			       jiffies + TCP_DELACK_MIN);
		goto out;
	}

	if (sk->sk_state == GMTP_CLOSED ||
	    !(icsk->icsk_ack.pending & ICSK_ACK_TIMER))
		goto out;
	if (time_after(icsk->icsk_ack.timeout, jiffies)) {
		sk_reset_timer(sk, &icsk->icsk_delack_timer,
			       icsk->icsk_ack.timeout);
		goto out;
	}

	icsk->icsk_ack.pending &= ~ICSK_ACK_TIMER;

	if (inet_csk_ack_scheduled(sk)) {
		if (!icsk->icsk_ack.pingpong) {
			/* Delayed ACK missed: inflate ATO. */
			icsk->icsk_ack.ato = min(icsk->icsk_ack.ato << 1,
						 icsk->icsk_rto);
		} else {
			/* Delayed ACK missed: leave pingpong mode and
			 * deflate ATO.
			 */
			icsk->icsk_ack.pingpong = 0;
			icsk->icsk_ack.ato = TCP_ATO_MIN;
		}
		gmtp_send_ack(sk, GMTP_ACK_NO_CODE);
		NET_INC_STATS_BH(sock_net(sk), LINUX_MIB_DELAYEDACKS);
	}
out:
	bh_unlock_sock(sk);
	sock_put(sk);
}

/**
 * gmtp_write_xmitlet  -  Workhorse for CCID packet dequeueing interface
 * See the comments above %ccid_dequeueing_decision for supported modes.
 */
static void gmtp_write_xmitlet(unsigned long data)
{
	struct sock *sk = (struct sock *)data;

	gmtp_print_function();

	bh_lock_sock(sk);
	if(sock_owned_by_user(sk))
		sk_reset_timer(sk, &gmtp_sk(sk)->xmit_timer, jiffies + 1);
	/*
	 * FIXME Treat it
	 else
	 gmtp_write_xmit(sk);
	 */
	bh_unlock_sock(sk);
}

static void gmtp_write_xmit_timer(unsigned long data)
{
	gmtp_print_function();
	gmtp_write_xmitlet(data);
	sock_put((struct sock *)data);
}

void gmtp_init_xmit_timers(struct sock *sk)
{
	struct gmtp_sock *gp = gmtp_sk(sk);

	gmtp_print_function();

	tasklet_init(&gp->xmitlet, gmtp_write_xmitlet, (unsigned long)sk);
	setup_timer(&gp->xmit_timer, gmtp_write_xmit_timer, (unsigned long )sk);
	inet_csk_init_xmit_timers(sk, &gmtp_write_timer, &gmtp_delack_timer,
			&gmtp_keepalive_timer);
}


