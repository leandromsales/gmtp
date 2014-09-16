#include <linux/init.h>
#include <linux/module.h>
#include <linux/swap.h>
#include <linux/types.h>

#include <net/inet_hashtables.h>
#include <net/sock.h>

#include <linux/bootmem.h>
#include <linux/mm.h>
//#include <../mm/page_alloc.c>

#include "gmtp.h"

struct inet_hashinfo gmtp_hashinfo;
EXPORT_SYMBOL_GPL(gmtp_hashinfo);

//TODO Study thash_entries... This is from DCCP thash_entries
static int thash_entries;
module_param(thash_entries, int, 0444);
MODULE_PARM_DESC(thash_entries, "Number of ehash buckets");

long sysctl_gmtp_mem[3] __read_mostly;
int sysctl_gmtp_wmem[3] __read_mostly;
int sysctl_gmtp_rmem[3] __read_mostly;
EXPORT_SYMBOL(sysctl_gmtp_mem);
EXPORT_SYMBOL(sysctl_gmtp_wmem);
EXPORT_SYMBOL(sysctl_gmtp_rmem);

//atomic_long_t tcp_memory_allocated;	/* Current allocated memory. */
//EXPORT_SYMBOL(tcp_memory_allocated);

void gmtp_set_state(struct sock *sk, const int state)
{
	gmtp_print_debug("gmtp_set_state");

	if(!sk)
		gmtp_print_warning("sk is NULL!");
	else
		sk->sk_state = state;
}

int gmtp_init_sock(struct sock *sk, const __u8 ctl_sock_initialized)
{
	gmtp_print_debug("gmtp_init_sock");

	struct gmtp_sock *gs = gmtp_sk(sk);
	struct inet_connection_sock *icsk = inet_csk(sk);

	sk->sk_state		= GMTP_CLOSED;

	//TODO Study those:
	gs->gmtps_role		= GMTP_ROLE_UNDEFINED;
	gs->gmtps_service	= 0;

	return 0;
}
EXPORT_SYMBOL_GPL(gmtp_init_sock);

void gmtp_destroy_sock(struct sock *sk)
{
	//TODO Implement gmtp_destroy_sock
	gmtp_print_debug("gmtp_destroy_sock");
}
EXPORT_SYMBOL_GPL(gmtp_destroy_sock);

int inet_gmtp_listen(struct socket *sock, int backlog)
{
	gmtp_print_debug("inet_gmtp_listen");

	struct sock *sk = sock->sk;

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

//TODO Implement gmtp callbacks
void gmtp_close(struct sock *sk, long timeout)
{
	gmtp_print_debug("gmtp_close");
}
EXPORT_SYMBOL_GPL(gmtp_close);


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

static void gmtp_init_mem(void)
{
	unsigned long limit = nr_free_buffer_pages() / 8;
	limit = max(limit, 128UL);
	sysctl_gmtp_mem[0] = limit / 4 * 3;
	sysctl_gmtp_mem[1] = limit;
	sysctl_gmtp_mem[2] = sysctl_gmtp_mem[0] * 2;
}

/**
 * This method is a copy from tcp hashinfo initialization
 * with some dccp hash verifications
 */
static int gmtp_init_hashinfo(void)
{
	gmtp_print_debug("gmtp_init_hashinfo");

	struct sk_buff *skb = NULL;
	unsigned long limit;
	int max_rshare, max_wshare, cnt;
	unsigned int i;
	int rc;

	BUILD_BUG_ON(sizeof(struct gmtp_skb_cb) >
	FIELD_SIZEOF(struct sk_buff, cb));

	rc = -ENOBUFS;

	gmtp_hashinfo.bind_bucket_cachep =
			kmem_cache_create("gmtp_bind_bucket",
					sizeof(struct inet_bind_bucket), 0,
					SLAB_HWCACHE_ALIGN|SLAB_PANIC, NULL);
	if (!gmtp_hashinfo.bind_bucket_cachep)
			goto out_fail;


	/* Size and allocate the main established and bind bucket
	 * hash tables.
	 *
	 * The methodology is similar to that of the buffer cache.
	 */
	gmtp_hashinfo.ehash =
			alloc_large_system_hash("GMTP established",
					sizeof(struct inet_ehash_bucket),
					thash_entries,
					17, /* one slot per 128 KB of memory */
					0,
					NULL,
					&gmtp_hashinfo.ehash_mask,
					0,
					thash_entries ? 0 : 512 * 1024);
	if (!gmtp_hashinfo.ehash) {
		gmtp_print_error("Failed to allocate GMTP established hash table");
		goto out_free_bind_bucket_cachep;
	}
	for (i = 0; i <= gmtp_hashinfo.ehash_mask; i++)
		INIT_HLIST_NULLS_HEAD(&gmtp_hashinfo.ehash[i].chain, i);

	if (inet_ehash_locks_alloc(&gmtp_hashinfo))
		panic("GMTP: failed to alloc ehash_locks");

	gmtp_hashinfo.bhash =
			alloc_large_system_hash("GMTP bind",
					sizeof(struct inet_bind_hashbucket),
					gmtp_hashinfo.ehash_mask + 1,
					17, /* one slot per 128 KB of memory */
					0,
					&gmtp_hashinfo.bhash_size,
					NULL,
					0,
					64 * 1024);
	if (!gmtp_hashinfo.bhash) {
		gmtp_print_error("Failed to allocate GMTP bind hash table");
		goto out_free_gmtp_locks;
	}

	gmtp_hashinfo.bhash_size = 1U << gmtp_hashinfo.bhash_size;
	for (i = 0; i < gmtp_hashinfo.bhash_size; i++) {
		spin_lock_init(&gmtp_hashinfo.bhash[i].lock);
		INIT_HLIST_HEAD(&gmtp_hashinfo.bhash[i].chain);
	}

	cnt = gmtp_hashinfo.ehash_mask + 1;

	gmtp_init_mem();
	/* Set per-socket limits to no more than 1/128 the pressure threshold */
	limit = nr_free_buffer_pages() << (PAGE_SHIFT - 7);
	max_wshare = min(4UL*1024*1024, limit);
	max_rshare = min(6UL*1024*1024, limit);

	sysctl_gmtp_wmem[0] = SK_MEM_QUANTUM;
	sysctl_gmtp_wmem[1] = 16*1024;
	sysctl_gmtp_wmem[2] = max(64*1024, max_wshare);

	sysctl_gmtp_rmem[0] = SK_MEM_QUANTUM;
	sysctl_gmtp_rmem[1] = 87380;
	sysctl_gmtp_rmem[2] = max(87380, max_rshare);

	gmtp_print_debug("Hash tables configured (established %u bind %u)\n",
			gmtp_hashinfo.ehash_mask + 1, gmtp_hashinfo.bhash_size);

	return 0;

out_free_gmtp_locks:
	inet_ehash_locks_free(&gmtp_hashinfo);
out_free_bind_bucket_cachep:
	kmem_cache_destroy(gmtp_hashinfo.bind_bucket_cachep);
out_fail:
	gmtp_print_error("gmtp_init_hashinfo - ERROR");
	gmtp_hashinfo.bhash = NULL;
	gmtp_hashinfo.ehash = NULL;
	gmtp_hashinfo.bind_bucket_cachep = NULL;

	return rc;
}

static int __init gmtp_init(void)
{
	gmtp_print_debug("GMTP init!\n");
	int rc;
	rc = gmtp_init_hashinfo();
	return rc;
}

static void __exit gmtp_exit(void)
{
	gmtp_print_debug("GMTP exit!\n");
	free_pages((unsigned long)gmtp_hashinfo.bhash,
			get_order(gmtp_hashinfo.bhash_size *
					sizeof(struct inet_bind_hashbucket)));
	free_pages((unsigned long)gmtp_hashinfo.ehash,
			get_order((gmtp_hashinfo.ehash_mask + 1) *
					sizeof(struct inet_ehash_bucket)));
	inet_ehash_locks_free(&gmtp_hashinfo);
	kmem_cache_destroy(gmtp_hashinfo.bind_bucket_cachep);
}

module_init(gmtp_init);
module_exit(gmtp_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Wendell Silva Soares <wendell@compelab.org>");
MODULE_DESCRIPTION("GMTP - Global Media Transmission Protocol");
