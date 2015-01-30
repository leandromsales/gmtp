#include <asm/ioctls.h>

#include <linux/init.h>
#include <linux/module.h>
#include <linux/types.h>

#include <net/inet_hashtables.h>
#include <net/sock.h>

#include <linux/gmtp.h>
#include "gmtp.h"

struct inet_hashinfo gmtp_hashinfo;
EXPORT_SYMBOL_GPL(gmtp_hashinfo);

struct inet_timewait_death_row gmtp_death_row = {
	.sysctl_max_tw_buckets = NR_FILE * 2,
	.period		= GMTP_TIMEWAIT_LEN / INET_TWDR_TWKILL_SLOTS,
	.death_lock	= __SPIN_LOCK_UNLOCKED(gmpt_death_row.death_lock),
	.hashinfo	= &gmtp_hashinfo,
	.tw_timer	= TIMER_INITIALIZER(inet_twdr_hangman, 0,
					    (unsigned long)&gmtp_death_row),
	.twkill_work	= __WORK_INITIALIZER(gmtp_death_row.twkill_work,
					     inet_twdr_twkill_work),
/* Short-time timewait calendar */

	.twcal_hand	= -1,
	.twcal_timer	= TIMER_INITIALIZER(inet_twdr_twcal_tick, 0,
					    (unsigned long)&gmtp_death_row),
};
EXPORT_SYMBOL_GPL(gmtp_death_row);

//TODO Study thash_entries... This is from DCCP thash_entries
static int thash_entries;
module_param(thash_entries, int, 0444);
MODULE_PARM_DESC(thash_entries, "Number of ehash buckets");

const char *gmtp_packet_name(const int type)
{
	static const char *const gmtp_packet_names[] = {
		[GMTP_PKT_REQUEST]  = "REQUEST",
		[GMTP_PKT_REQUESTNOTIFY]  = "REQUESTNOTIFY",
		[GMTP_PKT_RESPONSE] = "RESPONSE",
		[GMTP_PKT_REGISTER] = "REGISTER",
		[GMTP_PKT_REGISTER_REPLY] = "REGISTER_REPLY",
		[GMTP_PKT_ROUTE_NOTIFY] = "ROUTE_NOTIFY",
		[GMTP_PKT_RELAYQUERY] = "RELAYQUERY",
		[GMTP_PKT_RELAYQUERY_REPLY] = "RELAYQUERY_REPLY",
		[GMTP_PKT_DATA]	    = "DATA",
		[GMTP_PKT_ACK]	    = "ACK",
		[GMTP_PKT_DATAACK]  = "DATAACK",
		[GMTP_PKT_MEDIADESC]  = "MEDIADESC",
		[GMTP_PKT_DATAPULL_REQUEST]  = "DATAPULL_REQUEST",
		[GMTP_PKT_DATAPULL_RESPONSE]  = "DATAPULL_RESPONSE",
		[GMTP_PKT_ELECT_REQUEST]  = "ELECT_REQUEST",
		[GMTP_PKT_ELECT_RESPONSE]  = "ELECT_RESPONSE",
		[GMTP_PKT_CLOSE]    = "CLOSE",
		[GMTP_PKT_RESET]    = "RESET",
	};

	if (type >= GMTP_NR_PKT_TYPES)
		return "INVALID";
	else
		return gmtp_packet_names[type];
}
EXPORT_SYMBOL_GPL(gmtp_packet_name);

void gmtp_done(struct sock *sk)
{
    gmtp_print_function();
	gmtp_set_state(sk, GMTP_CLOSED);
	// gmtp_clear_xmit_timers(sk);

	sk->sk_shutdown = SHUTDOWN_MASK;

	if (!sock_flag(sk, SOCK_DEAD))
		sk->sk_state_change(sk);
	else
		inet_csk_destroy_sock(sk);
}
EXPORT_SYMBOL_GPL(gmtp_done);

static const char *gmtp_state_name(const int state)
{
	static const char *const gmtp_state_names[] = {
	[GMTP_OPEN]		= "OPEN",
	[GMTP_REQUESTING]	= "REQUESTING",
	[GMTP_LISTEN]		= "LISTEN",
	[GMTP_RESPOND]		= "RESPOND",
	[GMTP_ACTIVE_CLOSEREQ]	= "CLOSEREQ",
	[GMTP_PASSIVE_CLOSE]	= "PASSIVE_CLOSE",
	[GMTP_CLOSING]		= "CLOSING",
	[GMTP_TIME_WAIT]	= "TIME_WAIT",
	[GMTP_CLOSED]		= "CLOSED",
	};

	if (state >= GMTP_PKT_INVALID)
		return "INVALID STATE!";
	else
		return gmtp_state_names[state];
}
EXPORT_SYMBOL_GPL(gmtp_state_name);

static inline const char *gmtp_role(const struct sock *sk)
{
	switch (gmtp_sk(sk)->gmtps_role) {
	case GMTP_ROLE_UNDEFINED: return "undefined";
	case GMTP_ROLE_LISTEN:	  return "listen";
	case GMTP_ROLE_SERVER:	  return "server";
	case GMTP_ROLE_CLIENT:	  return "client";
	}
	return NULL;
}

void gmtp_set_state(struct sock *sk, const int state)
{
	const int oldstate = sk->sk_state;

	gmtp_print_function();
	gmtp_print_debug("%s(%p)  %s  -->  %s\n", gmtp_role(sk), sk,
			      gmtp_state_name(oldstate), gmtp_state_name(state));

	if(state == oldstate)
		gmtp_print_warning("new state == old state!");

//	switch (state) {
//	case GMTP_OPEN:
//		if (oldstate != GMTP_OPEN)
////			DCCP_INC_STATS(DCCP_MIB_CURRESTAB);
//			;
//		/* Client retransmits all Confirm options until entering OPEN */
////		if (oldstate == DCCP_PARTOPEN)
////			dccp_feat_list_purge(&dccp_sk(sk)->dccps_featneg);
//		break;
//	case GMTP_CLOSED:
//		if (oldstate == GMTP_OPEN || oldstate == GMTP_ACTIVE_CLOSEREQ
//				|| oldstate == GMTP_CLOSING)
////			DCCP_INC_STATS(DCCP_MIB_ESTABRESETS);
//			;
//
//		sk->sk_prot->unhash(sk);
//		if (inet_csk(sk)->icsk_bind_hash != NULL
//				&& !(sk->sk_userlocks & SOCK_BINDPORT_LOCK))
//			inet_put_port(sk);
//		/* fall through */
//	default:
//		if (oldstate == GMTP_OPEN)
////			DCCP_DEC_STATS(DCCP_MIB_CURRESTAB);
//			;
//	}


	/* Change state AFTER socket is unhashed to avoid closed
	 * socket sitting in hash tables.
	 */
	sk->sk_state = state;
}
EXPORT_SYMBOL_GPL(gmtp_set_state);

int gmtp_init_sock(struct sock *sk, const __u8 ctl_sock_initialized)
{
	struct gmtp_sock *gs = gmtp_sk(sk);
//	struct inet_connection_sock *icsk = inet_csk(sk);

	gmtp_print_function();

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
	gmtp_print_function();
}
EXPORT_SYMBOL_GPL(gmtp_destroy_sock);

int inet_gmtp_listen(struct socket *sock, int backlog)
{
	struct sock *sk = sock->sk;
	struct gmtp_sock *gs = gmtp_sk(sk);
	unsigned char old_state;
	int err;

	gmtp_print_function();

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
		gmtp_print_debug("inet_csk_listen_start(sk, %d) "
				"returns: %d", backlog, err);
		if (err)
			goto out;
	}
	err = 0;
out:
	release_sock(sk);
	return err;
}
EXPORT_SYMBOL_GPL(inet_gmtp_listen);

//TODO Implement gmtp callbacks
void gmtp_close(struct sock *sk, long timeout)
{
	gmtp_print_function();
}
EXPORT_SYMBOL_GPL(gmtp_close);


int gmtp_disconnect(struct sock *sk, int flags)
{
	gmtp_print_function();
	return 0;
}
EXPORT_SYMBOL_GPL(gmtp_disconnect);

int gmtp_ioctl(struct sock *sk, int cmd, unsigned long arg)
{
	int rc = -ENOTCONN;

	gmtp_print_function();
	lock_sock(sk);

	if (sk->sk_state == GMTP_LISTEN)
		goto out;

	switch (cmd) {
	case SIOCINQ: {
		struct sk_buff *skb;
		unsigned long amount = 0;

		skb = skb_peek(&sk->sk_receive_queue);
		if (skb != NULL) {
			/*
			 * We will only return the amount of this packet since
			 * that is all that will be read.
			 */
			amount = skb->len;
		}
		rc = put_user(amount, (int __user *)arg);
	}
	break;
	default:
		rc = -ENOIOCTLCMD;
		break;
	}
out:
	release_sock(sk);
	return rc;
}
EXPORT_SYMBOL_GPL(gmtp_ioctl);

int gmtp_getsockopt(struct sock *sk, int level, int optname,
		char __user *optval, int __user *optlen)
{
	gmtp_print_function();
	return 0;
}
EXPORT_SYMBOL_GPL(gmtp_getsockopt);

int gmtp_sendmsg(struct kiocb *iocb, struct sock *sk, struct msghdr *msg,
		size_t size)
{
	gmtp_print_function();
	return 0;
}
EXPORT_SYMBOL_GPL(gmtp_sendmsg);

int gmtp_setsockopt(struct sock *sk, int level, int optname,
		char __user *optval, unsigned int optlen)
{
	gmtp_print_function();
	return 0;
}
EXPORT_SYMBOL_GPL(gmtp_setsockopt);

int gmtp_recvmsg(struct kiocb *iocb, struct sock *sk,
		struct msghdr *msg, size_t len, int nonblock, int flags,
		int *addr_len)
{
	gmtp_print_function();

	lock_sock(sk);

	if (sk->sk_state == GMTP_LISTEN) {
		len = -ENOTCONN;
		goto out;
	}

out:
	release_sock(sk);
	return len;
}
EXPORT_SYMBOL_GPL(gmtp_recvmsg);

void gmtp_shutdown(struct sock *sk, int how)
{
	gmtp_print_function();
	gmtp_print_debug("called shutdown(%x)", how);
}
EXPORT_SYMBOL_GPL(gmtp_shutdown);

/**
 * Unfortunately, we can't use the alloc_large_system_hash method...
 * So, this method is an adaptation from dccp hashinfo initialization
 */
static int gmtp_init_hashinfo(void)
{
	unsigned long goal;
	int ehash_order, bhash_order, i;
	int rc;

	gmtp_print_function();

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
	int rc;
	gmtp_print_debug("GMTP init!");
	gmtp_print_function();
	rc = gmtp_init_hashinfo();
	return rc;
}

static void __exit gmtp_exit(void)
{
	gmtp_print_function();
	gmtp_print_debug("GMTP exit!");
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
MODULE_AUTHOR("Wendell Silva Soares <wss@ic.ufal.br>");
MODULE_DESCRIPTION("GMTP - Global Media Transmission Protocol");
