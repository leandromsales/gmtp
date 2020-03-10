// SPDX-License-Identifier: GPL-2.0-or-later
/*
 *  net/gmtp/output.c
 *
 *  An implementation of the GMTP protocol
 *  Wendell Silva Soares <wendell@ic.ufal.br>
 */
#include <linux/gmtp.h>
#include <linux/if_ether.h>
#include <linux/kernel.h>
#include <linux/skbuff.h>
#include <linux/slab.h>
#include <linux/types.h>

#include <net/inet_sock.h>
#include <net/sock.h>
#include <net/tcp.h>

#include <uapi/linux/gmtp.h>

#include "gmtp.h"
#include "mcc.h"

static inline void gmtp_event_ack_sent(struct sock *sk)
{
    gmtp_sk(sk)->ack_tx_tstamp = jiffies_to_msecs(jiffies);
    inet_csk_clear_xmit_timer(sk, ICSK_TIME_DACK);
}

/* enqueue @skb on sk_send_head for retransmission, return clone to send now */
static struct sk_buff *gmtp_skb_entail(struct sock *sk, struct sk_buff *skb) {
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
static int gmtp_transmit_skb(struct sock *sk, struct sk_buff *skb) {

    if (likely(skb != NULL)) {

        struct inet_sock *inet = inet_sk(sk);
        const struct inet_connection_sock *icsk = inet_csk(sk);
        struct gmtp_sock *gp = gmtp_sk(sk);
        struct gmtp_skb_cb *gcb = GMTP_SKB_CB(skb);
        struct gmtp_hdr *gh;
        const u32 gmtp_header_size = sizeof(struct gmtp_hdr) +
                gmtp_packet_hdr_variable_len(gcb->type);
        int err, set_ack = 1;

        gp->ndp_sent++;

        switch (gcb->type) {
        case GMTP_PKT_DATA:
            set_ack = 0;
            gp->ndp_sent--;
            /* fall through */
        case GMTP_PKT_DATAACK:
        case GMTP_PKT_RESET:
            break;
        case GMTP_PKT_REQUEST:
            set_ack = 0;
            GMTP_SKB_CB(skb)->retransmits = icsk->icsk_retransmits;
            /* Use ISS on the first (non-retransmitted) Request. */
            if (icsk->icsk_retransmits == 0)
                gcb->seq = gp->iss;
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

        gh = gmtp_zeroed_hdr(skb, gmtp_header_size);

        gh->version = GMTP_VERSION;
        gh->type = gcb->type;
        gh->sport = inet->inet_sport;
        gh->dport = inet->inet_dport;
        gh->hdrlen = gmtp_header_size;
        gh->server_rtt = gp->role == GMTP_ROLE_SERVER ?
                TO_U12(gp->tx_avg_rtt) : TO_U12(gp->rx_rtt);

        memcpy(gh->flowname, gp->flowname, GMTP_FLOWNAME_LEN);

        gh->transm_r = (__be32) gp->tx_media_rate;
        if (gcb->type == GMTP_PKT_RESET)
            gmtp_hdr_reset(skb)->reset_code = gcb->reset_code;

        gcb->seq = ++gp->gss;
        if (set_ack) {
            gcb->seq = --gp->gss;
            gmtp_event_ack_sent(sk);
        }
        gh->seq = gcb->seq;

        /* Specific headers */
        switch(gcb->type) {
        case GMTP_PKT_DATA: {
            struct gmtp_hdr_data *gh_data = gmtp_hdr_data(skb);
            gh_data->tstamp = jiffies_to_msecs(jiffies);
            /*pr_info("Server RTT: %u ms\n", gh->server_rtt);*/
            break;
        }
        case GMTP_PKT_FEEDBACK: {
            struct gmtp_hdr_feedback *fh = gmtp_hdr_feedback(skb);
            gh->seq = gcb->seq = gp->gsr;
            gh->transm_r = gp->rx_max_rate;
            fh->orig_tstamp = gcb->orig_tstamp;
            /*fh->nclients = gp->myself->nclients;*/
            break;
        }
        case GMTP_PKT_ELECT_REQUEST: {
            struct gmtp_hdr_elect_request *gh_ereq;
            gh_ereq = gmtp_hdr_elect_request(skb);
            memcpy(gh_ereq->relay_id, gp->relay_id,
            GMTP_RELAY_ID_LEN);
            gh_ereq->max_nclients = 0;
            break;
        }
        case GMTP_PKT_ELECT_RESPONSE: {
            struct gmtp_hdr_elect_response *gh_eresp;
            gh_eresp = gmtp_hdr_elect_response(skb);
            gh_eresp->elect_code = GMTP_SKB_CB(skb)->elect_code;
            break;
        }
        }

        err = icsk->icsk_af_ops->queue_xmit(sk, skb, &inet->cork.fl);
        return net_xmit_eval(err);
    }
    return -ENOBUFS;
}

/*
 * All SKB's seen here are completely header full.
 * Here, we don't build the GMTP header. We only pass the packet down to
 * IP so it can do the same plus pass the packet off to the device.
 */
int gmtp_transmit_built_skb(struct sock *sk, struct sk_buff *skb) {

    if (likely(skb != NULL)) {

        const struct inet_connection_sock *icsk = inet_csk(sk);
        struct inet_sock *inet = inet_sk(sk);
        int err;

        if(likely(gmtp_hdr(skb)->type != GMTP_PKT_DATA))
            gmtp_sk(sk)->ndp_sent++;

        err = icsk->icsk_af_ops->queue_xmit(sk, skb, &inet->cork.fl);
        return net_xmit_eval(err);
    }
    return -ENOBUFS;
}
EXPORT_SYMBOL_GPL(gmtp_transmit_built_skb);

unsigned int gmtp_sync_mss(struct sock *sk, u32 pmtu)
{
    struct inet_connection_sock *icsk = inet_csk(sk);
    struct gmtp_sock *gp = gmtp_sk(sk);
    u32 cur_mps = pmtu;

    /* Account for header lengths and IPv4/v6 option overhead */
    /* FIXME Account variable part of GMTP Header */
    cur_mps -= (icsk->icsk_af_ops->net_header_len + icsk->icsk_ext_hdr_len +
            sizeof(struct gmtp_hdr));

    /* And store cached results */
    icsk->icsk_pmtu_cookie = pmtu;
    gp->mss = cur_mps;
    gmtp_pr_info("MSS: %u\n", gp->mss);

    return cur_mps;
}
EXPORT_SYMBOL_GPL(gmtp_sync_mss);

void gmtp_write_space(struct sock *sk)
{
    struct socket_wq *wq;

    rcu_read_lock();
    wq = rcu_dereference(sk->sk_wq);
    if (wq_has_sleeper(&wq->wait))
        wake_up_interruptible(&wq->wait);
    /* Should agree with poll, otherwise some programs break */
    if (sock_writeable(sk))
        sk_wake_async(sk, SOCK_WAKE_SPACE, POLL_OUT);

    rcu_read_unlock();
}

/**
 * gmtp_retransmit_skb  -  Retransmit Request, Close, or CloseReq packets
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

struct sk_buff *gmtp_make_register_reply(struct sock *sk, struct dst_entry *dst,
                   struct request_sock *req)
{
    struct gmtp_hdr *gh;
    struct gmtp_request_sock *greq;

    const u32 gmtp_header_size = sizeof(struct gmtp_hdr) +
            gmtp_packet_hdr_variable_len(GMTP_PKT_REGISTER_REPLY);

    struct sk_buff *skb = sock_wmalloc(sk, sk->sk_prot->max_header, 1,
            GFP_ATOMIC);

    gmtp_print_function();

    if (skb == NULL)
        return NULL;

    /* Reserve space for headers. */
    skb_reserve(skb, sk->sk_prot->max_header);

    skb_dst_set(skb, dst_clone(dst));

    greq = gmtp_rsk(req);
    if (inet_rsk(req)->acked)   /* increase GSS upon retransmission */
        greq->gss++;
    GMTP_SKB_CB(skb)->type = GMTP_PKT_REGISTER_REPLY;
    GMTP_SKB_CB(skb)->seq  = greq->gss;

    /* Build header */
    gh = gmtp_zeroed_hdr(skb, gmtp_header_size);

    /* At this point, Register-Reply specific header is zeroed. */
    gh->sport   = htons(inet_rsk(req)->ir_num);
    gh->dport   = inet_rsk(req)->ir_rmt_port;
    gh->type    = GMTP_PKT_REGISTER_REPLY;
    gh->seq     = greq->gss;
    gh->server_rtt  = GMTP_DEFAULT_RTT;
    gh->transm_r    = (__be32) gmtp_sk(sk)->tx_media_rate;
    gh->hdrlen  = gmtp_header_size;
    memcpy(gh->flowname, greq->flowname, GMTP_FLOWNAME_LEN);

    gmtp_hdr_register_reply(skb)->ucc_type = greq->tx_ucc_type;

    /* We use `acked' to remember that a Register-Reply was already sent. */
    inet_rsk(req)->acked = 1;

    return skb;
}
EXPORT_SYMBOL_GPL(gmtp_make_register_reply);

struct sk_buff *gmtp_make_register_reply_open(struct sock *sk,
        struct sk_buff *rcv_skb)
{
    struct sk_buff *skb;
    struct gmtp_hdr *rxgh = gmtp_hdr(rcv_skb), *gh;
    const u32 gmtp_hdr_len = sizeof(struct gmtp_hdr) +
            gmtp_packet_hdr_variable_len(GMTP_PKT_REGISTER_REPLY);

    gmtp_print_function();

    skb = alloc_skb(sk->sk_prot->max_header, GFP_ATOMIC);
    if(skb == NULL)
        return NULL;

    skb_reserve(skb, sk->sk_prot->max_header);

    gh = gmtp_zeroed_hdr(skb, gmtp_hdr_len);

    gh->type = GMTP_PKT_REGISTER_REPLY;
    gh->seq = rxgh->seq;
    gh->sport = rxgh->dport;
    gh->dport = rxgh->sport;
    gh->hdrlen = gmtp_hdr_len;
    gh->server_rtt = rxgh->server_rtt;
    gh->transm_r = rxgh->transm_r;
    memcpy(gh->flowname, rxgh->flowname, GMTP_FLOWNAME_LEN);

    gmtp_hdr_register_reply(skb)->ucc_type = gmtp_sk(sk)->tx_ucc_type;

    return skb;
}
EXPORT_SYMBOL_GPL(gmtp_make_register_reply_open);


struct sk_buff *gmtp_make_route_notify(struct sock *sk,
        struct sk_buff *rcv_skb)
{
    struct sk_buff *skb;
    struct gmtp_hdr *rxgh = gmtp_hdr(rcv_skb), *gh;

    gmtp_print_function();

    skb = alloc_skb(sk->sk_prot->max_header, GFP_ATOMIC);
    if(skb == NULL)
        return NULL;

    skb_reserve(skb, sk->sk_prot->max_header);

    gh = gmtp_zeroed_hdr(skb, rxgh->hdrlen);

    memcpy(gh, rxgh, rxgh->hdrlen);
    gh->type = GMTP_PKT_ROUTE_NOTIFY;
    swap(gh->sport, gh->dport);
    gh->relay = 0;

    return skb;
}

void gmtp_send_route_notify(struct sock *sk,
        struct sk_buff *rcv_skb)
{
    struct sk_buff *skb = gmtp_make_route_notify(sk, rcv_skb);

    gmtp_pr_func();
    if(skb != NULL)
        return gmtp_transmit_built_skb(sk, skb);
}
EXPORT_SYMBOL_GPL(gmtp_send_route_notify);

struct sk_buff *gmtp_make_delegate(struct sock *sk, struct sk_buff *rcv_skb,
        __u8 *rid)
{
    struct sk_buff *skb;
    struct gmtp_hdr *rxgh = gmtp_hdr(rcv_skb), *gh;
    const u32 gmtp_hdr_len = sizeof(struct gmtp_hdr)
            + gmtp_packet_hdr_variable_len(GMTP_PKT_DELEGATE);

    gmtp_print_function();

    skb = alloc_skb(sk->sk_prot->max_header, GFP_ATOMIC);
    if(skb == NULL)
        return NULL;

    skb_reserve(skb, sk->sk_prot->max_header);

    gh = gmtp_zeroed_hdr(skb, gmtp_hdr_len);

    gh->type = GMTP_PKT_DELEGATE;
    gh->seq = rxgh->seq;
    gh->sport = rxgh->dport;
    gh->dport = sk->sk_dport;
    gh->hdrlen = gmtp_hdr_len;
    gh->server_rtt = gmtp_sk(sk)->tx_avg_rtt;
    gh->transm_r = rxgh->transm_r;
    memcpy(gh->flowname, rxgh->flowname, GMTP_FLOWNAME_LEN);

    memcpy(gmtp_hdr_delegate(skb)->relay.relay_id, rid, GMTP_RELAY_ID_LEN);
    gmtp_hdr_delegate(skb)->relay.relay_ip = ip_hdr(rcv_skb)->saddr;
    gmtp_hdr_delegate(skb)->relay_port = rxgh->sport;

    return skb;
}
EXPORT_SYMBOL_GPL(gmtp_make_delegate);

/* answer offending packet in @rcv_skb with Reset from control socket @ctl */
struct sk_buff *gmtp_ctl_make_reset(struct sock *sk, struct sk_buff *rcv_skb)
{
    struct gmtp_hdr *rxgh = gmtp_hdr(rcv_skb), *gh;
    struct gmtp_skb_cb *gcb = GMTP_SKB_CB(rcv_skb);
    const u32 gmtp_hdr_len = sizeof(struct gmtp_hdr) +
            sizeof(struct gmtp_hdr_reset);

    struct gmtp_hdr_reset *ghr;
    struct sk_buff *skb;

    gmtp_pr_func();

    skb = alloc_skb(sk->sk_prot->max_header, GFP_ATOMIC);
    if (skb == NULL)
        return NULL;

    skb_reserve(skb, sk->sk_prot->max_header);

    /* Swap the send and the receive. */
    gh = gmtp_zeroed_hdr(skb, gmtp_hdr_len);
    gh->type    = GMTP_PKT_RESET;
    gh->sport   = rxgh->dport;
    gh->dport   = rxgh->sport;
    gh->hdrlen  = gmtp_hdr_len;
    gh->seq     = rxgh->seq;
    gh->server_rtt  = rxgh->server_rtt;
    gh->transm_r    = rxgh->transm_r;
    memcpy(gh->flowname, rxgh->flowname, GMTP_FLOWNAME_LEN);

    ghr = gmtp_hdr_reset(skb);
    ghr->reset_code = gcb->reset_code;

    switch (gcb->reset_code) {
    case GMTP_RESET_CODE_PACKET_ERROR:
        ghr->reset_data[0] = rxgh->type;
        break;
    case GMTP_RESET_CODE_MANDATORY_ERROR:
        memcpy(ghr->reset_data, gcb->reset_data, 3);
        break;
    default:
        break;
    }

    return skb;
}
EXPORT_SYMBOL_GPL(gmtp_ctl_make_reset);

int gmtp_send_reset(struct sock *sk, enum gmtp_reset_codes code)
{
    struct sk_buff *skb;
    int err = inet_csk(sk)->icsk_af_ops->rebuild_header(sk);

    gmtp_print_function();

    if (err != 0)
        return err;

    skb = sock_wmalloc(sk, sk->sk_prot->max_header, 1, GFP_ATOMIC);
    if (skb == NULL)
        return -ENOBUFS;

    /* Reserve space for headers and prepare control bits. */
    skb_reserve(skb, sk->sk_prot->max_header);
    GMTP_SKB_CB(skb)->type = GMTP_PKT_RESET;
    GMTP_SKB_CB(skb)->reset_code = code;

    return gmtp_transmit_skb(sk, skb);
}

/*
 * Do all connect socket setups that can be done AF independent.
 */
int gmtp_connect(struct sock *sk)
{
    struct sk_buff *skb;
    struct gmtp_sock *gp = gmtp_sk(sk);
    struct dst_entry *dst = __sk_dst_get(sk);
    struct inet_sock *inet = inet_sk(sk);
    struct inet_connection_sock *icsk = inet_csk(sk);
    struct gmtp_client_entry *client_entry;
    int err = 0;

    gmtp_pr_func();

    sk->sk_err = 0;
    sock_reset_flag(sk, SOCK_DONE);

    gmtp_sync_mss(sk, dst_mtu(dst));

    skb = alloc_skb(sk->sk_prot->max_header, sk->sk_allocation);
    if (unlikely(skb == NULL))
        return -ENOBUFS;

    gmtp_pr_info("skb created");

    /* Reserve space for headers. */
    skb_reserve(skb, sk->sk_prot->max_header);
    gmtp_pr_info("gmtp header reserved");
    GMTP_SKB_CB(skb)->type = GMTP_PKT_REQUEST;

    gmtp_pr_info("TODO: Call gmtp_lookup_client");
    /*client_entry = gmtp_lookup_client(client_hashtable, gp->flowname);
    if(client_entry == NULL)
        err = gmtp_add_client_entry(client_hashtable, gp->flowname,
                inet->inet_saddr, inet->inet_sport, 0, 0);

    client_entry = gmtp_lookup_client(client_hashtable, gp->flowname);
    if(err != 0 || client_entry == NULL) {
        return -ENOBUFS;
    }

    gp->myself = gmtp_list_add_client(0, inet->inet_saddr, inet->inet_sport,
            0, &client_entry->clients->list);

    if(gp->myself == NULL)
        return -ENOBUFS;*/

    /** First transmission: gss <- iss */
    gp->gss = gp->iss;
    gp->req_stamp = jiffies_to_msecs(jiffies);
    gp->ack_rx_tstamp = jiffies_to_msecs(jiffies);

    gmtp_pr_info("TODO: Add link to myself");
    /*
    gp->myself->ack_rx_tstamp = gp->ack_rx_tstamp;
    gp->myself->mysock = sk;*/

    gmtp_pr_info("Transmitting packet...");
    gmtp_transmit_skb(sk, gmtp_skb_entail(sk, skb));

    icsk->icsk_retransmits = 0;
    gmtp_pr_info("Starting xmit_timer...");
    inet_csk_reset_xmit_timer(sk, ICSK_TIME_RETRANS,
                      icsk->icsk_rto, GMTP_RTO_MAX);
    return 0;
}
EXPORT_SYMBOL_GPL(gmtp_connect);

void gmtp_send_ack(struct sock *sk)
{
    gmtp_pr_func();

    /* If we have been reset, we may not send again. */
    if(sk->sk_state != GMTP_CLOSED) {

        struct sk_buff *skb = alloc_skb(sk->sk_prot->max_header,
        GFP_ATOMIC);

        /* FIXME How to define the ackcode in this case? */
        if(skb == NULL) {
            inet_csk_schedule_ack(sk);
            inet_csk(sk)->icsk_ack.ato = TCP_ATO_MIN;
            inet_csk_reset_xmit_timer(sk, ICSK_TIME_DACK,
                    TCP_DELACK_MAX, GMTP_RTO_MAX);
            return;
        }

        /* Reserve space for headers */
        skb_reserve(skb, sk->sk_prot->max_header);
        GMTP_SKB_CB(skb)->type = GMTP_PKT_ACK;
        gmtp_transmit_skb(sk, skb);
    }
}
EXPORT_SYMBOL_GPL(gmtp_send_ack);

void gmtp_send_elect_request(struct sock *sk, unsigned long interval)
{
    struct sk_buff *skb = alloc_skb(sk->sk_prot->max_header, GFP_ATOMIC);
    struct gmtp_sock *gp = gmtp_sk(sk);

    gmtp_pr_func();

    if(skb == NULL)
        return;

    if(gp->req_stamp == 0)
        gp->req_stamp = jiffies_to_msecs(jiffies);

    /* Reserve space for headers */
    skb_reserve(skb, sk->sk_prot->max_header);
    GMTP_SKB_CB(skb)->type = GMTP_PKT_ELECT_REQUEST;

    gmtp_transmit_skb(sk, skb);

    if(interval > 0)
        inet_csk_reset_keepalive_timer(sk, interval);
}
EXPORT_SYMBOL_GPL(gmtp_send_elect_request);

void gmtp_send_elect_response(struct sock *sk, __u8 code)
{
    struct sk_buff *skb = alloc_skb(sk->sk_prot->max_header, GFP_ATOMIC);
    struct gmtp_sock *gp = gmtp_sk(sk);

    gmtp_pr_func();

    if(skb == NULL)
        return;

    /* Reserve space for headers */
    skb_reserve(skb, sk->sk_prot->max_header);
    GMTP_SKB_CB(skb)->type = GMTP_PKT_ELECT_RESPONSE;
    GMTP_SKB_CB(skb)->elect_code = code;

    /*if(code == GMTP_ELECT_AUTO) {

        pr_info("Turning a client into a Reporter\n");

         We dont need init mcc, because mcc is already started
        gp->role = GMTP_ROLE_REPORTER;
        gp->myself->max_nclients =
                GMTP_REPORTER_DEFAULT_PROPORTION - 1;
        gp->myself->clients = kmalloc(sizeof(struct gmtp_client),
        GFP_ATOMIC);
        INIT_LIST_HEAD(&gp->myself->clients->list);
        mcc_rx_init(sk);
        inet_csk_reset_keepalive_timer(sk, GMTP_ACK_TIMEOUT);
    }*/

    gmtp_transmit_skb(sk, skb);
}
EXPORT_SYMBOL_GPL(gmtp_send_elect_response);

struct sk_buff *gmtp_ctl_make_elect_response(struct sock *sk,
        struct sk_buff *rcv_skb)
{
    struct gmtp_hdr *rxgh = gmtp_hdr(rcv_skb), *gh;
    struct gmtp_skb_cb *gcb = GMTP_SKB_CB(rcv_skb);
    const u32 gmtp_hdr_len = sizeof(struct gmtp_hdr) +
            sizeof(struct gmtp_hdr_elect_response);

    struct gmtp_hdr_elect_response *ghr;
    struct sk_buff *skb;

    gmtp_print_function();

    skb = alloc_skb(sk->sk_prot->max_header, GFP_ATOMIC);
    if (skb == NULL)
        return NULL;

    skb_reserve(skb, sk->sk_prot->max_header);

    /* Swap the send and the receive. */
    gh = gmtp_zeroed_hdr(skb, gmtp_hdr_len);
    gh->type    = GMTP_PKT_ELECT_RESPONSE;
    gh->sport   = rxgh->dport;
    gh->dport   = rxgh->sport;
    gh->hdrlen  = gmtp_hdr_len;
    gh->seq     = rxgh->seq;
    gh->server_rtt  = rxgh->server_rtt;
    gh->transm_r    = rxgh->transm_r;
    memcpy(gh->flowname, rxgh->flowname, GMTP_FLOWNAME_LEN);

    ghr = gmtp_hdr_elect_response(skb);
    ghr->elect_code = gcb->elect_code;

    return skb;
}
EXPORT_SYMBOL_GPL(gmtp_ctl_make_elect_response);

struct sk_buff *gmtp_ctl_make_ack(struct sock *sk, struct sk_buff *rcv_skb)
{
    struct gmtp_hdr *rxgh = gmtp_hdr(rcv_skb), *gh;
    struct gmtp_hdr_ack *gack;
    const u32 gmtp_hdr_len = sizeof(struct gmtp_hdr)
            + sizeof(struct gmtp_hdr_ack);
    struct sk_buff *skb;

    gmtp_print_function();

    skb = alloc_skb(sk->sk_prot->max_header, GFP_ATOMIC);
    if(skb == NULL)
        return NULL;

    skb_reserve(skb, sk->sk_prot->max_header);

    /* Swap the send and the receive. */
    gh = gmtp_zeroed_hdr(skb, gmtp_hdr_len);
    gh->type = GMTP_PKT_ACK;
    gh->sport = rxgh->dport;
    gh->dport = rxgh->sport;
    gh->hdrlen = gmtp_hdr_len;
    gh->seq = rxgh->seq;
    gh->server_rtt = rxgh->server_rtt;
    gh->transm_r = rxgh->transm_r;
    memcpy(gh->flowname, rxgh->flowname, GMTP_FLOWNAME_LEN);


    if(rxgh->type == GMTP_PKT_DATA) {
        struct gmtp_hdr_data *ghd = gmtp_hdr_data(rcv_skb);
        pr_info("Responding a DATA with a ACK");
        gack = gmtp_hdr_ack(skb);
        gack->orig_tstamp = ghd->tstamp;
    } else {
        pr_info("Responding a NON-DATA with a ACK\n");
    }

    return skb;
}
EXPORT_SYMBOL_GPL(gmtp_ctl_make_ack);

void gmtp_send_feedback(struct sock *sk)
{
    if(sk->sk_state != GMTP_CLOSED) {

        struct sk_buff *skb = alloc_skb(sk->sk_prot->max_header,
        GFP_ATOMIC);

        /* Reserve space for headers */
        skb_reserve(skb, sk->sk_prot->max_header);
        GMTP_SKB_CB(skb)->type = GMTP_PKT_FEEDBACK;
        GMTP_SKB_CB(skb)->orig_tstamp = gmtp_sk(sk)->rx_last_orig_tstamp;

        gmtp_transmit_skb(sk, skb);
    }
}
EXPORT_SYMBOL_GPL(gmtp_send_feedback);

/*
 * Send a GMTP_PKT_CLOSE/CLOSEREQ. The caller locks the socket for us.
 * This cannot be allowed to fail queueing a GMTP_PKT_CLOSE/CLOSEREQ frame
 * under any circumstances.
 */
void gmtp_send_close(struct sock *sk, const int active)
{
    struct sk_buff *skb;
    const gfp_t prio = active ? GFP_KERNEL : GFP_ATOMIC;

    gmtp_print_function();

    skb = alloc_skb(sk->sk_prot->max_header, prio);
    if(skb == NULL)
        return;

    /* Reserve space for headers and prepare control bits. */
    skb_reserve(skb, sk->sk_prot->max_header);
    GMTP_SKB_CB(skb)->type = GMTP_PKT_CLOSE;

    if(active) {
        skb = gmtp_skb_entail(sk, skb);
        /*
         * TODO Verify if we need Retransmission timer
         * for active-close...
         */
        /*inet_csk_reset_xmit_timer(sk, ICSK_TIME_RETRANS,
                GMTP_TIMEOUT_INIT, GMTP_RTO_MAX);*/
    }
    gmtp_transmit_skb(sk, skb);
}

/**
 * gmtp_wait_for_delay  -  Await GMTP send permission
 * @sk:    socket to wait for
 * @delay: timeout in jiffies
 *
 * To delay the send time in process context.
 */
static long gmtp_wait_for_delay(struct sock *sk, unsigned long delay)
{
    DEFINE_WAIT(wait);
    long remaining;
    gmtp_pr_func();
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

static inline unsigned int packet_len(struct sk_buff *skb)
{
    /* Total = Data + (GMTP + IP + MAC) */
    return skb->len + (gmtp_data_hdr_len() + 20 + ETH_HLEN);
}

static inline unsigned int payload_len(struct sk_buff *skb)
{
    /* Data = Total - (GMTP + IP + MAC) */
    return skb->len - (gmtp_data_hdr_len() + 20 + ETH_HLEN);
}

static inline void packet_sent(struct sock *sk, struct sk_buff *skb)
{
    struct gmtp_sock *gp = gmtp_sk(sk);
    unsigned long elapsed = 0;
    int data_len = payload_len(skb);

    if(unlikely(gp->tx_dpkts_sent == 0))
        gmtp_pr_info("Start sending data packets...\n\n");

    ++gp->tx_dpkts_sent;
    gp->tx_data_sent += data_len;
    gp->tx_bytes_sent += skb->len;

    gp->tx_last_stamp = jiffies;
    if(unlikely(gp->tx_first_stamp == 0)) { /* This is the first sent */
        gp->tx_first_stamp = gp->tx_last_stamp;
        gp->tx_time_sample = jiffies;
        gp->tx_byte_sample = gp->tx_bytes_sent;
    }

    if(gp->tx_byte_budget != INT_MIN)
        gp->tx_byte_budget -= (int) skb->len;

    elapsed = jiffies - gp->tx_first_stamp;
    if(gp->tx_first_stamp != 0 && elapsed != 0)
        gp->tx_total_rate = mult_frac(HZ, gp->tx_bytes_sent, elapsed);

    if(gp->tx_dpkts_sent >= gp->tx_sample_len) {

        gp->tx_time_sample = jiffies - gp->tx_time_sample;
        gp->tx_byte_sample = gp->tx_bytes_sent - gp->tx_byte_sample;

        if(gp->tx_time_sample != 0 && gp->tx_byte_sample != 0)
            gp->tx_sample_rate = mult_frac(HZ,
                            gp->tx_byte_sample,
                            gp->tx_time_sample);
    }
}

static void gmtp_xmit_packet(struct sock *sk, struct sk_buff *skb) {

    int err;

    GMTP_SKB_CB(skb)->type = GMTP_PKT_DATA;
    err = gmtp_transmit_skb(sk, skb);

    /*
    char label[30];
    sprintf(label, "To %pI4 (%d pkts sent, err=%d)", &sk->sk_daddr,
            gmtp_sk(sk)->tx_dpkts_sent, err);
    print_gmtp_data(skb, label);
    */

    /*
     * Register this one as sent (even if an error occurred).
     * To the remote end a local packet drop is indistinguishable from
     * network loss.
     */
    packet_sent(sk, skb);

    if (unlikely(err)) {
        gmtp_pr_error("transmit_skb() returned err=%d\n", err);

        print_gmtp_packet(ip_hdr(skb), gmtp_hdr(skb));

        pr_info("ISS: %u, GSS: %u, N: %u\n", gmtp_sk(sk)->iss,
                gmtp_sk(sk)->gss,
                (gmtp_sk(sk)->gss - gmtp_sk(sk)->iss));
    }
}

/**
 * Return difference between actual tx_rate and max tx_rate
 * If return > 0, actual tx_rate is greater than max tx_rate
 */
static long get_rate_gap(struct gmtp_sock *gp, int acum)
{
    long rate = (long) gp->tx_sample_rate;
    long tx = (long) min(gp->tx_max_rate, gp->tx_ucc_rate);

    long coef_adj = 0;

    if(gp->tx_dpkts_sent < GMTP_MIN_SAMPLE_LEN)
        return 0;

    if(rate <= gp->tx_total_rate/2) /* Eliminate discrepancies */
        rate = (long)gp->tx_total_rate;

    coef_adj = mult_frac((rate - tx), 100, tx);

    if(acum) {
        coef_adj += gp->tx_adj_budget;
        if(coef_adj > 110 || coef_adj < -90)
            gp->tx_adj_budget = 0;
        else
            gp->tx_adj_budget += coef_adj;
    }

    return coef_adj;
}

void gmtp_write_xmit(struct sock *sk, struct sk_buff *skb)
{
    struct gmtp_sock *gp = gmtp_sk(sk);
    unsigned long elapsed = 0;
    long delay = 0, delay2 = 0, delay_budget = 0;
    unsigned long tx_rate = min(gp->tx_max_rate, gp->tx_ucc_rate);
    int len;

    /** TODO Continue tests with different scales... */
    static const int scale = 1;
    /*static const int scale = HZ/100;*/

    if(unlikely(skb == NULL))
        return;

    if(tx_rate == UINT_MAX || gp->tx_ucc_type != GMTP_DELAY_UCC)
        goto send;

    /*pr_info("[%d] Tx rate: %lu bytes/s\n", gp->tx_dpkts_sent, gp->tx_total_rate);
    pr_info("[-] Tx rate (sample): %lu bytes/s\n", gp->tx_sample_rate);*/

    elapsed = jiffies - gp->tx_last_stamp; /* time elapsed since last sent */

    len = packet_len(skb);
    if(gp->tx_byte_budget >= mult_frac(len, 3, 4)) {
        goto send;
    } else if(gp->tx_byte_budget != INT_MIN) {
        delay_budget = scale;
        goto wait;
    }

    delay = DIV_ROUND_CLOSEST((HZ * len), tx_rate);
    delay2 = delay - elapsed;

    if(delay2 > 0)
        delay2 += mult_frac(delay2, get_rate_gap(gp, 1), 100);

wait:
    delay2 += delay_budget;

    /*
     * TODO More tests with byte_budgets...
     */
    if(delay <= 0)
        gp->tx_byte_budget = mult_frac(scale, tx_rate, HZ) -
            mult_frac(gp->tx_byte_budget, (int) get_rate_gap(gp, 0), 100);
    else
        gp->tx_byte_budget = INT_MIN;

    if(delay2 > 0) {
        struct gmtp_packet_info *pkt_info;
        pkt_info = kmalloc(sizeof(struct gmtp_packet_info), GFP_KERNEL);
        pkt_info->sk = sk;
        pkt_info->skb = skb;

        /* FIXME Comment because: Compilation error in linux-5.4.21 */
        /*setup_timer(&gp->xmit_timer, gmtp_write_xmit_timer,
                (unsigned long ) pkt_info);
        mod_timer(&gp->xmit_timer, jiffies + delay2);*/

        /* Never use gmtp_wait_for_delay(sk, delay2); in NS-3/dce*/
        /* FIXME Send using a queue and send window for linux 5.4.21... */
        schedule_timeout(delay2);
        return;
    }

send:
    gmtp_xmit_packet(sk, skb);

}
EXPORT_SYMBOL_GPL(gmtp_write_xmit);

