// SPDX-License-Identifier: GPL-2.0-or-later
/*
 *  net/gmtp/minisocks.c
 *
 *  An implementation of the GMTP protocol
 *  Wendell Silva Soares <wendell@ic.ufal.br>
 */
#include <linux/gfp.h>
#include <linux/gmtp.h>
#include <linux/kernel.h>
#include <linux/skbuff.h>
#include <linux/timer.h>

#include <net/sock.h>
#include <net/xfrm.h>
#include <net/inet_timewait_sock.h>

#include "gmtp.h"

struct inet_timewait_death_row gmtp_death_row = {
    .sysctl_max_tw_buckets = NR_FILE * 2,
    .hashinfo   = &gmtp_inet_hashinfo,
};
EXPORT_SYMBOL_GPL(gmtp_death_row);

void gmtp_time_wait(struct sock *sk, int state, int timeo)
{
    struct inet_timewait_sock *tw = NULL;

    gmtp_print_function();

    /*print_gmtp_sock(sk);

    tw = inet_twsk_alloc(sk, &gmtp_death_row, state);

    if(tw != NULL) {
        const struct inet_connection_sock *icsk = inet_csk(sk);
        const int rto = (icsk->icsk_rto << 2) - (icsk->icsk_rto >> 1);
         Linkage updates.
        __inet_twsk_hashdance(tw, sk, &gmtp_inet_hashinfo);

         Get the TIME_WAIT timeout firing.
        if(timeo < rto)
            timeo = rto;

        tw->tw_timeout = GMTP_TIMEWAIT_LEN;
        if(state == GMTP_TIME_WAIT)
            timeo = GMTP_TIMEWAIT_LEN;

        inet_twsk_schedule(tw, timeo);
        inet_twsk_put(tw);
    } else {
         If we're out of memory, just CLOSE this
         * socket up.


        gmtp_print_warning("time wait bucket table overflow!");
    }*/

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

    gmtp_pr_func();

    if (newsk != NULL) {
        struct gmtp_request_sock *dreq = gmtp_rsk(req);
        struct inet_connection_sock *newicsk = inet_csk(newsk);
        struct gmtp_sock *newdp = gmtp_sk(newsk);

        newdp->role     = GMTP_ROLE_SERVER;
        memcpy(newdp->flowname, dreq->flowname, GMTP_FLOWNAME_LEN);
        newicsk->icsk_rto   = GMTP_TIMEOUT_INIT;

        /* Step 3: Process LISTEN state
         *
         *    Choose S.ISS (initial seqno)
         *    Set S.ISR, S.GSR from packet
         */
        newdp->iss = dreq->iss;
        newdp->gss = dreq->gss;
        newdp->isr = dreq->isr;
        newdp->gsr = dreq->gsr;


        gmtp_init_xmit_timers(newsk);
    }
    return newsk;
}
EXPORT_SYMBOL_GPL(gmtp_create_openreq_child);

/*
 * Process an incoming packet for RESPOND sockets represented
 * as an request_sock.
 */
struct sock *gmtp_check_req(struct sock *sk, struct sk_buff *skb,
                struct request_sock *req)
{
    struct sock *child = NULL;
    struct gmtp_request_sock *greq = gmtp_rsk(req);
    __be32 seq;
    bool own_req;

    gmtp_pr_func();

	/* TCP/DCCP/GMTP listeners became lockless.
	 * GMTP stores complex state in its request_sock, so we need
	 * a protection for them, now this code runs without being protected
	 * by the parent (listener) lock.
	 */
    /* FIXME spin_lock_bh(&greq->lock) causes kernel panic */
	/*spin_lock_bh(&greq->lock);*/

    if (gmtp_hdr(skb)->type == GMTP_PKT_REQUEST ||
    		gmtp_hdr(skb)->type == GMTP_PKT_REGISTER) {

        if(GMTP_SKB_CB(skb)->seq > greq->gsr) {

            gmtp_pr_debug("seq: %u > gsr: %llu",
            		GMTP_SKB_CB(skb)->seq, greq->gsr);

            greq->gsr = GMTP_SKB_CB(skb)->seq;

            /* Send another Register-Reply packet
             * To protect against Request floods, increment retrans
             * counter (backoff, monitored by gmtp_response_timer).
             */

            inet_rtx_syn_ack(sk, req);
         }
        /* Network Duplicate, discard packet */
        return NULL;
    }

    GMTP_SKB_CB(skb)->reset_code = GMTP_RESET_CODE_PACKET_ERROR;

    if (gmtp_hdr(skb)->type != GMTP_PKT_ROUTE_NOTIFY &&
        gmtp_hdr(skb)->type != GMTP_PKT_ACK &&
        gmtp_hdr(skb)->type != GMTP_PKT_DATAACK)
        goto drop;

    /* FIXME Check for invalid Sequence nuber */
    seq = GMTP_SKB_CB(skb)->seq;
    if(!(seq >= greq->iss && seq <= greq->gss)) {
        gmtp_print_debug("Invalid Seq number: "
                "seq=%llu, iss=%llu, gss=%llu",
                (unsigned long long ) seq,
                (unsigned long long ) greq->iss,
                (unsigned long long ) greq->gss);
        print_gmtp_packet(ip_hdr(skb), gmtp_hdr(skb));
        goto drop;
    }

	child = inet_csk(sk)->icsk_af_ops->syn_recv_sock(sk, skb, req, NULL, req,
			&own_req);

	if (child) {
		gmtp_pr_info("child is OK before hashdance");
		child = inet_csk_complete_hashdance(sk, child, req, own_req);
		goto out;
	}
	gmtp_pr_info("child is NULL before hashdance");

    GMTP_SKB_CB(skb)->reset_code = GMTP_RESET_CODE_TOO_BUSY;
drop:
	if (gmtp_hdr(skb)->type != GMTP_PKT_RESET)
			req->rsk_ops->send_reset(sk, skb);

    inet_csk_reqsk_queue_drop(sk, req);
out:
	/*spin_unlock_bh(&greq->lock);*/

	if(child)
		gmtp_pr_info("child is OK after all");
	else
		gmtp_pr_info("child is NULL after all");

	return child;
}
EXPORT_SYMBOL_GPL(gmtp_check_req);

/*
 *  Queue segment on the new socket if the new socket is active,
 *  otherwise we just shortcircuit this and continue with
 *  the new socket.
 */
int gmtp_child_process(struct sock *parent, struct sock *child,
               struct sk_buff *skb)
__releases(child)
{
    int ret = 0;
    const int state = child->sk_state;

    gmtp_pr_func();

    if (!sock_owned_by_user(child)) {
        ret = gmtp_rcv_state_process(child, skb, gmtp_hdr(skb),
                         skb->len);

        /* Wakeup parent, send SIGIO */
        if (state == GMTP_REQUEST_RECV && child->sk_state != state)
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

void gmtp_reqsk_send_ack(const struct sock *sk, struct sk_buff *skb,
             struct request_sock *rsk)
{
    gmtp_print_error("GMTP-ACK packets are never sent in LISTEN state");
}
EXPORT_SYMBOL_GPL(gmtp_reqsk_send_ack);

int gmtp_reqsk_init(struct request_sock *req,
            struct gmtp_sock const *gp, struct sk_buff const *skb)
{
    gmtp_print_function();

    inet_rsk(req)->ir_rmt_port = gmtp_hdr(skb)->sport;
    inet_rsk(req)->ir_num      = ntohs(gmtp_hdr(skb)->dport);
    inet_rsk(req)->acked       = 0;

    return 0;
}
EXPORT_SYMBOL_GPL(gmtp_reqsk_init);

