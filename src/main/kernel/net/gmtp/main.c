#include <linux/init.h>
#include <linux/module.h>

#include <net/sock.h>
#include <net/request_sock.h>
#include <net/protocol.h>
#include <net/inet_hashtables.h>
#include <net/inet_common.h>

#include "gmtp.h"

/**
 * BASED ON IPV4 DCCP
 */
static struct request_sock_ops gmtp_request_sock_ops __read_mostly = {
	.family		= PF_INET,
	.obj_size	= sizeof(struct gmtp_request_sock)
//	.rtx_syn_ack	= dccp_v4_send_response,
//	.send_ack	= dccp_reqsk_send_ack,
//	.destructor	= dccp_v4_reqsk_destructor,
//	.send_reset	= dccp_v4_ctl_send_reset,
//	.syn_ack_timeout = dccp_syn_ack_timeout,
};

/**
 * We define the gmtp_protocol object (net_protocol object) and add it with the
 * inet_add_protocol() method.
 * This sets the gmtp_protocol object to be an element in the global
 * protocols array (inet_protos).
 */
static const struct net_protocol gmtp_protocol = {
//	.handler	= dccp_v4_rcv,
//	.err_handler	= dccp_v4_err,
	.no_policy	= 1,
	.netns_ok	= 1,
	.icmp_strict_tag_validation = 1,
};

static int gmtp_init_sock(struct sock *sk)
{
	return 0;
}
EXPORT_SYMBOL_GPL(gmtp_init_sock);

/**
 * We further define a gmtp_prot object and register it by calling the
 * proto_register() method. This object contains mostly callbacks.
 * These callbacks are invoked when opening a GMTP socket in userspace and using
 * the socket API. For example, calling the setsockopt() system call on a GMTP
 * socket will invoke the gmtp_setsockopt() callback.
 */
static struct proto gmtp_prot = {
	.name			= "GMTP",
	.owner			= THIS_MODULE,
	.close			= gmtp_close, //dccp_close
	.connect		= gmtp_connect, //dccp_v4_connect
	.disconnect		= gmtp_disconnect, //dccp_disconnect
	.ioctl			= gmtp_ioctl, //dccp_ioctl
	.init			= gmtp_init_sock,  //dccp_v4_init_sock
	.setsockopt		= gmtp_setsockopt, //dccp_setsockopt
	.getsockopt		= gmtp_getsockopt, //dccp_getsockopt
	.sendmsg		= gmtp_sendmsg, //dccp_sendmsg
	.recvmsg		= gmtp_recvmsg, //dccp_recvmsg
//	.backlog_rcv		= dccp_v4_do_rcv,
	.hash			= inet_hash,
	.unhash			= inet_unhash,
	.accept			= inet_csk_accept,
	.get_port		= inet_csk_get_port,
	.shutdown		= gmtp_shutdown,  //dccp_shutdown
	.destroy		= gmtp_destroy_sock, //dccp_destroy_sock
//	.orphan_count		= &dccp_orphan_count,
	.max_header		= MAX_GMTP_HEADER,
	.obj_size		= sizeof(struct gmtp_sock),
	.slab_flags		= SLAB_DESTROY_BY_RCU,
	.rsk_prot		= &gmtp_request_sock_ops,
//	.twsk_prot		= &dccp_timewait_sock_ops,
//	.h.hashinfo		= &dccp_hashinfo,
};

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

	printk(KERN_INFO "GMTP Init!\n");
	printk(KERN_INFO "GMTP proto_register\n");
	err = proto_register(&gmtp_prot, 1);
	if (err != 0)
		goto out;

	printk(KERN_INFO "GMTP inet_add_protocol\n");
	err = inet_add_protocol(&gmtp_protocol, IPPROTO_GMTP);
	if (err != 0)
		goto out_proto_unregister;

	printk(KERN_INFO "GMTP inet_register_protosw\n");
	inet_register_protosw(&gmtp_protosw);

	if (err)
		goto out_destroy_ctl_sock;

out:
	return err;

out_destroy_ctl_sock:
	printk(KERN_ERR "GMTP Error: inet_unregister_protosw\n");
	inet_unregister_protosw(&gmtp_protosw);
	printk(KERN_ERR "\tinet_del_protocol\n");
	inet_del_protocol(&gmtp_protocol, IPPROTO_GMTP);
out_proto_unregister:
	printk(KERN_ERR "GMTP Error: proto_unregister\n");
	proto_unregister(&gmtp_prot);
	goto out;
}

static void __exit gmtp_exit(void)
{
	printk(KERN_INFO "GMTP Exit!\n");
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
