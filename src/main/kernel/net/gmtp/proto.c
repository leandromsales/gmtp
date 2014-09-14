#include <linux/types.h>
#include <net/inet_hashtables.h>

#include "gmtp.h"

void gmtp_set_state(struct sock *sk, const int state)
{
	gmtp_print_debug("gmtp_set_state");

	if(!sk)
		gmtp_print_warning("sk is NULL!");
	else
		sk->sk_state = state;
}

int inet_gmtp_listen(struct socket *sock, int backlog)
{
	int err = 0;
	if(!sock)
	{
		err = -1;
		gmtp_print_debug("sock is NULL!");
		return err;
	}

	struct sock *sk = sock->sk;
//	unsigned char old_state;


//	old_state = sk->sk_state;

	gmtp_print_debug("inet_gmtp_listen");
	return 0;
}
EXPORT_SYMBOL_GPL(inet_gmtp_listen);

/* this is called when real data arrives */
int gmtp_rcv(struct sk_buff *skb)
{
	gmtp_print_debug("gmtp_rcv");
	return 0;
}
EXPORT_SYMBOL_GPL(gmtp_rcv);

void gmtp_err(struct sk_buff *skb, u32 info)
{
	gmtp_print_debug("gmtp_err");
}
EXPORT_SYMBOL_GPL(gmtp_err);

int gmtp_init_sock(struct sock *sk)
{
	gmtp_print_debug("gmtp_init_sock");
	return 0;
}
EXPORT_SYMBOL_GPL(gmtp_init_sock);

//TODO Implement gmtp callbacks
void gmtp_close(struct sock *sk, long timeout)
{
	gmtp_print_debug("gmtp_close");
}
EXPORT_SYMBOL_GPL(gmtp_close);


int gmtp_proto_connect(struct sock *sk, struct sockaddr *uaddr, int addr_len)
{
	gmtp_print_debug("gmtp_proto_connect");
	return 0;
}
EXPORT_SYMBOL_GPL(gmtp_proto_connect);

int gmtp_disconnect(struct sock *sk, int flags)
{
	gmtp_print_debug("gmtp_disconnect");
	return 0;
}
EXPORT_SYMBOL_GPL(gmtp_disconnect);

int gmtp_ioctl(struct sock *sk, int cmd, unsigned long arg)
{
	gmtp_print_debug("gmtp_ioctl");
	return 0;
}
EXPORT_SYMBOL_GPL(gmtp_ioctl);

int gmtp_getsockopt(struct sock *sk, int level, int optname,
		    char __user *optval, int __user *optlen)
{
	gmtp_print_debug("gmtp_getsockopt");
	return 0;
}
EXPORT_SYMBOL_GPL(gmtp_getsockopt);

int gmtp_sendmsg(struct kiocb *iocb, struct sock *sk, struct msghdr *msg,
		 size_t size)
{
	gmtp_print_debug("gmtp_sendmsg");
	return 0;
}
EXPORT_SYMBOL_GPL(gmtp_sendmsg);

int gmtp_setsockopt(struct sock *sk, int level, int optname,
		    char __user *optval, unsigned int optlen)
{
	gmtp_print_debug("gmtp_setsockopt");
	return 0;
}
EXPORT_SYMBOL_GPL(gmtp_setsockopt);

int gmtp_recvmsg(struct kiocb *iocb, struct sock *sk,
		 struct msghdr *msg, size_t len, int nonblock, int flags,
		 int *addr_len)
{
	gmtp_print_debug("gmtp_recvmsg");
	return 0;
}
EXPORT_SYMBOL_GPL(gmtp_recvmsg);

int gmtp_do_rcv(struct sock *sk, struct sk_buff *skb)
{
	gmtp_print_debug("gmtp_do_rcv");
	return 0;
}
EXPORT_SYMBOL_GPL(gmtp_do_rcv);

void gmtp_shutdown(struct sock *sk, int how)
{
	gmtp_print_debug("gmtp_shutdown");
	printk(KERN_INFO "called shutdown(%x)\n", how);
}
EXPORT_SYMBOL_GPL(gmtp_shutdown);

void gmtp_destroy_sock(struct sock *sk)
{
	gmtp_print_debug("gmtp_destroy_sock");
}
EXPORT_SYMBOL_GPL(gmtp_destroy_sock);


