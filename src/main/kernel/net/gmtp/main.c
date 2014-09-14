#include <linux/init.h>
#include <linux/module.h>

#include <linux/err.h>
#include <linux/errno.h>
#include <net/sock.h>
#include <net/request_sock.h>
#include <net/protocol.h>
#include <net/inet_hashtables.h>
#include <net/inet_common.h>

#include "gmtp.h"

extern int sysctl_ip_nonlocal_bind __read_mostly;

struct inet_hashinfo gmtp_hashinfo;
EXPORT_SYMBOL_GPL(gmtp_hashinfo);

static struct request_sock_ops gmtp_request_sock_ops __read_mostly = {
	.family		= PF_INET,
	.obj_size	= sizeof(struct gmtp_request_sock)
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
	.handler	= gmtp_rcv,  //dccp_v4_rcv
	.err_handler	= gmtp_err, //dccp_v4_err
	.no_policy	= 1,
	.netns_ok	= 1, //mandatory
	.icmp_strict_tag_validation = 1,
};

/* Networking protocol blocks we attach to sockets.
 * socket layer -> transport layer interface (struct proto)
 * transport -> network interface is defined by (struct inet_proto)
 */
static struct proto gmtp_prot = {
	.name			= "GMTP",
	.owner			= THIS_MODULE,
	.close			= gmtp_close, //dccp_close
//	.connect		= gmtp_proto_connect, //dccp_v4_connect
//	.disconnect		= gmtp_disconnect, //dccp_disconnect
//	.ioctl			= gmtp_ioctl, //dccp_ioctl
//	.init			= gmtp_init_sock,  //dccp_v4_init_sock
//	.setsockopt		= gmtp_setsockopt, //dccp_setsockopt
//	.getsockopt		= gmtp_getsockopt, //dccp_getsockopt
//	.sendmsg		= gmtp_sendmsg, //dccp_sendmsg
//	.recvmsg		= gmtp_recvmsg, //dccp_recvmsg
	.hash			= inet_hash,
	.unhash			= inet_unhash,
	.accept			= inet_csk_accept,
	.get_port		= inet_csk_get_port,
//	.shutdown		= gmtp_shutdown,  //dccp_shutdown
//	.destroy		= gmtp_destroy_sock, //dccp_destroy_sock
	.max_header		= MAX_GMTP_HEADER,
	.obj_size		= sizeof(struct gmtp_sock),
	.slab_flags		= SLAB_DESTROY_BY_RCU,
	.rsk_prot		= &gmtp_request_sock_ops,
	.h.hashinfo		= &gmtp_hashinfo,          //dccp_hashinfo,
};

static int gmtp_inet_bind(struct socket *sock, struct sockaddr *uaddr, int addr_len)
{
	gmtp_print_debug("gmtp_inet_bind\n");

	struct sockaddr_in *addr = (struct sockaddr_in *)uaddr;
	gmtp_print_debug("1\n");

	struct sock *sk = sock->sk;
	struct inet_sock *inet = inet_sk(sk);
	gmtp_print_debug("2\n");

	struct net *net = sock_net(sk);
	gmtp_print_debug("3\n");

	unsigned short snum;
	int chk_addr_ret;
	int err;

	/* If the socket has its own bind function then use it. (RAW) */
	if (sk->sk_prot->bind) {
		gmtp_print_debug("5\n");
		err = sk->sk_prot->bind(sk, uaddr, addr_len);
		goto out;
	}
	gmtp_print_debug("6\n");

	err = -EINVAL;
	if (addr_len < sizeof(struct sockaddr_in))
		goto out;
	gmtp_print_debug("7\n");

	if (addr->sin_family != AF_INET) {
		/* Compatibility games : accept AF_UNSPEC (mapped to AF_INET)
		 * only if s_addr is INADDR_ANY.
		 */
		gmtp_print_debug("8\n");
		err = -EAFNOSUPPORT;
		if (addr->sin_family != AF_UNSPEC ||
				addr->sin_addr.s_addr != htonl(INADDR_ANY))
			goto out;
	}
	gmtp_print_debug("9\n");

	chk_addr_ret = inet_addr_type(net, addr->sin_addr.s_addr);

	/* Not specified by any standard per-se, however it breaks too
	 * many applications when removed.  It is unfortunate since
	 * allowing applications to make a non-local bind solves
	 * several problems with systems using dynamic addressing.
	 * (ie. your servers still start up even if your ISDN link
	 *  is temporarily down)
	 */
	gmtp_print_debug("10\n");

	err = -EADDRNOTAVAIL;
	if (!sysctl_ip_nonlocal_bind &&
			!(inet->freebind || inet->transparent) &&
			addr->sin_addr.s_addr != htonl(INADDR_ANY) &&
			chk_addr_ret != RTN_LOCAL &&
			chk_addr_ret != RTN_MULTICAST &&
			chk_addr_ret != RTN_BROADCAST) {
		gmtp_print_debug("11\n");
		goto out;
	}
	gmtp_print_debug("12\n");

	snum = ntohs(addr->sin_port);
	err = -EACCES;
	gmtp_print_debug("13\n");

	if (snum && snum < PROT_SOCK &&
			!ns_capable(net->user_ns, CAP_NET_BIND_SERVICE))
		goto out;

	/*      We keep a pair of addresses. rcv_saddr is the one
	 *      used by hash lookups, and saddr is used for transmit.
	 *
	 *      In the BSD API these are the same except where it
	 *      would be illegal to use them (multicast/broadcast) in
	 *      which case the sending device address is used.
	 */
	gmtp_print_debug("14\n");

	lock_sock(sk);
	gmtp_print_debug("15\n");
	/* Check these errors (active socket, double bind). */
	err = -EINVAL;
	if (sk->sk_state != TCP_CLOSE || inet->inet_num)
	{
		goto out_release_sock;
		gmtp_print_debug("16.A\n");
	}
	else
	{
		gmtp_print_debug("16.B\n");
	}

	inet->inet_rcv_saddr = inet->inet_saddr = addr->sin_addr.s_addr;
	if (chk_addr_ret == RTN_MULTICAST || chk_addr_ret == RTN_BROADCAST)
	{
		gmtp_print_debug("16.C\n");
		inet->inet_saddr = 0;  /* Use device */
	}
	gmtp_print_debug("17\n");

	/* Make sure we are allowed to bind here. */
	//FIXME ERRO FATAL DO BIND < AQUI!
	if (sk->sk_prot->get_port(sk, snum)) {
		gmtp_print_debug("17.ERR\n");
		inet->inet_saddr = inet->inet_rcv_saddr = 0;
		err = -EADDRINUSE;
		goto out_release_sock;
	}
//
	gmtp_print_debug("18\n");
	if (inet->inet_rcv_saddr)
		sk->sk_userlocks |= SOCK_BINDADDR_LOCK;
	if (snum)
		sk->sk_userlocks |= SOCK_BINDPORT_LOCK;
	gmtp_print_debug("19\n");
	inet->inet_sport = htons(inet->inet_num);
	inet->inet_daddr = 0;
	inet->inet_dport = 0;
	gmtp_print_debug("20\n");
	sk_dst_reset(sk);

	gmtp_print_debug("21\n");
	err = 0;
	gmtp_print_debug("22\n");

	out_release_sock:
	gmtp_print_debug("23\n");
	release_sock(sk);
	gmtp_print_debug("24\n");

	out:
	if(err!=0)
		gmtp_print_error("ERROR\n");
	else
		gmtp_print_error("EVERYTHING OK!\n");

	return err;
}

/**
 * In the socket creation routine, protocol implementer specifies a
 * “struct proto_ops” (/include/linux/net.h) instance.
 * The socket layer calls function members of this proto_ops instance before the
 * protocol specific functions are called.
 */
static const struct proto_ops inet_gmtp_ops = {
	.family		   = PF_INET,
	.owner		   = THIS_MODULE,
	.release	   = inet_release,
	.bind		   = gmtp_inet_bind, //inet_bind,
	.connect	   = inet_stream_connect,
	.socketpair	   = sock_no_socketpair,
	.accept		   = inet_accept,
	.getname	   = inet_getname,
//	.poll		   = dccp_poll,
	.ioctl		   = inet_ioctl,
	.listen		   = inet_gmtp_listen, //inet_dccp_listen
	.shutdown	   = inet_shutdown,
	.setsockopt	   = sock_common_setsockopt,
	.getsockopt	   = sock_common_getsockopt,
	.sendmsg	   = inet_sendmsg,
	.recvmsg	   = sock_common_recvmsg,
	.mmap		   = sock_no_mmap,
	.sendpage	   = sock_no_sendpage,
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
 * inetsw[] array. This is an L4 protocol number (L4→Transport layer protocol).
 * @prot: This is a pointer to struct proto.
 * ops: This is a pointer to the structure of type ‘proto_ops’.
 */
static struct inet_protosw gmtp_protosw = {
	.type		= SOCK_GMTP,
	.protocol	= IPPROTO_GMTP,
	.prot		= &gmtp_prot,
	.ops		= &inet_gmtp_ops,
	.no_check	= 0,
	.flags		= INET_PROTOSW_ICSK,
};

//////////////////////////////////////////////////////////
static int __init gmtp_init(void)
{
	int err = 0;
	int i;

	gmtp_print_debug("GMTP Init!\n");

	inet_hashinfo_init(&gmtp_hashinfo);

	// PROTOCOLS REGISTER
	gmtp_print_debug("GMTP proto_register\n");
	err = proto_register(&gmtp_prot, 1);
	if (err != 0)
		goto out;

	gmtp_print_debug("GMTP inet_add_protocol\n");
	err = inet_add_protocol(&gmtp_protocol, IPPROTO_GMTP);
	if (err != 0)
		goto out_proto_unregister;

	gmtp_print_debug("GMTP inet_register_protosw\n");
	inet_register_protosw(&gmtp_protosw);

	if (err)
		goto out_destroy_ctl_sock;

out:
	return err;

out_destroy_ctl_sock:
	gmtp_print_error("inet_unregister_protosw\n");
	inet_unregister_protosw(&gmtp_protosw);
	gmtp_print_error("inet_del_protocol\n");
	inet_del_protocol(&gmtp_protocol, IPPROTO_GMTP);

out_proto_unregister:
	gmtp_print_error("proto_unregister\n");
	proto_unregister(&gmtp_prot);
	goto out;
}

static void __exit gmtp_exit(void)
{
	gmtp_print_debug("GMTP Exit!\n");

//	unregister_pernet_subsys(&gmtp_ops);
	inet_unregister_protosw(&gmtp_protosw);
	inet_del_protocol(&gmtp_protocol, IPPROTO_GMTP);
	proto_unregister(&gmtp_prot);
}

module_init(gmtp_init);
module_exit(gmtp_exit);

MODULE_ALIAS_NET_PF_PROTO_TYPE(PF_INET, IPPROTO_GMTP, SOCK_GMTP);
MODULE_ALIAS_NET_PF_PROTO_TYPE(PF_INET, 0, SOCK_GMTP);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Wendell Silva Soares <wendell@compelab.org>");
MODULE_DESCRIPTION("GMTP - Global Media Transmission Protocol");
