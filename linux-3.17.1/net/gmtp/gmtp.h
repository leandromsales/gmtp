/*
 * gmtp.h
 *
 *  Created on: 18/06/2014
 *      Author: Wendell Silva Soares <wendell@compelab.org>
 */

#ifndef GMTP_H_
#define GMTP_H_

#include <net/inet_timewait_sock.h>
#include <net/inet_hashtables.h>
#include <uapi/asm-generic/errno.h>

#include <linux/gmtp.h>
#include <net/netns/gmtp.h>

extern struct inet_hashinfo gmtp_hashinfo;

#define GMTP_INFO "[GMTP_INFO] %s:%d - "
#define GMTP_WARNING "[GMTP_WARNING]  %s:%d - "
#define GMTP_ERROR "[GMTP_ERROR] %s:%d, at %s - "

#define gmtp_print_debug(fmt, args...) printk(KERN_INFO GMTP_INFO fmt "\n", __FUNCTION__, __LINE__, ##args)
#define gmtp_print_warning(fmt, args...) printk(KERN_WARNING GMTP_WARNING fmt "\n", __FUNCTION__, __LINE__, ##args)
#define gmtp_print_error(fmt, args...) printk(KERN_ERR GMTP_ERROR fmt "\n", __FUNCTION__, __LINE__, __FILE__, ##args)

#define MAX_GMTP_SPECIFIC_HEADER (8 * sizeof(uint32_t))
#define MAX_GMTP_VARIABLE_HEADER (2047 * sizeof(uint32_t))
#define MAX_GMTP_HEADER (MAX_GMTP_SPECIFIC_HEADER + MAX_GMTP_VARIABLE_HEADER)

extern struct percpu_counter gmtp_orphan_count;


const char *gmtp_packet_name(const int);

int gmtp_init_sock(struct sock *sk, const __u8 ctl_sock_initialized);

void gmtp_close(struct sock *sk, long timeout);
int gmtp_v4_connect(struct sock *sk, struct sockaddr *uaddr, int addr_len);
int gmtp_connect(struct sock *sk);
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
void gmtp_shutdown(struct sock *sk, int how);
void gmtp_destroy_sock(struct sock *sk);

//unsigned int dccp_poll(struct file *file, struct socket *sock,
//		       poll_table *wait);
int inet_gmtp_listen(struct socket *sock, int backlog);


int gmtp_insert_options(struct sock *sk, struct sk_buff *skb);


/**
 * This is the control buffer. It is free to use by any layer.
 * This is an opaque area used to store private information.
 *
 * struct sk_buff {
 * (...)
 * 		char cb[48]
 * }
 *
 * gmtp_skb_cb  -  GMTP per-packet control information
 *
 * @gmtpd_type: one of %dccp_pkt_type (or unknown)
 * @gmtpd_seq: sequence number
 * @gmtpd_reset_code: one of %dccp_reset_codes
 * @gmtpd_reset_data: Data1..3 fields (depend on @dccpd_reset_code)
 *
 * This is used for transmission as well as for reception.
 */
struct gmtp_skb_cb {
	__u8  gmtpd_type:5;
	__u64 gmtpd_seq;

	__u8  gmtpd_reset_code,
	gmtpd_reset_data[3];
};

#define GMTP_SKB_CB(__skb) ((struct gmtp_skb_cb *)&((__skb)->cb[0]))
void gmtp_set_state(sk, GMTP_REQUESTING);

#define GMTP_TIMEWAIT_LEN (60 * HZ) 

#endif /* GMTP_H_ */

