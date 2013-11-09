/*
 *  net/gmtp/output.c
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
#include <linux/kernel.h>
#include <linux/skbuff.h>
#include <linux/slab.h>

#include <net/inet_sock.h>
#include <net/sock.h>

#include "ackvec.h"
#include "ccid.h"
#include "gmtp.h"

static inline void gmtp_event_ack_sent(struct sock *sk)
{
	inet_csk_clear_xmit_timer(sk, ICSK_TIME_DACK);
}

/* enqueue @skb on sk_send_head for retransmission, return clone to send now */
static struct sk_buff *gmtp_skb_entail(struct sock *sk, struct sk_buff *skb)
{
	skb_set_owner_w(skb, sk);
	WARN_ON(sk->sk_send_head);
	sk->sk_send_head = skb;
	return skb_clone(sk->sk_send_head, gfp_any());
}

/*
 * All SKB's seen here are completely headerless. It is our
 * job to build the GMTP header, and pass the packet down to
 * IP so it can do the same plus pass the packet off to the
 * device.
 */
static int gmtp_transmit_skb(struct sock *sk, struct sk_buff *skb)
{
	if (likely(skb != NULL)) {
		struct inet_sock *inet = inet_sk(sk);
		const struct inet_connection_sock *icsk = inet_csk(sk);
		struct gmtp_sock *dp = gmtp_sk(sk);
		struct gmtp_skb_cb *dcb = GMTP_SKB_CB(skb);
		struct gmtp_hdr *dh;
		/* XXX For now we're using only 48 bits sequence numbers */
		const u32 gmtp_header_size = sizeof(*dh) +
					     sizeof(struct gmtp_hdr_ext) +
					  gmtp_packet_hdr_len(dcb->gmtpd_type);
		int err, set_ack = 1;
		u64 ackno = dp->gmtps_gsr;
		/*
		 * Increment GSS here already in case the option code needs it.
		 * Update GSS for real only if option processing below succeeds.
		 */
		dcb->gmtpd_seq = ADD48(dp->gmtps_gss, 1);

		switch (dcb->gmtpd_type) {
		case GMTP_PKT_DATA:
			set_ack = 0;
			/* fall through */
		case GMTP_PKT_DATAACK:
		case GMTP_PKT_RESET:
			break;

		case GMTP_PKT_REQUEST:
			set_ack = 0;
			/* Use ISS on the first (non-retransmitted) Request. */
			if (icsk->icsk_retransmits == 0)
				dcb->gmtpd_seq = dp->gmtps_iss;
			/* fall through */

		case GMTP_PKT_SYNC:
		case GMTP_PKT_SYNCACK:
			ackno = dcb->gmtpd_ack_seq;
			/* fall through */
		default:
			/*
			 * Set owner/destructor: some skbs are allocated via
			 * alloc_skb (e.g. when retransmission may happen).
			 * Only Data, DataAck, and Reset packets should come
			 * through here with skb->sk set.
			 */
			WARN_ON(skb->sk);
			skb_set_owner_w(skb, sk);
			break;
		}

		if (gmtp_insert_options(sk, skb)) {
			kfree_skb(skb);
			return -EPROTO;
		}


		/* Build GMTP header and checksum it. */
		dh = gmtp_zeroed_hdr(skb, gmtp_header_size);
		dh->gmtph_type	= dcb->gmtpd_type;
		dh->gmtph_sport	= inet->inet_sport;
		dh->gmtph_dport	= inet->inet_dport;
		dh->gmtph_doff	= (gmtp_header_size + dcb->gmtpd_opt_len) / 4;
		dh->gmtph_ccval	= dcb->gmtpd_ccval;
		dh->gmtph_cscov = dp->gmtps_pcslen;
		/* XXX For now we're using only 48 bits sequence numbers */
		dh->gmtph_x	= 1;

		gmtp_update_gss(sk, dcb->gmtpd_seq);
		gmtp_hdr_set_seq(dh, dp->gmtps_gss);
		if (set_ack)
			gmtp_hdr_set_ack(gmtp_hdr_ack_bits(skb), ackno);

		switch (dcb->gmtpd_type) {
		case GMTP_PKT_REQUEST:
			gmtp_hdr_request(skb)->gmtph_req_service =
							dp->gmtps_service;
			/*
			 * Limit Ack window to ISS <= P.ackno <= GSS, so that
			 * only Responses to Requests we sent are considered.
			 */
			dp->gmtps_awl = dp->gmtps_iss;
			break;
		case GMTP_PKT_RESET:
			gmtp_hdr_reset(skb)->gmtph_reset_code =
							dcb->gmtpd_reset_code;
			break;
		}

		icsk->icsk_af_ops->send_check(sk, skb);

		if (set_ack)
			gmtp_event_ack_sent(sk);

		GMTP_INC_STATS(GMTP_MIB_OUTSEGS);

		err = icsk->icsk_af_ops->queue_xmit(skb, &inet->cork.fl);
		return net_xmit_eval(err);
	}
	return -ENOBUFS;
}

/**
 * gmtp_determine_ccmps  -  Find out about CCID-specific packet-size limits
 * We only consider the HC-sender CCID for setting the CCMPS (RFC 4340, 14.),
 * since the RX CCID is restricted to feedback packets (Acks), which are small
 * in comparison with the data traffic. A value of 0 means "no current CCMPS".
 */
static u32 gmtp_determine_ccmps(const struct gmtp_sock *dp)
{
	const struct ccid *tx_ccid = dp->gmtps_hc_tx_ccid;

	if (tx_ccid == NULL || tx_ccid->ccid_ops == NULL)
		return 0;
	return tx_ccid->ccid_ops->ccid_ccmps;
}

unsigned int gmtp_sync_mss(struct sock *sk, u32 pmtu)
{
	struct inet_connection_sock *icsk = inet_csk(sk);
	struct gmtp_sock *dp = gmtp_sk(sk);
	u32 ccmps = gmtp_determine_ccmps(dp);
	u32 cur_mps = ccmps ? min(pmtu, ccmps) : pmtu;

	/* Account for header lengths and IPv4/v6 option overhead */
	cur_mps -= (icsk->icsk_af_ops->net_header_len + icsk->icsk_ext_hdr_len +
		    sizeof(struct gmtp_hdr) + sizeof(struct gmtp_hdr_ext));

	/*
	 * Leave enough headroom for common GMTP header options.
	 * This only considers options which may appear on GMTP-Data packets, as
	 * per table 3 in RFC 4340, 5.8. When running out of space for other
	 * options (eg. Ack Vector which can take up to 255 bytes), it is better
	 * to schedule a separate Ack. Thus we leave headroom for the following:
	 *  - 1 byte for Slow Receiver (11.6)
	 *  - 6 bytes for Timestamp (13.1)
	 *  - 10 bytes for Timestamp Echo (13.3)
	 *  - 8 bytes for NDP count (7.7, when activated)
	 *  - 6 bytes for Data Checksum (9.3)
	 *  - %GMTPAV_MIN_OPTLEN bytes for Ack Vector size (11.4, when enabled)
	 */
	cur_mps -= roundup(1 + 6 + 10 + dp->gmtps_send_ndp_count * 8 + 6 +
			   (dp->gmtps_hc_rx_ackvec ? GMTPAV_MIN_OPTLEN : 0), 4);

	/* And store cached results */
	icsk->icsk_pmtu_cookie = pmtu;
	dp->gmtps_mss_cache = cur_mps;

	return cur_mps;
}

EXPORT_SYMBOL_GPL(gmtp_sync_mss);

void gmtp_write_space(struct sock *sk)
{
	struct socket_wq *wq;

	rcu_read_lock();
	wq = rcu_dereference(sk->sk_wq);
	if (wq_has_sleeper(wq))
		wake_up_interruptible(&wq->wait);
	/* Should agree with poll, otherwise some programs break */
	if (sock_writeable(sk))
		sk_wake_async(sk, SOCK_WAKE_SPACE, POLL_OUT);

	rcu_read_unlock();
}

/**
 * gmtp_wait_for_ccid  -  Await CCID send permission
 * @sk:    socket to wait for
 * @delay: timeout in jiffies
 *
 * This is used by CCIDs which need to delay the send time in process context.
 */
static int gmtp_wait_for_ccid(struct sock *sk, unsigned long delay)
{
	DEFINE_WAIT(wait);
	long remaining;

	prepare_to_wait(sk_sleep(sk), &wait, TASK_INTERRUPTIBLE);
	sk->sk_write_pending++;
	release_sock(sk);

	remaining = schedule_timeout(delay);

	lock_sock(sk);
	sk->sk_write_pending--;
	finish_wait(sk_sleep(sk), &wait);

	if (signal_pending(current) || sk->sk_err)
		return -1;
	return remaining;
}

/**
 * gmtp_xmit_packet  -  Send data packet under control of CCID
 * Transmits next-queued payload and informs CCID to account for the packet.
 */
static void gmtp_xmit_packet(struct sock *sk)
{
	int err, len;
	struct gmtp_sock *dp = gmtp_sk(sk);
	struct sk_buff *skb = gmtp_qpolicy_pop(sk);

	if (unlikely(skb == NULL))
		return;
	len = skb->len;

	if (sk->sk_state == GMTP_PARTOPEN) {
		const u32 cur_mps = dp->gmtps_mss_cache - GMTP_FEATNEG_OVERHEAD;
		/*
		 * See 8.1.5 - Handshake Completion.
		 *
		 * For robustness we resend Confirm options until the client has
		 * entered OPEN. During the initial feature negotiation, the MPS
		 * is smaller than usual, reduced by the Change/Confirm options.
		 */
		if (!list_empty(&dp->gmtps_featneg) && len > cur_mps) {
			GMTP_WARN("Payload too large (%d) for featneg.\n", len);
			gmtp_send_ack(sk);
			gmtp_feat_list_purge(&dp->gmtps_featneg);
		}

		inet_csk_schedule_ack(sk);
		inet_csk_reset_xmit_timer(sk, ICSK_TIME_DACK,
					      inet_csk(sk)->icsk_rto,
					      GMTP_RTO_MAX);
		GMTP_SKB_CB(skb)->gmtpd_type = GMTP_PKT_DATAACK;
	} else if (gmtp_ack_pending(sk)) {
		GMTP_SKB_CB(skb)->gmtpd_type = GMTP_PKT_DATAACK;
	} else {
		GMTP_SKB_CB(skb)->gmtpd_type = GMTP_PKT_DATA;
	}

	err = gmtp_transmit_skb(sk, skb);
	if (err)
		gmtp_pr_debug("transmit_skb() returned err=%d\n", err);
	/*
	 * Register this one as sent even if an error occurred. To the remote
	 * end a local packet drop is indistinguishable from network loss, i.e.
	 * any local drop will eventually be reported via receiver feedback.
	 */
	ccid_hc_tx_packet_sent(dp->gmtps_hc_tx_ccid, sk, len);

	/*
	 * If the CCID needs to transfer additional header options out-of-band
	 * (e.g. Ack Vectors or feature-negotiation options), it activates this
	 * flag to schedule a Sync. The Sync will automatically incorporate all
	 * currently pending header options, thus clearing the backlog.
	 */
	if (dp->gmtps_sync_scheduled)
		gmtp_send_sync(sk, dp->gmtps_gsr, GMTP_PKT_SYNC);
}

/**
 * gmtp_flush_write_queue  -  Drain queue at end of connection
 * Since gmtp_sendmsg queues packets without waiting for them to be sent, it may
 * happen that the TX queue is not empty at the end of a connection. We give the
 * HC-sender CCID a grace period of up to @time_budget jiffies. If this function
 * returns with a non-empty write queue, it will be purged later.
 */
void gmtp_flush_write_queue(struct sock *sk, long *time_budget)
{
	struct gmtp_sock *dp = gmtp_sk(sk);
	struct sk_buff *skb;
	long delay, rc;

	while (*time_budget > 0 && (skb = skb_peek(&sk->sk_write_queue))) {
		rc = ccid_hc_tx_send_packet(dp->gmtps_hc_tx_ccid, sk, skb);

		switch (ccid_packet_dequeue_eval(rc)) {
		case CCID_PACKET_WILL_DEQUEUE_LATER:
			/*
			 * If the CCID determines when to send, the next sending
			 * time is unknown or the CCID may not even send again
			 * (e.g. remote host crashes or lost Ack packets).
			 */
			GMTP_WARN("CCID did not manage to send all packets\n");
			return;
		case CCID_PACKET_DELAY:
			delay = msecs_to_jiffies(rc);
			if (delay > *time_budget)
				return;
			rc = gmtp_wait_for_ccid(sk, delay);
			if (rc < 0)
				return;
			*time_budget -= (delay - rc);
			/* check again if we can send now */
			break;
		case CCID_PACKET_SEND_AT_ONCE:
			gmtp_xmit_packet(sk);
			break;
		case CCID_PACKET_ERR:
			skb_dequeue(&sk->sk_write_queue);
			kfree_skb(skb);
			gmtp_pr_debug("packet discarded due to err=%ld\n", rc);
		}
	}
}

void gmtp_write_xmit(struct sock *sk)
{
	struct gmtp_sock *dp = gmtp_sk(sk);
	struct sk_buff *skb;

	while ((skb = gmtp_qpolicy_top(sk))) {
		int rc = ccid_hc_tx_send_packet(dp->gmtps_hc_tx_ccid, sk, skb);

		switch (ccid_packet_dequeue_eval(rc)) {
		case CCID_PACKET_WILL_DEQUEUE_LATER:
			return;
		case CCID_PACKET_DELAY:
			sk_reset_timer(sk, &dp->gmtps_xmit_timer,
				       jiffies + msecs_to_jiffies(rc));
			return;
		case CCID_PACKET_SEND_AT_ONCE:
			gmtp_xmit_packet(sk);
			break;
		case CCID_PACKET_ERR:
			gmtp_qpolicy_drop(sk, skb);
			gmtp_pr_debug("packet discarded due to err=%d\n", rc);
		}
	}
}

/**
 * gmtp_retransmit_skb  -  Retransmit Request, Close, or CloseReq packets
 * There are only four retransmittable packet types in GMTP:
 * - Request  in client-REQUEST  state (sec. 8.1.1),
 * - CloseReq in server-CLOSEREQ state (sec. 8.3),
 * - Close    in   node-CLOSING  state (sec. 8.3),
 * - Acks in client-PARTOPEN state (sec. 8.1.5, handled by gmtp_delack_timer()).
 * This function expects sk->sk_send_head to contain the original skb.
 */
int gmtp_retransmit_skb(struct sock *sk)
{
	WARN_ON(sk->sk_send_head == NULL);

	if (inet_csk(sk)->icsk_af_ops->rebuild_header(sk) != 0)
		return -EHOSTUNREACH; /* Routing failure or similar. */

	/* this count is used to distinguish original and retransmitted skb */
	inet_csk(sk)->icsk_retransmits++;

	return gmtp_transmit_skb(sk, skb_clone(sk->sk_send_head, GFP_ATOMIC));
}

struct sk_buff *gmtp_make_response(struct sock *sk, struct dst_entry *dst,
				   struct request_sock *req)
{
	struct gmtp_hdr *dh;
	struct gmtp_request_sock *dreq;
	const u32 gmtp_header_size = sizeof(struct gmtp_hdr) +
				     sizeof(struct gmtp_hdr_ext) +
				     sizeof(struct gmtp_hdr_response);
	struct sk_buff *skb = sock_wmalloc(sk, sk->sk_prot->max_header, 1,
					   GFP_ATOMIC);
	if (skb == NULL)
		return NULL;

	/* Reserve space for headers. */
	skb_reserve(skb, sk->sk_prot->max_header);

	skb_dst_set(skb, dst_clone(dst));

	dreq = gmtp_rsk(req);
	if (inet_rsk(req)->acked)	/* increase GSS upon retransmission */
		gmtp_inc_seqno(&dreq->dreq_gss);
	GMTP_SKB_CB(skb)->gmtpd_type = GMTP_PKT_RESPONSE;
	GMTP_SKB_CB(skb)->gmtpd_seq  = dreq->dreq_gss;

	/* Resolve feature dependencies resulting from choice of CCID */
	if (gmtp_feat_server_ccid_dependencies(dreq))
		goto response_failed;

	if (gmtp_insert_options_rsk(dreq, skb))
		goto response_failed;

	/* Build and checksum header */
	dh = gmtp_zeroed_hdr(skb, gmtp_header_size);

	dh->gmtph_sport	= inet_rsk(req)->loc_port;
	dh->gmtph_dport	= inet_rsk(req)->rmt_port;
	dh->gmtph_doff	= (gmtp_header_size +
			   GMTP_SKB_CB(skb)->gmtpd_opt_len) / 4;
	dh->gmtph_type	= GMTP_PKT_RESPONSE;
	dh->gmtph_x	= 1;
	gmtp_hdr_set_seq(dh, dreq->dreq_gss);
	gmtp_hdr_set_ack(gmtp_hdr_ack_bits(skb), dreq->dreq_gsr);
	gmtp_hdr_response(skb)->gmtph_resp_service = dreq->dreq_service;

	gmtp_csum_outgoing(skb);

	/* We use `acked' to remember that a Response was already sent. */
	inet_rsk(req)->acked = 1;
	GMTP_INC_STATS(GMTP_MIB_OUTSEGS);
	return skb;
response_failed:
	kfree_skb(skb);
	return NULL;
}

EXPORT_SYMBOL_GPL(gmtp_make_response);

/* answer offending packet in @rcv_skb with Reset from control socket @ctl */
struct sk_buff *gmtp_ctl_make_reset(struct sock *sk, struct sk_buff *rcv_skb)
{
	struct gmtp_hdr *rxdh = gmtp_hdr(rcv_skb), *dh;
	struct gmtp_skb_cb *dcb = GMTP_SKB_CB(rcv_skb);
	const u32 gmtp_hdr_reset_len = sizeof(struct gmtp_hdr) +
				       sizeof(struct gmtp_hdr_ext) +
				       sizeof(struct gmtp_hdr_reset);
	struct gmtp_hdr_reset *dhr;
	struct sk_buff *skb;

	skb = alloc_skb(sk->sk_prot->max_header, GFP_ATOMIC);
	if (skb == NULL)
		return NULL;

	skb_reserve(skb, sk->sk_prot->max_header);

	/* Swap the send and the receive. */
	dh = gmtp_zeroed_hdr(skb, gmtp_hdr_reset_len);
	dh->gmtph_type	= GMTP_PKT_RESET;
	dh->gmtph_sport	= rxdh->gmtph_dport;
	dh->gmtph_dport	= rxdh->gmtph_sport;
	dh->gmtph_doff	= gmtp_hdr_reset_len / 4;
	dh->gmtph_x	= 1;

	dhr = gmtp_hdr_reset(skb);
	dhr->gmtph_reset_code = dcb->gmtpd_reset_code;

	switch (dcb->gmtpd_reset_code) {
	case GMTP_RESET_CODE_PACKET_ERROR:
		dhr->gmtph_reset_data[0] = rxdh->gmtph_type;
		break;
	case GMTP_RESET_CODE_OPTION_ERROR:	/* fall through */
	case GMTP_RESET_CODE_MANDATORY_ERROR:
		memcpy(dhr->gmtph_reset_data, dcb->gmtpd_reset_data, 3);
		break;
	}
	/*
	 * From RFC 4340, 8.3.1:
	 *   If P.ackno exists, set R.seqno := P.ackno + 1.
	 *   Else set R.seqno := 0.
	 */
	if (dcb->gmtpd_ack_seq != GMTP_PKT_WITHOUT_ACK_SEQ)
		gmtp_hdr_set_seq(dh, ADD48(dcb->gmtpd_ack_seq, 1));
	gmtp_hdr_set_ack(gmtp_hdr_ack_bits(skb), dcb->gmtpd_seq);

	gmtp_csum_outgoing(skb);
	return skb;
}

EXPORT_SYMBOL_GPL(gmtp_ctl_make_reset);

/* send Reset on established socket, to close or abort the connection */
int gmtp_send_reset(struct sock *sk, enum gmtp_reset_codes code)
{
	struct sk_buff *skb;
	/*
	 * FIXME: what if rebuild_header fails?
	 * Should we be doing a rebuild_header here?
	 */
	int err = inet_csk(sk)->icsk_af_ops->rebuild_header(sk);

	if (err != 0)
		return err;

	skb = sock_wmalloc(sk, sk->sk_prot->max_header, 1, GFP_ATOMIC);
	if (skb == NULL)
		return -ENOBUFS;

	/* Reserve space for headers and prepare control bits. */
	skb_reserve(skb, sk->sk_prot->max_header);
	GMTP_SKB_CB(skb)->gmtpd_type	   = GMTP_PKT_RESET;
	GMTP_SKB_CB(skb)->gmtpd_reset_code = code;

	return gmtp_transmit_skb(sk, skb);
}

/*
 * Do all connect socket setups that can be done AF independent.
 */
int gmtp_connect(struct sock *sk)
{
	struct sk_buff *skb;
	struct gmtp_sock *dp = gmtp_sk(sk);
	struct dst_entry *dst = __sk_dst_get(sk);
	struct inet_connection_sock *icsk = inet_csk(sk);

	sk->sk_err = 0;
	sock_reset_flag(sk, SOCK_DONE);

	gmtp_sync_mss(sk, dst_mtu(dst));

	/* do not connect if feature negotiation setup fails */
	if (gmtp_feat_finalise_settings(gmtp_sk(sk)))
		return -EPROTO;

	/* Initialise GAR as per 8.5; AWL/AWH are set in gmtp_transmit_skb() */
	dp->gmtps_gar = dp->gmtps_iss;

	skb = alloc_skb(sk->sk_prot->max_header, sk->sk_allocation);
	if (unlikely(skb == NULL))
		return -ENOBUFS;

	/* Reserve space for headers. */
	skb_reserve(skb, sk->sk_prot->max_header);

	GMTP_SKB_CB(skb)->gmtpd_type = GMTP_PKT_REQUEST;

	gmtp_transmit_skb(sk, gmtp_skb_entail(sk, skb));
	GMTP_INC_STATS(GMTP_MIB_ACTIVEOPENS);

	/* Timer for repeating the REQUEST until an answer. */
	icsk->icsk_retransmits = 0;
	inet_csk_reset_xmit_timer(sk, ICSK_TIME_RETRANS,
				  icsk->icsk_rto, GMTP_RTO_MAX);
	return 0;
}

EXPORT_SYMBOL_GPL(gmtp_connect);

void gmtp_send_ack(struct sock *sk)
{
	/* If we have been reset, we may not send again. */
	if (sk->sk_state != GMTP_CLOSED) {
		struct sk_buff *skb = alloc_skb(sk->sk_prot->max_header,
						GFP_ATOMIC);

		if (skb == NULL) {
			inet_csk_schedule_ack(sk);
			inet_csk(sk)->icsk_ack.ato = TCP_ATO_MIN;
			inet_csk_reset_xmit_timer(sk, ICSK_TIME_DACK,
						  TCP_DELACK_MAX,
						  GMTP_RTO_MAX);
			return;
		}

		/* Reserve space for headers */
		skb_reserve(skb, sk->sk_prot->max_header);
		GMTP_SKB_CB(skb)->gmtpd_type = GMTP_PKT_ACK;
		gmtp_transmit_skb(sk, skb);
	}
}

EXPORT_SYMBOL_GPL(gmtp_send_ack);

#if 0
/* FIXME: Is this still necessary (11.3) - currently nowhere used by GMTP. */
void gmtp_send_delayed_ack(struct sock *sk)
{
	struct inet_connection_sock *icsk = inet_csk(sk);
	/*
	 * FIXME: tune this timer. elapsed time fixes the skew, so no problem
	 * with using 2s, and active senders also piggyback the ACK into a
	 * DATAACK packet, so this is really for quiescent senders.
	 */
	unsigned long timeout = jiffies + 2 * HZ;

	/* Use new timeout only if there wasn't a older one earlier. */
	if (icsk->icsk_ack.pending & ICSK_ACK_TIMER) {
		/* If delack timer was blocked or is about to expire,
		 * send ACK now.
		 *
		 * FIXME: check the "about to expire" part
		 */
		if (icsk->icsk_ack.blocked) {
			gmtp_send_ack(sk);
			return;
		}

		if (!time_before(timeout, icsk->icsk_ack.timeout))
			timeout = icsk->icsk_ack.timeout;
	}
	icsk->icsk_ack.pending |= ICSK_ACK_SCHED | ICSK_ACK_TIMER;
	icsk->icsk_ack.timeout = timeout;
	sk_reset_timer(sk, &icsk->icsk_delack_timer, timeout);
}
#endif

void gmtp_send_sync(struct sock *sk, const u64 ackno,
		    const enum gmtp_pkt_type pkt_type)
{
	/*
	 * We are not putting this on the write queue, so
	 * gmtp_transmit_skb() will set the ownership to this
	 * sock.
	 */
	struct sk_buff *skb = alloc_skb(sk->sk_prot->max_header, GFP_ATOMIC);

	if (skb == NULL) {
		/* FIXME: how to make sure the sync is sent? */
		GMTP_CRIT("could not send %s", gmtp_packet_name(pkt_type));
		return;
	}

	/* Reserve space for headers and prepare control bits. */
	skb_reserve(skb, sk->sk_prot->max_header);
	GMTP_SKB_CB(skb)->gmtpd_type = pkt_type;
	GMTP_SKB_CB(skb)->gmtpd_ack_seq = ackno;

	/*
	 * Clear the flag in case the Sync was scheduled for out-of-band data,
	 * such as carrying a long Ack Vector.
	 */
	gmtp_sk(sk)->gmtps_sync_scheduled = 0;

	gmtp_transmit_skb(sk, skb);
}

EXPORT_SYMBOL_GPL(gmtp_send_sync);

/*
 * Send a GMTP_PKT_CLOSE/CLOSEREQ. The caller locks the socket for us. This
 * cannot be allowed to fail queueing a GMTP_PKT_CLOSE/CLOSEREQ frame under
 * any circumstances.
 */
void gmtp_send_close(struct sock *sk, const int active)
{
	struct gmtp_sock *dp = gmtp_sk(sk);
	struct sk_buff *skb;
	const gfp_t prio = active ? GFP_KERNEL : GFP_ATOMIC;

	skb = alloc_skb(sk->sk_prot->max_header, prio);
	if (skb == NULL)
		return;

	/* Reserve space for headers and prepare control bits. */
	skb_reserve(skb, sk->sk_prot->max_header);
	if (dp->gmtps_role == GMTP_ROLE_SERVER && !dp->gmtps_server_timewait)
		GMTP_SKB_CB(skb)->gmtpd_type = GMTP_PKT_CLOSEREQ;
	else
		GMTP_SKB_CB(skb)->gmtpd_type = GMTP_PKT_CLOSE;

	if (active) {
		skb = gmtp_skb_entail(sk, skb);
		/*
		 * Retransmission timer for active-close: RFC 4340, 8.3 requires
		 * to retransmit the Close/CloseReq until the CLOSING/CLOSEREQ
		 * state can be left. The initial timeout is 2 RTTs.
		 * Since RTT measurement is done by the CCIDs, there is no easy
		 * way to get an RTT sample. The fallback RTT from RFC 4340, 3.4
		 * is too low (200ms); we use a high value to avoid unnecessary
		 * retransmissions when the link RTT is > 0.2 seconds.
		 * FIXME: Let main module sample RTTs and use that instead.
		 */
		inet_csk_reset_xmit_timer(sk, ICSK_TIME_RETRANS,
					  GMTP_TIMEOUT_INIT, GMTP_RTO_MAX);
	}
	gmtp_transmit_skb(sk, skb);
}
