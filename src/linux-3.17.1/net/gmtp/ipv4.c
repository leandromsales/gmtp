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

#include "include/net/netns/gmtp.h"
#include "gmtp.h"

extern int sysctl_ip_nonlocal_bind __read_mostly;

int gmtp_v4_connect(struct sock *sk, struct sockaddr *uaddr, int addr_len)
{
	gmtp_print_debug("gmtp_v4_connect");
	return 0;
}
EXPORT_SYMBOL_GPL(gmtp_v4_connect);

static const struct inet_connection_sock_af_ops gmtp_ipv4_af_ops = {
	.queue_xmit	   = ip_queue_xmit,
//	.send_check	   = dccp_v4_send_check,
	.rebuild_header	   = inet_sk_rebuild_header,
//	.conn_request	   = dccp_v4_conn_request,
//	.syn_recv_sock	   = dccp_v4_request_recv_sock,
	.net_header_len	   = sizeof(struct iphdr),
//	.setsockopt	   = ip_setsockopt,
//	.getsockopt	   = ip_getsockopt,
	.addr2sockaddr	   = inet_csk_addr2sockaddr,
	.sockaddr_len	   = sizeof(struct sockaddr_in),
	.bind_conflict	   = inet_csk_bind_conflict,
#ifdef CONFIG_COMPAT
	.compat_setsockopt = compat_ip_setsockopt,
	.compat_getsockopt = compat_ip_getsockopt,
#endif
};

static int gmtp_v4_init_sock(struct sock *sk)
{
	static __u8 gmtp_v4_ctl_sock_initialized;
	int err = gmtp_init_sock(sk, gmtp_v4_ctl_sock_initialized);

	if (err == 0) {
		if (unlikely(!gmtp_v4_ctl_sock_initialized))
			gmtp_v4_ctl_sock_initialized = 1;
		//TODO Study this line:
		inet_csk(sk)->icsk_af_ops = &gmtp_ipv4_af_ops;
	}

	return err;
}

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
	.close			= gmtp_close,
	.connect		= gmtp_v4_connect,
//	.disconnect		= gmtp_disconnect, //dccp_disconnect
//	.ioctl			= gmtp_ioctl, //dccp_ioctl
	.init			= gmtp_v4_init_sock,
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

/**
 * In the socket creation routine, protocol implementer specifies a
 * â€œstruct proto_opsâ€� (/include/linux/net.h) instance.
 * The socket layer calls function members of this proto_ops instance before the
 * protocol specific functions are called.
 */
static const struct proto_ops inet_gmtp_ops = {
	.family		   = PF_INET,
	.owner		   = THIS_MODULE,
	.release	   = inet_release,
	.bind		   = inet_bind,
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
 * inetsw[] array. This is an L4 protocol number (L4â†’Transport layer protocol).
 * @prot: This is a pointer to struct proto.
 * ops: This is a pointer to the structure of type â€˜proto_opsâ€™.
 */
static struct inet_protosw gmtp_protosw = {
	.type		= SOCK_GMTP,
	.protocol	= IPPROTO_GMTP,
	.prot		= &gmtp_prot,
	.ops		= &inet_gmtp_ops,
	.flags		= INET_PROTOSW_ICSK,
};

//static int __net_init gmtp_v4_init_net(struct net *net)
//{
//	if (gmtp_hashinfo.bhash == NULL)
//		return -ESOCKTNOSUPPORT;
//
//	return inet_ctl_sock_create(&net->gmtp.v4_ctl_sk, PF_INET,
//				    SOCK_GMTP, IPPROTO_DCCP, net);
//}
//
//static void __net_exit gmtp_v4_exit_net(struct net *net)
//{
//	inet_ctl_sock_destroy(net->gmtp.v4_ctl_sk);
//}
//
//static struct pernet_operations gmtp_v4_ops = {
//	.init	= gmtp_v4_init_net,
//	.exit	= gmtp_v4_exit_net,
//};

//////////////////////////////////////////////////////////
static int __init gmtp_v4_init(void)
{
	int err = 0;
	int i;

	gmtp_print_debug("GMTP IPv4 init!\n");

	inet_hashinfo_init(&gmtp_hashinfo);

	// PROTOCOLS REGISTER
	gmtp_print_debug("GMTP IPv4 proto_register\n");
	err = proto_register(&gmtp_prot, 1);
	if (err != 0)
		goto out;

	gmtp_print_debug("GMTP IPv4 inet_add_protocol\n");
	err = inet_add_protocol(&gmtp_protocol, IPPROTO_GMTP);
	if (err != 0)
		goto out_proto_unregister;

	gmtp_print_debug("GMTP IPv4 inet_register_protosw\n");
	inet_register_protosw(&gmtp_protosw);

//	err = register_pernet_subsys(&gmtp_v4_ops);
	if (err)
		goto out_destroy_ctl_sock;

	return err;

out_destroy_ctl_sock:
	gmtp_print_error("inet_unregister_protosw GMTP IPv4\n");
	inet_unregister_protosw(&gmtp_protosw);
	gmtp_print_error("inet_del_protocol GMTP IPv4\n");
	inet_del_protocol(&gmtp_protocol, IPPROTO_GMTP);
    return err;

out_proto_unregister:
	gmtp_print_error("proto_unregister GMTP IPv4\n");
	proto_unregister(&gmtp_prot);
    return err;

out:
	return err;
}

static void __exit gmtp_v4_exit(void)
{
	gmtp_print_debug("GMTP IPv4 exit!\n");

//	unregister_pernet_subsys(&gmtp_ops);
	inet_unregister_protosw(&gmtp_protosw);
	inet_del_protocol(&gmtp_protocol, IPPROTO_GMTP);
	proto_unregister(&gmtp_prot);
}

module_init(gmtp_v4_init);
module_exit(gmtp_v4_exit);

MODULE_ALIAS_NET_PF_PROTO_TYPE(PF_INET, IPPROTO_GMTP, SOCK_GMTP);
MODULE_ALIAS_NET_PF_PROTO_TYPE(PF_INET, 0, SOCK_GMTP);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Wendell Silva Soares <wendell@compelab.org>");
MODULE_DESCRIPTION("GMTP - Global Media Transmission Protocol");
