/*
 * output.c
 *
 *  Created on: 26/07/2014
 *      Author: wendell
 */

#include <linux/kernel.h>
#include <linux/skbuff.h>

#include "gmtp.h"

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
	gmtp_print_debug("gmtp_transmit_skb");
	if (likely(!skb)) {
		struct inet_sock *inet = inet_sk(sk);
		const struct inet_connection_sock *icsk = inet_csk(sk);
		//TODO Configure gmtp_sock...
		//struct gmtp_sock *gs = gmtp_sk(sk);
		struct gmtp_skb_cb *gcb = GMTP_SKB_CB(skb);
		struct gmtp_hdr *gh;

		const u32 gmtp_header_size = sizeof(*gh)
				+ gmtp_packet_hdr_len(gcb->gmtpd_type); //variable header size...

		int err = 1;

		//switch header type...
		switch (gcb->gmtpd_type) {
		case GMTP_PKT_DATA:
		case GMTP_PKT_DATAACK:
		case GMTP_PKT_RESET:
			gmtp_print_debug("gmtp_transmit_skb: DATA, DATAACK or RESET");
			break;

		case GMTP_PKT_REQUEST:
			//TODO treat request
			gmtp_print_debug("gmtp_transmit_skb: GMTP_PKT_REQUEST");
			break;

		default:
			/*
			 * Set owner/destructor: some skbs are allocated via
			 * alloc_skb (e.g. when retransmission may happen).
			 * Only Data, DataAck, and Reset packets should come
			 * through here with skb->sk set.
			 */
			gmtp_print_debug("gmtp_transmit_skb: default");
			WARN_ON(skb->sk);
			skb_set_owner_w(skb, sk);
			break;
		}

		if (gmtp_insert_options(sk, skb)) {
			kfree_skb(skb);
			return -EPROTO;
		}


		/* Build GMTP header */
		gmtp_print_debug("gmtp_transmit_skb: Building GMTP header...");
		gh = gmtp_zeroed_hdr(skb, gmtp_header_size);
		gh->type	= gcb->gmtpd_type;
		gh->src_port	= inet->inet_sport;
		gh->dest_port	= inet->inet_dport;
		//TODO Fill GMTP header

		//TODO Fill sequence number

		gmtp_print_debug("gmtp_transmit_skb: checking icsk");
		if(!icsk)
		{
			gmtp_print_warning("icsk is NULL!");
			return -ENOBUFS;
		}
		icsk->icsk_af_ops->send_check(sk, skb);

		err = icsk->icsk_af_ops->queue_xmit(skb, &inet->cork.fl);
		return net_xmit_eval(err);
	} else
		gmtp_print_warning("gmtp_transmit_skb: skb is NULL!");

	return -ENOBUFS;
}

/**
 * Do all connect socket setups that can be done AF independent.
 */
int gmtp_connect(struct sock *sk)
{
	struct sk_buff *skb;
//	struct gmtp_sock *gs = gmtp_sk(sk);
//	struct dst_entry *dst = __sk_dst_get(sk);
//	struct inet_connection_sock *icsk = inet_csk(sk);

	gmtp_print_debug("gmtp_connect");

	sk->sk_err = 0;
	sock_reset_flag(sk, SOCK_DONE);

//	dccp_sync_mss(sk, dst_mtu(dst));

	/* TODO do not connect if feature negotiation setup fails */
	//	if (dccp_feat_finalise_settings(dccp_sk(sk)))
	//		return -EPROTO;

	skb = alloc_skb(sk->sk_prot->max_header, sk->sk_allocation);
	if (unlikely(skb == NULL))
		return -ENOBUFS;

	/* Reserve space for headers. */
	skb_reserve(skb, sk->sk_prot->max_header);

//	DCCP_SKB_CB(skb)->dccpd_type = DCCP_PKT_REQUEST;
	gmtp_transmit_skb(sk, gmtp_skb_entail(sk, skb));


	/* TODO Timer for repeating the REQUEST until an answer.
	 * (...)
	 * */

	return 0;
}
