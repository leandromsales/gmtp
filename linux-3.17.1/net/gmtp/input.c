#include <linux/kernel.h>
#include <linux/skbuff.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/gmtp.h>
#include <linux/net.h>
#include <linux/security.h>
#include <linux/igmp.h>

#include <net/inet_common.h>
#include <net/inet_sock.h>
#include <net/sock.h>

#include <uapi/linux/gmtp.h>
#include "gmtp.h"
#include "mcc.h"

static void gmtp_enqueue_skb(struct sock *sk, struct sk_buff *skb)
{
	gmtp_print_function();
	__skb_pull(skb, gmtp_hdr(skb)->hdrlen);
	__skb_queue_tail(&sk->sk_receive_queue, skb);
	skb_set_owner_r(skb, sk);
	sk->sk_data_ready(sk);
}

static void gmtp_fin(struct sock *sk, struct sk_buff *skb)
{
	gmtp_print_function();
	sk->sk_shutdown = SHUTDOWN_MASK;
	sock_set_flag(sk, SOCK_DONE);
	gmtp_enqueue_skb(sk, skb);
}

static int gmtp_rcv_close(struct sock *sk, struct sk_buff *skb)
{
	struct gmtp_sock *gp = gmtp_sk(sk);
	int queued = 0;

	gmtp_print_function();

	switch(sk->sk_state) {
	/*
	 * We ignore Close when received in one of the following states:
	 *  - CLOSED		(may be a late or duplicate packet)
	 *  - PASSIVE_CLOSEREQ	(the peer has sent a CloseReq earlier)
	 *  - RESPOND		(already handled by dccp_check_req)
	 */
	case GMTP_CLOSING:
		/*
		 * Simultaneous-close: receiving a Close after sending one. This
		 * can happen if both client and server perform active-close and
		 * will result in an endless ping-pong of crossing and retrans-
		 * mitted Close packets, which only terminates when one of the
		 * nodes times out (min. 64 seconds). Quicker convergence can be
		 * achieved when one of the nodes acts as tie-breaker.
		 * This is ok as both ends are done with data transfer and each
		 * end is just waiting for the other to acknowledge termination.
		 */
		if(gp->role != GMTP_ROLE_CLIENT)
			break;
		/* fall through */
	case GMTP_REQUESTING:
	case GMTP_ACTIVE_CLOSEREQ:
		gmtp_send_reset(sk, GMTP_RESET_CODE_CLOSED);
		gmtp_done(sk);
		break;
	case GMTP_OPEN:
		/* Clear hash table */
		/* FIXME: Implement gmtp_del_server_entry() */
		/*if(gp->role == GMTP_ROLE_CLIENT)
			gmtp_del_client_entry(gmtp_hashtable, gp->flowname);
		else if(gp->role == GMTP_ROLE_SERVER)
			gmtp_print_error("FIXME: "
					"Implement gmtp_del_server_entry()");*/

		/* Give waiting application a chance to read pending data */
		queued = 1;
		gmtp_fin(sk, skb);
		gmtp_set_state(sk, GMTP_PASSIVE_CLOSE);
		/* fall through */
	case GMTP_PASSIVE_CLOSE:
		/*
		 * Retransmitted Close: we have already enqueued the first one.
		 */
		sk_wake_async(sk, SOCK_WAKE_WAITD, POLL_HUP);
	}
	return queued;
}

static int gmtp_rcv_request_sent_state_process(struct sock *sk,
					       struct sk_buff *skb,
					       const struct gmtp_hdr *gh,
					       const unsigned int len)
{
	gmtp_print_function();
	gmtp_print_debug("Packet received: %s", gmtp_packet_name(gh->type));

	/*
	 *  Step 4: Prepare sequence numbers in REQUEST
	 *     If S.state == REQUEST,
	 *	  If (P.type == REGISTER_REPLY or P.type == Reset)
	 *		and S.AWL <= P.ackno <= S.AWH,
	 *	     / * Set sequence number variables corresponding to the
	 *		other endpoint, so P will pass the tests in Step 6 * /
	 *	     Set S.GSR, S.ISR, S.SWL, S.SWH
	 *	     / * Response processing continues in Step 10; Reset
	 *		processing continues in Step 9 * /
	 *
	 * FIXME Clients must receive only GMTP_PKT_REQUESTNOTIFY
	*/
	if (gh->type == GMTP_PKT_REQUESTNOTIFY ||
			gh->type == GMTP_PKT_REGISTER_REPLY /* Accept it temporarily */) {

		const struct inet_connection_sock *icsk = inet_csk(sk);
		struct gmtp_sock *gp = gmtp_sk(sk);

		/*** FIXME Check sequence numbers  ***/

		/* Stop the REQUEST timer */
		inet_csk_clear_xmit_timer(sk, ICSK_TIME_RETRANS);
		WARN_ON(sk->sk_send_head == NULL);
		kfree_skb(sk->sk_send_head);
		sk->sk_send_head = NULL;

		gp->gsr = gp->isr = GMTP_SKB_CB(skb)->seq;
		gmtp_sync_mss(sk, icsk->icsk_pmtu_cookie);

		/** First reply received and i have a relay */
		if(gp->relay_rtt == 0 && gh->type == GMTP_PKT_REQUESTNOTIFY)
			gp->relay_rtt = jiffies_to_msecs(jiffies - gp->req_stamp);

		gp->rx_rtt = (__u32) gh->server_rtt + gp->relay_rtt;
		gmtp_pr_debug("Relay RTT: %u ms", gp->relay_rtt);

		/*
		 *    Step 10: Process REQUEST state (second part)
		 *       If S.state == REQUESTING,
		 *	  If we get here, P is a valid Response from the
		 *	      relay, and we should move to
		 *	      OPEN state.
		 *
		 *	 Connect to multicast channel
		 *
		 *	  S.state := OPEN
		 *	  / * Step 12 will send the Ack completing the
		 *	      three-way handshake * /
		 */
		if (gh->type == GMTP_PKT_REQUESTNOTIFY)
		{
			struct sock *newsk;
			struct gmtp_hdr_reqnotify *gh_rnotify;
			struct ip_mreqn mreq;
			struct gmtp_client_entry *media_entry;

			gh_rnotify = gmtp_hdr_reqnotify(skb);
			gmtp_print_debug("Processing RequestNotify packet...");
			gmtp_print_debug("RequestNotify => Channel: %pI4@%-5d "
					"| Error: %d",
					&gh_rnotify->mcst_addr,
					ntohs(gh_rnotify->mcst_port),
					gh_rnotify->error_code);

			/* Obtain usec RTT sample from Request (used by CC). */
			switch(gh_rnotify->error_code) {
			case GMTP_REQNOTIFY_CODE_OK: /* Process packet */
				break;
			case GMTP_REQNOTIFY_CODE_WAIT:  /* Do nothing... */
				return 0;
			/* FIXME Del entry in table when receiving error... */
			case GMTP_REQNOTIFY_CODE_ERROR:
				goto err;
			default:
				goto out_invalid_packet;
			}

			media_entry = gmtp_lookup_media(gmtp_hashtable,
					gh->flowname);
			if(media_entry == NULL)
				goto out_invalid_packet;

			/* Inserting information in client table */
			media_entry->channel_addr = gh_rnotify->mcst_addr;
			media_entry->channel_port = gh_rnotify->mcst_port;

			newsk = sk_clone_lock(sk, GFP_ATOMIC);
			if(newsk == NULL)
				goto err;

			/* FIXME Validate received multicast channel */
			newsk->sk_reuse = true;  /* SO_REUSEADDR */
			newsk->sk_reuseport = true;
			newsk->sk_rcv_saddr =  htonl(INADDR_ANY);

			mreq.imr_multiaddr.s_addr = gh_rnotify->mcst_addr;
			mreq.imr_address.s_addr = htonl(INADDR_ANY);
			/* FIXME Interface index must be filled */
			mreq.imr_ifindex = 0;

			gmtp_print_debug("Joining the multicast group...");
			ip_mc_join_group(newsk, &mreq);
			__inet_hash_nolisten(newsk, NULL);
			gmtp_set_state(newsk, GMTP_OPEN);
		}

		gmtp_set_state(sk, GMTP_OPEN);

		/* Make sure socket is routed, for correct metrics. */
		icsk->icsk_af_ops->rebuild_header(sk);

		if (!sock_flag(sk, SOCK_DEAD)) {
			sk->sk_state_change(sk);
			sk_wake_async(sk, SOCK_WAKE_IO, POLL_OUT);
		}

		if (sk->sk_write_pending || icsk->icsk_ack.pingpong ||
		    icsk->icsk_accept_queue.rskq_defer_accept) {
			/* Save one ACK. Data will be ready after
			 * several ticks, if write_pending is set.
			 */
			__kfree_skb(skb);
			return 0;
		}
		gmtp_send_ack(sk, GMTP_ACK_REQUESTNOTIFY);
		return -1;
 	}

	/* FIXME Treat invalid responses */
out_invalid_packet:
 	/* gmtp_v4_do_rcv will send a reset */
 	GMTP_SKB_CB(skb)->reset_code = GMTP_RESET_CODE_PACKET_ERROR;
 	return 1;

err:
 	/*
 	 * We mark this socket as no longer usable, so that the loop in
 	 * gmtp_sendmsg() terminates and the application gets notified.
 	 */
 	gmtp_set_state(sk, GMTP_CLOSED);
 	sk->sk_err = ECOMM;
 	return 1;
}

/**** FIXME Make GMTP reset codes */
static u16 gmtp_reset_code_convert(const u8 code)
{
	const u16 error_code[] = {
	[GMTP_RESET_CODE_CLOSED]	     = 0,	/* normal termination */
	[GMTP_RESET_CODE_UNSPECIFIED]	     = 0,	/* nothing known */
	[GMTP_RESET_CODE_ABORTED]	     = ECONNRESET,

	[GMTP_RESET_CODE_NO_CONNECTION]	     = ECONNREFUSED,
	[GMTP_RESET_CODE_CONNECTION_REFUSED] = ECONNREFUSED,
	[GMTP_RESET_CODE_TOO_BUSY]	     = EUSERS,
	[GMTP_RESET_CODE_AGGRESSION_PENALTY] = EDQUOT,

	[GMTP_RESET_CODE_PACKET_ERROR]	     = ENOMSG,
	[GMTP_RESET_CODE_MANDATORY_ERROR]    = EOPNOTSUPP,

	[GMTP_RESET_CODE_BAD_FLOWNAME]       = EBADR, /* Invalid request
								descriptor */
	};

	return code >= GMTP_MAX_RESET_CODES ? 0 : error_code[code];
}

static void gmtp_rcv_reset(struct sock *sk, struct sk_buff *skb)
{
	u16 err = gmtp_reset_code_convert(gmtp_hdr_reset(skb)->reset_code);

	gmtp_print_function();

	sk->sk_err = err;

	/*Queue the equivalent of TCP fin so that gmtp_recvmsg exits the loop */
	gmtp_fin(sk, skb);

	if (err && !sock_flag(sk, SOCK_DEAD))
		sk_wake_async(sk, SOCK_WAKE_IO, POLL_ERR);
	gmtp_time_wait(sk, GMTP_TIME_WAIT, 0);
}

static void gmtp_deliver_input_to_mcc(struct sock *sk, struct sk_buff *skb)
{
	/*const struct gmtp_sock *gp = gmtp_sk(sk);*/

	gmtp_print_function();

	/* Don't deliver to RX MCC when node has shut down read end. */
	if (!(sk->sk_shutdown & RCV_SHUTDOWN))
		mcc_rx_packet_recv(sk, skb);
	/*
	 * Until the TX queue has been drained, we can not honour SHUT_WR, since
	 * we need received feedback as input to adjust congestion control.
	 */
/*	if (sk->sk_write_queue.qlen > 0 || !(sk->sk_shutdown & SEND_SHUTDOWN))
		mcc_tx_packet_recv(sk, skb);*/
}


/* TODO Implement check sequence number */
static int gmtp_check_seqno(struct sock *sk, struct sk_buff *skb)
{
	gmtp_print_function();
	return 0;
}

static int __gmtp_rcv_established(struct sock *sk, struct sk_buff *skb,
		const struct gmtp_hdr *gh, const unsigned int len)
{
	struct gmtp_sock *gp = gmtp_sk(sk);

	gmtp_print_function();

	switch (gh->type) {
	case GMTP_PKT_DATAACK:
	case GMTP_PKT_DATA:
		if(gmtp_role_client(sk))
			gp->rx_rtt = (__u32) gh->server_rtt + gp->relay_rtt;
		gmtp_enqueue_skb(sk, skb);
		return 0;
	case GMTP_PKT_ACK:
		goto discard;
	case GMTP_PKT_RESET:
		/*
		 *  Step 9: Process Reset
		 *	If P.type == Reset,
		 *		Tear down connection
		 *		S.state := TIMEWAIT
		 *		Set TIMEWAIT timer
		 *		Drop packet and return
		 */
		gmtp_rcv_reset(sk, skb);
		return 0;
	case GMTP_PKT_CLOSE:
		if (gmtp_rcv_close(sk, skb))
			return 0;
		goto discard;
	}
discard:
	__kfree_skb(skb);
	return 0;
}

int gmtp_rcv_established(struct sock *sk, struct sk_buff *skb,
			 const struct gmtp_hdr *gh, const unsigned int len)
{
	struct gmtp_sock *gp = gmtp_sk(sk);

	gmtp_print_function();

	/* Check sequence numbers... */
	if(gmtp_check_seqno(sk, skb))
		goto discard;

	gmtp_deliver_input_to_mcc(sk, skb);
	gp->gsr = gh->seq;

	return __gmtp_rcv_established(sk, skb, gh, len);
discard:
	__kfree_skb(skb);
	return 0;
}
EXPORT_SYMBOL_GPL(gmtp_rcv_established);

int gmtp_rcv_route_notify(struct sock *sk, struct sk_buff *skb,
			 const struct gmtp_hdr *gh)
{
	struct gmtp_hdr_route *route = gmtp_hdr_route(skb);
	struct gmtp_relay *relay;
	__u8 nrelays = route->nrelays;

	gmtp_print_function();
	print_route(route);

	if(nrelays <= 0)
		goto out;

	relay = &route->relay_list[nrelays-1];

	gmtp_add_server_entry(gmtp_hashtable, relay->relay_id,
			(__u8*)gh->flowname, route);
out:
	return 0;
}
EXPORT_SYMBOL_GPL(gmtp_rcv_route_notify);

static int gmtp_rcv_request_rcv_state_process(struct sock *sk,
						   struct sk_buff *skb,
						   const struct gmtp_hdr *gh,
						   const unsigned int len)
{
	struct inet_connection_sock *icsk = inet_csk(sk);
	__u32 elapsed = 0;
	int queued = 0;

	gmtp_print_function();

	switch (gh->type) {
	case GMTP_PKT_RESET:
		inet_csk_clear_xmit_timer(sk, ICSK_TIME_DACK);
		break;
	case GMTP_PKT_DATA:
		if (sk->sk_state == GMTP_REQUEST_RECV)
			break;
	/* ROUTE_NOTIFY is a special ack */
	case GMTP_PKT_ROUTE_NOTIFY:
	case GMTP_PKT_DATAACK:
	case GMTP_PKT_ACK:
		elapsed = jiffies_to_msecs(jiffies) - gmtp_sk(sk)->reply_stamp;
		gmtp_sk(sk)->tx_rtt = elapsed;
		gmtp_print_debug("RTT: %u ms", gmtp_sk(sk)->tx_rtt);

		inet_csk_clear_xmit_timer(sk, ICSK_TIME_DACK);

		icsk->icsk_af_ops->rebuild_header(sk);
		smp_mb();
		gmtp_set_state(sk, GMTP_OPEN);
		sk->sk_state_change(sk);
		sk_wake_async(sk, SOCK_WAKE_IO, POLL_OUT);

		if(gh->type == GMTP_PKT_ROUTE_NOTIFY)
			gmtp_rcv_route_notify(sk, skb, gh);

		if (gh->type == GMTP_PKT_DATAACK)
		{
			__gmtp_rcv_established(sk, skb, gh, len);
			queued = 1;
		}

		break;
	}
	return queued;
}
EXPORT_SYMBOL_GPL(gmtp_rcv_request_rcv_state_process);

int gmtp_rcv_state_process(struct sock *sk, struct sk_buff *skb,
			   struct gmtp_hdr *gh, unsigned int len)
{
	struct gmtp_skb_cb *gcb = GMTP_SKB_CB(skb);
	int queued = 0;

	gmtp_print_function();
	gmtp_print_debug("State: %s | Packet: %s", gmtp_state_name(sk->sk_state),
			gmtp_packet_name(gh->type));

	/*
	 *  Step 3: Process LISTEN state
	 *
	 *  If S.state == LISTEN,
	 *
	 *  If P.type == Request
	 *  (* Generate a new socket and switch to that socket *)
	 *	      Set S := new socket for this port pair
	 *	      S.state = GMTP_REQUEST_RECV
	 *	      Continue with S.state == REQ_RECV
	 *	      (* A REQUEST_REPLY packet will be generated in Step 11 (GMTP) *)
	 *	 Otherwise,
	 *	      Generate Reset(No Connection) unless P.type == Reset
	 *	      Drop packet and return
	 */

	if (sk->sk_state == GMTP_LISTEN)  {

		if (gh->type == GMTP_PKT_REQUEST || gh->type == GMTP_PKT_REGISTER) {
			if (inet_csk(sk)->icsk_af_ops->conn_request(sk, skb) < 0)
				return 1;
			goto discard;
		}
		if (gh->type == GMTP_PKT_RESET)
			goto discard;

		/* Caller (gmtp_v4_do_rcv) will send Reset */
		gcb->reset_code = GMTP_RESET_CODE_NO_CONNECTION;
		return 1;
	} else if (sk->sk_state == GMTP_CLOSED) {
		gcb->reset_code = GMTP_RESET_CODE_NO_CONNECTION;
		return 1;
	}

	/* Step 6: Check sequence numbers (omitted in LISTEN/REQUEST state) */
	if (sk->sk_state != GMTP_REQUESTING && gmtp_check_seqno(sk, skb))
		goto discard;

	/*
	 *   Step 7: Check for unexpected packet types
	 */
	if ((!gmtp_role_client(sk) && gh->type == GMTP_PKT_REQUESTNOTIFY) ||
		(gmtp_role_client(sk) && gh->type == GMTP_PKT_REQUEST) ||
		(sk->sk_state == GMTP_REQUEST_RECV && gh->type == GMTP_PKT_DATA))
	{
		gmtp_print_error("Unexpected packet type");
		goto discard;
	}

	/*
	 *  Step 9: Process Reset
	 *	If P.type == Reset,
	 *		Tear down connection
	 *		S.state := TIMEWAIT
	 *		Set TIMEWAIT timer
	 *		Drop packet and return
	 */
	if (gh->type == GMTP_PKT_RESET) {
		gmtp_rcv_reset(sk, skb);
		return 0;
	} else if (gh->type == GMTP_PKT_CLOSE) {		/* Step 14 */
		if (gmtp_rcv_close(sk, skb))
			return 0;
		goto discard;
	}

	switch (sk->sk_state) {
	case GMTP_REQUESTING: /* client */
		queued = gmtp_rcv_request_sent_state_process(sk, skb, gh, len);
		if (queued >= 0)
			return queued;

		__kfree_skb(skb);
		return 0;

	case GMTP_REQUEST_RECV: /* Request or Register was received. */
		queued = gmtp_rcv_request_rcv_state_process(sk, skb, gh, len);
		break;
	}

discard:
	__kfree_skb(skb);
	return 0;

}
EXPORT_SYMBOL_GPL(gmtp_rcv_state_process);
