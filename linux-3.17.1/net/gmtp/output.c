#include <linux/kernel.h>
#include <linux/skbuff.h>
#include <linux/slab.h>
#include <linux/gmtp.h>
#include <linux/types.h>

#include <net/inet_sock.h>
#include <net/sock.h>
#include "gmtp.h"

/* enqueue @skb on sk_send_head for retransmission, return clone to send now */
static struct sk_buff *gmtp_skb_entail(struct sock *sk, struct sk_buff *skb)
{
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
static int gmtp_transmit_skb(struct sock *sk, struct sk_buff *skb)
{
	if (likely(skb != NULL)) {

		gmtp_print_debug("skb (%p) != NULL", skb);

		struct inet_sock *inet = inet_sk(sk);
		const struct inet_connection_sock *icsk = inet_csk(sk);
		struct gmtp_sock *gp = gmtp_sk(sk);
		gmtp_print_debug("gmtp_sock *gp = %p", gp);

		struct gmtp_skb_cb *gcb = GMTP_SKB_CB(skb);
		gmtp_print_debug("gmtp_skb_cb *gcb  = %p", gcb);

		struct gmtp_hdr *gh;

		/* TODO Calculate size of header */
		const u32 gmtp_header_size = sizeof(*gh) +
//				sizeof(struct gmtp_hdr_ext) +
				gmtp_packet_hdr_len(gcb->gmtpd_type);

		int err, set_ack = 1;
		u64 ackno = gp->gmtps_gsr;

		/*
		 * Increment GSS here already in case the option code needs it.
		 * Update GSS for real only if option processing below succeeds.
		 */
		gcb->gmtpd_seq = gp->gmtps_gss + 1;

		switch (gcb->gmtpd_type) {
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
		//TODO Build GMTP header here!
		gh = gmtp_zeroed_hdr(skb, gmtp_header_size);
		gmtp_print_debug("gh = %p", gh);

		gh->type	= gcb->gmtpd_type;
		gh->sport	= inet->inet_sport;
		gh->dport	= inet->inet_dport;
		gh->hdrlen	= gmtp_header_size;

		//TODO set sequence numbers and ack...

		switch (gcb->gmtpd_type) {
		case GMTP_PKT_REQUEST:
			//TODO treat request
			break;
		}

		//TODO icsk_af_ops is null...
//		icsk->icsk_af_ops->send_check(sk, skb);
//		err = icsk->icsk_af_ops->queue_xmit(sk, skb, &inet->cork.fl);
//		return net_xmit_eval(err);


	}
	return -ENOBUFS;
}

/*
 * Do all connect socket setups that can be done AF independent.
 */
int gmtp_connect(struct sock *sk)
{
	//TODO Implement gmtp_connect
	gmtp_print_debug("gmtp_connect");

	struct sk_buff *skb;
	struct gmtp_sock *dp = gmtp_sk(sk);
	struct dst_entry *dst = __sk_dst_get(sk);
	struct inet_connection_sock *icsk = inet_csk(sk);

	sk->sk_err = 0;
	sock_reset_flag(sk, SOCK_DONE);

	//TODO Sync GMTP MSS
//	dccp_sync_mss(sk, dst_mtu(dst));

	//TODO Initialize variables...
	skb = alloc_skb(sk->sk_prot->max_header, sk->sk_allocation);
	gmtp_print_debug("skb == %p", skb);

	if (unlikely(skb == NULL))
	{
		gmtp_print_error("skb = alloc_skb(...) == NULL");
		return -ENOBUFS;
	}

	/* Reserve space for headers. */

	gmtp_print_debug("sk->sk_prot->max_header == %d", sk->sk_prot->max_header);
	skb_reserve(skb, sk->sk_prot->max_header);

	GMTP_SKB_CB(skb)->gmtpd_type = GMTP_PKT_REQUEST;

	gmtp_print_debug("Calling gmtp_transmit_skb(sk, gmtp_skb_entail(sk, skb))");
//	dccp_transmit_skb(sk, dccp_skb_entail(sk, skb));
	gmtp_transmit_skb(sk, gmtp_skb_entail(sk, skb));

	//TODO Retransmit?
//	icsk->icsk_retransmits = 0;
//	inet_csk_reset_xmit_timer(sk, ICSK_TIME_RETRANS,
//					  icsk->icsk_rto, DCCP_RTO_MAX);

	return 0;
}
EXPORT_SYMBOL_GPL(gmtp_connect);

