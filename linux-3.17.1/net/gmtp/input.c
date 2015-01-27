#include <linux/kernel.h>
#include <linux/skbuff.h>
#include <linux/slab.h>
#include <linux/gmtp.h>
#include <linux/types.h>

#include <net/inet_sock.h>
#include <net/sock.h>

#include <uapi/linux/gmtp.h>
#include "gmtp.h"

static int gmtp_rcv_request_sent_state_process(struct sock *sk,
					       struct sk_buff *skb,
					       const struct dccp_hdr *dh,
					       const unsigned int len)
{
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
// 	if (dh->dccph_type == DCCP_PKT_RESPONSE) {
// 		const struct inet_connection_sock *icsk = inet_csk(sk);
// 		struct dccp_sock *dp = dccp_sk(sk);
// 		long tstamp = dccp_timestamp();
// 
// 		if (!between48(DCCP_SKB_CB(skb)->dccpd_ack_seq,
// 			       dp->dccps_awl, dp->dccps_awh)) {
// 			dccp_pr_debug("invalid ackno: S.AWL=%llu, "
// 				      "P.ackno=%llu, S.AWH=%llu\n",
// 				      (unsigned long long)dp->dccps_awl,
// 			   (unsigned long long)DCCP_SKB_CB(skb)->dccpd_ack_seq,
// 				      (unsigned long long)dp->dccps_awh);
// 			goto out_invalid_packet;
// 		}
// 
// 		/*
// 		 * If option processing (Step 8) failed, return 1 here so that
// 		 * dccp_v4_do_rcv() sends a Reset. The Reset code depends on
// 		 * the option type and is set in dccp_parse_options().
// 		 */
// 		if (dccp_parse_options(sk, NULL, skb))
// 			return 1;
// 
// 		/* Obtain usec RTT sample from SYN exchange (used by TFRC). */
// 		if (likely(dp->dccps_options_received.dccpor_timestamp_echo))
// 			dp->dccps_syn_rtt = dccp_sample_rtt(sk, 10 * (tstamp -
// 			    dp->dccps_options_received.dccpor_timestamp_echo));
// 
// 		/* Stop the REQUEST timer */
// 		inet_csk_clear_xmit_timer(sk, ICSK_TIME_RETRANS);
// 		WARN_ON(sk->sk_send_head == NULL);
// 		kfree_skb(sk->sk_send_head);
// 		sk->sk_send_head = NULL;
// 
// 		/*
// 		 * Set ISR, GSR from packet. ISS was set in dccp_v{4,6}_connect
// 		 * and GSS in dccp_transmit_skb(). Setting AWL/AWH and SWL/SWH
// 		 * is done as part of activating the feature values below, since
// 		 * these settings depend on the local/remote Sequence Window
// 		 * features, which were undefined or not confirmed until now.
// 		 */
// 		dp->dccps_gsr = dp->dccps_isr = DCCP_SKB_CB(skb)->dccpd_seq;
// 
// 		dccp_sync_mss(sk, icsk->icsk_pmtu_cookie);
// 
// 		/*
// 		 *    Step 10: Process REQUEST state (second part)
// 		 *       If S.state == REQUEST,
// 		 *	  / * If we get here, P is a valid Response from the
// 		 *	      server (see Step 4), and we should move to
// 		 *	      PARTOPEN state. PARTOPEN means send an Ack,
// 		 *	      don't send Data packets, retransmit Acks
// 		 *	      periodically, and always include any Init Cookie
// 		 *	      from the Response * /
// 		 *	  S.state := PARTOPEN
// 		 *	  Set PARTOPEN timer
// 		 *	  Continue with S.state == PARTOPEN
// 		 *	  / * Step 12 will send the Ack completing the
// 		 *	      three-way handshake * /
// 		 */
// 		dccp_set_state(sk, DCCP_PARTOPEN);
// 
// 		/*
// 		 * If feature negotiation was successful, activate features now;
// 		 * an activation failure means that this host could not activate
// 		 * one ore more features (e.g. insufficient memory), which would
// 		 * leave at least one feature in an undefined state.
// 		 */
// 		if (dccp_feat_activate_values(sk, &dp->dccps_featneg))
// 			goto unable_to_proceed;
// 
// 		/* Make sure socket is routed, for correct metrics. */
// 		icsk->icsk_af_ops->rebuild_header(sk);
// 
// 		if (!sock_flag(sk, SOCK_DEAD)) {
// 			sk->sk_state_change(sk);
// 			sk_wake_async(sk, SOCK_WAKE_IO, POLL_OUT);
// 		}
// 
// 		if (sk->sk_write_pending || icsk->icsk_ack.pingpong ||
// 		    icsk->icsk_accept_queue.rskq_defer_accept) {
// 			/* Save one ACK. Data will be ready after
// 			 * several ticks, if write_pending is set.
// 			 *
// 			 * It may be deleted, but with this feature tcpdumps
// 			 * look so _wonderfully_ clever, that I was not able
// 			 * to stand against the temptation 8)     --ANK
// 			 */
// 			/*
// 			 * OK, in DCCP we can as well do a similar trick, its
// 			 * even in the draft, but there is no need for us to
// 			 * schedule an ack here, as dccp_sendmsg does this for
// 			 * us, also stated in the draft. -acme
// 			 */
// 			__kfree_skb(skb);
// 			return 0;
// 		}
// 		dccp_send_ack(sk);
// 		return -1;
// 	}
// 
// out_invalid_packet:
// 	/* dccp_v4_do_rcv will send a reset */
// 	DCCP_SKB_CB(skb)->dccpd_reset_code = DCCP_RESET_CODE_PACKET_ERROR;
// 	return 1;
// 
// unable_to_proceed:
// 	DCCP_SKB_CB(skb)->dccpd_reset_code = DCCP_RESET_CODE_ABORTED;
// 	/*
// 	 * We mark this socket as no longer usable, so that the loop in
// 	 * dccp_sendmsg() terminates and the application gets notified.
// 	 */
// 	dccp_set_state(sk, DCCP_CLOSED);
// 	sk->sk_err = ECOMM;
 	return 1;
}


static void gmtp_rcv_reset(struct sock *sk, struct sk_buff *skb)
{
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
	gmtp_print_debug("gmtp_check_seqno");
	return 0;
}

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
	gmtp_print_debug("gmtp_parse_options");
	return 0;
}
EXPORT_SYMBOL_GPL(gmtp_parse_options);

static int __gmtp_rcv_established(struct sock *sk, struct sk_buff *skb,
				  const struct gmtp_hdr *gh, const unsigned int len)
{
	//struct gmtp_sock *dp = gmtp_sk(sk);

	gmtp_print_debug("__gmtp_rcv_established");

	switch (gmtp_hdr(skb)->type) {
	case GMTP_PKT_DATAACK:
	case GMTP_PKT_DATA:
		return 0;
	case GMTP_PKT_ACK:
		goto discard;
	}

	return 0;
discard:
	__kfree_skb(skb);
	return 0;
}

int gmtp_rcv_established(struct sock *sk, struct sk_buff *skb,
			 const struct gmtp_hdr *gh, const unsigned int len)
{
	gmtp_print_debug("gmtp_rcv_established");

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
	struct gmtp_skb_cb *dcb = GMTP_SKB_CB(skb);
	const int old_state = sk->sk_state;
	int queued = 0;

	gmtp_print_debug("gmtp_rcv_state_process");

	/*
	 *  Step 3: Process LISTEN state
	 *
	 *  If P.type == Request
	 *  (* Generate a new socket and switch to that socket *)
	 *	      Set S := new socket for this port pair
	 *	      S.state = GMTP_RESPOND
	 *	      Choose S.ISS (initial seqno)
	 *	      Initialize S.GAR := S.ISS
	 *	      Set S.ISR, S.GSR, S.SWL, S.SWH from packet
	 *	      Continue with S.state == RESPOND
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
	if (sk->sk_state != GMTP_REQUESTING/* && dccp_check_seqno(sk, skb)*/)
	{
		gmtp_print_debug("state != GMTP_REQUESTING (goto discard...)");
		goto discard;
	}

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
					(sk->sk_state == GMTP_RESPOND && gh->type == GMTP_PKT_DATA)) {
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
		//dccp_rcv_reset(sk, skb);
		return 0;
//	} else if (gh->type == GMTP_PKT_CLOSEREQ) {	/* Step 13 */
//		if (dccp_rcv_closereq(sk, skb))
//			return 0;
//		goto discard;
	} else if (gh->type == GMTP_PKT_CLOSE) {		/* Step 14 */
//		if (dccp_rcv_close(sk, skb))
//			return 0;
		goto discard;
	}

	switch (sk->sk_state) {
	case GMTP_REQUESTING:
		gmtp_print_debug("State == GMTP_REQUESTING");
//		queued = dccp_rcv_request_sent_state_process(sk, skb, gh, len);
		if (queued >= 0)
			return queued;

		__kfree_skb(skb);
		return 0;

		//TODO Tratar GMTP_CLOSE...
//	case DCCP_PARTOPEN:
//		/* Step 8: if using Ack Vectors, mark packet acknowledgeable */
//		dccp_handle_ackvec_processing(sk, skb);
//		dccp_deliver_input_to_ccids(sk, skb);
//		/* fall through */
	case GMTP_RESPOND:
//		queued = dccp_rcv_respond_partopen_state_process(sk, skb,
//				dh, len);
		queued = 0;
		break;
	}

//	if (gh->type == GMTP_PKT_ACK ||
//			gh->type == GMTP_PKT_DATAACK) {
//		switch (old_state) {
//		case DCCP_PARTOPEN:
//			sk->sk_state_change(sk);
//			sk_wake_async(sk, SOCK_WAKE_IO, POLL_OUT);
//			break;
//		}
//	} else if (unlikely(gh->type == GMTP_PKT_SYNC)) {
//		dccp_send_sync(sk, dcb->dccpd_seq, GMTP_PKT_SYNCACK);
		//goto discard;
	//}


	if (!queued) {
discard:
		__kfree_skb(skb);
	}
	return 0;
}
EXPORT_SYMBOL_GPL(gmtp_rcv_state_process);








