#include <linux/kernel.h>
#include <linux/skbuff.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/if_ether.h>
#include <asm/param.h>

#include <net/inet_sock.h>
#include <net/tcp.h>
#include <net/sock.h>

#include <uapi/linux/gmtp.h>
#include <linux/gmtp.h>
#include "gmtp.h"

static inline void gmtp_event_ack_sent(struct sock *sk)
{
	inet_csk_clear_xmit_timer(sk, ICSK_TIME_DACK);
}

/* enqueue @skb on sk_send_head for retransmission, return clone to send now */
static struct sk_buff *gmtp_skb_entail(struct sock *sk, struct sk_buff *skb) {
	gmtp_print_function();
	skb_set_owner_w(skb, sk);
	WARN_ON(sk->sk_send_head);
	sk->sk_send_head = skb;
	return skb_clone(sk->sk_send_head, gfp_any());
}
/*
 * All SKB's seen here are completely headerless. It is our
 * job to build the GMTP header, and pass the packet down to
 * IP so it can do the same plus pass the packet off to the
 * device.
 */
static int gmtp_transmit_skb(struct sock *sk, struct sk_buff *skb) {

	gmtp_print_function();

	if (likely(skb != NULL)) {

		struct inet_sock *inet = inet_sk(sk);
		const struct inet_connection_sock *icsk = inet_csk(sk);
		struct gmtp_sock *gp = gmtp_sk(sk);
		struct gmtp_skb_cb *gcb = GMTP_SKB_CB(skb);
		struct gmtp_hdr *gh;
		const u32 gmtp_header_size = sizeof(struct gmtp_hdr) +
				gmtp_packet_hdr_variable_len(gcb->type);
		int err, set_ack = 1;

		gmtp_print_debug("Packet: %s", gmtp_packet_name(gcb->type));

		switch (gcb->type) {
		case GMTP_PKT_DATA:
			set_ack = 0;
			/* fall through */
		case GMTP_PKT_DATAACK:
		case GMTP_PKT_RESET:
			break;
		case GMTP_PKT_REQUEST:
			set_ack = 0;
			/* Use ISS on the first (non-retransmitted) Request. */
			if (icsk->icsk_retransmits == 0)
				gcb->seq = gp->iss;
			gp->req_stamp = jiffies;
			/* fall through */
		default:
			/*
			 * Set owner/destructor: some skbs are allocated via
			 * alloc_skb (e.g. when retransmission may happen).
			 * Only Data, DataAck, and Reset packets should come
			 * through here with skb->sk set.
			 */
			WARN_ON(skb->sk);
			skb_set_owner_w(skb, sk);
			break;
		}

		gh = gmtp_zeroed_hdr(skb, gmtp_header_size);

		gh->version = 1;
		gh->type = gcb->type;
		gh->server_rtt = gp->rtt;
		gh->sport = inet->inet_sport;
		gh->dport = inet->inet_dport;
		gh->hdrlen = gmtp_header_size;
		memcpy(gh->flowname, gp->flowname, GMTP_FLOWNAME_LEN);

		if (gcb->type == GMTP_PKT_RESET)
			gmtp_hdr_reset(skb)->reset_code = gcb->reset_code;

		gcb->seq = ++gp->gss;
		if (set_ack) {
			gcb->seq = --gp->gss;
			gmtp_event_ack_sent(sk);
		}

		gh->seq = gcb->seq;

		err = icsk->icsk_af_ops->queue_xmit(sk, skb, &inet->cork.fl);
		return net_xmit_eval(err);
	}
	return -ENOBUFS;
}

unsigned int gmtp_sync_mss(struct sock *sk, u32 pmtu)
{
	struct inet_connection_sock *icsk = inet_csk(sk);
	struct gmtp_sock *gp = gmtp_sk(sk);
	u32 cur_mps = pmtu;

	gmtp_print_function();

	/* Account for header lengths and IPv4/v6 option overhead */
	/* FIXME Account variable part of GMTP Header */
	cur_mps -= (icsk->icsk_af_ops->net_header_len + icsk->icsk_ext_hdr_len +
		    sizeof(struct gmtp_hdr));

	/* And store cached results */
	icsk->icsk_pmtu_cookie = pmtu;
	gp->mss = cur_mps;

	gmtp_print_debug("mss: %u", gp->mss);

	return cur_mps;
}
EXPORT_SYMBOL_GPL(gmtp_sync_mss);

void gmtp_write_space(struct sock *sk)
{
	struct socket_wq *wq;

	rcu_read_lock();
	wq = rcu_dereference(sk->sk_wq);
	if (wq_has_sleeper(wq))
		wake_up_interruptible(&wq->wait);
	/* Should agree with poll, otherwise some programs break */
	if (sock_writeable(sk))
		sk_wake_async(sk, SOCK_WAKE_SPACE, POLL_OUT);

	rcu_read_unlock();
}

/**
 * gmtp_retransmit_skb  -  Retransmit Request, Close, or CloseReq packets
 * This function expects sk->sk_send_head to contain the original skb.
 */
int gmtp_retransmit_skb(struct sock *sk)
{
	WARN_ON(sk->sk_send_head == NULL);

	if (inet_csk(sk)->icsk_af_ops->rebuild_header(sk) != 0)
		return -EHOSTUNREACH; /* Routing failure or similar. */

	/* this count is used to distinguish original and retransmitted skb */
	inet_csk(sk)->icsk_retransmits++;

	return gmtp_transmit_skb(sk, skb_clone(sk->sk_send_head, GFP_ATOMIC));
}

struct sk_buff *gmtp_make_register_reply(struct sock *sk, struct dst_entry *dst,
				   struct request_sock *req)
{
	struct gmtp_hdr *gh;
	struct gmtp_request_sock *greq;

	const u32 gmtp_header_size = sizeof(struct gmtp_hdr) +
			gmtp_packet_hdr_variable_len(GMTP_PKT_REGISTER_REPLY);

	struct sk_buff *skb = sock_wmalloc(sk, sk->sk_prot->max_header, 1,
			GFP_ATOMIC);

	gmtp_print_function();

	if (skb == NULL)
		return NULL;

	/* Reserve space for headers. */
	skb_reserve(skb, sk->sk_prot->max_header);

	skb_dst_set(skb, dst_clone(dst));

	greq = gmtp_rsk(req);
	if (inet_rsk(req)->acked)	/* increase GSS upon retransmission */
		greq->gss++;
	GMTP_SKB_CB(skb)->type = GMTP_PKT_REGISTER_REPLY;
	GMTP_SKB_CB(skb)->seq  = greq->gss;

	/* Build header */
	gh = gmtp_zeroed_hdr(skb, gmtp_header_size);

	/* At this point, Register-Reply specific header is zeroed. */
	gh->sport	= htons(inet_rsk(req)->ir_num);
	gh->dport	= inet_rsk(req)->ir_rmt_port;
	gh->type	= GMTP_PKT_REGISTER_REPLY;
	gh->seq 	= greq->gss;
	gh->server_rtt	= gmtp_sock(sk)->rtt;
	gh->hdrlen	= gmtp_header_size;
	memcpy(gh->flowname, greq->flowname, GMTP_FLOWNAME_LEN);

	/* We use `acked' to remember that a Register-Reply was already sent. */
	inet_rsk(req)->acked = 1;

	return skb;
}
EXPORT_SYMBOL_GPL(gmtp_make_register_reply);


/* answer offending packet in @rcv_skb with Reset from control socket @ctl */
struct sk_buff *gmtp_ctl_make_reset(struct sock *sk, struct sk_buff *rcv_skb)
{
	struct gmtp_hdr *rxgh = gmtp_hdr(rcv_skb), *gh;
	struct gmtp_sock *gp = gmtp_sk(sk);
	struct gmtp_skb_cb *gcb = GMTP_SKB_CB(rcv_skb);
	const u32 gmtp_hdr_reset_len = sizeof(struct gmtp_hdr) +
			sizeof(struct gmtp_hdr_reset);

	struct gmtp_hdr_reset *ghr;
	struct sk_buff *skb;

	gmtp_print_function();

	skb = alloc_skb(sk->sk_prot->max_header, GFP_ATOMIC);
	if (skb == NULL)
		return NULL;

	skb_reserve(skb, sk->sk_prot->max_header);

	/* FIXME Insert reset data in GMTP header (variable part) */

	/* Swap the send and the receive. */
	gh = gmtp_zeroed_hdr(skb, gmtp_hdr_reset_len);
	gh->type	= GMTP_PKT_RESET;
	gh->sport	= rxgh->dport;
	gh->dport	= rxgh->sport;
	gh->hdrlen	= gmtp_hdr_reset_len;
	gh->seq 	= gp->iss;
	gh->server_rtt	= gp->rtt;
	memcpy(gh->flowname, gp->flowname, GMTP_FLOWNAME_LEN);

	ghr = gmtp_hdr_reset(skb);
	ghr->reset_code = gcb->reset_code;

	switch (gcb->reset_code) {
	case GMTP_RESET_CODE_PACKET_ERROR:
		ghr->reset_data[0] = rxgh->type;
		break;
	case GMTP_RESET_CODE_MANDATORY_ERROR:
		memcpy(ghr->reset_data, gcb->reset_data, 3);
		break;
	default:
		break;
	}

	return skb;
}
EXPORT_SYMBOL_GPL(gmtp_ctl_make_reset);

int gmtp_send_reset(struct sock *sk, enum gmtp_reset_codes code)
{
	struct sk_buff *skb;
	int err = inet_csk(sk)->icsk_af_ops->rebuild_header(sk);

	gmtp_print_function();

	if (err != 0)
		return err;

	skb = sock_wmalloc(sk, sk->sk_prot->max_header, 1, GFP_ATOMIC);
	if (skb == NULL)
		return -ENOBUFS;

	/* Reserve space for headers and prepare control bits. */
	skb_reserve(skb, sk->sk_prot->max_header);
	GMTP_SKB_CB(skb)->type = GMTP_PKT_RESET;
	GMTP_SKB_CB(skb)->reset_code = code;

	return gmtp_transmit_skb(sk, skb);
}

/*
 * Do all connect socket setups that can be done AF independent.
 */
int gmtp_connect(struct sock *sk) {

	struct sk_buff *skb;
	struct gmtp_sock *gp = gmtp_sk(sk);
	struct dst_entry *dst = __sk_dst_get(sk);
	struct inet_sock *inet = inet_sk(sk);
	struct inet_connection_sock *icsk = inet_csk(sk);

	gmtp_print_function();

	sk->sk_err = 0;
	sock_reset_flag(sk, SOCK_DONE);

	gmtp_sync_mss(sk, dst_mtu(dst));

	/** First transmission: gss <- iss */
	gp->gss = gp->iss;

	skb = alloc_skb(sk->sk_prot->max_header, sk->sk_allocation);
	if (unlikely(skb == NULL))
		return -ENOBUFS;

	/* Reserve space for headers. */
	skb_reserve(skb, sk->sk_prot->max_header);
	GMTP_SKB_CB(skb)->type = GMTP_PKT_REQUEST;

	/** FIXME Treat two clients getting the same media at same server...
	 *
	 * Utilizar o mesmo sk para cada novo cliente conectado localmente para
	 * o mesmo flowname.
	 *
	 * Pesquisar semáforos, para controlar o close() do socket.
	 */
	/** FIXME TODO - Keep only SK in hashtable... */
	/** Maybe we dont need lookup sk... maybe... */
	if(gp->flowname != NULL)
		gmtp_add_client_entry(gmtp_hashtable, gp->flowname,
				inet->inet_saddr, inet->inet_sport, 0, 0);

	gmtp_transmit_skb(sk, gmtp_skb_entail(sk, skb));

	/* FIXME Retransmit it  */
	icsk->icsk_retransmits = 0;
	inet_csk_reset_xmit_timer(sk, ICSK_TIME_RETRANS,
					  icsk->icsk_rto, GMTP_RTO_MAX);
	return 0;
}
EXPORT_SYMBOL_GPL(gmtp_connect);

void gmtp_send_ack(struct sock *sk)
{
	gmtp_print_function();

	/* If we have been reset, we may not send again. */
	if(sk->sk_state != GMTP_CLOSED) {

		struct gmtp_sock *gs = gmtp_sk(sk);
		struct sk_buff *skb = alloc_skb(sk->sk_prot->max_header,
		GFP_ATOMIC);

		struct gmtp_hdr_ack *gack;

		if(skb == NULL) {
			inet_csk_schedule_ack(sk);
			inet_csk(sk)->icsk_ack.ato = TCP_ATO_MIN;
			inet_csk_reset_xmit_timer(sk, ICSK_TIME_DACK,
			TCP_DELACK_MAX,
			GMTP_RTO_MAX);
			return;
		}

		/* Reserve space for headers */
		skb_reserve(skb, sk->sk_prot->max_header);
		GMTP_SKB_CB(skb)->type = GMTP_PKT_ACK;

		/* This works only for clients */
		if(gs->role == GMTP_ROLE_CLIENT) {
			gmtp_print_debug("Building ack header...");
			gack = (struct gmtp_hdr_ack *)skb_put(skb,
					sizeof(struct gmtp_hdr_ack));

			/* Acked packet */
			gack->ackcode = GMTP_PKT_REQUESTNOTIFY;
		}
		gmtp_transmit_skb(sk, skb);
	}
}
EXPORT_SYMBOL_GPL(gmtp_send_ack);

/*
 * Send a GMTP_PKT_CLOSE/CLOSEREQ. The caller locks the socket for us.
 * This cannot be allowed to fail queueing a GMTP_PKT_CLOSE/CLOSEREQ frame
 * under any circumstances.
 */
void gmtp_send_close(struct sock *sk, const int active)
{
	struct sk_buff *skb;
	const gfp_t prio = active ? GFP_KERNEL : GFP_ATOMIC;

	gmtp_print_function();

	skb = alloc_skb(sk->sk_prot->max_header, prio);
	if(skb == NULL)
		return;

	/* Reserve space for headers and prepare control bits. */
	skb_reserve(skb, sk->sk_prot->max_header);
	GMTP_SKB_CB(skb)->type = GMTP_PKT_CLOSE;

	if(active) {
		skb = gmtp_skb_entail(sk, skb);
		/*
		 * TODO Verify if we need Retransmission timer
		 * for active-close...
		 */
		/*inet_csk_reset_xmit_timer(sk, ICSK_TIME_RETRANS,
				GMTP_TIMEOUT_INIT, GMTP_RTO_MAX);*/
	}
	gmtp_transmit_skb(sk, skb);
}

/**
 * gmtp_wait_for_delay  -  Await GMTP send permission
 * @sk:    socket to wait for
 * @delay: timeout in jiffies
 *
 * To delay the send time in process context.
 */
static int gmtp_wait_for_delay(struct sock *sk, unsigned long delay)
{
	DEFINE_WAIT(wait);
	long remaining;

	gmtp_print_function();
	pr_info("Delay: %lu jiffies (%u ms)\n", delay, jiffies_to_msecs(delay));

	prepare_to_wait(sk_sleep(sk), &wait, TASK_INTERRUPTIBLE);
	sk->sk_write_pending++;
	release_sock(sk);

	remaining = schedule_timeout(delay);

	lock_sock(sk);
	sk->sk_write_pending--;
	finish_wait(sk_sleep(sk), &wait);

	if (signal_pending(current) || sk->sk_err)
		return -1;
	return remaining;
}

static inline int pkt_len(int data_len)
{
	/* Data + GMTP + IP + MAC */
	return data_len + sizeof(struct gmtp_hdr) + 20 + ETH_HLEN;
}

static inline void packet_sent(struct gmtp_sock *gp, int data_len)
{
	unsigned long elapsed;
	int len = pkt_len(data_len);

	++gp->pkt_sent;
	gp->data_sent += (unsigned long) data_len;
	gp->bytes_sent += (unsigned long) len;

	gp->tx_stamp = jiffies;
	if(gp->first_tx_stamp == 0) { /* This is the first sent */
		gp->first_tx_stamp = gp->tx_stamp;
		gp->t_sample = jiffies;
		gp->b_sample = gp->bytes_sent;
	}

	if(gp->byte_budget != LONG_MIN)
		gp->byte_budget -= (long)(len);

	elapsed = jiffies - gp->first_tx_stamp;
	if(gp->first_tx_stamp != 0 && elapsed != 0)
		gp->total_rate = (HZ * gp->bytes_sent) / elapsed;

	if(gp->pkt_sent >= gp->sample_len) {

		gp->t_sample = jiffies - gp->t_sample;
		gp->b_sample = gp->bytes_sent - gp->b_sample;

		if(gp->t_sample != 0 && gp->b_sample != 0)
			gp->sample_rate = (HZ * gp->b_sample) / gp->t_sample;
	}
}

static void gmtp_xmit_packet(struct sock *sk, struct sk_buff *skb) {

	int err, data_len;
	struct gmtp_sock *gp = gmtp_sk(sk);

	data_len = skb->len; /* Only data */
	GMTP_SKB_CB(skb)->type = GMTP_PKT_DATA;

	err = gmtp_transmit_skb(sk, skb);
	if (err)
		gmtp_print_debug("transmit_skb() returned err=%d\n", err);

	/*
	 * Register this one as sent (even if an error occurred).
	 * @data_len: only data (before gmtp_transmit_skb)
	 * @skb->len: packet (after gmtp_transmit_skb)
	 */
	packet_sent(gp, data_len);
}

/**
 * Return difference between actual tx_rate and max tx_rate
 * If return > 0, actual tx_rate is greater than max tx_rate
 */
static int get_rate_gap(struct gmtp_sock *gp, int acum)
{
	long rate = (long)gp->sample_rate;
	long tx = (long)gp->max_tx;
	int coef_adj = 0;

	if(gp->pkt_sent < GMTP_MIN_SAMPLE_LEN)
		return 0;

	if(rate <= gp->total_rate/2) /* Eliminate discrepancies */
		rate = (long)gp->total_rate;

	coef_adj = (int) (((rate - tx) * 100) / tx);

	if(acum) {
		coef_adj += gp->adj_budget;
		if(coef_adj > 110 || coef_adj < -90)
			gp->adj_budget = 0;
		else
			gp->adj_budget += coef_adj;
	}

	return coef_adj;
}

void gmtp_write_xmit(struct sock *sk, struct sk_buff *skb)
{
	struct gmtp_sock *gp = gmtp_sk(sk);
	unsigned long rest, elapsed;
	long delay = 0;
	long delay2 = 0;
	long delay_budget = 0;
	int len = pkt_len(skb->len);
	int rc = 0;

	/** TODO Continue tests with different factors... */
	static const int factor = 1;
	/*static const int factor = HZ/100;*/

	gmtp_print_function();

	if(gp->max_tx == 0)
		goto send;

	/*
	pr_info("[%d] Tx rate: %lu bytes/s\n", gp->pkt_sent, gp->total_rate);
	pr_info("[-] Tx rate (sample): %lu bytes/s\n", gp->sample_rate);
	*/

	elapsed = jiffies - gp->tx_stamp; /* time elapsed since last sent */

	if(gp->byte_budget >= (3*len)/4) {
		goto send;
	} else if(gp->byte_budget != LONG_MIN) {
		delay_budget = factor;
		goto wait;
	}

	delay = (HZ * len) / gp->max_tx;
	rest = (HZ * len) % gp->max_tx;
	if(rest >= gp->max_tx/2)
		++delay;

	delay2 = delay - elapsed;

	if(delay2 > 0) {
		long coef_adj = (long) get_rate_gap(gp, 1);
		long delay_adjust = 0;
		delay_adjust = (delay2 * coef_adj) / 100;
		delay2 += delay_adjust;
	}

wait:
	delay2 += delay_budget;

	/* We set LONG_MIN to indicate that byte_budget is over */
	if(delay2 > 0)
		rc = gmtp_wait_for_delay(sk, delay2);
	if(rc < 0)
		return;

	/* Work fine!
	 * TODO More tests with byte_budgets...
	 */
	if(delay <= 0) {
		long coef_adj = 0;
		coef_adj = (long) get_rate_gap(gp, 0);
		gp->byte_budget = factor * (gp->max_tx / HZ);
		gp->byte_budget -= (gp->byte_budget * coef_adj) / 100;
	} else
		gp->byte_budget = LONG_MIN;

send:
	gmtp_xmit_packet(sk, skb);
}
EXPORT_SYMBOL_GPL(gmtp_write_xmit);

