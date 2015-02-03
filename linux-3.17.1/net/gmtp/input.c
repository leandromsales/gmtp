#include <linux/kernel.h>
#include <linux/skbuff.h>
#include <linux/slab.h>
#include <linux/gmtp.h>
#include <linux/types.h>
#include <linux/net.h>
#include <linux/security.h>

#include <net/inet_sock.h>
#include <net/sock.h>

#include <uapi/linux/gmtp.h>
#include <linux/gmtp.h>
#include "gmtp.h"

static void gmtp_enqueue_skb(struct sock *sk, struct sk_buff *skb)
{
	gmtp_print_function();
//	__skb_pull(skb, dccp_hdr(skb)->dccph_doff * 4);
	__skb_pull(skb, gmtp_hdr(skb)->hdrlen);
	__skb_queue_tail(&sk->sk_receive_queue, skb);
	skb_set_owner_r(skb, sk);
	sk->sk_data_ready(sk);
}

static void gmtp_fin(struct sock *sk, struct sk_buff *skb)
{
	/*
	 * On receiving Close/CloseReq, both RD/WR shutdown are performed.
	 * RFC 4340, 8.3 says that we MAY send further Data/DataAcks after
	 * receiving the closing segment, but there is no guarantee that such
	 * data will be processed at all.
	 */
    gmtp_print_function();
	sk->sk_shutdown = SHUTDOWN_MASK;
	sock_set_flag(sk, SOCK_DONE);
	gmtp_enqueue_skb(sk, skb);
}

static int gmtp_rcv_close(struct sock *sk, struct sk_buff *skb)
{
	int queued = 0;

    gmtp_print_function();

	switch (sk->sk_state) {
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
		if (gmtp_sk(sk)->gmtps_role != GMTP_ROLE_CLIENT)
			break;
		/* fall through */
	case GMTP_REQUESTING:
	case GMTP_ACTIVE_CLOSEREQ:
		gmtp_send_reset(sk, GMTP_RESET_CODE_CLOSED);
		gmtp_done(sk);
		break;
	case GMTP_OPEN:
	// case GMTP_PARTOPEN:
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
					       const struct gmtp_hdr *dh,
					       const unsigned int len)
{
	gmtp_print_function();

	/*
	 *  Step 4: Prepare sequence numbers in REQUEST
	 *     If S.state == REQUEST,
	 *	  If (P.type == Response or P.type == Reset)
	 *		and S.AWL <= P.ackno <= S.AWH,
	 *	     / * Set sequence number variables corresponding to the
	 *		other endpoint, so P will pass the tests in Step 6 * /
	 *	     Set S.GSR, S.ISR, S.SWL, S.SWH
	 *	     / * Response processing continues in Step 10; Reset
	 *		processing continues in Step 9 * /
	*/
	if (dh->type == GMTP_PKT_REGISTER_REPLY) {
		const struct inet_connection_sock *icsk = inet_csk(sk);
		struct gmtp_sock *dp = gmtp_sk(sk);
		// long tstamp = dccp_timestamp();

    /*** TODO verificar numero de sequencia ***/
    //	if (!between48(DCCP_SKB_CB(skb)->dccpd_ack_seq,
	//		       dp->dccps_awl, dp->dccps_awh)) {
	//		dccp_pr_debug("invalid ackno: S.AWL=%llu, "
	//			      "P.ackno=%llu, S.AWH=%llu\n",
	//			      (unsigned long long)dp->dccps_awl,
	//		   (unsigned long long)DCCP_SKB_CB(skb)->dccpd_ack_seq,
	//			      (unsigned long long)dp->dccps_awh);
	//		goto out_invalid_packet;
	//	}

		/*
		 * If option processing (Step 8) failed, return 1 here so that
		 * dccp_v4_do_rcv() sends a Reset. The Reset code depends on
		 * the option type and is set in dccp_parse_options().
		 */
		if (gmtp_parse_options(sk, NULL, skb))
			return 1;

		/* Obtain usec RTT sample from SYN exchange (used by TFRC). */
//		if (likely(dp->dccps_options_received.dccpor_timestamp_echo))
//			dp->dccps_syn_rtt = dccp_sample_rtt(sk, 10 * (tstamp -
//			    dp->dccps_options_received.dccpor_timestamp_echo));
//
		/* Stop the REQUEST timer */
//		inet_csk_clear_xmit_timer(sk, ICSK_TIME_RETRANS);
//		WARN_ON(sk->sk_send_head == NULL);
//		kfree_skb(sk->sk_send_head);
//		sk->sk_send_head = NULL;

		/*
		 * Set ISR, GSR from packet. ISS was set in dccp_v{4,6}_connect
		 * and GSS in dccp_transmit_skb(). Setting AWL/AWH and SWL/SWH
		 * is done as part of activating the feature values below, since
		 * these settings depend on the local/remote Sequence Window
		 * features, which were undefined or not confirmed until now.
		 */
//		dp->dccps_gsr = dp->dccps_isr = DCCP_SKB_CB(skb)->dccpd_seq;

//		dccp_sync_mss(sk, icsk->icsk_pmtu_cookie);

		/*
		 *    Step 10: Process REQUEST state (second part)
		 *       If S.state == REQUEST,
		 *	  / * If we get here, P is a valid Response from the
		 *	      server (see Step 4), and we should move to
		 *	      PARTOPEN state. PARTOPEN means send an Ack,
		 *	      don't send Data packets, retransmit Acks
		 *	      periodically, and always include any Init Cookie
		 *	      from the Response * /
		 *	  S.state := PARTOPEN
		 *	  Set PARTOPEN timer
		 *	  Continue with S.state == PARTOPEN
		 *	  / * Step 12 will send the Ack completing the
		 *	      three-way handshake * /
		 */
//		 gmtp_set_state(sk, DCCP_PARTOPEN);

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
			 *
			 * It may be deleted, but with this feature tcpdumps
			 * look so _wonderfully_ clever, that I was not able
			 * to stand against the temptation 8)     --ANK
			 */
			/*
			 * OK, in DCCP we can as well do a similar trick, its
			 * even in the draft, but there is no need for us to
			 * schedule an ack here, as dccp_sendmsg does this for
			 * us, also stated in the draft. -acme
			 */
			__kfree_skb(skb);
			return 0;
		}
 		gmtp_send_ack(sk);
 		return -1;
 	}
// 
out_invalid_packet:
 	/* dccp_v4_do_rcv will send a reset */
 	GMTP_SKB_CB(skb)->gmtpd_reset_code = GMTP_RESET_CODE_PACKET_ERROR;
 	return 1;
unable_to_proceed:
 	GMTP_SKB_CB(skb)->gmtpd_reset_code = GMTP_RESET_CODE_ABORTED;
 	/*
 	 * We mark this socket as no longer usable, so that the loop in
 	 * dccp_sendmsg() terminates and the application gets notified.
 	 */
 	gmtp_set_state(sk, GMTP_CLOSED);
 	sk->sk_err = ECOMM;
 	return 1;
}


static void gmtp_rcv_reset(struct sock *sk, struct sk_buff *skb)
{
	//TODO Implement gmtp_rcv_reset
	gmtp_print_function();
    /****
	u16 err = dccp_reset_code_convert(dccp_hdr_reset(skb)->dccph_reset_code);

	sk->sk_err = err;
    ****/

	/* Queue the equivalent of TCP fin so that dccp_recvmsg exits the loop */

    /****
	dccp_fin(sk, skb);

	if (err && !sock_flag(sk, SOCK_DEAD))
		sk_wake_async(sk, SOCK_WAKE_IO, POLL_ERR);
	dccp_time_wait(sk, DCCP_TIME_WAIT, 0);
    ****/
}

static int gmtp_check_seqno(struct sock *sk, struct sk_buff *skb)
{
	gmtp_print_function();
	return 0;
}

static int __gmtp_rcv_established(struct sock *sk, struct sk_buff *skb,
				  const struct gmtp_hdr *dh, const unsigned int len)
{
	struct gmtp_sock *dp = gmtp_sk(sk);

	gmtp_print_function();

	switch (gmtp_hdr(skb)->type) {
	case GMTP_PKT_DATAACK:
	case GMTP_PKT_DATA:
//		/*
//		 * FIXME: schedule DATA_DROPPED (RFC 4340, 11.7.2) if and when
//		 * - sk_shutdown == RCV_SHUTDOWN, use Code 1, "Not Listening"
//		 * - sk_receive_queue is full, use Code 2, "Receive Buffer"
//		 */
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
//	case DCCP_PKT_CLOSEREQ:
//		if (dccp_rcv_closereq(sk, skb))
//			return 0;
//		goto discard;
//	case DCCP_PKT_CLOSE:
//		if (dccp_rcv_close(sk, skb))
//			return 0;
//		goto discard;
	case GMTP_PKT_REQUEST:
		//TODO Treat Step 7.
		/* Step 7
		 *   or (S.is_server and P.type == Response)
		 *   or (S.is_client and P.type == Request)
		 *   or (S.state >= OPEN and P.type == Request
		 *	and P.seqno >= S.OSR)
		 *    or (S.state >= OPEN and P.type == Response
		 *	and P.seqno >= S.OSR)
		 *    or (S.state == RESPOND and P.type == Data),
		 *  Send Sync packet acknowledging P.seqno
		 *  Drop packet and return
		 */
		if (dp->gmtps_role != GMTP_ROLE_LISTEN)
			goto send_sync;
		goto check_seq;
	case GMTP_PKT_RESPONSE:
		if (dp->gmtps_role != GMTP_ROLE_CLIENT)
			goto send_sync;
check_seq:  ;
//		if (dccp_delta_seqno(dp->dccps_osr,
//				     DCCP_SKB_CB(skb)->dccpd_seq) >= 0) {
send_sync:  ;
//			dccp_send_sync(sk, DCCP_SKB_CB(skb)->dccpd_seq,
//				       DCCP_PKT_SYNC);
//		}
//		break;
//	case DCCP_PKT_SYNC:
//		dccp_send_sync(sk, DCCP_SKB_CB(skb)->dccpd_seq,
//			       DCCP_PKT_SYNCACK);
//		/*
//		 * From RFC 4340, sec. 5.7
//		 *
//		 * As with DCCP-Ack packets, DCCP-Sync and DCCP-SyncAck packets
//		 * MAY have non-zero-length application data areas, whose
//		 * contents receivers MUST ignore.
//		 */
		goto discard;
	}
//
//	DCCP_INC_STATS_BH(DCCP_MIB_INERRS);
discard:
	__kfree_skb(skb);
	return 0;
}

static int gmtp_rcv_respond_partopen_state_process(struct sock *sk,
						   struct sk_buff *skb,
						   const struct gmtp_hdr *dh,
						   const unsigned int len)
{
	struct inet_connection_sock *icsk = inet_csk(sk);
	struct gmtp_sock *dp = gmtp_sk(sk);
	int queued = 0;

	gmtp_print_function();

	switch (dh->type) {
	case GMTP_PKT_RESET:
		gmtp_print_debug("GMTP_PKT_RESET");
		break;
	case GMTP_PKT_DATA:
	case GMTP_PKT_DATAACK:
	case GMTP_PKT_ACK:

		gmtp_set_state(sk, GMTP_OPEN);

		if (skb != NULL) {
			gmtp_print_debug("Nao chamo nada...");
//			icsk->icsk_af_ops->sk_rx_dst_set(sk, skb);
//			security_inet_conn_established(sk, skb);
		}

		/* Make sure socket is routed, for correct metrics.  */
//		icsk->icsk_af_ops->rebuild_header(sk);

//		tcp_init_buffer_space(sk);

//		if (sock_flag(sk, SOCK_KEEPOPEN))
//			inet_csk_reset_keepalive_timer(sk, keepalive_time_when(dp));

//		if (!sock_flag(sk, SOCK_DEAD)) {
//			sk->sk_state_change(sk);
//			sk_wake_async(sk, SOCK_WAKE_IO, POLL_OUT);
//		}

		break;
	}
	gmtp_print_debug("queued = %d", queued);
	return queued;
}


int gmtp_rcv_established(struct sock *sk, struct sk_buff *skb,
			 const struct gmtp_hdr *gh, const unsigned int len)
{
	gmtp_print_function();

	//Check sequence numbers...
	if (gmtp_check_seqno(sk, skb))
		goto discard;

	if (gmtp_parse_options(sk, NULL, skb))
		return 1;

	//TODO verificar acks..s
//	dccp_handle_ackvec_processing(sk, skb);

	return __gmtp_rcv_established(sk, skb, gh, len);
discard:
	__kfree_skb(skb);
	return 0;
}
EXPORT_SYMBOL_GPL(gmtp_rcv_established);

int gmtp_rcv_state_process(struct sock *sk, struct sk_buff *skb,
			   struct gmtp_hdr *gh, unsigned int len)
{
	struct gmtp_sock *dp = gmtp_sk(sk);
	struct inet_connection_sock *icsk = inet_csk(sk);
	struct gmtp_skb_cb *dcb = GMTP_SKB_CB(skb);
	const int old_state = sk->sk_state;
	int queued = 0;

	gmtp_print_function();

	/*
	 *  Step 3: Process LISTEN state
	 *
	 *  If P.type == Request
	 *  (* Generate a new socket and switch to that socket *)
	 *	      Set S := new socket for this port pair
	 *	      S.state = GMTP_REQ_RECV
	 *	      Continue with S.state == REQ_RECV
	 *	      (* A Response packet will be generated in Step 11 (dccp) *)
	 *	 Otherwise,
	 *	      Generate Reset(No Connection) unless P.type == Reset
	 *	      Drop packet and return
	 */

	if (sk->sk_state == GMTP_LISTEN) {
		gmtp_print_debug("state == GMTP_LISTEN");
		if (gh->type == GMTP_PKT_REQUEST) {

			gmtp_print_debug("type == GMTP_PKT_REQUEST");

			if (inet_csk(sk)->icsk_af_ops->conn_request(sk, skb) < 0) {
				gmtp_print_debug("inet_csk(sk)->icsk_af_ops->conn_request(sk, skb) < 0");
				return 1;
			}
			goto discard;
		}
		if (gh->type == GMTP_PKT_RESET) {
			gmtp_print_debug("type == GMTP_PKT_REQUEST (goto discard...)");
			goto discard;
		}

		/* Caller (gmtp_v4_do_rcv) will send Reset */
		gmtp_print_debug("Caller (gmtp_v4_do_rcv) will send Reset...");
		dcb->gmtpd_reset_code = GMTP_RESET_CODE_NO_CONNECTION;
		return 1;
	} else if (sk->sk_state == GMTP_CLOSED) {
		gmtp_print_debug("state == GMTP_CLOSED");
		dcb->gmtpd_reset_code = GMTP_RESET_CODE_NO_CONNECTION;
		return 1;
	}

	/* Step 6: Check sequence numbers (omitted in LISTEN/REQUEST state) */
	//TODO check sequence number
//	if (sk->sk_state != GMTP_REQUESTING/* && dccp_check_seqno(sk, skb)*/)
//	{
//		gmtp_print_debug("state != GMTP_REQUESTING (goto discard...)");
//		goto discard;
//	}

	/*
	 *   Step 7: Check for unexpected packet types
	 *      If (S.is_server and P.type == Response)
	 *	    or (S.is_client and P.type == Request)
	 *	    or (S.state == RESPOND and P.type == Data),
	 *	  Send Sync packet acknowledging P.seqno
	 *	  Drop packet and return
	 */
	if ((dp->gmtps_role != GMTP_ROLE_CLIENT &&
			gh->type == GMTP_PKT_RESPONSE) ||
			(dp->gmtps_role == GMTP_ROLE_CLIENT &&
					gh->type == GMTP_PKT_REQUEST) ||
					(sk->sk_state == GMTP_REQ_RECV && gh->type == GMTP_PKT_DATA)) {
		//TODO Send sync
		//dccp_send_sync(sk, dcb->dccpd_seq, DCCP_PKT_SYNC);
		gmtp_print_debug("unexpected packet types...");
		goto discard;
	}

	/*  Step 8: Process options */
	//TODO Parse options
//	if (dccp_parse_options(sk, NULL, skb))
//		return 1;

	/*
	 *  Step 9: Process Reset
	 *	If P.type == Reset,
	 *		Tear down connection
	 *		S.state := TIMEWAIT
	 *		Set TIMEWAIT timer
	 *		Drop packet and return
	 */
	//TODO Tratar RESET e CLOSE
	if (gh->type == GMTP_PKT_RESET) {
		gmtp_rcv_reset(sk, skb);
		return 0;
//	} else if (gh->type == GMTP_PKT_CLOSEREQ) {	/* Step 13 */
//		if (dccp_rcv_closereq(sk, skb))
//			return 0;
//		goto discard;
	} else if (gh->type == GMTP_PKT_CLOSE) {		/* Step 14 */
		if (gmtp_rcv_close(sk, skb))
			return 0;
		goto discard;
	}

	switch (sk->sk_state) {
	case GMTP_REQUESTING:
		gmtp_print_debug("State == GMTP_REQUESTING");
		queued = gmtp_rcv_request_sent_state_process(sk, skb, gh, len);
		if (queued >= 0)
			return queued;

		__kfree_skb(skb);
		return 0;

	case GMTP_REQ_RECV:
		gmtp_print_debug("State == GMTP_REQ_RECV");
//		queued = gmtp_rcv_respond_partopen_state_process(sk, skb, gh, len);
		gmtp_print_debug("Rebuilding");
		icsk->icsk_af_ops->rebuild_header(sk);

		//TODO MTU probing init per socket tcp_outuput.c
		//tcp_mtup_init(sk);
		//tcp_init_buffer_space(sk); ??

		gmtp_print_debug("Calling smp_mb");
		smp_mb();
		gmtp_set_state(sk, GMTP_OPEN);

		sk->sk_state_change(sk);
		sk_wake_async(sk, SOCK_WAKE_IO, POLL_OUT);

		gmtp_print_debug("break;");
		break;
	}

discard:
	__kfree_skb(skb);
	return 0;

}
EXPORT_SYMBOL_GPL(gmtp_rcv_state_process);


/**
 * gmtp_parse_options  -  Parse GMTP options present in @skb
 * @sk: client|server|listening gmtp socket (when @dreq != NULL)
 * @dreq: request socket to use during connection setup, or NULL
 *
 * TODO Verificar se a fc eh necessaria
 */
int gmtp_parse_options(struct sock *sk, struct gmtp_request_sock *dreq,
		       struct sk_buff *skb)
{
	gmtp_print_function();
	return 0;
}
EXPORT_SYMBOL_GPL(gmtp_parse_options);






