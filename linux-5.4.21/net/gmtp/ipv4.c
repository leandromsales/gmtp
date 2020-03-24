// SPDX-License-Identifier: GPL-2.0-or-later
/*
 *  net/gmtp/ipv4.c
 *
 *  An implementation of the GMTP protocol
 *  Wendell Silva Soares <wendell@ic.ufal.br>
 */
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/gmtp.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/sched/mm.h>
#include <linux/netfilter.h>
#include <linux/netfilter_ipv4.h>
#include <linux/types.h>

#include <net/inet_hashtables.h>
#include <net/inet_common.h>
#include <net/inet_connection_sock.h>
#include <net/icmp.h>
#include <net/ip.h>
#include <net/protocol.h>
#include <net/request_sock.h>
#include <net/secure_seq.h>
#include <net/sock.h>
#include <net/xfrm.h>


#include <uapi/linux/gmtp.h>

#include "gmtp-inter/gmtp-inter.h"
#include "hash/hash.h"
#include "gmtp.h"

static struct nf_hook_ops nfho_gmtp_out;

extern int sysctl_ip_nonlocal_bind __read_mostly;
extern struct inet_timewait_death_row gmtp_death_row;

static inline __u32 gmtp_v4_init_sequence(const struct sk_buff *skb)
{
    return secure_gmtp_sequence_number(ip_hdr(skb)->daddr,
            ip_hdr(skb)->saddr, gmtp_hdr(skb)->sport,
            gmtp_hdr(skb)->dport);
}

struct sock* gmtp_v4_build_control_sk(struct sock *sk)
{
    struct sockaddr_in *local;
    struct sock *mcast_sk;
    unsigned char *channel = "\xee\xff\xff\xfa"; /* 238.255.255.250 */

    gmtp_pr_func();

    local = kmalloc(sizeof(struct sockaddr_in), GFP_KERNEL);
    if(local == NULL)
    	goto err;

    local->sin_family = AF_INET;
    local->sin_port = htons(1900);
    local->sin_addr.s_addr = *(unsigned int *)channel;
    memset(&local->sin_zero, 0, sizeof(local->sin_zero));

    gmtp_info->ctrl_addr = local;

    mcast_sk = gmtp_multicast_connect(sk, GMTP_SOCK_TYPE_CONTROL_CHANNEL,
            local->sin_addr.s_addr, local->sin_port);

    if(mcast_sk == NULL)
    	goto err;

    return mcast_sk;
err:
	return 0;
}

int gmtp_v4_connect(struct sock *sk, struct sockaddr *uaddr, int addr_len)
{
    const struct sockaddr_in *usin = (struct sockaddr_in *)uaddr;
    struct inet_sock *inet = inet_sk(sk);
    struct gmtp_sock *gp = gmtp_sk(sk);
    __be16 orig_sport, orig_dport;
    __be32 daddr, nexthop;
    struct flowi4 *fl4;
    struct rtable *rt;
    int err;
    struct ip_options_rcu *inet_opt;

    gmtp_pr_func();

    gp->role = GMTP_ROLE_CLIENT;

    if(addr_len < sizeof(struct sockaddr_in))
        return -EINVAL;

    if(usin->sin_family != AF_INET)
        return -EAFNOSUPPORT;

    nexthop = daddr = usin->sin_addr.s_addr;

    inet_opt = rcu_dereference_protected(inet->inet_opt,
            sock_owned_by_user(sk));
    if(inet_opt != NULL && inet_opt->opt.srr) {
        if(daddr == 0)
            return -EINVAL;
        nexthop = inet_opt->opt.faddr;
    }

    orig_sport = inet->inet_sport;
    orig_dport = usin->sin_port;
    fl4 = &inet->cork.fl.u.ip4;

    rt = ip_route_connect(fl4, nexthop, inet->inet_saddr, RT_CONN_FLAGS(sk),
            sk->sk_bound_dev_if,
            IPPROTO_GMTP, orig_sport, orig_dport, sk);
    if(IS_ERR(rt))
        return PTR_ERR(rt);

    if(rt->rt_flags & (RTCF_MULTICAST | RTCF_BROADCAST)) {
        ip_rt_put(rt);
        return -ENETUNREACH;
    }

    if(inet_opt == NULL || !inet_opt->opt.srr)
        daddr = fl4->daddr;

    if(inet->inet_saddr == 0)
        inet->inet_saddr = fl4->saddr;
    inet->inet_rcv_saddr = inet->inet_saddr;
    inet->inet_dport = usin->sin_port;
    inet->inet_daddr = daddr;

    inet_csk(sk)->icsk_ext_hdr_len = 0;
    if(inet_opt)
        inet_csk(sk)->icsk_ext_hdr_len = inet_opt->opt.optlen;
    /*
     * Socket identity is still unknown (sport may be zero).
     * However we set state to GMTP_REQUESTING and not releasing socket
     * lock select source port, enter ourselves into the hash tables and
     * complete initialization after this.
     */
    gmtp_set_state(sk, GMTP_REQUESTING);
    err = inet_hash_connect(&gmtp_death_row, sk);
    if(err != 0)
        goto failure;

    err = gmtp_sk_hash_connect(&gmtp_sk_hash, sk);
    if(err != 0)
    	goto failure;

    rt = ip_route_newports(fl4, rt, orig_sport, orig_dport,
            inet->inet_sport, inet->inet_dport, sk);

    if(IS_ERR(rt)) {
        err = PTR_ERR(rt);
        rt = NULL;
        goto failure;
    }
    /* OK, now commit destination to socket.  */
    sk_setup_caps(sk, &rt->dst);

    gp->iss = secure_gmtp_sequence_number(inet->inet_saddr,
            inet->inet_daddr, inet->inet_sport, inet->inet_dport);
    inet->inet_id = gp->iss ^ jiffies;

    err = gmtp_connect(sk);
    rt = NULL;
    if(err != 0)
        goto failure;

    gmtp_info->control_sk = gmtp_v4_build_control_sk(sk);

    if(gmtp_info->control_sk == NULL)
    	goto failure;

out:
    return err;

failure:
    /*
     * This unhashes the socket and releases the local port, if necessary.
     */
    gmtp_set_state(sk, GMTP_CLOSED);
    ip_rt_put(rt);
    sk->sk_route_caps = 0;
    inet->inet_dport = 0;
    goto out;
}
EXPORT_SYMBOL_GPL(gmtp_v4_connect);

static void gmtp_do_redirect(struct sk_buff *skb, struct sock *sk)
{
    struct dst_entry *dst = __sk_dst_check(sk, 0);

    if(dst)
        dst->ops->redirect(dst, sk, skb);
}

/**
 * This routine is called by the ICMP module when it gets some sort of error
 * condition. If err < 0 then the socket should be closed and the error
 * returned to the user. If err > 0 it's just the icmp type << 8 | icmp code.
 * After adjustment header points to the first 8 bytes of the tcp header. We
 * need to find the appropriate port.
 *
 * The locking strategy used here is very "optimistic". When someone else
 * accesses the socket the ICMP is just dropped and for some paths there is no
 * check at all. A more general error queue to queue errors for later handling
 * is probably better.
 */
static int gmtp_v4_err(struct sk_buff *skb, u32 info)
{
    const struct iphdr *iph = (struct iphdr *)skb->data;
    const u8 offset = iph->ihl << 2;
    const struct gmtp_hdr *gh = (struct gmtp_hdr *)(skb->data + offset);
    struct gmtp_sock *gp;
    struct inet_sock *inet;
    struct inet_connection_sock *icsk;
    const int type = icmp_hdr(skb)->type;
    const int code = icmp_hdr(skb)->code;
    struct sock *sk;
    __be32 seq;
    int err;
    struct net *net = dev_net(skb->dev);

    struct request_sock *req, **prev;

    gmtp_pr_func();

    /* FIXME Kernel panic when receive ICMP type 3 code 2 (Protocol Unreachable) */

    gmtp_pr_info("ICMP: Type: %d, Code: %d | ", type, code);
    /*** print_packet(skb, true); ***/

    if(skb->len < offset + sizeof(*gh)) {
        __ICMP_INC_STATS(net, ICMP_MIB_INERRORS);
        return 0;
    }

    gmtp_pr_debug("calling inet_lookup...");
    sk = inet_lookup(net, &gmtp_inet_hashinfo,
            skb, 0,
            iph->daddr, gh->dport,
            iph->saddr, gh->sport, inet_iif(skb));
    if(sk == NULL) {
    	gmtp_pr_debug("sk is NULL");
        __ICMP_INC_STATS(net, ICMP_MIB_INERRORS);
        return -ENOENT;
    }

    if(sk->sk_state == GMTP_TIME_WAIT) {
    	gmtp_pr_debug("sk->sk_state == GMTP_TIME_WAIT");
        inet_twsk_put(inet_twsk(sk));
        return 0;
    }

    gmtp_pr_debug("locking sock");
    bh_lock_sock(sk);
    /* If too many ICMPs get dropped on busy
     * servers this needs to be solved differently.
     */
    if(sock_owned_by_user(sk))
        __NET_INC_STATS(net, LINUX_MIB_LOCKDROPPEDICMPS);

    if(sk->sk_state == GMTP_CLOSED)
        goto out;

    gmtp_pr_debug("Getting gp");
    gp = gmtp_sk(sk);
    seq = gh->seq;
    if((1 << sk->sk_state) & ~(GMTPF_REQUESTING | GMTPF_LISTEN)) {
        __NET_INC_STATS(net, LINUX_MIB_OUTOFWINDOWICMPS);
        goto out;
    }

    gmtp_pr_debug("Verifying ICMP type...");
    switch(type) {
    case ICMP_REDIRECT:
        gmtp_do_redirect(skb, sk);
        goto out;
    case ICMP_SOURCE_QUENCH:
        /* Just silently ignore these. */
        goto out;
    case ICMP_PARAMETERPROB:
        err = EPROTO;
        break;
    case ICMP_DEST_UNREACH:
        if(code > NR_ICMP_UNREACH)
            goto out;

        if(code == ICMP_FRAG_NEEDED) { /* PMTU discovery (RFC1191) */
            if(!sock_owned_by_user(sk))
                /*FIXME gmtp_do_pmtu_discovery(sk, iph, info);*/;
            goto out;
        }
        err = icmp_err_convert[code].errno;
        break;
    case ICMP_TIME_EXCEEDED:
        err = EHOSTUNREACH;
        break;
    default:
    	gmtp_pr_debug("ICMP other types: goto out...");
        goto out;
    }

    icsk = inet_csk(sk);
    switch(sk->sk_state) {
    case GMTP_REQUESTING:
    	gmtp_pr_debug("GMTP is requesting...");
        if(err == EHOSTUNREACH && icsk->icsk_retransmits <= 3)
            goto out;
    case GMTP_REQUEST_RECV:
    	gmtp_pr_debug("GMTP received a request...");
        if(!sock_owned_by_user(sk)) {
            sk->sk_err = err;
            sk->sk_error_report(sk);
            gmtp_done(sk);
        } else
            sk->sk_err_soft = err;
        goto out;
    }

    /* If we've already connected we will keep trying
     * until we time out, or the user gives up.
     */
    inet = inet_sk(sk);
    if(!sock_owned_by_user(sk) && inet->recverr) {
        sk->sk_err = err;
        sk->sk_error_report(sk);
    } else /* Only an error on timeout */
        sk->sk_err_soft = err;
out:
	gmtp_pr_debug("Out: Unlocking sock... ");
    bh_unlock_sock(sk);
    sock_put(sk);
    return 0;
}
EXPORT_SYMBOL_GPL(gmtp_v4_err);

static struct dst_entry* gmtp_v4_route_skb(struct net *net, struct sock *sk,
        struct sk_buff *skb)
{
    struct rtable *rt;
    const struct iphdr *iph = ip_hdr(skb);
    struct flowi4 fl4 = {
            .flowi4_oif = inet_iif(skb),
            .daddr = iph->saddr,
            .saddr = iph->daddr,
            .flowi4_tos = RT_CONN_FLAGS(sk),
            .flowi4_proto = sk->sk_protocol,
            .fl4_sport = gmtp_hdr(skb)->dport,
            .fl4_dport = gmtp_hdr(skb)->sport, };

    security_skb_classify_flow(skb, flowi4_to_flowi(&fl4));
    rt = ip_route_output_flow(net, &fl4, sk);
    if(IS_ERR(rt)) {
        __IP_INC_STATS(net, IPSTATS_MIB_OUTNOROUTES);
        return NULL;
    }

    return &rt->dst;
}

static int gmtp_v4_send_register_reply(const struct sock *sk,
        struct request_sock *req)
{
    int err = -1;
    struct sk_buff *skb;
    struct dst_entry *dst;
    struct flowi4 fl4;

    gmtp_print_function();

    dst = inet_csk_route_req(sk, &fl4, req);
    if(dst == NULL)
        goto out;

    skb = gmtp_make_register_reply(sk, dst, req);
    if(skb != NULL) {
        const struct inet_request_sock *ireq = inet_rsk(req);
        gmtp_sk(sk)->reply_stamp = jiffies_to_msecs(jiffies);

        rcu_read_lock();
        err = ip_build_and_send_pkt(skb, sk, ireq->ir_loc_addr,
                ireq->ir_rmt_addr, rcu_dereference(ireq->ireq_opt));
        rcu_read_unlock();
        err = net_xmit_eval(err);
    }

out:
    dst_release(dst);
    return err;
}

static void gmtp_v4_ctl_send_packet(struct sock *sk, struct sk_buff *rxskb, __u8 type)
{
    int err;
    const struct iphdr *rxiph;
    struct sk_buff *skb = NULL;
    struct dst_entry *dst;
    struct net *net = dev_net(skb_dst(rxskb)->dev);
    struct sock *ctl_sk = net->gmtp.v4_ctl_sk;

    gmtp_pr_debug("%s (%u)", gmtp_packet_name(type), type);

    if(skb_rtable(rxskb)->rt_type != RTN_LOCAL)
        return;

    dst = gmtp_v4_route_skb(net, ctl_sk, rxskb);
    if(dst == NULL)
        return;

    switch(type) {
    case GMTP_PKT_RESET:
        /* Never send a reset in response to a reset. */
        if(gmtp_hdr(rxskb)->type == GMTP_PKT_RESET)
            return;
        skb = gmtp_ctl_make_reset(ctl_sk, rxskb);
        break;
    case GMTP_PKT_ELECT_RESPONSE:
        skb = gmtp_ctl_make_elect_response(ctl_sk, rxskb);
        break;
    case GMTP_PKT_ACK:
        skb = gmtp_ctl_make_ack(ctl_sk, rxskb);
        break;
    }

    if(skb == NULL)
        goto out;

    rxiph = ip_hdr(rxskb);
    skb_dst_set(skb, dst_clone(dst));

    bh_lock_sock(ctl_sk);
    err = ip_build_and_send_pkt(skb, ctl_sk, rxiph->daddr, rxiph->saddr,
            NULL);
    bh_unlock_sock(ctl_sk);

    if(net_xmit_eval(err) == 0) {
        /*      DCCP_INC_STATS_BH(DCCP_MIB_OUTSEGS);
         DCCP_INC_STATS_BH(DCCP_MIB_OUTRSTS);*/
    }

out:
    dst_release(dst);
}

static void gmtp_v4_ctl_send_reset(const struct sock *sk, struct sk_buff *rxskb)
{
	gmtp_pr_func();
    gmtp_v4_ctl_send_packet(sk, rxskb, GMTP_PKT_RESET);
}

static struct sock *gmtp_v4_hnd_req(struct sock *sk, struct sk_buff *skb)
{
    const struct gmtp_hdr *gh = gmtp_hdr(skb);
    const struct iphdr *iph = ip_hdr(skb);
    struct sock *nsk;

    struct request_sock *req = inet_reqsk(sk);

    gmtp_pr_func();

    if (req == NULL)
    	return NULL;

    nsk = req->rsk_listener;
    if(nsk == NULL)
    	goto lookup;

    /** This causes kernel panic:*/
    if (unlikely(nsk->sk_state != GMTP_LISTEN)) {
		gmtp_pr_error("nsk->sk_state != GMTP_LISTEN");
		 inet_csk_reqsk_queue_drop_and_put(nsk, req);
		goto lookup;
	}

    gmtp_pr_info("Calling sock_hold...");
    sock_hold(sk);

    gmtp_pr_info("Calling gmtp_check_req... KERNEL PANIC HERE!");
    /*nsk = gmtp_check_req(sk, skb, req);*/

    if(nsk == NULL)
    	nsk = sk;
    return nsk;
lookup:
	gmtp_pr_info("TODO: Lookup for established connections");
	/*
    nsk = inet_lookup_established(sock_net(sk), &gmtp_inet_hashinfo,
            iph->saddr, gh->sport, iph->daddr, gh->dport,
            inet_iif(skb));

    if(nsk != NULL) {
        if(nsk->sk_state != GMTP_TIME_WAIT) {
            bh_lock_sock(nsk);
            return nsk;
        }
        inet_twsk_put(inet_twsk(nsk));
        return NULL;
    }*/

    return sk;
}

int gmtp_v4_do_rcv(struct sock *sk, struct sk_buff *skb)
{
    struct gmtp_hdr *gh = gmtp_hdr(skb);

    gmtp_pr_func();

    if(sk->sk_state == GMTP_OPEN) { /* Fast path*/
        if(gmtp_rcv_established(sk, skb, gh, skb->len))
            goto reset;
        return 0;
    }

    /*
     * Step 3: Process LISTEN state
     *
     * If P.type == Register
     *     Generate a new socket and switch to that socket.
     *     Set S := new socket for this port pair
     *        S.state = RESPOND //TODO change to GMTP_REQUEST_RECV
     *    A Response packet will be generated in Step 11 *)
     *  Otherwise,
     *        Generate Reset(No Connection) unless P.type == Reset
     *        Drop packet and return
     */
    if(gmtp_rcv_state_process(sk, skb, gh, skb->len))
        goto reset;
    return 0;

reset:
    gmtp_v4_ctl_send_reset(sk, skb);
discard:
    kfree_skb(skb);
    return 0;

}
EXPORT_SYMBOL_GPL(gmtp_v4_do_rcv);

/**
 *  gmtp_check_packet  -  check for malformed packets
 *  Packets that fail these checks are ignored and do not receive Resets.
 */
static int gmtp_check_packet(struct sk_buff *skb)
{
    const struct gmtp_hdr *gh = gmtp_hdr(skb);

    /** Accept multicast only for Data, Close and Reset packets */
    if(skb->pkt_type != PACKET_HOST && gh->type != GMTP_PKT_DATA
            && gh->type != GMTP_PKT_CLOSE
            && gh->type != GMTP_PKT_RESET) {
        gmtp_pr_warning("invalid packet destiny\n");
        return 1;
    }

    /* If the packet is shorter than sizeof(struct gmtp_hdr),
     * drop packet and return */
    if(!pskb_may_pull(skb, sizeof(struct gmtp_hdr))) {
        gmtp_pr_warning("pskb_may_pull failed\n");
        return 1;
    }

    /* If P.type is not understood, drop packet and return */
    if(gh->type >= GMTP_PKT_INVALID) {
        gmtp_pr_warning("invalid packet type\n");
        return 1;
    }

    /*
     * If P.hdrlen is too small for packet type, drop packet and return
     */
    if(gh->hdrlen < gmtp_hdr_len(skb) / sizeof(u32)) {
        gmtp_pr_warning("P.hdrlen(%u) too small\n", gh->hdrlen);
        return 1;
    }

    /* If header checksum is incorrect, drop packet and return.
     * (This step is completed in the AF-dependent functions.) */
    skb->csum = skb_checksum(skb, 0, skb->len, 0);

    return 0;
}

/**
 * FIXME verify if client already is added.
 */
static int gmtp_v4_reporter_rcv_elect_request(struct sk_buff *skb)
{
    const struct iphdr *iph = ip_hdr(skb);
    const struct gmtp_hdr *gh = gmtp_hdr(skb);
    struct gmtp_client_entry *media_entry;
    struct gmtp_client *r;
    struct gmtp_client *c;

    gmtp_pr_func();

    media_entry = gmtp_lookup_client(client_hashtable, gh->flowname);
    if(media_entry == NULL) {
        pr_info("Media entry == NULL\n");
        return 1;
    }

    r = gmtp_get_client(&media_entry->clients->list, iph->daddr, gh->dport);
    if(r == NULL) {
        pr_info("Reporter == NULL\n");
        return 1;
    }

    pr_info("Reporter: ADDR=%pI4@%-5d, max_nclients: %u, nclients: %u\n",
            &r->addr, ntohs(r->port), r->max_nclients, r->nclients);

    if((r->max_nclients <= 0) || (r->nclients >= r->max_nclients)) {
        pr_info("r->max_nclients <= 0 || r->nclients >= r->max_nclients\n");
        GMTP_SKB_CB(skb)->elect_code = GMTP_ELECT_REJECT;
    } else {
        r->nclients++;
        c = gmtp_list_add_client(0, iph->saddr, gh->sport, 0,
                &r->clients->list);
        GMTP_SKB_CB(skb)->elect_code = GMTP_ELECT_ACCEPT;
    }

    gmtp_v4_ctl_send_packet(0, skb, GMTP_PKT_ELECT_RESPONSE);
    return 0;
}
EXPORT_SYMBOL_GPL(gmtp_v4_reporter_rcv_elect_request);

static int gmtp_v4_client_rcv_elect_response(struct sk_buff *skb)
{
    const struct iphdr *iph = ip_hdr(skb);
    const struct gmtp_hdr *gh = gmtp_hdr(skb);
    struct gmtp_client_entry *media_entry;
    struct gmtp_client *client;

    gmtp_pr_func();

    media_entry = gmtp_lookup_client(client_hashtable, gh->flowname);
    if(media_entry == NULL) {
        pr_info("Media entry == NULL\n");
        return 1;
    }

    client = gmtp_get_client(&media_entry->clients->list, iph->daddr,
            gh->dport);
    if(client == NULL) {
        pr_info("Client == NULL\n");
        return 1;
    }

    pr_info("Client: ADDR=%pI4@%-5d\n", &client->addr, ntohs(client->port));
    if(client->rsock == NULL) {
        pr_info("client->rsock == NULL\n");
        return 1;
    }

    gmtp_set_state(client->rsock, GMTP_OPEN);
    gmtp_send_ack(client->rsock);

    return 0;
}
EXPORT_SYMBOL_GPL(gmtp_v4_client_rcv_elect_response);

static int gmtp_v4_client_rcv_reporter_ack(struct sk_buff *skb,
        struct gmtp_client *c)
{
    const struct iphdr *iph = ip_hdr(skb);
    const struct gmtp_hdr *gh = gmtp_hdr(skb);

    gmtp_pr_func();

    if(c == NULL) {
        gmtp_pr_error("Client is NULL!");
        return 1;
    }

    if(c->reporter == NULL || c->rsock == NULL) {
        gmtp_pr_warning("Reporter %pI4@%-5d null!", &iph->saddr,
                ntohs(gh->sport));
        return 1;
    }

    if((iph->saddr != c->reporter->addr) || (gh->sport != c->reporter->port)) {
        gmtp_pr_warning("Reporter %pI4@%-5d invalid!", &iph->saddr,
                ntohs(gh->sport));
        return 1;
    }

    c->ack_rx_tstamp = jiffies_to_msecs(jiffies);
    gmtp_sk(c->rsock)->ack_rx_tstamp = c->ack_rx_tstamp;

    pr_info("ACK rcv from reporter %pI4@%-5d\n", &iph->saddr, ntohs(gh->sport));

    return 0;
}

static int gmtp_v4_reporter_rcv_ack(struct sk_buff *skb)
{
    const struct iphdr *iph = ip_hdr(skb);
    const struct gmtp_hdr *gh = gmtp_hdr(skb);
    struct gmtp_client_entry *media_entry;
    struct gmtp_client *r;
    struct gmtp_client *c;

    gmtp_pr_func();

    media_entry = gmtp_lookup_client(client_hashtable, gh->flowname);
    if(media_entry == NULL) {
        pr_info("Media entry == NULL\n");
        return 1;
    }

    r = gmtp_get_client(&media_entry->clients->list, iph->daddr, gh->dport);
    if(r == NULL) {
        gmtp_pr_warning("Reporter %pI4@%-5d not found!", &iph->daddr,
                ntohs(gh->dport));
        return 1;
    }

    /* If i'm a client, check ack from reporter */
    if(r->max_nclients <= 0) {
        return gmtp_v4_client_rcv_reporter_ack(skb, r);
    }

    c = gmtp_get_client(&r->clients->list, iph->saddr, gh->sport);
    if(c == NULL) {
        gmtp_pr_warning("Client %pI4@%-5d not found!", &iph->saddr,
                ntohs(gh->sport));
        return 1;
    }
    c->ack_rx_tstamp = jiffies_to_msecs(jiffies);

    pr_info("ACK rcv from client %pI4@%-5d\n", &iph->saddr, ntohs(gh->sport));

    gmtp_v4_ctl_send_packet(0, skb, GMTP_PKT_ACK);

    return 0;
}

/*

 static int gmtp_v4_reporter_rcv_close(struct sk_buff *skb)
 {
 const struct iphdr *iph = ip_hdr(skb);
 const struct gmtp_hdr *gh = gmtp_hdr(skb);
 struct gmtp_client_entry *media_entry;
 struct gmtp_client *r;
 struct gmtp_client *c;

 gmtp_pr_func();

 media_entry = gmtp_lookup_client(gmtp_hashtable, gh->flowname);
 if(media_entry == NULL) {
 pr_info("Media entry == NULL\n");
 return 1;
 }

 r = gmtp_get_client(&media_entry->clients->list, iph->daddr, gh->dport);
 if(r == NULL) {
 pr_info("Reporter == NULL\n");
 return 1;
 }

 if(r->max_nclients <= 0) {
 pr_info("r->max_nclients <= 0\n");
 return 1;
 }

 c = gmtp_get_client(&r->clients->list, iph->saddr, gh->sport);
 if(c == NULL) {
 pr_info("client == NULL\n");
 return 1;
 }

 pr_info("CLOSE received from client %pI4@%-5d\n", &c->addr, ntohs(c->port));

 return 0;
 }
 */

static int gmtp_v4_sk_receive_skb(struct sk_buff *skb, struct sock *sk)
{
    const struct gmtp_hdr *gh = gmtp_hdr(skb);

    gmtp_pr_func();

    if(sk == NULL) {

        const struct iphdr *iph = ip_hdr(skb);

        if(gmtp_info->relay_enabled) {
            if(gh->type == GMTP_PKT_DATA)
                goto ignore_it;
        }

        /* TODO Make a reset code for each error here! */
        switch(gh->type) {
        case GMTP_PKT_ELECT_REQUEST:
            if(gmtp_v4_reporter_rcv_elect_request(skb))
                goto no_gmtp_socket;
            break;
        case GMTP_PKT_ELECT_RESPONSE:
            if(gmtp_v4_client_rcv_elect_response(skb))
                goto no_gmtp_socket;
            break;
        case GMTP_PKT_ACK:
            if(gmtp_v4_reporter_rcv_ack(skb)) {
                print_gmtp_packet(iph, gh);
                goto no_gmtp_socket;
            }
            break;
            /* FIXME Manage close from server... */
            /*case GMTP_PKT_CLOSE:
             pr_info("CLOSE received!\n");
             if(gmtp_v4_reporter_rcv_close(skb))
             goto no_gmtp_socket;
             break;*/
        default:
            gmtp_pr_error("Failed to look up flow ID in table and "
                "get corresponding socket for this packet: ");
            print_gmtp_packet(iph, gh);
            goto no_gmtp_socket;
        }
        goto discard_it;
    }

    /*
     * Step 2:
     *  ... or S.state == TIMEWAIT,
     *      Generate Reset(No Connection) unless P.type == Reset
     *      Drop packet and return
     */
    if(sk->sk_state == GMTP_TIME_WAIT) {
        inet_twsk_put(inet_twsk(sk));
        goto no_gmtp_socket;
    }

    if(!xfrm4_policy_check(sk, XFRM_POLICY_IN, skb)) {
        sock_put(sk);
        goto discard_it;
    }

    nf_reset_ct(skb);


    /* When refcounted is true, __sk_receive_skb call sock_put, wich
     * deletes sk and causes kernel panic
     */
    return __sk_receive_skb(sk, skb, 1, gh->hdrlen, false);


no_gmtp_socket:

    if(!xfrm4_policy_check(NULL, XFRM_POLICY_IN, skb))
        goto discard_it;
    /*
     * Step 2:
     *  If no socket ...
     *      Generate Reset(No Connection) unless P.type == Reset
     *      Drop packet and return
     */
    if(gh->type != GMTP_PKT_RESET) {
        GMTP_SKB_CB(skb)->reset_code =
                GMTP_RESET_CODE_NO_CONNECTION;
        gmtp_v4_ctl_send_reset(sk, skb);
    }

discard_it:
    kfree_skb(skb);

ignore_it:
    return 0;
}

/* this is called when real data arrives */
static int gmtp_v4_rcv(struct sk_buff *skb)
{
    const struct gmtp_hdr *gh;
    const struct iphdr *iph;
    struct net *net;
    struct sock *sk;
    bool refcounted = false;
    int ret;

    /* Step 1: Check header basics */
    if(gmtp_check_packet(skb)) {
        gmtp_pr_error("Dropping invalid packet!");
        goto discard_it;
    }

    gh = gmtp_hdr(skb);
    iph = ip_hdr(skb);

    GMTP_SKB_CB(skb)->seq = gh->seq;
    GMTP_SKB_CB(skb)->type = gh->type;

    /*if(unlikely(gh->type != GMTP_PKT_DATA && gh->type != GMTP_PKT_ACK)) {
    	gmtp_pr_func();
        print_gmtp_packet(iph, gh);
        gmtp_pr_debug("pkt_type: %u", skb->pkt_type);
    }*/

    gmtp_pr_func();
    print_gmtp_packet(iph, gh);

    /**
     * FIXME Change Election algorithm to fully distributed using multicast
     */
    if(skb->pkt_type == PACKET_MULTICAST) {

        struct gmtp_client *tmp;
        struct gmtp_client_entry *media_entry = gmtp_lookup_client(
                client_hashtable, gh->flowname);

        if(media_entry == NULL)
            goto discard_it;

        list_for_each_entry(tmp, &(media_entry->clients->list), list)
        {
            if(!tmp)
                goto discard_it;

            sk = gmtp_lookup_established(&gmtp_sk_hash,
            			iph->saddr, gh->sport,
            			tmp->addr, tmp->port);

            /*sk = __inet_lookup(net,
                    &gmtp_inet_hashinfo,
                    skb, __gmtp_hdr_len(gh),
                    iph->saddr, gh->sport,
                    tmp->addr, tmp->port,
                    inet_iif(skb), 0, &refcounted);
             */
            /** FIXME Check warnings at receive skb... */
            gmtp_v4_sk_receive_skb(skb_copy(skb, GFP_ATOMIC), sk);
        }
        /*
         * We made a copy of skb for each client.
         * So, we can discard the original skb.
         */
        goto discard_it;

    }
lookup:

	/*if (gh->type == GMTP_PKT_RESET) {
		gmtp_pr_info("RESET RECEIVED!");
		goto discard_it;
	}*/

	/* FIXME Calling inet_lookup functions causes kernel panic
	 * GMTP is using its own list to avoid inet lookup
	 */
	/*sk = __inet_lookup(net, &gmtp_inet_hashinfo, skb, __gmtp_hdr_len(gh),
			iph->saddr, gh->sport, iph->daddr, gh->dport, inet_iif(skb), 0,
			&refcounted);*/

	/*
	 * Unicast packet...
	 */
	sk = gmtp_lookup_established(&gmtp_sk_hash,
			iph->saddr, gh->sport,
			iph->daddr, gh->dport);

    if (!sk)
    	goto lookup_listener;

    gmtp_pr_debug("Socket addr: %p", sk);
    print_gmtp_sock(sk);

	if(sk->sk_state == GMTP_NEW_SYN_RECV) {

		struct request_sock *req = inet_reqsk(sk);
		struct sock *nsk;

		sk = req->rsk_listener;

		if (unlikely(sk->sk_state != GMTP_LISTEN)) {
			gmtp_pr_info("req->rsk_listener->sk_state != GMTP_LISTEN");
			gmtp_pr_debug("req->rsk_listener addr: %p", sk);
			print_gmtp_sock(sk);
			inet_csk_reqsk_queue_drop_and_put(sk, req);

			/* FIXME Endless loop when send close and receive close back */
			goto lookup_listener;
		}
		sock_hold(sk);
		refcounted = true;

		nsk = gmtp_check_req(sk, skb, req);

		if (!nsk) {
			reqsk_put(req);
			goto discard_and_relse;
		}

		if (nsk == sk) {
			reqsk_put(req);
		} else if (gmtp_child_process(sk, nsk, skb)) {
			gmtp_v4_ctl_send_reset(sk, skb);
			goto discard_and_relse;;
		} else {
			sock_put(sk);
			return 0;
		}
	}
	goto receive_it;

lookup_listener:
	sk = gmtp_lookup_listener(&gmtp_sk_hash, iph->daddr, gh->dport);
	if(!sk)
		goto receive_it;

	print_gmtp_sock(sk);
	if (sk->sk_state == GMTP_LISTEN) {
		ret = gmtp_v4_do_rcv(sk, skb);
		goto put_and_return;
	}

put_and_return:
	return ret;

receive_it:
	return gmtp_v4_sk_receive_skb(skb, sk);

discard_it:
    kfree_skb(skb);
    return 0;

discard_and_relse:
	if (refcounted)
		sock_put(sk);
	goto discard_it;
}
EXPORT_SYMBOL_GPL(gmtp_v4_rcv);

static void gmtp_v4_reqsk_destructor(struct request_sock *req)
{
    gmtp_print_function();
    kfree(inet_rsk(req)->ireq_opt);
}

void gmtp_syn_ack_timeout(const struct request_sock *req)
{
}
EXPORT_SYMBOL(gmtp_syn_ack_timeout);

static struct request_sock_ops gmtp_request_sock_ops __read_mostly = {
        .family = PF_INET,
        .obj_size = sizeof(struct gmtp_request_sock),
        .rtx_syn_ack = gmtp_v4_send_register_reply,
        .send_ack = gmtp_reqsk_send_ack,
        .destructor = gmtp_v4_reqsk_destructor,
        .send_reset = gmtp_v4_ctl_send_reset,
        .syn_ack_timeout = gmtp_syn_ack_timeout,
};

/*
 * Called by SERVER when it received a request/register from client/relay
 */
int gmtp_v4_conn_request(struct sock *sk, struct sk_buff *skb)
{
    struct inet_request_sock *ireq;

    struct gmtp_sock *gp = gmtp_sk(sk);
    struct gmtp_hdr *gh = gmtp_hdr(skb);
    struct gmtp_hdr_register *gr = gmtp_hdr_register(skb);

    struct request_sock *req;
    struct gmtp_request_sock *greq;
    struct gmtp_skb_cb *gcb = GMTP_SKB_CB(skb);

    gmtp_pr_func();

    /* Never answer to GMTP_PKT_REQUESTs send to broadcast or multicast */
    if(skb_rtable(skb)->rt_flags & (RTCF_BROADCAST | RTCF_MULTICAST))
        goto drop; /* discard, don't send a reset here */


    /*
     * TW buckets are converted to open requests without
     * limitations, they conserve resources and peer is
     * evidently real one.
     */
    gcb->reset_code = GMTP_RESET_CODE_TOO_BUSY;
    if(inet_csk_reqsk_queue_is_full(sk))
        goto drop;

    /**
     * FIXME Update sk->sk_ack_backlog correctly
     */
    sk->sk_ack_backlog = 0;

    if(sk_acceptq_is_full(sk))
        goto drop;

    req = inet_reqsk_alloc(&gmtp_request_sock_ops, sk, true);
    if(req == NULL)
        goto drop;

    if(gmtp_reqsk_init(req, gmtp_sk(sk), skb))
        goto drop_and_free;

    if(security_inet_conn_request(sk, skb, req))
        goto drop_and_free;

    ireq = inet_rsk(req);
    sk_rcv_saddr_set(req_to_sk(req), ip_hdr(skb)->daddr);
    sk_daddr_set(req_to_sk(req), ip_hdr(skb)->saddr);

    inet_sk(req_to_sk(req))->inet_sport = gh->dport;
    ireq->ireq_family = AF_INET;
    ireq->ir_iif = sk->sk_bound_dev_if;

    /*
     * Step 3: Process LISTEN state
     */
    gmtp_pr_info("Processing LISTEN state...");
    greq = gmtp_rsk(req);
    greq->isr = gcb->seq;
    greq->gsr = greq->isr;
    greq->iss = greq->isr;
    greq->gss = greq->iss;
    greq->tx_ucc_type = gp->tx_ucc_type;

    if(memcmp(gh->flowname, gp->flowname, GMTP_FLOWNAME_LEN))
        goto reset;

    memcpy(greq->flowname, gp->flowname, GMTP_FLOWNAME_LEN);
    memcpy(gp->relay_id, gr->relay_id, sizeof(gr->relay_id));

    if(gmtp_sk_hash_connect(&gmtp_sk_hash, req_to_sk(req)))
    	goto drop_and_free;

    if(gmtp_v4_send_register_reply(sk, req))
        goto drop_and_free;

    inet_csk_reqsk_queue_hash_add(sk, req, GMTP_TIMEOUT_INIT);
    reqsk_put(req);
    return 0;

reset:
    gcb->reset_code = GMTP_RESET_CODE_BAD_FLOWNAME;
    gmtp_v4_ctl_send_reset(sk, skb);

drop_and_free:
    reqsk_free(req);
drop:
    return 0;
}
EXPORT_SYMBOL_GPL(gmtp_v4_conn_request);


bool static inline gmtp_ehash_nolisten(struct sock *sk, struct sock *osk)
{
	gmtp_sk_ehash_insert(&gmtp_sk_hash, sk, osk);
	return inet_ehash_nolisten(sk, osk);
}

/*
 * The three way handshake has completed - we got a valid ACK or DATAACK -
 * now create the new socket.
 *
 * This is the equivalent of TCP's tcp_v4_syn_recv_sock
 */
struct sock *gmtp_v4_request_recv_sock(const struct sock *sk,
		struct sk_buff *skb, struct request_sock *req, struct dst_entry *dst,
                      struct request_sock *req_unhash, bool *own_req)

{
    struct inet_request_sock *ireq;
    struct inet_sock *newinet;
    struct sock *newsk;

    gmtp_pr_func();

    if(sk_acceptq_is_full(sk))
        goto exit_overflow;

    newsk = gmtp_create_openreq_child(sk, req, skb);
    if(newsk == NULL)
        goto exit_nonewsk;

    newinet = inet_sk(newsk);
    ireq = inet_rsk(req);
    sk_daddr_set(newsk, ireq->ir_rmt_addr);
    sk_rcv_saddr_set(newsk, ireq->ir_loc_addr);
    newinet->inet_saddr = ireq->ir_loc_addr;
    RCU_INIT_POINTER(newinet->inet_opt, rcu_dereference(ireq->ireq_opt));
    newinet->mc_index = inet_iif(skb);
    newinet->mc_ttl = ip_hdr(skb)->ttl;
    newinet->inet_id = prandom_u32();

    if(dst == NULL  &&
            (dst = inet_csk_route_child_sock(sk, newsk, req)) == NULL)
        goto put_and_exit;

    sk_setup_caps(newsk, dst);

    gmtp_sync_mss(newsk, dst_mtu(dst));

    if(__inet_inherit_port(sk, newsk) < 0)
        goto put_and_exit;

    /** TODO Add newsk and remove oldsk into GMTP hash table) */
    gmtp_pr_info("New sk: %p", newsk);
    gmtp_pr_info("old sk: %p", req_to_sk(req_unhash));
	*own_req = gmtp_ehash_nolisten(newsk, req_to_sk(req_unhash));
	if (*own_req)
		ireq->ireq_opt = NULL;
	else
		newinet->inet_opt = NULL;
	return newsk;

exit_overflow:
    __NET_INC_STATS(sock_net(sk), LINUX_MIB_LISTENOVERFLOWS);
exit_nonewsk:
    dst_release(dst);
exit:
    __NET_INC_STATS(sock_net(sk), LINUX_MIB_LISTENDROPS);
    return NULL;
put_and_exit:
    inet_csk_prepare_forced_close(newsk);
    gmtp_done(newsk);
    goto exit;
}
EXPORT_SYMBOL_GPL(gmtp_v4_request_recv_sock);

void gmtp_v4_send_check(struct sock *sk, struct sk_buff *skb)
{
    gmtp_pr_warning("GMTP has no ckecksum!");
}
EXPORT_SYMBOL_GPL(gmtp_v4_send_check);

static const struct inet_connection_sock_af_ops gmtp_ipv4_af_ops = {
        .queue_xmit = ip_queue_xmit,
        .send_check = gmtp_v4_send_check, /* GMTP has no checksum... */
        .rebuild_header = inet_sk_rebuild_header,
        .conn_request = gmtp_v4_conn_request,
        .syn_recv_sock = gmtp_v4_request_recv_sock,
        .net_header_len = sizeof(struct iphdr),
        .setsockopt = ip_setsockopt,
        .getsockopt = ip_getsockopt,
        .addr2sockaddr = inet_csk_addr2sockaddr,
        .sockaddr_len = sizeof(struct sockaddr_in),
#ifdef CONFIG_COMPAT
        .compat_setsockopt = compat_ip_setsockopt,
        .compat_getsockopt = compat_ip_getsockopt,
#endif
    };

static int gmtp_v4_init_sock(struct sock *sk)
{

    int err = 0;

    gmtp_pr_func();

    err = gmtp_init_sock(sk);
    if(err == 0) {
        /* Setting AF options */
        inet_csk(sk)->icsk_af_ops = &gmtp_ipv4_af_ops;
    }

    return err;
}

/* Networking protocol blocks we attach to sockets.
 *
 * socket layer -> transport layer interface (struct proto)
 * transport -> network interface is defined by (struct inet_proto)
 */
static struct proto gmtp_v4_prot = {
        .name = "GMTP",
        .owner = THIS_MODULE,
        .close = gmtp_close,
        .connect = gmtp_v4_connect,
        .disconnect = gmtp_disconnect,
        .ioctl = gmtp_ioctl,
        .init = gmtp_v4_init_sock,
        .setsockopt = gmtp_setsockopt,
        .getsockopt = gmtp_getsockopt,
        .sendmsg = gmtp_sendmsg,
        .recvmsg = gmtp_recvmsg,
        .backlog_rcv = gmtp_v4_do_rcv,
        .hash = inet_hash,
        .unhash = inet_unhash,
        .accept = inet_csk_accept,
        .get_port = inet_csk_get_port,
        .shutdown = gmtp_shutdown,
        .destroy = gmtp_destroy_sock,
        .orphan_count = &gmtp_orphan_count,
        .max_header = GMTP_MAX_HDR_LEN,
        .obj_size = sizeof(struct gmtp_sock),
        .slab_flags = SLAB_TYPESAFE_BY_RCU,
        .rsk_prot = &gmtp_request_sock_ops,
        .h.hashinfo = &gmtp_inet_hashinfo,
};

/**
 * We define the gmtp_protocol object (net_protocol object) and add it with the
 * inet_add_protocol() method.
 * This sets the gmtp_protocol object to be an element in the global
 * protocols array (inet_protos).
 *
 * @handler is called when real data arrives
 *
 */
static const struct net_protocol gmtp_protocol = {
        .handler = gmtp_v4_rcv,
        .err_handler = gmtp_v4_err,
        .no_policy = 1,
        .netns_ok = 1, /* mandatory */
        .icmp_strict_tag_validation = 1,
};

/**
 * In the socket creation routine, protocol implementer specifies a
 * 'struct proto_ops' (/include/linux/net.h) instance.
 * The socket layer calls function members of this proto_ops instance before the
 * protocol specific functions are called.
 *
 * socket layer -> transport layer interface (struct proto)
 * transport -> network interface is defined by (struct inet_proto)
 */
static const struct proto_ops inet_gmtp_ops = {
        .family = PF_INET,
        .owner = THIS_MODULE,
        .release = inet_release,
        .bind = inet_bind,
        .connect = inet_stream_connect,
        .socketpair = sock_no_socketpair,
        .accept = inet_accept,
        .getname = inet_getname,
        .poll = gmtp_poll,
        .ioctl = inet_ioctl,
        .listen = inet_gmtp_listen,
        .shutdown = inet_shutdown,
        .setsockopt = sock_common_setsockopt,
        .getsockopt = sock_common_getsockopt,
        .sendmsg = inet_sendmsg,
        .recvmsg = sock_common_recvmsg,
        .mmap = sock_no_mmap,
        .sendpage = sock_no_sendpage,
};

/**
 * Describes the PF_INET protocols
 * Defines the different SOCK types for PF_INET
 * Ex: SOCK_STREAM (TCP), SOCK_DGRAM (UDP), SOCK_RAW
 *
 * inet_register_protosw() is the function called to register inet sockets.
 * There is a static array of type inet_protosw inetsw_array[] which contains
 * information about all the inet socket types.
 *
 * @list: This is a pointer to the next node in the list.
 * @type: This is the socket type and is a key to search entry for a given
 * socket and type in inetsw[] array.
 * @protocol: This is again a key to find an entry for the socket type in the
 * inetsw[] array. This is an L4 protocol number (L4 Transport layer protocol).
 * @prot: This is a pointer to struct proto.
 * ops: This is a pointer to the structure of type 'proto_ops'.
 */
static struct inet_protosw gmtp_protosw = {
        .type = SOCK_GMTP,
        .protocol = IPPROTO_GMTP,
        .prot = &gmtp_v4_prot,
        .ops = &inet_gmtp_ops,
        .flags = INET_PROTOSW_ICSK,
};

static int __net_init gmtp_v4_init_net(struct net *net)
{
    gmtp_print_function();

    if(gmtp_inet_hashinfo.bhash == NULL)
        return -ESOCKTNOSUPPORT;

	return inet_ctl_sock_create(&net->gmtp.v4_ctl_sk, PF_INET, SOCK_GMTP,
			IPPROTO_GMTP, net);
}

static void __net_exit gmtp_v4_exit_net(struct net *net)
{
    gmtp_print_function();
    inet_ctl_sock_destroy(net->gmtp.v4_ctl_sk);
}

static struct pernet_operations gmtp_v4_ops = {
        .init = gmtp_v4_init_net,
        .exit = gmtp_v4_exit_net,
};


static unsigned int hook_func_gmtp_out(void *priv,
		struct sk_buff *skb, const struct nf_hook_state *state)
{
    struct iphdr *iph = ip_hdr(skb);

    if(iph->protocol == IPPROTO_GMTP) {

        struct gmtp_hdr *gh = gmtp_hdr(skb);
        int new_ttl = 1;

        switch(gh->type) {
        case GMTP_PKT_REQUEST:
        	/* FIXME Uncomment after testing client/server */
            /*if(GMTP_SKB_CB(skb)->retransmits <= 3) {
            	gmtp_pr_debug("Changing TTL to %d\n", new_ttl);
                iph->ttl = new_ttl;
                ip_send_check(iph);
            } else */{
                gmtp_pr_debug("Auto promoting the client to a relay\n");
                gh->type = GMTP_PKT_REGISTER;
            }
        }
    }

    return NF_ACCEPT;
}

/*************************************************************/
static int __init gmtp_v4_init(void)
{
    int err = 0;

    /****
    pr_info("GMTP: LAST %s err value %d\n", __FUNCTION__, err);
    while(time_before(jiffies, 50000))
        schedule();
    ****/

    gmtp_pr_func();

    inet_hashinfo_init(&gmtp_inet_hashinfo);

    err = proto_register(&gmtp_v4_prot, 1);
    if(err != 0)
        goto out;

    err = inet_add_protocol(&gmtp_protocol, IPPROTO_GMTP);
    if(err != 0)
        goto out_proto_unregister;

    inet_register_protosw(&gmtp_protosw);

    err = register_pernet_subsys(&gmtp_v4_ops);
    if(err)
        goto out_destroy_ctl_sock;

    nfho_gmtp_out.hook = hook_func_gmtp_out;
    nfho_gmtp_out.hooknum = NF_INET_LOCAL_OUT;
    nfho_gmtp_out.pf = PF_INET;
    nfho_gmtp_out.priority = NF_IP_PRI_FIRST;
    nf_register_net_hook(&init_net, &nfho_gmtp_out);

    return err;

out_destroy_ctl_sock:
    inet_unregister_protosw(&gmtp_protosw);
    inet_del_protocol(&gmtp_protocol, IPPROTO_GMTP);
    return err;

out_proto_unregister:
    proto_unregister(&gmtp_v4_prot);

out:
    return err;
}

static void __exit gmtp_v4_exit(void)
{

    gmtp_print_function();

    /* nf_unregister_hook(&nfho_gmtp_out); */
    unregister_pernet_subsys(&gmtp_v4_ops);
    inet_unregister_protosw(&gmtp_protosw);
    inet_del_protocol(&gmtp_protocol, IPPROTO_GMTP);
    proto_unregister(&gmtp_v4_prot);
}

module_init(gmtp_v4_init);
module_exit(gmtp_v4_exit);

MODULE_ALIAS_NET_PF_PROTO_TYPE(PF_INET, IPPROTO_GMTP, SOCK_GMTP);
MODULE_ALIAS_NET_PF_PROTO_TYPE(PF_INET, 0, SOCK_GMTP);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Joilnen Leite <joilnen@gmail.com>");
MODULE_AUTHOR("Mário André Menezes <mariomenezescosta@gmail.com>");
MODULE_AUTHOR("Wendell Silva Soares <wendell@ic.ufal.br>");
MODULE_AUTHOR("Leandro Melo de Sales <leandro@ic.ufal.br>");
MODULE_DESCRIPTION("GMTP - Global Media Transmission Protocol");

