#include <asm/ioctls.h>

#include <linux/init.h>
#include <linux/module.h>
#include <linux/types.h>

#include <net/inet_hashtables.h>
#include <net/sock.h>

#include "gmtp.h"

struct inet_hashinfo gmtp_hashinfo;
EXPORT_SYMBOL_GPL(gmtp_hashinfo);

//TODO Study thash_entries... This is from DCCP thash_entries
static int thash_entries;
module_param(thash_entries, int, 0444);
MODULE_PARM_DESC(thash_entries, "Number of ehash buckets");

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
	gmtp_print_debug("gmtp_listen");
	struct sock *sk = sock->sk;
	struct gmtp_sock *gs = gmtp_sk(sk);
	unsigned char old_state;
	int err;

	lock_sock(sk);

	err = -EINVAL;
	if (sock->state != SS_UNCONNECTED || sock->type != SOCK_GMTP)
		goto out;

	old_state = sk->sk_state;
	if (!((1 << old_state) & (GMTPF_CLOSED | GMTPF_LISTEN)))
		goto out;

	/* Really, if the socket is already in listen state
	 * we can only allow the backlog to be adjusted.
	 */
	if (old_state != GMTP_LISTEN) {

		//TODO Verificar se precisa
		gs->gmtps_role = GMTP_ROLE_LISTEN;

		err = inet_csk_listen_start(sk, backlog);
		if (err)
			goto out;
	}
	err = 0;

out:
	release_sock(sk);
	return err;
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

/**
 * Unfortunately, we can't use the alloc_large_system_hash method...
 * So, this method is an adaptation from dccp hashinfo initialization
 */
static int gmtp_init_hashinfo(void)
{
	gmtp_print_debug("gmtp_init_hashinfo");
	unsigned long goal;
	int ehash_order, bhash_order, i;
	int rc;

	BUILD_BUG_ON(sizeof(struct gmtp_skb_cb) >
	FIELD_SIZEOF(struct sk_buff, cb));

	rc = -ENOBUFS;

	inet_hashinfo_init(&gmtp_hashinfo);
	gmtp_hashinfo.bind_bucket_cachep =
			kmem_cache_create("gmtp_bind_bucket",
					sizeof(struct inet_bind_bucket), 0,
					SLAB_HWCACHE_ALIGN, NULL);
	if (!gmtp_hashinfo.bind_bucket_cachep)
		goto out_fail;

	/*
	 * Size and allocate the main established and bind bucket
	 * hash tables.
	 *
	 * The methodology is similar to that of the buffer cache.
	 */
	if (totalram_pages >= (128 * 1024))
		goal = totalram_pages >> (21 - PAGE_SHIFT);
	else
		goal = totalram_pages >> (23 - PAGE_SHIFT);

	if (thash_entries)
		goal = (thash_entries *
				sizeof(struct inet_ehash_bucket)) >> PAGE_SHIFT;
	for (ehash_order = 0; (1UL << ehash_order) < goal; ehash_order++)
		;

	do {
		unsigned long hash_size = (1UL << ehash_order) * PAGE_SIZE /
				sizeof(struct inet_ehash_bucket);

		while (hash_size & (hash_size - 1))
			hash_size--;
		gmtp_hashinfo.ehash_mask = hash_size - 1;
		gmtp_hashinfo.ehash = (struct inet_ehash_bucket *)
											__get_free_pages(GFP_ATOMIC|__GFP_NOWARN, ehash_order);
	} while (!gmtp_hashinfo.ehash && --ehash_order > 0);

	if (!gmtp_hashinfo.ehash) {
		gmtp_print_error("Failed to allocate GMTP established hash table");
		goto out_free_bind_bucket_cachep;
	}

	for (i = 0; i <= gmtp_hashinfo.ehash_mask; i++)
		INIT_HLIST_NULLS_HEAD(&gmtp_hashinfo.ehash[i].chain, i);

	if (inet_ehash_locks_alloc(&gmtp_hashinfo))
		goto out_free_gmtp_ehash;

	bhash_order = ehash_order;

	do {
		gmtp_hashinfo.bhash_size = (1UL << bhash_order) * PAGE_SIZE /
				sizeof(struct inet_bind_hashbucket);
		if ((gmtp_hashinfo.bhash_size > (64 * 1024)) &&
				bhash_order > 0)
			continue;
		gmtp_hashinfo.bhash = (struct inet_bind_hashbucket *)
											__get_free_pages(GFP_ATOMIC|__GFP_NOWARN, bhash_order);
	} while (!gmtp_hashinfo.bhash && --bhash_order >= 0);

	if (!gmtp_hashinfo.bhash) {
		gmtp_print_error("Failed to allocate GMTP bind hash table");
		goto out_free_gmtp_locks;
	}

	for (i = 0; i < gmtp_hashinfo.bhash_size; i++) {
		spin_lock_init(&gmtp_hashinfo.bhash[i].lock);
		INIT_HLIST_HEAD(&gmtp_hashinfo.bhash[i].chain);
	}

	return 0;

	out_free_gmtp_locks:
	inet_ehash_locks_free(&gmtp_hashinfo);
	out_free_gmtp_ehash:
	free_pages((unsigned long)gmtp_hashinfo.ehash, ehash_order);
	out_free_bind_bucket_cachep:
	kmem_cache_destroy(gmtp_hashinfo.bind_bucket_cachep);
	out_fail:
	gmtp_print_error("gmtp_init_hashinfo: FAIL");
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
