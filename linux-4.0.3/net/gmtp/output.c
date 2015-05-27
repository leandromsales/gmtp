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

	if (likely(skb != NULL)) {

		struct inet_sock *inet = inet_sk(sk);
		const struct inet_connection_sock *icsk = inet_csk(sk);
		struct gmtp_sock *gp = gmtp_sk(sk);
		struct gmtp_skb_cb *gcb = GMTP_SKB_CB(skb);
		struct gmtp_hdr *gh;
		const u32 gmtp_header_size = sizeof(struct gmtp_hdr) +
				gmtp_packet_hdr_variable_len(gcb->type);
		int err, set_ack = 1;

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

		gh->version = GMTP_VERSION;
		gh->type = gcb->type;
		gh->sport = inet->inet_sport;
		gh->dport = inet->inet_dport;
		gh->hdrlen = gmtp_header_size;
		gh->server_rtt = gp->role == GMTP_ROLE_SERVER ?
				TO_U8(gp->tx_rtt) : TO_U8(gp->rx_rtt);

		memcpy(gh->flowname, gp->flowname, GMTP_FLOWNAME_LEN);

		if (gcb->type == GMTP_PKT_FEEDBACK)
			gh->transm_r = gp->rx_max_rate;
		else
			gh->transm_r = (__be32) gp->tx_max_rate;

		if (gcb->type == GMTP_PKT_RESET)
			gmtp_hdr_reset(skb)->reset_code = gcb->reset_code;

		if(gcb->type == GMTP_PKT_DATA) {
			struct gmtp_hdr_data *gh_data = gmtp_hdr_data(skb);
			gh_data->tstamp = jiffies_to_msecs(jiffies);
		}

		gcb->seq = ++gp->gss;
		if (set_ack) {
			struct gmtp_hdr_ack *gack = gmtp_hdr_ack(skb);
			gack->ackcode = gcb->ackcode;
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
	gh->server_rtt	= TO_U8(gmtp_sk(sk)->tx_rtt);
	gh->transm_r	= (__be32) gmtp_sk(sk)->tx_max_rate;
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
	gh->server_rtt	= TO_U8(gp->tx_rtt);
	gh->transm_r	= (__be32) gp->tx_max_rate;
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
int gmtp_connect(struct sock *sk)
{
	struct sk_buff *skb;
	struct gmtp_sock *gp = gmtp_sk(sk);
	struct dst_entry *dst = __sk_dst_get(sk);
	struct inet_sock *inet = inet_sk(sk);
	struct inet_connection_sock *icsk = inet_csk(sk);

	struct gmtp_client_entry *client_entry;
	int err = 0;

	gmtp_print_function();

	sk->sk_err = 0;
	sock_reset_flag(sk, SOCK_DONE);

	gmtp_sync_mss(sk, dst_mtu(dst));

	skb = alloc_skb(sk->sk_prot->max_header, sk->sk_allocation);
	if (unlikely(skb == NULL))
		return -ENOBUFS;

	/* Reserve space for headers. */
	skb_reserve(skb, sk->sk_prot->max_header);
	GMTP_SKB_CB(skb)->type = GMTP_PKT_REQUEST;

	client_entry = gmtp_lookup_client(gmtp_hashtable, gp->flowname);
	if(client_entry == NULL)
		err = gmtp_add_client_entry(gmtp_hashtable, gp->flowname,
				inet->inet_saddr, inet->inet_sport, 0, 0);
	else
		gmtp_list_add_client(0, inet->inet_saddr, inet->inet_sport, 0,
				&client_entry->clients->list);

	/** First transmission: gss <- iss */
	gp->gss = gp->iss;
	gp->req_stamp = jiffies_to_msecs(jiffies);
	gp->ack_rcv_tstamp = jiffies_to_msecs(jiffies);

	gmtp_transmit_skb(sk, gmtp_skb_entail(sk, skb));

	icsk->icsk_retransmits = 0;
	inet_csk_reset_xmit_timer(sk, ICSK_TIME_RETRANS,
					  icsk->icsk_rto, GMTP_RTO_MAX);
	return err;
}
EXPORT_SYMBOL_GPL(gmtp_connect);

void gmtp_send_ack(struct sock *sk, __u8 ackcode)
{
	gmtp_print_function();

	/* If we have been reset, we may not send again. */
	if(sk->sk_state != GMTP_CLOSED) {

		struct sk_buff *skb = alloc_skb(sk->sk_prot->max_header,
		GFP_ATOMIC);

		/* FIXME How to define the ackcode in this case? */
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
		GMTP_SKB_CB(skb)->ackcode = ackcode;
		gmtp_transmit_skb(sk, skb);
	}
}
EXPORT_SYMBOL_GPL(gmtp_send_ack);


void gmtp_send_feedback(struct sock *sk)
{
	gmtp_pr_func();

	if(sk->sk_state != GMTP_CLOSED) {

		struct sk_buff *skb = alloc_skb(sk->sk_prot->max_header,
		GFP_ATOMIC);

		/* Reserve space for headers */
		skb_reserve(skb, sk->sk_prot->max_header);
		GMTP_SKB_CB(skb)->type = GMTP_PKT_FEEDBACK;
		gmtp_transmit_skb(sk, skb);
	}
}
EXPORT_SYMBOL_GPL(gmtp_send_feedback);

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
static long gmtp_wait_for_delay(struct sock *sk, unsigned long delay)
{
	DEFINE_WAIT(wait);
	long remaining;

	prepare_to_wait(sk_sleep(sk), &wait, TASK_UNINTERRUPTIBLE);
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

static inline unsigned int packet_len(struct sk_buff *skb)
{
	/* Total = Data + (GMTP + IP + MAC) */
	return skb->len + (gmtp_hdr_len(skb) + 20 + ETH_HLEN);
}

static inline unsigned int payload_len(struct sk_buff *skb)
{
	/* Data = Total - (GMTP + IP + MAC) */
	return skb->len - (gmtp_hdr_len(skb) + 20 + ETH_HLEN);
}

/*static inline void packet_sent(struct gmtp_sock *gp, int data_len)*/
static inline void packet_sent(struct sock *sk, struct sk_buff *skb)
{
	struct gmtp_sock *gp = gmtp_sk(sk);
	unsigned long elapsed = 0;
	int data_len = payload_len(skb);

	if(gp->tx_dpkts_sent == 0)
		gmtp_pr_info("Start sending data packets...\n\n");

	++gp->tx_dpkts_sent;
	gp->tx_data_sent += data_len;
	gp->tx_bytes_sent += skb->len;

	gp->tx_last_stamp = jiffies;
	if(gp->tx_first_stamp == 0) { /* This is the first sent */
		gp->tx_first_stamp = gp->tx_last_stamp;
		gp->tx_time_sample = jiffies;
		gp->tx_byte_sample = gp->tx_bytes_sent;
	}

	if(gp->tx_byte_budget != INT_MIN)
		gp->tx_byte_budget -= (int) skb->len;

	elapsed = jiffies - gp->tx_first_stamp;
	if(gp->tx_first_stamp != 0 && elapsed != 0)
		gp->tx_total_rate = mult_frac(HZ, gp->tx_bytes_sent, elapsed);

	if(gp->tx_dpkts_sent >= gp->tx_sample_len) {

		gp->tx_time_sample = jiffies - gp->tx_time_sample;
		gp->tx_byte_sample = gp->tx_bytes_sent - gp->tx_byte_sample;

		if(gp->tx_time_sample != 0 && gp->tx_byte_sample != 0)
			gp->tx_sample_rate = mult_frac(HZ,
							gp->tx_byte_sample,
							gp->tx_time_sample);
	}
}

static void gmtp_xmit_packet(struct sock *sk, struct sk_buff *skb) {

	int err;

	GMTP_SKB_CB(skb)->type = GMTP_PKT_DATA;

	err = gmtp_transmit_skb(sk, skb);
	if (err)
		gmtp_pr_error("transmit_skb() returned err=%d\n", err);

	/*
	 * Register this one as sent (even if an error occurred).
	 */
	packet_sent(sk, skb);
}

/**
 * Return difference between actual tx_rate and max tx_rate
 * If return > 0, actual tx_rate is greater than max tx_rate
 */
static long get_rate_gap(struct gmtp_sock *gp, int acum)
{
	long rate = (long)gp->tx_sample_rate;
	long tx = (long)gp->tx_max_rate;
	long coef_adj = 0;

	if(gp->tx_dpkts_sent < GMTP_MIN_SAMPLE_LEN)
		return 0;

	if(rate <= gp->tx_total_rate/2) /* Eliminate discrepancies */
		rate = (long)gp->tx_total_rate;

	coef_adj = mult_frac((rate - tx), 100, tx);

	if(acum) {
		coef_adj += gp->tx_adj_budget;
		if(coef_adj > 110 || coef_adj < -90)
			gp->tx_adj_budget = 0;
		else
			gp->tx_adj_budget += coef_adj;
	}

	return coef_adj;
}

void gmtp_write_xmit(struct sock *sk, struct sk_buff *skb)
{
	struct gmtp_sock *gp = gmtp_sk(sk);
	int len = (int) packet_len(skb);
	unsigned long elapsed = 0;
	long delay = 0, delay2 = 0, delay_budget = 0;

	/** TODO Continue tests with different scales... */
	static const int scale = 1;
	/*static const int scale = HZ/100;*/

	if(gp->tx_max_rate == 0UL)
		goto send;

	/*
	pr_info("[%d] Tx rate: %lu bytes/s\n", gp->pkt_sent, gp->total_rate);
	pr_info("[-] Tx rate (sample): %lu bytes/s\n", gp->sample_rate);
	*/

	elapsed = jiffies - gp->tx_last_stamp; /* time elapsed since last sent */

	if(gp->tx_byte_budget >= mult_frac(len, 3, 4)) {
		goto send;
	} else if(gp->tx_byte_budget != INT_MIN) {
		delay_budget = scale;
		goto wait;
	}

	delay = DIV_ROUND_CLOSEST((HZ * len), gp->tx_max_rate);
	delay2 = delay - elapsed;

	if(delay2 > 0)
		delay2 += mult_frac(delay2, get_rate_gap(gp, 1), 100);

wait:
	delay2 += delay_budget;
	if(delay2 > 0)
		gmtp_wait_for_delay(sk, delay2);

	/*
	 * TODO More tests with byte_budgets...
	 */
	if(delay <= 0)
		gp->tx_byte_budget = mult_frac(scale, gp->tx_max_rate, HZ) -
			mult_frac(gp->tx_byte_budget, (int) get_rate_gap(gp, 0), 100);
	else
		gp->tx_byte_budget = INT_MIN;

send:
	gmtp_xmit_packet(sk, skb);
}
EXPORT_SYMBOL_GPL(gmtp_write_xmit);

