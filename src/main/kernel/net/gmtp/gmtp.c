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

static struct proto gmtp_prot = {
	.name			= "GMTP",
	.owner			= THIS_MODULE,
//	.close			= dccp_close,
//	.connect		= dccp_v4_connect,
//	.disconnect		= dccp_disconnect,
//	.ioctl			= dccp_ioctl,
//	.init			= dccp_v4_init_sock,
//	.setsockopt		= dccp_setsockopt,
//	.getsockopt		= dccp_getsockopt,
//	.sendmsg		= dccp_sendmsg,
//	.recvmsg		= dccp_recvmsg,
//	.backlog_rcv		= dccp_v4_do_rcv,
	.hash			= inet_hash,
	.unhash			= inet_unhash,
	.accept			= inet_csk_accept,
	.get_port		= inet_csk_get_port,
//	.shutdown		= dccp_shutdown,
//	.destroy		= dccp_destroy_sock,
//	.orphan_count		= &dccp_orphan_count,
//	.max_header		= MAX_DCCP_HEADER,
	.obj_size		= sizeof(struct gmtp_sock),
	.slab_flags		= SLAB_DESTROY_BY_RCU,
	.rsk_prot		= &gmtp_request_sock_ops,
//	.twsk_prot		= &dccp_timewait_sock_ops,
//	.h.hashinfo		= &dccp_hashinfo,
//#ifdef CONFIG_COMPAT
//	.compat_setsockopt	= compat_dccp_setsockopt,
//	.compat_getsockopt	= compat_dccp_getsockopt,
//#endif
};

static const struct net_protocol gmtp_protocol = {
//	.handler	= dccp_v4_rcv,
//	.err_handler	= dccp_v4_err,
	.no_policy	= 1,
	.netns_ok	= 1,
	.icmp_strict_tag_validation = 1,
};

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
//	.listen		   = inet_dccp_listen,
	.shutdown	   = inet_shutdown,
	.setsockopt	   = sock_common_setsockopt,
	.getsockopt	   = sock_common_getsockopt,
	.sendmsg	   = inet_sendmsg,
	.recvmsg	   = sock_common_recvmsg,
	.mmap		   = sock_no_mmap,
	.sendpage	   = sock_no_sendpage,
//#ifdef CONFIG_COMPAT
//	.compat_setsockopt = compat_sock_common_setsockopt,
//	.compat_getsockopt = compat_sock_common_getsockopt,
//#endif
};

static struct inet_protosw gmtp_protosw = {
	.type		= SOCK_DGRAM, //SOCK_STREAM	= 1; SOCK_DGRAM	= 2, SOCK_DCCP, etc ???
	.protocol	= IPPROTO_GMTP, //100
	.prot		= &gmtp_prot,
	.ops		= &inet_gmtp_ops,
	.no_check	= 0,
	.flags		= INET_PROTOSW_ICSK,
};

/*static int __net_init dccp_v4_init_net(struct net *net)
{
	if (dccp_hashinfo.bhash == NULL)
		return -ESOCKTNOSUPPORT;

	return inet_ctl_sock_create(&net->dccp.v4_ctl_sk, PF_INET,
				    SOCK_DCCP, IPPROTO_DCCP, net);
}*/

/*static void __net_exit dccp_v4_exit_net(struct net *net)
{
	inet_ctl_sock_destroy(net->dccp.v4_ctl_sk);
}*/

/*static struct pernet_operations dccp_v4_ops = {
	.init	= dccp_v4_init_net,
	.exit	= dccp_v4_exit_net,
};*/

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

//	err = register_pernet_subsys(&gmtp_ops);
	if (err)
		goto out_destroy_ctl_sock;

out:
	return err;

out_destroy_ctl_sock:
	printk(KERN_INFO "GMTP Error: inet_unregister_protosw\n");
	inet_unregister_protosw(&gmtp_protosw);
	printk(KERN_INFO "\tinet_del_protocol\n");
	inet_del_protocol(&gmtp_protocol, IPPROTO_GMTP);
out_proto_unregister:
	printk(KERN_INFO "GMTP Error: proto_unregister\n");
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

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Wendell Silva Soares <wendell@compelab.org>");
MODULE_DESCRIPTION("GMTP - Global Media Transmission Protocol");
