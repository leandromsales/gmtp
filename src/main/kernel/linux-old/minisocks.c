/*
 *  net/gmtp/minisocks.c
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
#include <linux/gfp.h>
#include <linux/kernel.h>
#include <linux/skbuff.h>
#include <linux/timer.h>

#include <net/sock.h>
#include <net/xfrm.h>
#include <net/inet_timewait_sock.h>

#include "ackvec.h"
#include "ccid.h"
#include "gmtp.h"
#include "feat.h"

struct inet_timewait_death_row gmtp_death_row = {
	.sysctl_max_tw_buckets = NR_FILE * 2,
	.period		= GMTP_TIMEWAIT_LEN / INET_TWDR_TWKILL_SLOTS,
	.death_lock	= __SPIN_LOCK_UNLOCKED(gmtp_death_row.death_lock),
	.hashinfo	= &gmtp_hashinfo,
	.tw_timer	= TIMER_INITIALIZER(inet_twdr_hangman, 0,
					    (unsigned long)&gmtp_death_row),
	.twkill_work	= __WORK_INITIALIZER(gmtp_death_row.twkill_work,
					     inet_twdr_twkill_work),
/* Short-time timewait calendar */

	.twcal_hand	= -1,
	.twcal_timer	= TIMER_INITIALIZER(inet_twdr_twcal_tick, 0,
					    (unsigned long)&gmtp_death_row),
};

EXPORT_SYMBOL_GPL(gmtp_death_row);

void gmtp_time_wait(struct sock *sk, int state, int timeo)
{
	struct inet_timewait_sock *tw = NULL;

	if (gmtp_death_row.tw_count < gmtp_death_row.sysctl_max_tw_buckets)
		tw = inet_twsk_alloc(sk, state);

	if (tw != NULL) {
		const struct inet_connection_sock *icsk = inet_csk(sk);
		const int rto = (icsk->icsk_rto << 2) - (icsk->icsk_rto >> 1);
#if IS_ENABLED(CONFIG_IPV6)
		if (tw->tw_family == PF_INET6) {
			const struct ipv6_pinfo *np = inet6_sk(sk);
			struct inet6_timewait_sock *tw6;

			tw->tw_ipv6_offset = inet6_tw_offset(sk->sk_prot);
			tw6 = inet6_twsk((struct sock *)tw);
			tw6->tw_v6_daddr = np->daddr;
			tw6->tw_v6_rcv_saddr = np->rcv_saddr;
			tw->tw_ipv6only = np->ipv6only;
		}
#endif
		/* Linkage updates. */
		__inet_twsk_hashdance(tw, sk, &gmtp_hashinfo);

		/* Get the TIME_WAIT timeout firing. */
		if (timeo < rto)
			timeo = rto;

		tw->tw_timeout = GMTP_TIMEWAIT_LEN;
		if (state == GMTP_TIME_WAIT)
			timeo = GMTP_TIMEWAIT_LEN;

		inet_twsk_schedule(tw, &gmtp_death_row, timeo,
				   GMTP_TIMEWAIT_LEN);
		inet_twsk_put(tw);
	} else {
		/* Sorry, if we're out of memory, just CLOSE this
		 * socket up.  We've got bigger problems than
		 * non-graceful socket closings.
		 */
		GMTP_WARN("time wait bucket table overflow\n");
	}

	gmtp_done(sk);
}

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

	if (newsk != NULL) {
		struct gmtp_request_sock *dreq = gmtp_rsk(req);
		struct inet_connection_sock *newicsk = inet_csk(newsk);
		struct gmtp_sock *newdp = gmtp_sk(newsk);

		newdp->gmtps_role	    = GMTP_ROLE_SERVER;
		newdp->gmtps_hc_rx_ackvec   = NULL;
		newdp->gmtps_service_list   = NULL;
		newdp->gmtps_service	    = dreq->dreq_service;
		newdp->gmtps_timestamp_echo = dreq->dreq_timestamp_echo;
		newdp->gmtps_timestamp_time = dreq->dreq_timestamp_time;
		newicsk->icsk_rto	    = GMTP_TIMEOUT_INIT;

		INIT_LIST_HEAD(&newdp->gmtps_featneg);
		/*
		 * Step 3: Process LISTEN state
		 *
		 *    Choose S.ISS (initial seqno) or set from Init Cookies
		 *    Initialize S.GAR := S.ISS
		 *    Set S.ISR, S.GSR from packet (or Init Cookies)
		 *
		 *    Setting AWL/AWH and SWL/SWH happens as part of the feature
		 *    activation below, as these windows all depend on the local
		 *    and remote Sequence Window feature values (7.5.2).
		 */
		newdp->gmtps_iss = dreq->dreq_iss;
		newdp->gmtps_gss = dreq->dreq_gss;
		newdp->gmtps_gar = newdp->gmtps_iss;
		newdp->gmtps_isr = dreq->dreq_isr;
		newdp->gmtps_gsr = dreq->dreq_gsr;

		/*
		 * Activate features: initialise CCIDs, sequence windows etc.
		 */
		if (gmtp_feat_activate_values(newsk, &dreq->dreq_featneg)) {
			/* It is still raw copy of parent, so invalidate
			 * destructor and make plain sk_free() */
			newsk->sk_destruct = NULL;
			sk_free(newsk);
			return NULL;
		}
		gmtp_init_xmit_timers(newsk);

		GMTP_INC_STATS_BH(GMTP_MIB_PASSIVEOPENS);
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

	/* Check for retransmitted REQUEST */
	if (gmtp_hdr(skb)->gmtph_type == GMTP_PKT_REQUEST) {

		if (after48(GMTP_SKB_CB(skb)->gmtpd_seq, dreq->dreq_gsr)) {
			gmtp_pr_debug("Retransmitted REQUEST\n");
			dreq->dreq_gsr = GMTP_SKB_CB(skb)->gmtpd_seq;
			/*
			 * Send another RESPONSE packet
			 * To protect against Request floods, increment retrans
			 * counter (backoff, monitored by gmtp_response_timer).
			 */
			req->retrans++;
			req->rsk_ops->rtx_syn_ack(sk, req, NULL);
		}
		/* Network Duplicate, discard packet */
		return NULL;
	}

	GMTP_SKB_CB(skb)->gmtpd_reset_code = GMTP_RESET_CODE_PACKET_ERROR;

	if (gmtp_hdr(skb)->gmtph_type != GMTP_PKT_ACK &&
	    gmtp_hdr(skb)->gmtph_type != GMTP_PKT_DATAACK)
		goto drop;

	/* Invalid ACK */
	if (!between48(GMTP_SKB_CB(skb)->gmtpd_ack_seq,
				dreq->dreq_iss, dreq->dreq_gss)) {
		gmtp_pr_debug("Invalid ACK number: ack_seq=%llu, "
			      "dreq_iss=%llu, dreq_gss=%llu\n",
			      (unsigned long long)
			      GMTP_SKB_CB(skb)->gmtpd_ack_seq,
			      (unsigned long long) dreq->dreq_iss,
			      (unsigned long long) dreq->dreq_gss);
		goto drop;
	}

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
	gmtp_pr_debug("listen_overflow!\n");
	GMTP_SKB_CB(skb)->gmtpd_reset_code = GMTP_RESET_CODE_TOO_BUSY;
drop:
	if (gmtp_hdr(skb)->gmtph_type != GMTP_PKT_RESET)
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
			parent->sk_data_ready(parent, 0);
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
	GMTP_BUG("GMTP-ACK packets are never sent in LISTEN/RESPOND state");
}

EXPORT_SYMBOL_GPL(gmtp_reqsk_send_ack);

int gmtp_reqsk_init(struct request_sock *req,
		    struct gmtp_sock const *dp, struct sk_buff const *skb)
{
	struct gmtp_request_sock *dreq = gmtp_rsk(req);

	inet_rsk(req)->rmt_port	  = gmtp_hdr(skb)->gmtph_sport;
	inet_rsk(req)->loc_port	  = gmtp_hdr(skb)->gmtph_dport;
	inet_rsk(req)->acked	  = 0;
	dreq->dreq_timestamp_echo = 0;

	/* inherit feature negotiation options from listening socket */
	return gmtp_feat_clone_list(&dp->gmtps_featneg, &dreq->dreq_featneg);
}

EXPORT_SYMBOL_GPL(gmtp_reqsk_init);
