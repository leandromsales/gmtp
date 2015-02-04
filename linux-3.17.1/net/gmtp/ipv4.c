#include <linux/init.h>
#include <linux/module.h>

#include <linux/err.h>
#include <linux/errno.h>
#include <linux/types.h>

#include <net/inet_hashtables.h>
#include <net/inet_common.h>
#include <net/inet_connection_sock.h>
#include <net/ip.h>
#include <net/protocol.h>
#include <net/request_sock.h>
#include <net/sock.h>
#include <net/xfrm.h>
#include <net/secure_seq.h>

#include <uapi/linux/if_packet.h>
#include <uapi/linux/snmp.h>

#include <uapi/linux/gmtp.h>
#include <linux/gmtp.h>
#include "gmtp.h"

extern int sysctl_ip_nonlocal_bind __read_mostly;
extern struct inet_timewait_death_row gmtp_death_row;

static inline u64 gmtp_v4_init_sequence(const struct sk_buff *skb) {
	gmtp_print_function();
	return secure_gmtp_sequence_number(ip_hdr(skb)->daddr, ip_hdr(skb)->saddr,
			gmtp_hdr(skb)->sport, gmtp_hdr(skb)->dport);
}

int gmtp_v4_connect(struct sock *sk, struct sockaddr *uaddr, int addr_len) {

	const struct sockaddr_in *usin = (struct sockaddr_in *) uaddr;
	struct inet_sock *inet = inet_sk(sk);
	struct gmtp_sock *gp = gmtp_sk(sk);
	__be16 orig_sport, orig_dport;
	__be32 daddr, nexthop;
	struct flowi4 *fl4;
	struct rtable *rt;
	int err;
	struct ip_options_rcu *inet_opt;

	gmtp_print_function();
	gmtp_print_debug("endereco %s\n", uaddr->sa_data);

	gp->gmtps_role = GMTP_ROLE_CLIENT;

	if (addr_len < sizeof(struct sockaddr_in))
		return -EINVAL;

	if (usin->sin_family != AF_INET)
		return -EAFNOSUPPORT;

	nexthop = daddr = usin->sin_addr.s_addr;

	inet_opt = rcu_dereference_protected(inet->inet_opt,
			sock_owned_by_user(sk));
	if (inet_opt != NULL && inet_opt->opt.srr) {
		if (daddr == 0)
			return -EINVAL;
		nexthop = inet_opt->opt.faddr;
	}

	orig_sport = inet->inet_sport;
	orig_dport = usin->sin_port;
	fl4 = &inet->cork.fl.u.ip4;

	gmtp_print_debug("rt = ip_route_connect(...)");
	rt = ip_route_connect(fl4, nexthop, inet->inet_saddr, RT_CONN_FLAGS(sk),
			sk->sk_bound_dev_if,
			IPPROTO_GMTP, orig_sport, orig_dport, sk);
	if (IS_ERR(rt))
		return PTR_ERR(rt);

	if (rt->rt_flags & (RTCF_MULTICAST | RTCF_BROADCAST)) {
		ip_rt_put(rt);
		return -ENETUNREACH;
	}

	if (inet_opt == NULL || !inet_opt->opt.srr)
		daddr = fl4->daddr;

	if (inet->inet_saddr == 0)
		inet->inet_saddr = fl4->saddr;
	inet->inet_rcv_saddr = inet->inet_saddr;

	inet->inet_dport = usin->sin_port;
	inet->inet_daddr = daddr;

	inet_csk(sk)->icsk_ext_hdr_len = 0;
	if (inet_opt)
		inet_csk(sk)->icsk_ext_hdr_len = inet_opt->opt.optlen;
	/*
	 * Socket identity is still unknown (sport may be zero).
	 * However we set state to GMTP_REQUESTING and not releasing socket
	 * lock select source port, enter ourselves into the hash tables and
	 * complete initialization after this.
	 */
	gmtp_set_state(sk, GMTP_REQUESTING);
	err = inet_hash_connect(&gmtp_death_row, sk);
	if (err != 0) {
		gmtp_print_error("err = inet_hash_connect(&gmtp_death_row, sk) == %d\n",
				err);
		goto failure;
	}

	rt = ip_route_newports(fl4, rt, orig_sport, orig_dport, inet->inet_sport,
			inet->inet_dport, sk);
	if (IS_ERR(rt)) {
		err = PTR_ERR(rt);
		rt = NULL;
		goto failure;
	}
	/* OK, now commit destination to socket.  */
	sk_setup_caps(sk, &rt->dst);

	gp->gmtps_iss = secure_gmtp_sequence_number(inet->inet_saddr,
			inet->inet_daddr, inet->inet_sport, inet->inet_dport);
	gmtp_print_debug("gp->gmtps_iss = %llu", gp->gmtps_iss);

	inet->inet_id = gp->gmtps_iss ^ jiffies;

	gmtp_print_debug("calling gmtp_connect(sk)...");
	err = gmtp_connect(sk);
	gmtp_print_debug("gmtp_connect(sk) returned %d.", err);

	rt = NULL;
	if (err != 0)
		goto failure;

	out: return err;

	/*
	 * This unhashes the socket and releases the local port, if necessary.
	 */
	failure: gmtp_set_state(sk, GMTP_CLOSED);
	ip_rt_put(rt);
	sk->sk_route_caps = 0;
	inet->inet_dport = 0;
	goto out;
}
EXPORT_SYMBOL_GPL(gmtp_v4_connect);

void gmtp_v4_err(struct sk_buff *skb, u32 info) {
	gmtp_print_debug("gmtp_v4_err");
}
EXPORT_SYMBOL_GPL(gmtp_v4_err);

/**
 *	gmtp_invalid_packet  -  check for malformed packets
 *	Packets that fail these checks are ignored and do not receive Resets.
 */
int gmtp_invalid_packet(struct sk_buff *skb) {
	const struct gmtp_hdr *gh;

	//TODO Verify each packet
	gmtp_print_function();
	gmtp_print_debug("Starting packet checking");
	gmtp_print_debug("sizeof(struct gmtp_hdr): %d", sizeof(struct gmtp_hdr));
	gmtp_print_debug("skb size: %d", skb_headlen(skb));

	if (skb->pkt_type != PACKET_HOST)
		return 1;

	/* If the packet is shorter than sizeof(struct gmtp_hdr),
	 * drop packet and return */
	if (!pskb_may_pull(skb, sizeof(struct gmtp_hdr))) {
		gmtp_print_warning("pskb_may_pull failed\n");
		return 1;
	}

	gh = gmtp_hdr(skb);

	/* If P.type is not understood, drop packet and return */
	if (gh->type >= GMTP_PKT_INVALID) {
		gmtp_print_warning("invalid packet type\n");
		return 1;
	}

	/* If header checksum is incorrect, drop packet and return.
	 * (This step is completed in the AF-dependent functions.) */
	skb->csum = skb_checksum(skb, 0, skb->len, 0);

	gmtp_print_debug("The packet is valid! - OK!");

	return 0;
}

static struct dst_entry* gmtp_v4_route_skb(struct net *net, struct sock *sk,
		struct sk_buff *skb) {
	struct rtable *rt;
	const struct iphdr *iph = ip_hdr(skb);
	struct flowi4 fl4 = { .flowi4_oif = inet_iif(skb), .daddr = iph->saddr,
			.saddr = iph->daddr, .flowi4_tos = RT_CONN_FLAGS(sk),
			.flowi4_proto = sk->sk_protocol, .fl4_sport = gmtp_hdr(skb)->dport,
			.fl4_dport = gmtp_hdr(skb)->sport, };

	gmtp_print_function();

	security_skb_classify_flow(skb, flowi4_to_flowi(&fl4));
	rt = ip_route_output_flow(net, &fl4, sk);
	if (IS_ERR(rt)) {
		IP_INC_STATS_BH(net, IPSTATS_MIB_OUTNOROUTES);
		return NULL;
	}

	return &rt->dst;
}

static int gmtp_v4_send_response(struct sock *sk, struct request_sock *req) {
	int err = -1;
	struct sk_buff *skb;
	struct dst_entry *dst;
	struct flowi4 fl4;

	gmtp_print_function();

	dst = inet_csk_route_req(sk, &fl4, req);
	if (dst == NULL)
		goto out;

	skb = gmtp_make_register_reply(sk, dst, req);
	if (skb != NULL) {
		const struct inet_request_sock *ireq = inet_rsk(req);
		err = ip_build_and_send_pkt(skb, sk, ireq->ir_loc_addr,
				ireq->ir_rmt_addr, ireq->opt);
		gmtp_print_debug("Response skb header size (IP): %d", skb_headlen(skb));
		err = net_xmit_eval(err);
	}

	out: if (err == 0)
		gmtp_set_state(sk, GMTP_REQ_RECV);

	gmtp_print_debug("%s returned: %d", __FUNCTION__, err);
	dst_release(dst);
	return err;
}

static void gmtp_v4_ctl_send_reset(struct sock *sk, struct sk_buff *rxskb) {
	int err;
	const struct iphdr *rxiph;
	struct sk_buff *skb;
	struct dst_entry *dst;
	struct net *net = dev_net(skb_dst(rxskb)->dev);
	struct sock *ctl_sk = net->gmtp.v4_ctl_sk;

	gmtp_print_function();

	/* Never send a reset in response to a reset. */
	if (gmtp_hdr(rxskb)->type == GMTP_PKT_RESET)
		return;

	if (skb_rtable(rxskb)->rt_type != RTN_LOCAL)
		return;

	dst = gmtp_v4_route_skb(net, ctl_sk, rxskb);
	if (dst == NULL)
		return;

	skb = gmtp_ctl_make_reset(ctl_sk, rxskb);
	if (skb == NULL)
		goto out;

	rxiph = ip_hdr(rxskb);
	skb_dst_set(skb, dst_clone(dst));

	bh_lock_sock(ctl_sk);
	err = ip_build_and_send_pkt(skb, ctl_sk, rxiph->daddr, rxiph->saddr, NULL);
	bh_unlock_sock(ctl_sk);

	if (net_xmit_eval(err) == 0) {
//		DCCP_INC_STATS_BH(DCCP_MIB_OUTSEGS);
//		DCCP_INC_STATS_BH(DCCP_MIB_OUTRSTS);
	}

	out: dst_release(dst);
}

/* this is called when real data arrives */
static int gmtp_v4_rcv(struct sk_buff *skb) {
	const struct gmtp_hdr *gh;
	const struct iphdr *iph;
	struct sock *sk;

	gmtp_print_function();

	/* Step 1: Check header basics */
	if (gmtp_invalid_packet(skb))
		goto discard_it;

	gh = gmtp_hdr(skb);
	iph = ip_hdr(skb);

	GMTP_SKB_CB(skb)->gmtpd_seq = gh->seq;
	GMTP_SKB_CB(skb)->gmtpd_type = gh->type;

	gmtp_print_debug("%s (%d) src=%pI4@%-5d dst=%pI4@%-5d seq=%llu",
			gmtp_packet_name(gh->type),
			gh->type,
			&iph->saddr, ntohs(gh->sport),
			&iph->daddr, ntohs(gh->dport),
			(unsigned long long) GMTP_SKB_CB(skb)->gmtpd_seq);

	/* Step 2:
	 *	Look up flow ID in table and get corresponding socket */
	sk = __inet_lookup_skb(&gmtp_hashinfo, skb, gh->sport, gh->dport);

	/*
	 * Step 2:
	 *	If no socket ...
	 */
	if (sk == NULL) {
		gmtp_print_error(
				"failed to look up flow ID in table and " "get corresponding socket\n");
		goto no_gmtp_socket;
	}

	/*
	 * Step 2:
	 *	... or S.state == TIMEWAIT,
	 *		Generate Reset(No Connection) unless P.type == Reset
	 *		Drop packet and return
	 */
	if (sk->sk_state == GMTP_TIME_WAIT) {
		gmtp_print_debug("sk->sk_state == GMTP_TIME_WAIT: do_time_wait\n");
		inet_twsk_put(inet_twsk(sk));
		goto no_gmtp_socket;
	}

	if (!xfrm4_policy_check(sk, XFRM_POLICY_IN, skb))
		goto discard_and_relse;

	nf_reset(skb);
	return sk_receive_skb(sk, skb, 1);

no_gmtp_socket:
	gmtp_print_debug("no_gmtp_socket");
	if (!xfrm4_policy_check(NULL, XFRM_POLICY_IN, skb))
		goto discard_it;
	/*
	 * Step 2:
	 *	If no socket ...
	 *		Generate Reset(No Connection) unless P.type == Reset
	 *		Drop packet and return
	 */
	if (gh->type != GMTP_PKT_RESET) {
		GMTP_SKB_CB(skb)->gmtpd_reset_code = GMTP_RESET_CODE_NO_CONNECTION;
		gmtp_v4_ctl_send_reset(sk, skb);
	}

discard_it: kfree_skb(skb);
	return 0;

discard_and_relse: sock_put(sk);
	goto discard_it;
}
EXPORT_SYMBOL_GPL(gmtp_v4_rcv);

static void gmtp_v4_reqsk_destructor(struct request_sock *req) {
	gmtp_print_function();
	kfree(inet_rsk(req)->opt);
}

void gmtp_syn_ack_timeout(struct sock *sk, struct request_sock *req) {
	gmtp_print_function();
}
EXPORT_SYMBOL(gmtp_syn_ack_timeout);

static struct request_sock_ops gmtp_request_sock_ops __read_mostly = {
	.family = PF_INET,
	.obj_size = sizeof(struct gmtp_request_sock),
	.rtx_syn_ack = gmtp_v4_send_response,
	.send_ack = gmtp_reqsk_send_ack,
	.destructor = gmtp_v4_reqsk_destructor,
	.send_reset = gmtp_v4_ctl_send_reset,
	.syn_ack_timeout = gmtp_syn_ack_timeout,
};

//================================================
//================================================

void gmtp_csk_reset_keepalive_timer(struct sock *sk, unsigned long len)
{
	gmtp_print_function();
	sk_reset_timer(sk, &sk->sk_timer, jiffies + len);
}
EXPORT_SYMBOL(gmtp_csk_reset_keepalive_timer);

static inline void gmtp_csk_reqsk_queue_added(struct sock *sk,
					      const unsigned long timeout)
{
	gmtp_print_function();

	if (reqsk_queue_added(&inet_csk(sk)->icsk_accept_queue) == 0)
		gmtp_csk_reset_keepalive_timer(sk, timeout);
}

static inline u32 gmtp_synq_hash(const __be32 raddr, const __be16 rport,
				 const u32 rnd, const u32 synq_hsize)
{
	gmtp_print_function();
	u32 ret = jhash_2words((__force u32)raddr, (__force u32)rport, rnd) & (synq_hsize - 1);
	gmtp_print_debug("ret: %u", ret);
	return ret;
}

void gmtp_csk_reqsk_queue_hash_add(struct sock *sk, struct request_sock *req,
				   unsigned long timeout)
{
	struct inet_connection_sock *icsk = inet_csk(sk);
	struct listen_sock *lopt = icsk->icsk_accept_queue.listen_opt;
	const u32 h = gmtp_synq_hash(inet_rsk(req)->ir_rmt_addr,
				     inet_rsk(req)->ir_rmt_port,
				     lopt->hash_rnd, lopt->nr_table_entries);

	gmtp_print_function();

	gmtp_print_debug("h: %u", h);

	reqsk_queue_hash_req(&icsk->icsk_accept_queue, h, req, timeout);
	gmtp_csk_reqsk_queue_added(sk, timeout);
}
EXPORT_SYMBOL_GPL(gmtp_csk_reqsk_queue_hash_add);
//================================================
//================================================

int gmtp_v4_conn_request(struct sock *sk, struct sk_buff *skb) {
	struct inet_request_sock *ireq;
	struct request_sock *req;
	struct gmtp_request_sock *dreq;
	struct gmtp_skb_cb *dcb = GMTP_SKB_CB(skb);

	gmtp_print_function();

	/*
	 * FIXME GMTP must accept GMTP_PKT_REQUESTs send to broadcast or multicast
	 **/
	if (skb_rtable(skb)->rt_flags & (RTCF_BROADCAST | RTCF_MULTICAST))
		return 0; /* discard, don't send a reset here */

	/*
	 * TW buckets are converted to open requests without
	 * limitations, they conserve resources and peer is
	 * evidently real one.
	 */
	dcb->gmtpd_reset_code = GMTP_RESET_CODE_TOO_BUSY;
	if (inet_csk_reqsk_queue_is_full(sk))
		goto drop;

	/*
	 * Accept backlog is full. If we have already queued enough
	 * of warm entries in syn queue, drop request. It is better than
	 * clogging syn queue with openreqs with exponentially increasing
	 * timeout.
	 */
	if (sk_acceptq_is_full(sk) && inet_csk_reqsk_queue_young(sk) > 1)
		goto drop;

	req = inet_reqsk_alloc(&gmtp_request_sock_ops);
	if (req == NULL)
		goto drop;

	if (gmtp_reqsk_init(req, gmtp_sk(sk), skb))
		goto drop_and_free;

	dreq = gmtp_rsk(req);
	if (gmtp_parse_options(sk, dreq, skb))
		goto drop_and_free;

	if (security_inet_conn_request(sk, skb, req))
		goto drop_and_free;

	ireq = inet_rsk(req);
	ireq->ir_loc_addr = ip_hdr(skb)->daddr;
	ireq->ir_rmt_addr = ip_hdr(skb)->saddr;

	/*
	 * Step 3: Process LISTEN state
	 *
	 * Setting S.SWL/S.SWH to is deferred to dccp_create_openreq_child().
	 */

	if (gmtp_v4_send_response(sk, req))
		goto drop_and_free;

	gmtp_print_debug("sk: %p", sk);
	gmtp_print_debug("req: %p", req);
	gmtp_print_debug("GMTP_TIMEOUT_INIT: %u", GMTP_TIMEOUT_INIT);

	gmtp_print_debug("Calling gmtp_csk_reqsk_queue_hash_add...");
	//FIXME <- Tt crashs gmtp
//	inet_csk_reqsk_queue_hash_add(sk, req, GMTP_TIMEOUT_INIT);
	gmtp_csk_reqsk_queue_hash_add(sk, req, GMTP_TIMEOUT_INIT);

	return 0;

drop_and_free:
	reqsk_free(req);
drop:
	return 0;
}
EXPORT_SYMBOL_GPL(gmtp_v4_conn_request);


//=========================================================
//=========================================================

#define AF_INET_FAMILY(fam) 1

struct request_sock *gmtp_csk_search_req(const struct sock *sk,
		struct request_sock ***prevp, const __be16 rport, const __be32 raddr,
		const __be32 laddr) {

	const struct inet_connection_sock *icsk = inet_csk(sk);
	struct listen_sock *lopt = icsk->icsk_accept_queue.listen_opt;
	struct request_sock *req, **prev;

	gmtp_print_function();

	gmtp_print_debug("sk: %p", sk);
	gmtp_print_debug("icsk: %p", icsk);
	gmtp_print_debug("lopt: %p", lopt);

	gmtp_print_debug("rport: %d", ntohs(rport));
	gmtp_print_debug("raddr: %d", raddr);
	gmtp_print_debug("laddr: %d", laddr);

	if(lopt != NULL)
		gmtp_print_debug("lopt->nr_table_entries: %d", lopt->nr_table_entries);

	for (prev = &lopt->syn_table[gmtp_synq_hash(raddr, rport, lopt->hash_rnd,
			lopt->nr_table_entries)]; (req = *prev) != NULL;
			prev = &req->dl_next) {
		const struct inet_request_sock *ireq = inet_rsk(req);

		gmtp_print_debug("ireq: %p", ireq);
		gmtp_print_debug("ireq->ir_rmt_port: %d", ntohs(ireq->ir_rmt_port));
		gmtp_print_debug("ireq->ir_rmt_addr: %d", ireq->ir_rmt_addr);
		gmtp_print_debug("ireq->ir_loc_addr: %d", ireq->ir_loc_addr);
		gmtp_print_debug("AF_INET_FAMILY(req->rsk_ops->family): %d",
				AF_INET_FAMILY(req->rsk_ops->family));

		if (ireq->ir_rmt_port == rport && ireq->ir_rmt_addr == raddr
				&& ireq->ir_loc_addr == laddr
				&& AF_INET_FAMILY(req->rsk_ops->family)) {
			WARN_ON(req->sk);

			gmtp_print_debug("Inside if...");

			*prevp = prev;
			break;
		} else {
			gmtp_print_debug("Else.");
		}
	}
	gmtp_print_debug("req: %p", req);
	return req;
}
EXPORT_SYMBOL_GPL(gmtp_csk_search_req);
//=========================================================
//=========================================================

static struct sock *gmtp_v4_hnd_req(struct sock *sk, struct sk_buff *skb) {
	const struct gmtp_hdr *gh = gmtp_hdr(skb);
	const struct iphdr *iph = ip_hdr(skb);
	struct sock *nsk;
	struct request_sock **prev;

	/* Find possible connection requests. */
	//FIXME req is always null...
//	struct request_sock *req = inet_csk_search_req(sk, &prev, gh->sport,
//			iph->saddr, iph->daddr);

	struct request_sock *req = gmtp_csk_search_req(sk, &prev, gh->sport,
			iph->saddr, iph->daddr);

	if (req != NULL)
		return gmtp_check_req(sk, skb, req, prev);

	//FIXME nsk is always null...
	nsk = inet_lookup_established(sock_net(sk), &gmtp_hashinfo, iph->saddr,
			gh->sport, iph->daddr, gh->dport, inet_iif(skb));

	gmtp_print_function();

	gmtp_print_debug("req: %p", req);
	gmtp_print_debug("nsk: %p", nsk);

	if (nsk != NULL) {
		if (nsk->sk_state != GMTP_TIME_WAIT) {
			gmtp_print_debug(
					"nsk->sk_state != GMTP_TIME_WAIT (returning nsk...)");
			bh_lock_sock(nsk);
			return nsk;
		}
		gmtp_print_debug("calling inet_twsk_put(inet_twsk(nsk))");
		inet_twsk_put(inet_twsk(nsk));
		return NULL;
	}

	return sk;
}

int gmtp_v4_do_rcv(struct sock *sk, struct sk_buff *skb) {
	struct gmtp_hdr *gh = gmtp_hdr(skb);

	gmtp_print_function();

	if (sk->sk_state == GMTP_OPEN) { /* Fast path */

		gmtp_print_debug("sk_state: GMTP_OPEN (%d)", sk->sk_state);

		if (gmtp_rcv_established(sk, skb, gh, skb->len))
			goto reset;
		return 0;
	}

	/*
	 * Step 3: Process LISTEN state
	 *
	 * If P.type == Request
	 *     Generate a new socket and switch to that socket.
	 *     Set S := new socket for this port pair
	 *	      S.state = RESPOND //TODO change to Requet-Reply
	 *	  A Response packet will be generated in Step 11 *)
	 *  Otherwise,
	 *	      Generate Reset(No Connection) unless P.type == Reset
	 *	      Drop packet and return
	 */
	if (sk->sk_state == GMTP_LISTEN) {

		struct sock *nsk;

		gmtp_print_debug("sk_state: GMTP_LISTEN (%d)", sk->sk_state);

		nsk = gmtp_v4_hnd_req(sk, skb);

		if (nsk == NULL)
			goto discard;

		gmtp_print_debug("sk: %p", sk);
		gmtp_print_debug("nsk: %p", nsk);

		//TODO tratar isso
		if (nsk != sk) {
			gmtp_print_debug("nsk != sk");
			if (gmtp_child_process(sk, nsk, skb))
				goto reset;
			return 0;
		} else {
			gmtp_print_warning("nsk == sk");
		}
	}

	if (gmtp_rcv_state_process(sk, skb, gh, skb->len))
		goto reset;

	return 0;

	reset: gmtp_v4_ctl_send_reset(sk, skb);
	discard: kfree_skb(skb);
	return 0;

}
EXPORT_SYMBOL_GPL(gmtp_v4_do_rcv);

/*
 * The three way handshake has completed - we got a valid ACK or DATAACK -
 * now create the new socket.
 *
 * This is the equivalent of TCP's tcp_v4_syn_recv_sock
 */
struct sock *gmtp_v4_request_recv_sock(struct sock *sk, struct sk_buff *skb,
		struct request_sock *req, struct dst_entry *dst) {
	struct inet_request_sock *ireq;
	struct inet_sock *newinet;
	struct sock *newsk;

	gmtp_print_function();

	if (sk_acceptq_is_full(sk))
		goto exit_overflow;

	newsk = gmtp_create_openreq_child(sk, req, skb);
	if (newsk == NULL)
		goto exit_nonewsk;

	newinet = inet_sk(newsk);
	ireq = inet_rsk(req);
	newinet->inet_daddr = ireq->ir_rmt_addr;
	newinet->inet_rcv_saddr = ireq->ir_loc_addr;
	newinet->inet_saddr = ireq->ir_loc_addr;
	newinet->inet_opt = ireq->opt;
	ireq->opt = NULL;
	newinet->mc_index = inet_iif(skb);
	newinet->mc_ttl = ip_hdr(skb)->ttl;
	newinet->inet_id = jiffies;

	if (dst == NULL && (dst = inet_csk_route_child_sock(sk, newsk, req)) == NULL)
		goto put_and_exit;

	sk_setup_caps(newsk, dst);

//	dccp_sync_mss(newsk, dst_mtu(dst));

	if (__inet_inherit_port(sk, newsk) < 0)
		goto put_and_exit;
	__inet_hash_nolisten(newsk, NULL);

	return newsk;

	exit_overflow:
	NET_INC_STATS_BH(sock_net(sk), LINUX_MIB_LISTENOVERFLOWS);
	exit_nonewsk: dst_release(dst);
	exit:
	NET_INC_STATS_BH(sock_net(sk), LINUX_MIB_LISTENDROPS);
	return NULL;
	put_and_exit: inet_csk_prepare_forced_close(newsk);
	gmtp_done(newsk);
	goto exit;
}
EXPORT_SYMBOL_GPL(gmtp_v4_request_recv_sock);

void gmtp_v4_send_check(struct sock *sk, struct sk_buff *skb) {
	gmtp_print_warning("GMTP has no ckecksum!");
}
EXPORT_SYMBOL_GPL(gmtp_v4_send_check);

static const struct inet_connection_sock_af_ops gmtp_ipv4_af_ops = {
	.queue_xmit = ip_queue_xmit,
	.send_check = gmtp_v4_send_check, //GMTP has no checksum...
	.rebuild_header = inet_sk_rebuild_header,
	.conn_request =	gmtp_v4_conn_request,
	.syn_recv_sock = gmtp_v4_request_recv_sock,
	.net_header_len = sizeof(struct iphdr),
	.setsockopt = ip_setsockopt,
	.getsockopt = ip_getsockopt,
	.addr2sockaddr = inet_csk_addr2sockaddr,
	.sockaddr_len = sizeof(struct sockaddr_in),
	.bind_conflict = inet_csk_bind_conflict,
#ifdef CONFIG_COMPAT
	.compat_setsockopt = compat_ip_setsockopt,
	.compat_getsockopt = compat_ip_getsockopt,
#endif
};

static int gmtp_v4_init_sock(struct sock *sk) {

	static __u8 gmtp_v4_ctl_sock_initialized;
	int err = 0;

	gmtp_print_function();

	err = gmtp_init_sock(sk, gmtp_v4_ctl_sock_initialized);
	if (err == 0) {
		if (unlikely(!gmtp_v4_ctl_sock_initialized))
			gmtp_v4_ctl_sock_initialized = 1;

		//Setting AF options
		inet_csk(sk)->icsk_af_ops = &gmtp_ipv4_af_ops;
	}

	return err;
}

/* Networking protocol blocks we attach to sockets.
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
//	.setsockopt		= gmtp_setsockopt,
//	.getsockopt		= gmtp_getsockopt,
//	.sendmsg		= gmtp_sendmsg,
	.recvmsg = gmtp_recvmsg,
	.backlog_rcv = gmtp_v4_do_rcv,
	.hash =	inet_hash,
	.unhash = inet_unhash,
	.accept = inet_csk_accept,
	.get_port = inet_csk_get_port,
//	.shutdown		= gmtp_shutdown,
//	.destroy		= gmtp_destroy_sock,
	.max_header = MAX_GMTP_HEADER,
	.obj_size = sizeof(struct gmtp_sock),
	.slab_flags = SLAB_DESTROY_BY_RCU,
	.rsk_prot = &gmtp_request_sock_ops,
	.h.hashinfo = &gmtp_hashinfo,
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
	.netns_ok = 1, //mandatory
	.icmp_strict_tag_validation = 1,
};

/**
 * In the socket creation routine, protocol implementer specifies a
 * 'struct proto_ops' (/include/linux/net.h) instance.
 * The socket layer calls function members of this proto_ops instance before the
 * protocol specific functions are called.
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
//	.poll		   = dccp_poll,
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
	.protocol =  IPPROTO_GMTP,
	.prot = &gmtp_v4_prot,
	.ops = &inet_gmtp_ops,
	.flags = INET_PROTOSW_ICSK,
};

static int __net_init gmtp_v4_init_net(struct net *net) {
	gmtp_print_function();

	if (gmtp_hashinfo.bhash == NULL)
		return -ESOCKTNOSUPPORT;

	return inet_ctl_sock_create(&net->gmtp.v4_ctl_sk, PF_INET, SOCK_GMTP,
	IPPROTO_GMTP, net);
}

static void __net_exit gmtp_v4_exit_net(struct net *net) {
	gmtp_print_function();
	inet_ctl_sock_destroy(net->gmtp.v4_ctl_sk);
}

static struct pernet_operations gmtp_v4_ops = {
	.init = gmtp_v4_init_net,
	.exit = gmtp_v4_exit_net,
};

//////////////////////////////////////////////////////////
static int __init gmtp_v4_init(void) {
	int err = 0;

	gmtp_print_debug("GMTP IPv4 init!");
	gmtp_print_function();

	inet_hashinfo_init(&gmtp_hashinfo);

	// PROTOCOLS REGISTER
	gmtp_print_debug("GMTP IPv4 proto_register");
	err = proto_register(&gmtp_v4_prot, 1);
	if (err != 0)
		goto out;

	gmtp_print_debug("GMTP IPv4 inet_add_protocol");
	err = inet_add_protocol(&gmtp_protocol, IPPROTO_GMTP);
	if (err != 0)
		goto out_proto_unregister;

	gmtp_print_debug("GMTP IPv4 inet_register_protosw");
	inet_register_protosw(&gmtp_protosw);

	err = register_pernet_subsys(&gmtp_v4_ops);
	if (err)
		goto out_destroy_ctl_sock;

	return err;

	out_destroy_ctl_sock:
	gmtp_print_error("inet_unregister_protosw GMTP IPv4");
	inet_unregister_protosw(&gmtp_protosw);
	gmtp_print_error("inet_del_protocol GMTP IPv4");
	inet_del_protocol(&gmtp_protocol, IPPROTO_GMTP);
	return err;

	out_proto_unregister:
	gmtp_print_error("proto_unregister GMTP IPv4");
	proto_unregister(&gmtp_v4_prot);
	return err;

	out: return err;
}

static void __exit gmtp_v4_exit(void) {

	gmtp_print_function();
	gmtp_print_debug("GMTP IPv4 exit!");

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
MODULE_AUTHOR("Wendell Silva Soares <wss@ic.ufal.br>");
MODULE_DESCRIPTION("GMTP - Global Media Transmission Protocol");
