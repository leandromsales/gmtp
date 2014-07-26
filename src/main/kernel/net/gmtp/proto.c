#include <linux/types.h>
#include <net/inet_hashtables.h>

#include "gmtp.h"

void gmtp_set_state(struct sock *sk, const int state)
{
//	const int oldstate = sk->sk_state;

//	switch (state) {
//	default:
//		break;
	//TODO make treatment on set new state
//	}

	/* Change state AFTER socket is unhashed to avoid closed
	 * socket sitting in hash tables.
	 */
	sk->sk_state = state;
}

/* this is called when real data arrives */
int gmtp_rcv(struct sk_buff *skb)
{
	return 0;
}
EXPORT_SYMBOL_GPL(gmtp_rcv);

void gmtp_err(struct sk_buff *skb, u32 info)
{

}
EXPORT_SYMBOL_GPL(gmtp_err);

int gmtp_init_sock(struct sock *sk)
{
//	struct gmtp_sock *gs = gmtp_sk(sk);
//	struct inet_connection_sock *icsk = inet_csk(sk);

	sk->sk_state		= GMTP_CLOSED;

	/* Specific GMTP initialization
	 * (...)
	 */
	//init timers
	//init structures INIT_LIST_HEAD

	return 0;
}
EXPORT_SYMBOL_GPL(gmtp_init_sock);

//TODO Implement gmtp callbacks
void gmtp_close(struct sock *sk, long timeout)
{

}
EXPORT_SYMBOL_GPL(gmtp_close);


int gmtp_proto_connect(struct sock *sk, struct sockaddr *uaddr, int addr_len)
{

	const struct sockaddr_in *usin = (struct sockaddr_in *)uaddr;
	struct inet_sock *inet = inet_sk(sk);
	struct gmtp_sock *gs = gmtp_sk(sk);
	__be16 orig_sport, orig_dport;
	__be32 daddr, nexthop;
	struct flowi4 *fl4;
	struct rtable *rt;
	int err;
	struct ip_options_rcu *inet_opt;

	gs->gmtps_role = GMTP_ROLE_CLIENT;

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
	rt = ip_route_connect(fl4, nexthop, inet->inet_saddr,
			      RT_CONN_FLAGS(sk), sk->sk_bound_dev_if,
			      IPPROTO_GMTP,
			      orig_sport, orig_dport, sk);
	if (IS_ERR(rt))
		return PTR_ERR(rt);

//	if (rt->rt_flags & (RTCF_MULTICAST | RTCF_BROADCAST)) {
//		ip_rt_put(rt);
//		return -ENETUNREACH;
//	}

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

	rt = ip_route_newports(fl4, rt, orig_sport, orig_dport,
			       inet->inet_sport, inet->inet_dport, sk);
	if (IS_ERR(rt)) {
		err = PTR_ERR(rt);
		rt = NULL;
		goto failure;
	}
	/* OK, now commit destination to socket.  */
	sk_setup_caps(sk, &rt->dst);

//	gs->dccps_iss = secure_dccp_sequence_number(inet->inet_saddr,
//						    inet->inet_daddr,
//						    inet->inet_sport,
//						    inet->inet_dport);
//	inet->inet_id = gs->dccps_iss ^ jiffies;

	err = gmtp_connect(sk);
	rt = NULL;
	if (err != 0)
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
EXPORT_SYMBOL_GPL(gmtp_proto_connect);

int gmtp_disconnect(struct sock *sk, int flags)
{
	return 0;
}
EXPORT_SYMBOL_GPL(gmtp_disconnect);

int gmtp_ioctl(struct sock *sk, int cmd, unsigned long arg)
{
	return 0;
}
EXPORT_SYMBOL_GPL(gmtp_ioctl);

int gmtp_getsockopt(struct sock *sk, int level, int optname,
		    char __user *optval, int __user *optlen)
{
	return 0;
}
EXPORT_SYMBOL_GPL(gmtp_getsockopt);

int gmtp_sendmsg(struct kiocb *iocb, struct sock *sk, struct msghdr *msg,
		 size_t size)
{
	return 0;
}
EXPORT_SYMBOL_GPL(gmtp_sendmsg);

int gmtp_setsockopt(struct sock *sk, int level, int optname,
		    char __user *optval, unsigned int optlen)
{
	return 0;
}
EXPORT_SYMBOL_GPL(gmtp_setsockopt);

int gmtp_recvmsg(struct kiocb *iocb, struct sock *sk,
		 struct msghdr *msg, size_t len, int nonblock, int flags,
		 int *addr_len)
{
	return 0;
}
EXPORT_SYMBOL_GPL(gmtp_recvmsg);

int gmtp_do_rcv(struct sock *sk, struct sk_buff *skb)
{
	return 0;
}
EXPORT_SYMBOL_GPL(gmtp_do_rcv);

void gmtp_shutdown(struct sock *sk, int how)
{
	printk(KERN_INFO "called shutdown(%x)\n", how);
}
EXPORT_SYMBOL_GPL(gmtp_shutdown);

void gmtp_destroy_sock(struct sock *sk)
{
//	struct gmtp_sock *gs = gmtp_sk(sk);

	/*
	 * DCCP doesn't use sk_write_queue, just sk_send_head
	 * for retransmissions
	 * GMTP: ???
	 */
	if (sk->sk_send_head != NULL) {
		kfree_skb(sk->sk_send_head);
		sk->sk_send_head = NULL;
	}

	/* Clean up a referenced gmtp bind bucket. */
	if (inet_csk(sk)->icsk_bind_hash != NULL)
		inet_put_port(sk);


	/* clean up feature negotiation state */
//	dccp_feat_list_purge(&gs->dccps_featneg);
}
EXPORT_SYMBOL_GPL(gmtp_destroy_sock);

int inet_gmtp_listen(struct socket *sock, int backlog)
{
	return 0;
}
EXPORT_SYMBOL_GPL(inet_gmtp_listen);

