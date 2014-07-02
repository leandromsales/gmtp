/*
 *  net/gmtp/input.c
 *
 *  An implementation of the GMTP protocol
 *  Arnaldo Carvalho de Melo <acme@conectiva.com.br>
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License
 *	as published by the Free Software Foundation; either version
 *	2 of the License, or (at your option) any later version.
 */

#include <linux/gmtp.h>
#include <linux/skbuff.h>
#include <linux/slab.h>

#include <net/sock.h>

#include "ackvec.h"
#include "ccid.h"
#include "gmtp.h"

/* rate-limit for syncs in reply to sequence-invalid packets; RFC 4340, 7.5.4 */
int sysctl_gmtp_sync_ratelimit	__read_mostly = HZ / 8;

static void gmtp_enqueue_skb(struct sock *sk, struct sk_buff *skb)
{
	__skb_pull(skb, gmtp_hdr(skb)->gmtph_doff * 4);
	__skb_queue_tail(&sk->sk_receive_queue, skb);
	skb_set_owner_r(skb, sk);
	sk->sk_data_ready(sk, 0);
}

static void gmtp_fin(struct sock *sk, struct sk_buff *skb)
{
	/*
	 * On receiving Close/CloseReq, both RD/WR shutdown are performed.
	 * RFC 4340, 8.3 says that we MAY send further Data/DataAcks after
	 * receiving the closing segment, but there is no guarantee that such
	 * data will be processed at all.
	 */
	sk->sk_shutdown = SHUTDOWN_MASK;
	sock_set_flag(sk, SOCK_DONE);
	gmtp_enqueue_skb(sk, skb);
}

static int gmtp_rcv_close(struct sock *sk, struct sk_buff *skb)
{
	int queued = 0;

	switch (sk->sk_state) {
	/*
	 * We ignore Close when received in one of the following states:
	 *  - CLOSED		(may be a late or duplicate packet)
	 *  - PASSIVE_CLOSEREQ	(the peer has sent a CloseReq earlier)
	 *  - RESPOND		(already handled by gmtp_check_req)
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
	case GMTP_PARTOPEN:
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

static int gmtp_rcv_closereq(struct sock *sk, struct sk_buff *skb)
{
	int queued = 0;

	/*
	 *   Step 7: Check for unexpected packet types
	 *      If (S.is_server and P.type == CloseReq)
	 *	  Send Sync packet acknowledging P.seqno
	 *	  Drop packet and return
	 */
	if (gmtp_sk(sk)->gmtps_role != GMTP_ROLE_CLIENT) {
		gmtp_send_sync(sk, GMTP_SKB_CB(skb)->gmtpd_seq, GMTP_PKT_SYNC);
		return queued;
	}

	/* Step 13: process relevant Client states < CLOSEREQ */
	switch (sk->sk_state) {
	case GMTP_REQUESTING:
		gmtp_send_close(sk, 0);
		gmtp_set_state(sk, GMTP_CLOSING);
		break;
	case GMTP_OPEN:
	case GMTP_PARTOPEN:
		/* Give waiting application a chance to read pending data */
		queued = 1;
		gmtp_fin(sk, skb);
		gmtp_set_state(sk, GMTP_PASSIVE_CLOSEREQ);
		/* fall through */
	case GMTP_PASSIVE_CLOSEREQ:
		sk_wake_async(sk, SOCK_WAKE_WAITD, POLL_HUP);
	}
	return queued;
}

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
	[GMTP_RESET_CODE_BAD_INIT_COOKIE]    = EBADR,
	[GMTP_RESET_CODE_BAD_SERVICE_CODE]   = EBADRQC,
	[GMTP_RESET_CODE_OPTION_ERROR]	     = EILSEQ,
	[GMTP_RESET_CODE_MANDATORY_ERROR]    = EOPNOTSUPP,
	};

	return code >= GMTP_MAX_RESET_CODES ? 0 : error_code[code];
}

static void gmtp_rcv_reset(struct sock *sk, struct sk_buff *skb)
{
	u16 err = gmtp_reset_code_convert(gmtp_hdr_reset(skb)->gmtph_reset_code);

	sk->sk_err = err;

	/* Queue the equivalent of TCP fin so that gmtp_recvmsg exits the loop */
	gmtp_fin(sk, skb);

	if (err && !sock_flag(sk, SOCK_DEAD))
		sk_wake_async(sk, SOCK_WAKE_IO, POLL_ERR);
	gmtp_time_wait(sk, GMTP_TIME_WAIT, 0);
}

static void gmtp_handle_ackvec_processing(struct sock *sk, struct sk_buff *skb)
{
	struct gmtp_ackvec *av = gmtp_sk(sk)->gmtps_hc_rx_ackvec;

	if (av == NULL)
		return;
	if (GMTP_SKB_CB(skb)->gmtpd_ack_seq != GMTP_PKT_WITHOUT_ACK_SEQ)
		gmtp_ackvec_clear_state(av, GMTP_SKB_CB(skb)->gmtpd_ack_seq);
	gmtp_ackvec_input(av, skb);
}

static void gmtp_deliver_input_to_ccids(struct sock *sk, struct sk_buff *skb)
{
	const struct gmtp_sock *dp = gmtp_sk(sk);

	/* Don't deliver to RX CCID when node has shut down read end. */
	if (!(sk->sk_shutdown & RCV_SHUTDOWN))
		ccid_hc_rx_packet_recv(dp->gmtps_hc_rx_ccid, sk, skb);
	/*
	 * Until the TX queue has been drained, we can not honour SHUT_WR, since
	 * we need received feedback as input to adjust congestion control.
	 */
	if (sk->sk_write_queue.qlen > 0 || !(sk->sk_shutdown & SEND_SHUTDOWN))
		ccid_hc_tx_packet_recv(dp->gmtps_hc_tx_ccid, sk, skb);
}

static int gmtp_check_seqno(struct sock *sk, struct sk_buff *skb)
{
	const struct gmtp_hdr *dh = gmtp_hdr(skb);
	struct gmtp_sock *dp = gmtp_sk(sk);
	u64 lswl, lawl, seqno = GMTP_SKB_CB(skb)->gmtpd_seq,
			ackno = GMTP_SKB_CB(skb)->gmtpd_ack_seq;

	/*
	 *   Step 5: Prepare sequence numbers for Sync
	 *     If P.type == Sync or P.type == SyncAck,
	 *	  If S.AWL <= P.ackno <= S.AWH and P.seqno >= S.SWL,
	 *	     / * P is valid, so update sequence number variables
	 *		 accordingly.  After this update, P will pass the tests
	 *		 in Step 6.  A SyncAck is generated if necessary in
	 *		 Step 15 * /
	 *	     Update S.GSR, S.SWL, S.SWH
	 *	  Otherwise,
	 *	     Drop packet and return
	 */
	if (dh->gmtph_type == GMTP_PKT_SYNC ||
	    dh->gmtph_type == GMTP_PKT_SYNCACK) {
		if (between48(ackno, dp->gmtps_awl, dp->gmtps_awh) &&
		    gmtp_delta_seqno(dp->gmtps_swl, seqno) >= 0)
			gmtp_update_gsr(sk, seqno);
		else
			return -1;
	}

	/*
	 *   Step 6: Check sequence numbers
	 *      Let LSWL = S.SWL and LAWL = S.AWL
	 *      If P.type == CloseReq or P.type == Close or P.type == Reset,
	 *	  LSWL := S.GSR + 1, LAWL := S.GAR
	 *      If LSWL <= P.seqno <= S.SWH
	 *	     and (P.ackno does not exist or LAWL <= P.ackno <= S.AWH),
	 *	  Update S.GSR, S.SWL, S.SWH
	 *	  If P.type != Sync,
	 *	     Update S.GAR
	 */
	lswl = dp->gmtps_swl;
	lawl = dp->gmtps_awl;

	if (dh->gmtph_type == GMTP_PKT_CLOSEREQ ||
	    dh->gmtph_type == GMTP_PKT_CLOSE ||
	    dh->gmtph_type == GMTP_PKT_RESET) {
		lswl = ADD48(dp->gmtps_gsr, 1);
		lawl = dp->gmtps_gar;
	}

	if (between48(seqno, lswl, dp->gmtps_swh) &&
	    (ackno == GMTP_PKT_WITHOUT_ACK_SEQ ||
	     between48(ackno, lawl, dp->gmtps_awh))) {
		gmtp_update_gsr(sk, seqno);

		if (dh->gmtph_type != GMTP_PKT_SYNC &&
		    ackno != GMTP_PKT_WITHOUT_ACK_SEQ &&
		    after48(ackno, dp->gmtps_gar))
			dp->gmtps_gar = ackno;
	} else {
		unsigned long now = jiffies;
		/*
		 *   Step 6: Check sequence numbers
		 *      Otherwise,
		 *         If P.type == Reset,
		 *            Send Sync packet acknowledging S.GSR
		 *         Otherwise,
		 *            Send Sync packet acknowledging P.seqno
		 *      Drop packet and return
		 *
		 *   These Syncs are rate-limited as per RFC 4340, 7.5.4:
		 *   at most 1 / (gmtp_sync_rate_limit * HZ) Syncs per second.
		 */
		if (time_before(now, (dp->gmtps_rate_last +
				      sysctl_gmtp_sync_ratelimit)))
			return -1;

		GMTP_WARN("Step 6 failed for %s packet, "
			  "(LSWL(%llu) <= P.seqno(%llu) <= S.SWH(%llu)) and "
			  "(P.ackno %s or LAWL(%llu) <= P.ackno(%llu) <= S.AWH(%llu), "
			  "sending SYNC...\n",  gmtp_packet_name(dh->gmtph_type),
			  (unsigned long long) lswl, (unsigned long long) seqno,
			  (unsigned long long) dp->gmtps_swh,
			  (ackno == GMTP_PKT_WITHOUT_ACK_SEQ) ? "doesn't exist"
							      : "exists",
			  (unsigned long long) lawl, (unsigned long long) ackno,
			  (unsigned long long) dp->gmtps_awh);

		dp->gmtps_rate_last = now;

		if (dh->gmtph_type == GMTP_PKT_RESET)
			seqno = dp->gmtps_gsr;
		gmtp_send_sync(sk, seqno, GMTP_PKT_SYNC);
		return -1;
	}

	return 0;
}

static int __gmtp_rcv_established(struct sock *sk, struct sk_buff *skb,
				  const struct gmtp_hdr *dh, const unsigned int len)
{
	struct gmtp_sock *dp = gmtp_sk(sk);

	switch (gmtp_hdr(skb)->gmtph_type) {
	case GMTP_PKT_DATAACK:
	case GMTP_PKT_DATA:
		/*
		 * FIXME: schedule DATA_DROPPED (RFC 4340, 11.7.2) if and when
		 * - sk_shutdown == RCV_SHUTDOWN, use Code 1, "Not Listening"
		 * - sk_receive_queue is full, use Code 2, "Receive Buffer"
		 */
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
	case GMTP_PKT_CLOSEREQ:
		if (gmtp_rcv_closereq(sk, skb))
			return 0;
		goto discard;
	case GMTP_PKT_CLOSE:
		if (gmtp_rcv_close(sk, skb))
			return 0;
		goto discard;
	case GMTP_PKT_REQUEST:
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
check_seq:
		if (gmtp_delta_seqno(dp->gmtps_osr,
				     GMTP_SKB_CB(skb)->gmtpd_seq) >= 0) {
send_sync:
			gmtp_send_sync(sk, GMTP_SKB_CB(skb)->gmtpd_seq,
				       GMTP_PKT_SYNC);
		}
		break;
	case GMTP_PKT_SYNC:
		gmtp_send_sync(sk, GMTP_SKB_CB(skb)->gmtpd_seq,
			       GMTP_PKT_SYNCACK);
		/*
		 * From RFC 4340, sec. 5.7
		 *
		 * As with GMTP-Ack packets, GMTP-Sync and GMTP-SyncAck packets
		 * MAY have non-zero-length application data areas, whose
		 * contents receivers MUST ignore.
		 */
		goto discard;
	}

	GMTP_INC_STATS_BH(GMTP_MIB_INERRS);
discard:
	__kfree_skb(skb);
	return 0;
}

int gmtp_rcv_established(struct sock *sk, struct sk_buff *skb,
			 const struct gmtp_hdr *dh, const unsigned int len)
{
	if (gmtp_check_seqno(sk, skb))
		goto discard;

	if (gmtp_parse_options(sk, NULL, skb))
		return 1;

	gmtp_handle_ackvec_processing(sk, skb);
	gmtp_deliver_input_to_ccids(sk, skb);

	return __gmtp_rcv_established(sk, skb, dh, len);
discard:
	__kfree_skb(skb);
	return 0;
}

EXPORT_SYMBOL_GPL(gmtp_rcv_established);

static int gmtp_rcv_request_sent_state_process(struct sock *sk,
					       struct sk_buff *skb,
					       const struct gmtp_hdr *dh,
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
	if (dh->gmtph_type == GMTP_PKT_RESPONSE) {
		const struct inet_connection_sock *icsk = inet_csk(sk);
		struct gmtp_sock *dp = gmtp_sk(sk);
		long tstamp = gmtp_timestamp();

		if (!between48(GMTP_SKB_CB(skb)->gmtpd_ack_seq,
			       dp->gmtps_awl, dp->gmtps_awh)) {
			gmtp_pr_debug("invalid ackno: S.AWL=%llu, "
				      "P.ackno=%llu, S.AWH=%llu\n",
				      (unsigned long long)dp->gmtps_awl,
			   (unsigned long long)GMTP_SKB_CB(skb)->gmtpd_ack_seq,
				      (unsigned long long)dp->gmtps_awh);
			goto out_invalid_packet;
		}

		/*
		 * If option processing (Step 8) failed, return 1 here so that
		 * gmtp_v4_do_rcv() sends a Reset. The Reset code depends on
		 * the option type and is set in gmtp_parse_options().
		 */
		if (gmtp_parse_options(sk, NULL, skb))
			return 1;

		/* Obtain usec RTT sample from SYN exchange (used by TFRC). */
		if (likely(dp->gmtps_options_received.gmtpor_timestamp_echo))
			dp->gmtps_syn_rtt = gmtp_sample_rtt(sk, 10 * (tstamp -
			    dp->gmtps_options_received.gmtpor_timestamp_echo));

		/* Stop the REQUEST timer */
		inet_csk_clear_xmit_timer(sk, ICSK_TIME_RETRANS);
		WARN_ON(sk->sk_send_head == NULL);
		kfree_skb(sk->sk_send_head);
		sk->sk_send_head = NULL;

		/*
		 * Set ISR, GSR from packet. ISS was set in gmtp_v{4,6}_connect
		 * and GSS in gmtp_transmit_skb(). Setting AWL/AWH and SWL/SWH
		 * is done as part of activating the feature values below, since
		 * these settings depend on the local/remote Sequence Window
		 * features, which were undefined or not confirmed until now.
		 */
		dp->gmtps_gsr = dp->gmtps_isr = GMTP_SKB_CB(skb)->gmtpd_seq;

		gmtp_sync_mss(sk, icsk->icsk_pmtu_cookie);

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
		gmtp_set_state(sk, GMTP_PARTOPEN);

		/*
		 * If feature negotiation was successful, activate features now;
		 * an activation failure means that this host could not activate
		 * one ore more features (e.g. insufficient memory), which would
		 * leave at least one feature in an undefined state.
		 */
		if (gmtp_feat_activate_values(sk, &dp->gmtps_featneg))
			goto unable_to_proceed;

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
			 * OK, in GMTP we can as well do a similar trick, its
			 * even in the draft, but there is no need for us to
			 * schedule an ack here, as gmtp_sendmsg does this for
			 * us, also stated in the draft. -acme
			 */
			__kfree_skb(skb);
			return 0;
		}
		gmtp_send_ack(sk);
		return -1;
	}

out_invalid_packet:
	/* gmtp_v4_do_rcv will send a reset */
	GMTP_SKB_CB(skb)->gmtpd_reset_code = GMTP_RESET_CODE_PACKET_ERROR;
	return 1;

unable_to_proceed:
	GMTP_SKB_CB(skb)->gmtpd_reset_code = GMTP_RESET_CODE_ABORTED;
	/*
	 * We mark this socket as no longer usable, so that the loop in
	 * gmtp_sendmsg() terminates and the application gets notified.
	 */
	gmtp_set_state(sk, GMTP_CLOSED);
	sk->sk_err = ECOMM;
	return 1;
}

static int gmtp_rcv_respond_partopen_state_process(struct sock *sk,
						   struct sk_buff *skb,
						   const struct gmtp_hdr *dh,
						   const unsigned int len)
{
	struct gmtp_sock *dp = gmtp_sk(sk);
	u32 sample = dp->gmtps_options_received.gmtpor_timestamp_echo;
	int queued = 0;

	switch (dh->gmtph_type) {
	case GMTP_PKT_RESET:
		inet_csk_clear_xmit_timer(sk, ICSK_TIME_DACK);
		break;
	case GMTP_PKT_DATA:
		if (sk->sk_state == GMTP_RESPOND)
			break;
	case GMTP_PKT_DATAACK:
	case GMTP_PKT_ACK:
		/*
		 * FIXME: we should be reseting the PARTOPEN (DELACK) timer
		 * here but only if we haven't used the DELACK timer for
		 * something else, like sending a delayed ack for a TIMESTAMP
		 * echo, etc, for now were not clearing it, sending an extra
		 * ACK when there is nothing else to do in DELACK is not a big
		 * deal after all.
		 */

		/* Stop the PARTOPEN timer */
		if (sk->sk_state == GMTP_PARTOPEN)
			inet_csk_clear_xmit_timer(sk, ICSK_TIME_DACK);

		/* Obtain usec RTT sample from SYN exchange (used by TFRC). */
		if (likely(sample)) {
			long delta = gmtp_timestamp() - sample;

			dp->gmtps_syn_rtt = gmtp_sample_rtt(sk, 10 * delta);
		}

		dp->gmtps_osr = GMTP_SKB_CB(skb)->gmtpd_seq;
		gmtp_set_state(sk, GMTP_OPEN);

		if (dh->gmtph_type == GMTP_PKT_DATAACK ||
		    dh->gmtph_type == GMTP_PKT_DATA) {
			__gmtp_rcv_established(sk, skb, dh, len);
			queued = 1; /* packet was queued
				       (by __gmtp_rcv_established) */
		}
		break;
	}

	return queued;
}

int gmtp_rcv_state_process(struct sock *sk, struct sk_buff *skb,
			   struct gmtp_hdr *dh, unsigned int len)
{
	struct gmtp_sock *dp = gmtp_sk(sk);
	struct gmtp_skb_cb *dcb = GMTP_SKB_CB(skb);
	const int old_state = sk->sk_state;
	int queued = 0;

	/*
	 *  Step 3: Process LISTEN state
	 *
	 *     If S.state == LISTEN,
	 *	 If P.type == Request or P contains a valid Init Cookie option,
	 *	      (* Must scan the packet's options to check for Init
	 *		 Cookies.  Only Init Cookies are processed here,
	 *		 however; other options are processed in Step 8.  This
	 *		 scan need only be performed if the endpoint uses Init
	 *		 Cookies *)
	 *	      (* Generate a new socket and switch to that socket *)
	 *	      Set S := new socket for this port pair
	 *	      S.state = RESPOND
	 *	      Choose S.ISS (initial seqno) or set from Init Cookies
	 *	      Initialize S.GAR := S.ISS
	 *	      Set S.ISR, S.GSR, S.SWL, S.SWH from packet or Init
	 *	      Cookies Continue with S.state == RESPOND
	 *	      (* A Response packet will be generated in Step 11 *)
	 *	 Otherwise,
	 *	      Generate Reset(No Connection) unless P.type == Reset
	 *	      Drop packet and return
	 */
	if (sk->sk_state == GMTP_LISTEN) {
		if (dh->gmtph_type == GMTP_PKT_REQUEST) {
			if (inet_csk(sk)->icsk_af_ops->conn_request(sk,
								    skb) < 0)
				return 1;
			goto discard;
		}
		if (dh->gmtph_type == GMTP_PKT_RESET)
			goto discard;

		/* Caller (gmtp_v4_do_rcv) will send Reset */
		dcb->gmtpd_reset_code = GMTP_RESET_CODE_NO_CONNECTION;
		return 1;
	} else if (sk->sk_state == GMTP_CLOSED) {
		dcb->gmtpd_reset_code = GMTP_RESET_CODE_NO_CONNECTION;
		return 1;
	}

	/* Step 6: Check sequence numbers (omitted in LISTEN/REQUEST state) */
	if (sk->sk_state != GMTP_REQUESTING && gmtp_check_seqno(sk, skb))
		goto discard;

	/*
	 *   Step 7: Check for unexpected packet types
	 *      If (S.is_server and P.type == Response)
	 *	    or (S.is_client and P.type == Request)
	 *	    or (S.state == RESPOND and P.type == Data),
	 *	  Send Sync packet acknowledging P.seqno
	 *	  Drop packet and return
	 */
	if ((dp->gmtps_role != GMTP_ROLE_CLIENT &&
	     dh->gmtph_type == GMTP_PKT_RESPONSE) ||
	    (dp->gmtps_role == GMTP_ROLE_CLIENT &&
	     dh->gmtph_type == GMTP_PKT_REQUEST) ||
	    (sk->sk_state == GMTP_RESPOND && dh->gmtph_type == GMTP_PKT_DATA)) {
		gmtp_send_sync(sk, dcb->gmtpd_seq, GMTP_PKT_SYNC);
		goto discard;
	}

	/*  Step 8: Process options */
	if (gmtp_parse_options(sk, NULL, skb))
		return 1;

	/*
	 *  Step 9: Process Reset
	 *	If P.type == Reset,
	 *		Tear down connection
	 *		S.state := TIMEWAIT
	 *		Set TIMEWAIT timer
	 *		Drop packet and return
	 */
	if (dh->gmtph_type == GMTP_PKT_RESET) {
		gmtp_rcv_reset(sk, skb);
		return 0;
	} else if (dh->gmtph_type == GMTP_PKT_CLOSEREQ) {	/* Step 13 */
		if (gmtp_rcv_closereq(sk, skb))
			return 0;
		goto discard;
	} else if (dh->gmtph_type == GMTP_PKT_CLOSE) {		/* Step 14 */
		if (gmtp_rcv_close(sk, skb))
			return 0;
		goto discard;
	}

	switch (sk->sk_state) {
	case GMTP_REQUESTING:
		queued = gmtp_rcv_request_sent_state_process(sk, skb, dh, len);
		if (queued >= 0)
			return queued;

		__kfree_skb(skb);
		return 0;

	case GMTP_PARTOPEN:
		/* Step 8: if using Ack Vectors, mark packet acknowledgeable */
		gmtp_handle_ackvec_processing(sk, skb);
		gmtp_deliver_input_to_ccids(sk, skb);
		/* fall through */
	case GMTP_RESPOND:
		queued = gmtp_rcv_respond_partopen_state_process(sk, skb,
								 dh, len);
		break;
	}

	if (dh->gmtph_type == GMTP_PKT_ACK ||
	    dh->gmtph_type == GMTP_PKT_DATAACK) {
		switch (old_state) {
		case GMTP_PARTOPEN:
			sk->sk_state_change(sk);
			sk_wake_async(sk, SOCK_WAKE_IO, POLL_OUT);
			break;
		}
	} else if (unlikely(dh->gmtph_type == GMTP_PKT_SYNC)) {
		gmtp_send_sync(sk, dcb->gmtpd_seq, GMTP_PKT_SYNCACK);
		goto discard;
	}

	if (!queued) {
discard:
		__kfree_skb(skb);
	}
	return 0;
}

EXPORT_SYMBOL_GPL(gmtp_rcv_state_process);

/**
 *  gmtp_sample_rtt  -  Validate and finalise computation of RTT sample
 *  @delta:	number of microseconds between packet and acknowledgment
 *
 *  The routine is kept generic to work in different contexts. It should be
 *  called immediately when the ACK used for the RTT sample arrives.
 */
u32 gmtp_sample_rtt(struct sock *sk, long delta)
{
	/* gmtpor_elapsed_time is either zeroed out or set and > 0 */
	delta -= gmtp_sk(sk)->gmtps_options_received.gmtpor_elapsed_time * 10;

	if (unlikely(delta <= 0)) {
		GMTP_WARN("unusable RTT sample %ld, using min\n", delta);
		return GMTP_SANE_RTT_MIN;
	}
	if (unlikely(delta > GMTP_SANE_RTT_MAX)) {
		GMTP_WARN("RTT sample %ld too large, using max\n", delta);
		return GMTP_SANE_RTT_MAX;
	}

	return delta;
}
