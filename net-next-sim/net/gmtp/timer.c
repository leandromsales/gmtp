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

			dst_negative_advice(sk);
		}

		retry_until = TCP_RETR2;
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
static void gmtp_register_reply_timer(struct sock *sk)
{

	/* FIXME DCE cu off syn-ack timer from TCP and register_reply_timer
	 * from us...
	 */
/*	inet_csk_reqsk_queue_prune(sk, GMTP_REQ_INTERVAL, GMTP_TIMEOUT_INIT,
				   GMTP_RTO_MAX);*/
	;
}

static void gmtp_reporter_ackrcv_timer(struct sock *sk)
{
	struct gmtp_sock *gp = gmtp_sk(sk);
	struct gmtp_client *client, *temp;

	gmtp_pr_func();

	if(gp->myself == NULL)
		return;

	pr_info("Reporter has %u clients\n", gp->myself->nclients);
	list_for_each_entry_safe(client, temp, &gp->myself->clients->list, list)
	{
		unsigned int now = jiffies_to_msecs(jiffies);
		int interval = (int) (now - client->ack_rx_tstamp);

		pr_info("Client found: %pI4@%-5d\n", &client->addr,
				ntohs(client->port));

		if(unlikely(interval > jiffies_to_msecs(GMTP_ACK_TIMEOUT))) {
			pr_info("Deleting client.\n");
			list_del(&client->list);
			kfree(client);
			gp->myself->nclients--;
		}
	}
	inet_csk_reset_keepalive_timer(sk, GMTP_ACK_TIMEOUT);
}

static void gmtp_client_sendack_timer(struct sock *sk)
{
	struct gmtp_sock *gp = gmtp_sk(sk);
	unsigned int now = jiffies_to_msecs(jiffies);
	unsigned int factor, next_ack_time = GMTP_ACK_INTERVAL;
	int r_ack_interval = 0;

	gmtp_pr_func();

	r_ack_interval = (int)(now - gp->ack_rx_tstamp);
	if(r_ack_interval > jiffies_to_msecs(GMTP_ACK_TIMEOUT)) {
		gmtp_send_elect_response(gp->myself->mysock, GMTP_ELECT_AUTO);
		return;
	}

	factor = DIV_ROUND_CLOSEST(r_ack_interval, GMTP_ACK_INTERVAL);
	if(factor > 0) {
		next_ack_time = DIV_ROUND_UP(GMTP_ACK_INTERVAL, factor);
		next_ack_time = max(next_ack_time, gp->rx_rtt);
	}

	gmtp_send_ack(sk);
	inet_csk_reset_keepalive_timer(sk, next_ack_time);
}

static void gmtp_keepalive_timer(unsigned long data)
{
	struct sock *sk = (struct sock *)data;
	struct gmtp_sock *gp = gmtp_sk(sk);

	gmtp_pr_func();

	print_gmtp_sock(sk);

	bh_lock_sock(sk);
	/* Only process if socket is not in use. */
	if (sock_owned_by_user(sk)) {
		/* Try again later. */
		inet_csk_reset_keepalive_timer(sk, HZ / 20);
		goto out;
	}

	if (sk->sk_state == GMTP_LISTEN) {
		gmtp_register_reply_timer(sk);
		goto out;
	}

	if(gp->role == GMTP_ROLE_REPORTER) {
		if(sk->sk_state == GMTP_OPEN) {
			gmtp_reporter_ackrcv_timer(sk);
			goto out;
		}
	}

	if(gp->type == GMTP_SOCK_TYPE_REPORTER) {
		unsigned int timeout = 0;
		switch(sk->sk_state) {
		case GMTP_REQUESTING:
			timeout = jiffies_to_msecs(jiffies) - gp->req_stamp;
			if(likely(timeout <= GMTP_TIMEOUT_INIT))
				gmtp_send_elect_request(sk, GMTP_REQ_INTERVAL);
			else
				gmtp_send_elect_response(gp->myself->mysock,
						GMTP_ELECT_AUTO);
			break;
		case GMTP_OPEN:
			gmtp_client_sendack_timer(sk);
			break;
		}
	}
out:
	bh_unlock_sock(sk);
	sock_put(sk);
}

/* This is the same as tcp_delack_timer, sans prequeue & mem_reclaim stuff
 * FIXME This function is probably not necessary.
 * Maybe we can keep it empty, just to avoid null pointers.
 **/
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
		gmtp_send_ack(sk);
		NET_INC_STATS_BH(sock_net(sk), LINUX_MIB_DELAYEDACKS);
	}
out:
	bh_unlock_sock(sk);
	sock_put(sk);
}

void gmtp_write_xmit_timer(unsigned long data)
{
	struct gmtp_packet_info *pkt_info = (struct gmtp_packet_info*) data;
	gmtp_pr_func();
	gmtp_write_xmit(pkt_info->sk, pkt_info->skb);
	del_timer_sync(&gmtp_sk(pkt_info->sk)->xmit_timer);
	kfree(pkt_info);
}
EXPORT_SYMBOL_GPL(gmtp_write_xmit_timer);

void gmtp_init_xmit_timers(struct sock *sk)
{
	struct gmtp_sock *gp = gmtp_sk(sk);
	gmtp_pr_func();

	inet_csk_init_xmit_timers(sk, &gmtp_write_timer, &gmtp_delack_timer,
				&gmtp_keepalive_timer);
}


