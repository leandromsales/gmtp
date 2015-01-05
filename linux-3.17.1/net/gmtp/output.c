#include <linux/kernel.h>
#include <linux/skbuff.h>
#include <linux/slab.h>
#include <linux/gmtp.h>
#include <linux/types.h>

#include <net/inet_sock.h>
#include <net/sock.h>
#include "gmtp.h"

/* enqueue @skb on sk_send_head for retransmission, return clone to send now */
static struct sk_buff *gmtp_skb_entail(struct sock *sk, struct sk_buff *skb) {
	gmtp_print_debug("gmtp_skb_entail...");
	skb_set_owner_w(skb, sk);
	WARN_ON(sk->sk_send_head);
	sk->sk_send_head = skb;
	return skb_clone(sk->sk_send_head, gfp_any());
}

/*
 * All SKB's seen here are completely headerless. It is our
 * job to build the DCCP header, and pass the packet down to
 * IP so it can do the same plus pass the packet off to the
 * device.
 */
static int gmtp_transmit_skb(struct sock *sk, struct sk_buff *skb) {

	gmtp_print_debug("gmtp_transmit_skb");

	if (likely(skb != NULL)) {

		struct inet_sock *inet = inet_sk(sk);
		const struct inet_connection_sock *icsk = inet_csk(sk);
		struct gmtp_sock *gp = gmtp_sk(sk);

		struct gmtp_skb_cb *gcb = GMTP_SKB_CB(skb);

		struct gmtp_hdr *gh;

		const u32 gmtp_header_size = sizeof(*gh)
				+ gmtp_packet_hdr_variable_len(gcb->gmtpd_type);

		int err, set_ack = 1;
//		u64 ackno = gp->gmtps_gsr;

		/*
		 * Increment GSS here already in case the option code needs it.
		 * Update GSS for real only if option processing below succeeds.
		 */
		gcb->gmtpd_seq = gp->gmtps_gss + 1;

		switch (gcb->gmtpd_type) {
		case GMTP_PKT_DATA:
			set_ack = 0;
			/* fall through */
		case GMTP_PKT_ACK:
		case GMTP_PKT_RESET:
			break;

		case GMTP_PKT_REQUEST:
			set_ack = 0;
			/* Use ISS on the first (non-retransmitted) Request. */
			if (icsk->icsk_retransmits == 0)
				gcb->gmtpd_seq = gp->gmtps_iss;
			/* fall through */

			//TODO Treat each GMTP packet type...
//		case GMTP_PKT_SYNC:
//		case GMTP_PKT_SYNCACK:
//			ackno = dcb->gmtpd_ack_seq;
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

		//TODO GMTP insert options here!
		//TODO Build complete GMTP header here!
		gh = gmtp_zeroed_hdr(skb, gmtp_header_size);

		gh->version = 1;
		gh->type = gcb->gmtpd_type;
		gh->sport = inet->inet_sport;
		gh->dport = inet->inet_dport;
		gh->hdrlen = gmtp_header_size;

		//If protocol has checksum... calculate here...
		//DCCP timer...
//		if (set_ack)
//			dccp_event_ack_sent(sk);

		gmtp_print_debug("skb header size: %d", skb_headlen(skb));
		err = icsk->icsk_af_ops->queue_xmit(sk, skb, &inet->cork.fl);
		return net_xmit_eval(err);

	}
	return -ENOBUFS;
}

/*
 * Do all connect socket setups that can be done AF independent.
 */
int gmtp_connect(struct sock *sk) {

	struct sk_buff *skb;
//	struct gmtp_sock *gp = gmtp_sk(sk);
//	struct dst_entry *dst = __sk_dst_get(sk);
//	struct inet_connection_sock *icsk = inet_csk(sk);

	gmtp_print_debug("gmtp_connect");

	sk->sk_err = 0;
	sock_reset_flag(sk, SOCK_DONE);

	//TODO Sync GMTP MSS?
//	dccp_sync_mss(sk, dst_mtu(dst));

	//TODO Initialize variables...
	skb = alloc_skb(sk->sk_prot->max_header, sk->sk_allocation);
	gmtp_print_debug("skb == %p", skb);

	if (unlikely(skb == NULL)) {
		gmtp_print_error("skb = alloc_skb(...) == NULL");
		return -ENOBUFS;
	}

	/* Reserve space for headers. */

	gmtp_print_debug("Reserving space for headers. ");
	skb_reserve(skb, sk->sk_prot->max_header);

	GMTP_SKB_CB(skb)->gmtpd_type = GMTP_PKT_REQUEST;

	gmtp_transmit_skb(sk, gmtp_skb_entail(sk, skb));

	//TODO Retransmit?
//	icsk->icsk_retransmits = 0;
//	inet_csk_reset_xmit_timer(sk, ICSK_TIME_RETRANS,
//					  icsk->icsk_rto, DCCP_RTO_MAX);

	return 0;
}
EXPORT_SYMBOL_GPL(gmtp_connect);


struct sk_buff *gmtp_make_response(struct sock *sk, struct dst_entry *dst,
				   struct request_sock *req)
{
	struct gmtp_hdr *dh;
	struct gmtp_request_sock *dreq;
	const u32 gmtp_header_size = sizeof(struct gmtp_hdr);/* +
			sizeof(struct dccp_hdr_ext) +
			sizeof(struct dccp_hdr_response);*/
	struct sk_buff *skb = sock_wmalloc(sk, sk->sk_prot->max_header, 1,
			GFP_ATOMIC);

	gmtp_print_debug("gmtp_make_response");

	if (skb == NULL)
		return NULL;

	/* Reserve space for headers. */
	skb_reserve(skb, sk->sk_prot->max_header);

	skb_dst_set(skb, dst_clone(dst));

	dreq = gmtp_rsk(req);
//	if (inet_rsk(req)->acked)	/* increase GSS upon retransmission */
//		dccp_inc_seqno(&dreq->dreq_gss);
	GMTP_SKB_CB(skb)->gmtpd_type = GMTP_PKT_RESPONSE;
	GMTP_SKB_CB(skb)->gmtpd_seq  = 1; //dreq->dreq_gss;

	/* Build header */
	dh = gmtp_zeroed_hdr(skb, gmtp_header_size);

	dh->sport	= htons(inet_rsk(req)->ir_num);
	dh->dport	= inet_rsk(req)->ir_rmt_port;
	dh->hdrlen	= (gmtp_header_size); /*+
			   DCCP_SKB_CB(skb)->dccpd_opt_len) / 4; */
	dh->type	= GMTP_PKT_RESPONSE;

	dh->seq = 1; //TODO configure sequence
//	dccp_hdr_set_ack(dccp_hdr_ack_bits(skb), dreq->dreq_gsr);
//	dccp_hdr_response(skb)->dccph_resp_service = dreq->dreq_service;

	/* We use `acked' to remember that a Response was already sent. */
	inet_rsk(req)->acked = 1;
//	DCCP_INC_STATS(DCCP_MIB_OUTSEGS);

	gmtp_print_debug("Response skb header size: %d", skb_headlen(skb));
	return skb;
//response_failed:
//	kfree_skb(skb);
//	return NULL;
}
EXPORT_SYMBOL_GPL(gmtp_make_response);

/* answer offending packet in @rcv_skb with Reset from control socket @ctl */
struct sk_buff *gmtp_ctl_make_reset(struct sock *sk, struct sk_buff *rcv_skb)
{
	struct gmtp_hdr *rxgh = gmtp_hdr(rcv_skb), *gh;
	struct gmtp_skb_cb *gcb = GMTP_SKB_CB(rcv_skb);
	const u32 gmtp_hdr_reset_len = sizeof(struct gmtp_hdr);/*  +
			sizeof(struct dccp_hdr_ext) +
			sizeof(struct dccp_hdr_reset);*/
	struct gmtp_hdr_reset *ghr;
	struct sk_buff *skb;

	gmtp_print_debug("gmtp_ctl_make_reset");

	skb = alloc_skb(sk->sk_prot->max_header, GFP_ATOMIC);
	if (skb == NULL)
		return NULL;

	skb_reserve(skb, sk->sk_prot->max_header);

	/* Swap the send and the receive. */
	gh = gmtp_zeroed_hdr(skb, gmtp_hdr_reset_len);
	gh->type	= GMTP_PKT_RESET;
	gh->sport	= rxgh->dport;
	gh->dport	= rxgh->sport;
	gh->hdrlen	= gmtp_hdr_reset_len;

//	ghr = gmtp_hdr_reset(skb);
//	ghr->gmtph_reset_code = gcb->gmtpd_reset_code;

	switch (gcb->gmtpd_reset_code) {
	case GMTP_RESET_CODE_PACKET_ERROR:
//		ghr->gmtph_reset_data[0] = rxgh->type;
		break;
	case GMTP_RESET_CODE_OPTION_ERROR:	 /* fall through */
	case GMTP_RESET_CODE_MANDATORY_ERROR:
//		memcpy(dhr->dccph_reset_data, gcb->dccpd_reset_data, 3);
		break;
	}

	/* In DCCP: From RFC 4340, 8.3.1:
	 *   If P.ackno exists, set R.seqno := P.ackno + 1.
	 *   Else set R.seqno := 0.
	 *
	 */  //TODO GMTP RFC...
//	if (gcb->dccpd_ack_seq != DCCP_PKT_WITHOUT_ACK_SEQ)
//		dccp_hdr_set_seq(gh, ADD48(gcb->dccpd_ack_seq, 1));
//	dccp_hdr_set_ack(dccp_hdr_ack_bits(skb), gcb->dccpd_seq);

//	dccp_csum_outgoing(skb);
	return skb;
}
EXPORT_SYMBOL_GPL(gmtp_ctl_make_reset);

