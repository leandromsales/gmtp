/*
 * minisocks.c
 *
 *  Created on: 21/01/2015
 *      Author: wendell
 */

#include <linux/gmtp.h>
#include <linux/kernel.h>

#include <net/sock.h>
#include <net/xfrm.h>
#include <net/inet_timewait_sock.h>

#include "gmtp.h"

struct sock *gmtp_create_openreq_child(struct sock *sk,
				       const struct request_sock *req,
				       const struct sk_buff *skb)
{
	/*
	 * Step 3: Process LISTEN state
	 *
	 *   (* Generate a new socket and switch to that socket *)
	 *   Set S := new socket for this port pair
	 */
	struct sock *newsk = inet_csk_clone_lock(sk, req, GFP_ATOMIC);

	gmtp_print_function();

	if (newsk != NULL) {
		struct gmtp_request_sock *dreq = gmtp_rsk(req);
		struct inet_connection_sock *newicsk = inet_csk(newsk);
		struct gmtp_sock *newdp = gmtp_sk(newsk);

		newdp->gmtps_role	    = GMTP_ROLE_SERVER;

		//TODO especifico do DCCP
//		INIT_LIST_HEAD(&newdp->dccps_featneg);
//		/*
//		 * Step 3: Process LISTEN state
//		 *
//		 *    Choose S.ISS (initial seqno) or set from Init Cookies
//		 *    Initialize S.GAR := S.ISS
//		 *    Set S.ISR, S.GSR from packet (or Init Cookies)
//		 *
//		 *    Setting AWL/AWH and SWL/SWH happens as part of the feature
//		 *    activation below, as these windows all depend on the local
//		 *    and remote Sequence Window feature values (7.5.2).
//		 */
//		newdp->dccps_iss = dreq->dreq_iss;
//		newdp->dccps_gss = dreq->dreq_gss;
//		newdp->dccps_gar = newdp->dccps_iss;
//		newdp->dccps_isr = dreq->dreq_isr;
//		newdp->dccps_gsr = dreq->dreq_gsr;
//
//		/*
//		 * Activate features: initialise CCIDs, sequence windows etc.
//		 */
//		if (dccp_feat_activate_values(newsk, &dreq->dreq_featneg)) {
//			/* It is still raw copy of parent, so invalidate
//			 * destructor and make plain sk_free() */
//			newsk->sk_destruct = NULL;
//			sk_free(newsk);
//			return NULL;
//		}
//		dccp_init_xmit_timers(newsk);
//
//		DCCP_INC_STATS_BH(DCCP_MIB_PASSIVEOPENS);
	}
	return newsk;
}
EXPORT_SYMBOL_GPL(gmtp_create_openreq_child);

/*
 * Process an incoming packet for RESPOND sockets represented
 * as an request_sock.
 */
struct sock *gmtp_check_req(struct sock *sk, struct sk_buff *skb,
			    struct request_sock *req,
			    struct request_sock **prev)
{
	struct sock *child = NULL;
	struct gmtp_request_sock *dreq = gmtp_rsk(req);

	gmtp_print_function();

	/* TODO Check for retransmitted REQUEST */
//	if (dccp_hdr(skb)->dccph_type == DCCP_PKT_REQUEST) {
//
//		if (after48(DCCP_SKB_CB(skb)->dccpd_seq, dreq->dreq_gsr)) {
//			dccp_pr_debug("Retransmitted REQUEST\n");
//			dreq->dreq_gsr = DCCP_SKB_CB(skb)->dccpd_seq;
//			/*
//			 * Send another RESPONSE packet
//			 * To protect against Request floods, increment retrans
//			 * counter (backoff, monitored by dccp_response_timer).
//			 */
//			inet_rtx_syn_ack(sk, req);
//		}
//		/* Network Duplicate, discard packet */
//		return NULL;
//	}

	GMTP_SKB_CB(skb)->gmtpd_reset_code = GMTP_RESET_CODE_PACKET_ERROR;

	if (gmtp_hdr(skb)->type != GMTP_PKT_ACK &&
	    gmtp_hdr(skb)->type != GMTP_PKT_DATAACK)
		goto drop;

	/* TODO Check for invalid ACK */
//	if (!between48(DCCP_SKB_CB(skb)->dccpd_ack_seq,
//				dreq->dreq_iss, dreq->dreq_gss)) {
//		dccp_pr_debug("Invalid ACK number: ack_seq=%llu, "
//			      "dreq_iss=%llu, dreq_gss=%llu\n",
//			      (unsigned long long)
//			      DCCP_SKB_CB(skb)->dccpd_ack_seq,
//			      (unsigned long long) dreq->dreq_iss,
//			      (unsigned long long) dreq->dreq_gss);
//		goto drop;
//	}

	if (gmtp_parse_options(sk, dreq, skb))
		 goto drop;

	child = inet_csk(sk)->icsk_af_ops->syn_recv_sock(sk, skb, req, NULL);
	if (child == NULL)
		goto listen_overflow;

	inet_csk_reqsk_queue_unlink(sk, req, prev);
	inet_csk_reqsk_queue_removed(sk, req);
	inet_csk_reqsk_queue_add(sk, req, child);
out:
	return child;
listen_overflow:
	gmtp_print_error("listen_overflow!\n");
	GMTP_SKB_CB(skb)->gmtpd_reset_code = GMTP_RESET_CODE_TOO_BUSY;
drop:
	if (gmtp_hdr(skb)->type != GMTP_PKT_RESET)
		req->rsk_ops->send_reset(sk, skb);

	inet_csk_reqsk_queue_drop(sk, req, prev);
	goto out;
}
EXPORT_SYMBOL_GPL(gmtp_check_req);

/*
 *  Queue segment on the new socket if the new socket is active,
 *  otherwise we just shortcircuit this and continue with
 *  the new socket.
 */
int gmtp_child_process(struct sock *parent, struct sock *child,
		       struct sk_buff *skb)
{
	int ret = 0;
	const int state = child->sk_state;

	if (!sock_owned_by_user(child)) {
		ret = gmtp_rcv_state_process(child, skb, gmtp_hdr(skb),
					     skb->len);

		/* Wakeup parent, send SIGIO */
		if (state == GMTP_RESPOND && child->sk_state != state)
			parent->sk_data_ready(parent);
	} else {
		/* Alas, it is possible again, because we do lookup
		 * in main socket hash table and lock on listening
		 * socket does not protect us more.
		 */
		__sk_add_backlog(child, skb);
	}

	bh_unlock_sock(child);
	sock_put(child);
	return ret;
}

EXPORT_SYMBOL_GPL(gmtp_child_process);

void gmtp_reqsk_send_ack(struct sock *sk, struct sk_buff *skb,
			 struct request_sock *rsk)
{
	gmtp_print_error("DCCP-ACK packets are never sent in LISTEN/RESPOND state");
}
EXPORT_SYMBOL_GPL(gmtp_reqsk_send_ack);

int gmtp_reqsk_init(struct request_sock *req,
		    struct gmtp_sock const *gp, struct sk_buff const *skb)
{
	struct gmtp_request_sock *greq = gmtp_rsk(req);

	gmtp_print_function();

	inet_rsk(req)->ir_rmt_port = gmtp_hdr(skb)->sport;
	inet_rsk(req)->ir_num	   = ntohs(gmtp_hdr(skb)->dport);
	inet_rsk(req)->acked	   = 0;
//	dreq->dreq_timestamp_echo  = 0;

	return 0;
}
EXPORT_SYMBOL_GPL(gmtp_reqsk_init);

