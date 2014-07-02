/*
 * gmtp_probe - Observe the GMTP flow with kprobes.
 *
 * The idea for this came from Werner Almesberger's umlsim
 * Copyright (C) 2004, Stephen Hemminger <shemminger@osdl.org>
 *
 * Modified for GMTP from Stephen Hemminger's code
 * Copyright (C) 2006, Ian McDonald <ian.mcdonald@jandi.co.nz>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/kernel.h>
#include <linux/kprobes.h>
#include <linux/socket.h>
#include <linux/gmtp.h>
#include <linux/proc_fs.h>
#include <linux/module.h>
#include <linux/kfifo.h>
#include <linux/vmalloc.h>
#include <linux/gfp.h>
#include <net/net_namespace.h>

#include "gmtp.h"
#include "ccid.h"
#include "ccids/ccid3.h"

static int port;

static int bufsize = 64 * 1024;

static const char procname[] = "gmtpprobe";

static struct {
	struct kfifo	  fifo;
	spinlock_t	  lock;
	wait_queue_head_t wait;
	struct timespec	  tstart;
} gmtpw;

static void printl(const char *fmt, ...)
{
	va_list args;
	int len;
	struct timespec now;
	char tbuf[256];

	va_start(args, fmt);
	getnstimeofday(&now);

	now = timespec_sub(now, gmtpw.tstart);

	len = sprintf(tbuf, "%lu.%06lu ",
		      (unsigned long) now.tv_sec,
		      (unsigned long) now.tv_nsec / NSEC_PER_USEC);
	len += vscnprintf(tbuf+len, sizeof(tbuf)-len, fmt, args);
	va_end(args);

	kfifo_in_locked(&gmtpw.fifo, tbuf, len, &gmtpw.lock);
	wake_up(&gmtpw.wait);
}

static int jgmtp_sendmsg(struct kiocb *iocb, struct sock *sk,
			 struct msghdr *msg, size_t size)
{
	const struct inet_sock *inet = inet_sk(sk);
	struct ccid3_hc_tx_sock *hc = NULL;

	if (ccid_get_current_tx_ccid(gmtp_sk(sk)) == GMTPC_CCID3)
		hc = ccid3_hc_tx_sk(sk);

	if (port == 0 || ntohs(inet->inet_dport) == port ||
	    ntohs(inet->inet_sport) == port) {
		if (hc)
			printl("%pI4:%u %pI4:%u %d %d %d %d %u %llu %llu %d\n",
			       &inet->inet_saddr, ntohs(inet->inet_sport),
			       &inet->inet_daddr, ntohs(inet->inet_dport), size,
			       hc->tx_s, hc->tx_rtt, hc->tx_p,
			       hc->tx_x_calc, hc->tx_x_recv >> 6,
			       hc->tx_x >> 6, hc->tx_t_ipi);
		else
			printl("%pI4:%u %pI4:%u %d\n",
			       &inet->inet_saddr, ntohs(inet->inet_sport),
			       &inet->inet_daddr, ntohs(inet->inet_dport),
			       size);
	}

	jprobe_return();
	return 0;
}

static struct jprobe gmtp_send_probe = {
	.kp	= {
		.symbol_name = "gmtp_sendmsg",
	},
	.entry	= jgmtp_sendmsg,
};

static int gmtpprobe_open(struct inode *inode, struct file *file)
{
	kfifo_reset(&gmtpw.fifo);
	getnstimeofday(&gmtpw.tstart);
	return 0;
}

static ssize_t gmtpprobe_read(struct file *file, char __user *buf,
			      size_t len, loff_t *ppos)
{
	int error = 0, cnt = 0;
	unsigned char *tbuf;

	if (!buf)
		return -EINVAL;

	if (len == 0)
		return 0;

	tbuf = vmalloc(len);
	if (!tbuf)
		return -ENOMEM;

	error = wait_event_interruptible(gmtpw.wait,
					 kfifo_len(&gmtpw.fifo) != 0);
	if (error)
		goto out_free;

	cnt = kfifo_out_locked(&gmtpw.fifo, tbuf, len, &gmtpw.lock);
	error = copy_to_user(buf, tbuf, cnt) ? -EFAULT : 0;

out_free:
	vfree(tbuf);

	return error ? error : cnt;
}

static const struct file_operations gmtpprobe_fops = {
	.owner	 = THIS_MODULE,
	.open	 = gmtpprobe_open,
	.read    = gmtpprobe_read,
	.llseek  = noop_llseek,
};

static __init int setup_jprobe(void)
{
	int ret = register_jprobe(&gmtp_send_probe);

	if (ret) {
		request_module("gmtp");
		ret = register_jprobe(&gmtp_send_probe);
	}
	return ret;
}

static __init int gmtpprobe_init(void)
{
	int ret = -ENOMEM;

	init_waitqueue_head(&gmtpw.wait);
	spin_lock_init(&gmtpw.lock);
	if (kfifo_alloc(&gmtpw.fifo, bufsize, GFP_KERNEL))
		return ret;
	if (!proc_net_fops_create(&init_net, procname, S_IRUSR, &gmtpprobe_fops))
		goto err0;

	ret = setup_jprobe();
	if (ret)
		goto err1;

	pr_info("GMTP watch registered (port=%d)\n", port);
	return 0;
err1:
	proc_net_remove(&init_net, procname);
err0:
	kfifo_free(&gmtpw.fifo);
	return ret;
}
module_init(gmtpprobe_init);

static __exit void gmtpprobe_exit(void)
{
	kfifo_free(&gmtpw.fifo);
	proc_net_remove(&init_net, procname);
	unregister_jprobe(&gmtp_send_probe);

}
module_exit(gmtpprobe_exit);

MODULE_PARM_DESC(port, "Port to match (0=all)");
module_param(port, int, 0);

MODULE_PARM_DESC(bufsize, "Log buffer size (default 64k)");
module_param(bufsize, int, 0);

MODULE_AUTHOR("Ian McDonald <ian.mcdonald@jandi.co.nz>");
MODULE_DESCRIPTION("GMTP snooper");
MODULE_LICENSE("GPL");
