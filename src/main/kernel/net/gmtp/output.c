/*
 * output.c
 *
 *  Created on: 26/07/2014
 *      Author: wendell
 */

#include <linux/kernel.h>
#include <linux/skbuff.h>

#include "include/linux/gmtp.h"

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
//		struct gmtp_sock *gs = gmtp_sk(sk);
//		struct dccp_skb_cb *dcb = DCCP_SKB_CB(skb);
//		struct gmtp_hdr *gh;
		/* XXX For now we're using only 48 bits sequence numbers */
//		const u32 gmtp_header_size = sizeof(*gh)
//				+ gmtp_packet_hdr_len(dcb->dccpd_type); //variable header size...

		int err = 1;

		//switch header type...
/*		switch (dcb->dccpd_type) {
		case DCCP_PKT_DATA:
			set_ack = 0;
			 fall through
		case DCCP_PKT_DATAACK:
		case DCCP_PKT_RESET:
			break;

		case DCCP_PKT_REQUEST:
			set_ack = 0;
			 Use ISS on the first (non-retransmitted) Request.
			if (icsk->icsk_retransmits == 0)
				dcb->dccpd_seq = gs->dccps_iss;
			 fall through

		case DCCP_PKT_SYNC:
		case DCCP_PKT_SYNCACK:
			ackno = dcb->dccpd_ack_seq;
			 fall through
		default:

			 * Set owner/destructor: some skbs are allocated via
			 * alloc_skb (e.g. when retransmission may happen).
			 * Only Data, DataAck, and Reset packets should come
			 * through here with skb->sk set.

			WARN_ON(skb->sk);
			skb_set_owner_w(skb, sk);
			break;
		}

		if (dccp_insert_options(sk, skb)) {
			kfree_skb(skb);
			return -EPROTO;
		}
		*/


		/* Build GMTP header */
//		gh = gmtp_zeroed_hdr(skb, gmtp_header_size);
//		gh->dccph_type	= dcb->dccpd_type;
//		gh->src_port	= inet->inet_sport;
//		gh->dest_port	= inet->inet_dport;
//		gh->dccph_doff	= (dccp_header_size + dcb->dccpd_opt_len) / 4;
//		gh->dccph_ccval	= dcb->dccpd_ccval;
//		gh->dccph_cscov = gs->dccps_pcslen;
		/* XXX For now we're using only 48 bits sequence numbers */
//		gh->dccph_x	= 1;

//		dccp_update_gss(sk, dcb->dccpd_seq);
//		dccp_hdr_set_seq(gh, gs->dccps_gss);
//		if (set_ack)
//			dccp_hdr_set_ack(dccp_hdr_ack_bits(skb), ackno);

		//switch header type... (again)
/*		switch (dcb->dccpd_type) {
		case DCCP_PKT_REQUEST:
			dccp_hdr_request(skb)->dccph_req_service =
					gs->dccps_service;

			 * Limit Ack window to ISS <= P.ackno <= GSS, so that
			 * only Responses to Requests we sent are considered.

			gs->dccps_awl = gs->dccps_iss;
			break;
		case DCCP_PKT_RESET:
			dccp_hdr_reset(skb)->dccph_reset_code =
					dcb->dccpd_reset_code;
			break;
		}
		*/

		icsk->icsk_af_ops->send_check(sk, skb);

//		if (set_ack)
//			dccp_event_ack_sent(sk);

//		DCCP_INC_STATS(DCCP_MIB_OUTSEGS);

		err = icsk->icsk_af_ops->queue_xmit(skb, &inet->cork.fl);
		return net_xmit_eval(err);
	}
	return -ENOBUFS;
}

/*
 * Do all connect socket setups that can be done AF independent.
 */
int gmtp_connect(struct sock *sk)
{
	struct sk_buff *skb;
//	struct gmtp_sock *gs = gmtp_sk(sk);
//	struct dst_entry *dst = __sk_dst_get(sk);
//	struct inet_connection_sock *icsk = inet_csk(sk);

	sk->sk_err = 0;
	sock_reset_flag(sk, SOCK_DONE);

//	dccp_sync_mss(sk, dst_mtu(dst));

	/* do not connect if feature negotiation setup fails */
	//	if (dccp_feat_finalise_settings(dccp_sk(sk)))
	//		return -EPROTO;

	skb = alloc_skb(sk->sk_prot->max_header, sk->sk_allocation);
	if (unlikely(skb == NULL))
		return -ENOBUFS;

	/* Reserve space for headers. */
	skb_reserve(skb, sk->sk_prot->max_header);

	//sk and sk_buffer
	gmtp_transmit_skb(sk, skb);

	/* Timer for repeating the REQUEST until an answer. */

	return 0;
}
