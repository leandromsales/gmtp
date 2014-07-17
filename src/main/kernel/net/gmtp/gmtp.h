/*
 * gmtp.h
 *
 *  Created on: 18/06/2014
 *      Author: Wendell Silva Soares <wendell@compelab.org>
 */

#ifndef GMTP_H_
#define GMTP_H_

#include "include/linux/gmtp.h"

#define MAX_GMTP_SPECIFIC_HEADER (8 * sizeof(uint32_t))
#define MAX_GMTP_VARIABLE_HEADER (2047 * sizeof(uint32_t))
#define MAX_GMTP_HEADER (MAX_GMTP_SPECIFIC_HEADER + MAX_GMTP_VARIABLE_HEADER)

void gmtp_close(struct sock *sk, long timeout);
int gmtp_connect(struct sock *sk, struct sockaddr *uaddr, int addr_len);
int gmtp_disconnect(struct sock *sk, int flags);
int gmtp_ioctl(struct sock *sk, int cmd, unsigned long arg);
int gmtp_getsockopt(struct sock *sk, int level, int optname,
		    char __user *optval, int __user *optlen);
int gmtp_setsockopt(struct sock *sk, int level, int optname,
		    char __user *optval, unsigned int optlen);
int gmtp_sendmsg(struct kiocb *iocb, struct sock *sk, struct msghdr *msg,
		 size_t size);
int gmtp_recvmsg(struct kiocb *iocb, struct sock *sk,
		 struct msghdr *msg, size_t len, int nonblock, int flags,
		 int *addr_len);
int gmtp_do_rcv(struct sock *sk, struct sk_buff *skb);
void gmtp_shutdown(struct sock *sk, int how);
void gmtp_destroy_sock(struct sock *sk);

//unsigned int dccp_poll(struct file *file, struct socket *sock,
//		       poll_table *wait);
int inet_gmtp_listen(struct socket *sock, int backlog);


#endif /* GMTP_H_ */
