#include <linux/init.h>
#include <linux/module.h>

#include <linux/err.h>
#include <linux/errno.h>

#include <net/inet_hashtables.h>
#include <net/inet_common.h>
#include <net/ip.h>
#include <net/protocol.h>
#include <net/request_sock.h>
#include <net/sock.h>
#include <net/xfrm.h>
#include <net/secure_seq.h>

#include <linux/gmtp.h>

#include "gmtp.h"

extern int sysctl_ip_nonlocal_bind __read_mostly;
extern struct inet_timewait_death_row gmtp_death_row;

static inline u64 gmtp_v4_init_sequence(const struct sk_buff *skb) {
	return secure_gmtp_sequence_number(ip_hdr(skb)->daddr, ip_hdr(skb)->saddr,
			gmtp_hdr(skb)->sport, gmtp_hdr(skb)->dport);
}

int gmtp_v4_connect(struct sock *sk, struct sockaddr *uaddr, int addr_len) {
	gmtp_print_debug("gmtp_v4_connect");
	const struct sockaddr_in *usin = (struct sockaddr_in *) uaddr;
	struct inet_sock *inet = inet_sk(sk);
	struct gmtp_sock *gp = gmtp_sk(sk);
	__be16 orig_sport, orig_dport;
	__be32 daddr, nexthop;
	struct flowi4 *fl4;
	struct rtable *rt;
	int err;
	struct ip_options_rcu *inet_opt;

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
	gmtp_print_error("gp->gmtps_iss = %u", gp->gmtps_iss);

	inet->inet_id = gp->gmtps_iss ^ jiffies;

	gmtp_print_debug("calling gmtp_connect(sk)");
	err = gmtp_connect(sk);
	gmtp_print_warning("err = gmtp_connect(sk) == %d", err);

	rt = NULL;
	if (err != 0)
		goto failure;
	out: return err;
	failure:
	/*
	 * This unhashes the socket and releases the local port, if necessary.
	 */
	gmtp_set_state(sk, GMTP_CLOSED);
	ip_rt_put(rt);
	sk->sk_route_caps = 0;
	inet->inet_dport = 0;
	goto out;

	return 0;
}
EXPORT_SYMBOL_GPL(gmtp_v4_connect);

void gmtp_v4_err(struct sk_buff *skb, u32 info)
{
	gmtp_print_debug("gmtp_v4_err");
}
EXPORT_SYMBOL_GPL(gmtp_v4_err);

static void gmtp_v4_ctl_send_reset(struct sock *sk, struct sk_buff *rxskb)
{
	//TODO Implement this...
	gmtp_print_debug("gmtp_v4_ctl_send_reset");
}

int gmtp_invalid_packet(struct sk_buff *skb)
{
	//TODO Verify each packet

	const struct gmtp_hdr *gh;

	if (skb->pkt_type != PACKET_HOST)
		return 1;

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

	return 0;
}

/* this is called when real data arrives */
static int gmtp_v4_rcv(struct sk_buff *skb)
{
	gmtp_print_debug("gmtp_v4_rcv");

	const struct gmtp_hdr *gh;
	const struct iphdr *iph;
	struct sock *sk;

	/* Step 1: Check header basics */

	if (gmtp_invalid_packet(skb))
		goto discard_it;

	GMTP_SKB_CB(skb)->gmtpd_seq  = gh->seq;
	GMTP_SKB_CB(skb)->gmtpd_type = gh->type;

	gmtp_print_debug("%8.8s src=%pI4@%-5d dst=%pI4@%-5d seq=%llu",
		      gmtp_packet_name(gh->type),
		      &iph->saddr, ntohs(gh->sport),
		      &iph->daddr, ntohs(gh->dport),
		      (unsigned long long) GMTP_SKB_CB(skb)->gmtpd_seq);


//	if (dccp_packet_without_ack(skb)) {
//		DCCP_SKB_CB(skb)->dccpd_ack_seq = DCCP_PKT_WITHOUT_ACK_SEQ;
//		dccp_pr_debug_cat("\n");
//	} else {
//		DCCP_SKB_CB(skb)->dccpd_ack_seq = dccp_hdr_ack_seq(skb);
//		dccp_pr_debug_cat(", ack=%llu\n", (unsigned long long)
//				  DCCP_SKB_CB(skb)->dccpd_ack_seq);
//	}


	/* Step 2:
	 *	Look up flow ID in table and get corresponding socket */
	sk = __inet_lookup_skb(&gmtp_hashinfo, skb,
			       gh->sport, gh->dport);
	/*
	 * Step 2:
	 *	If no socket ...
	 */
	if (sk == NULL) {
		gmtp_print_debug("failed to look up flow ID in table and "
			      "get corresponding socket\n");
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
	if (!xfrm4_policy_check(NULL, XFRM_POLICY_IN, skb))
		goto discard_it;
	/*
	 * Step 2:
	 *	If no socket ...
	 *		Generate Reset(No Connection) unless P.type == Reset
	 *		Drop packet and return
	 */
	if (gh->type != GMTP_PKT_RESET) {
		GMTP_SKB_CB(skb)->gmtpd_reset_code =
					GMTP_RESET_CODE_NO_CONNECTION;
		gmtp_v4_ctl_send_reset(sk, skb); //TODO serah implementado...
	}

discard_it:
	kfree_skb(skb);
	return 0;

discard_and_relse:
	sock_put(sk);
	goto discard_it;
}
EXPORT_SYMBOL_GPL(gmtp_v4_rcv);

static const struct inet_connection_sock_af_ops gmtp_ipv4_af_ops = {
	.queue_xmit = ip_queue_xmit,
//	.send_check	   = gmtp_v4_send_check, //GMTP has no checksum...
	.rebuild_header = inet_sk_rebuild_header,
//	.conn_request	   = dccp_v4_conn_request,
//	.syn_recv_sock	   = dccp_v4_request_recv_sock,
	.net_header_len = sizeof(struct iphdr),
	.setsockopt	   = ip_setsockopt,
	.getsockopt	   = ip_getsockopt,
	.addr2sockaddr = inet_csk_addr2sockaddr,
	.sockaddr_len  = sizeof(struct sockaddr_in),
	.bind_conflict = inet_csk_bind_conflict,
#ifdef CONFIG_COMPAT
	.compat_setsockopt = compat_ip_setsockopt,
	.compat_getsockopt = compat_ip_getsockopt,
#endif
};

static int gmtp_v4_init_sock(struct sock *sk) {
	gmtp_print_debug("gmtp_v4_init_sock");
	static __u8 gmtp_v4_ctl_sock_initialized;
	int err = gmtp_init_sock(sk, gmtp_v4_ctl_sock_initialized);

	if (err == 0) {
		if (unlikely(!gmtp_v4_ctl_sock_initialized))
			gmtp_v4_ctl_sock_initialized = 1;

		//Setting AF options
		inet_csk(sk)->icsk_af_ops = &gmtp_ipv4_af_ops;
	}

	return err;
}

static struct request_sock_ops gmtp_request_sock_ops __read_mostly = {
	.family = PF_INET,
	.obj_size = sizeof(struct gmtp_request_sock)
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
	.no_policy = 1, .netns_ok = 1, //mandatory
	.icmp_strict_tag_validation = 1, };

/* Networking protocol blocks we attach to sockets.
 * socket layer -> transport layer interface (struct proto)
 * transport -> network interface is defined by (struct inet_proto)
 */
static struct proto gmtp_prot = {
	.name = "GMTP",
	.owner = THIS_MODULE,
	.close = gmtp_close,
	.connect = gmtp_v4_connect,
	.disconnect = gmtp_disconnect,
//	.ioctl			= gmtp_ioctl,
	.init = gmtp_v4_init_sock,
//	.setsockopt		= gmtp_setsockopt,
//	.getsockopt		= gmtp_getsockopt,
//	.sendmsg		= gmtp_sendmsg,
//	.recvmsg		= gmtp_recvmsg,
	.hash = inet_hash,
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
	.protocol =	IPPROTO_GMTP,
	.prot = &gmtp_prot,
	.ops = &inet_gmtp_ops,
	.flags = INET_PROTOSW_ICSK,
};

static int __net_init gmtp_v4_init_net(struct net *net) {
	gmtp_print_debug("gmtp_v4_init_net");
	if (gmtp_hashinfo.bhash == NULL)
		return -ESOCKTNOSUPPORT;

	return inet_ctl_sock_create(&net->gmtp.v4_ctl_sk, PF_INET, SOCK_GMTP,
			IPPROTO_GMTP, net);
}

static void __net_exit gmtp_v4_exit_net(struct net *net) {
	gmtp_print_debug("gmtp_v4_exit_net");
	inet_ctl_sock_destroy(net->gmtp.v4_ctl_sk);
}

static struct pernet_operations gmtp_v4_ops = { .init = gmtp_v4_init_net,
		.exit = gmtp_v4_exit_net, };

//////////////////////////////////////////////////////////
static int __init gmtp_v4_init(void) {
	int err = 0;
	int i;

	gmtp_print_debug("GMTP IPv4 init!");

	inet_hashinfo_init(&gmtp_hashinfo);

	// PROTOCOLS REGISTER
	gmtp_print_debug("GMTP IPv4 proto_register");
	err = proto_register(&gmtp_prot, 1);
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
	proto_unregister(&gmtp_prot);
	return err;

	out: return err;
}

static void __exit gmtp_v4_exit(void) {
	gmtp_print_debug("GMTP IPv4 exit!");

	unregister_pernet_subsys(&gmtp_v4_ops);
	inet_unregister_protosw(&gmtp_protosw);
	inet_del_protocol(&gmtp_protocol, IPPROTO_GMTP);
	proto_unregister(&gmtp_prot);
}

module_init(gmtp_v4_init);
module_exit(gmtp_v4_exit);

MODULE_ALIAS_NET_PF_PROTO_TYPE(PF_INET, IPPROTO_GMTP, SOCK_GMTP);
MODULE_ALIAS_NET_PF_PROTO_TYPE(PF_INET, 0, SOCK_GMTP);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Wendell Silva Soares <wss@ic.ufal.br>");
MODULE_DESCRIPTION("GMTP - Global Media Transmission Protocol");
