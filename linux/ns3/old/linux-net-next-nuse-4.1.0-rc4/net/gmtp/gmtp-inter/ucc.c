/*
 * gmtp-ucc.c
 *
 *  Created on: 08/03/2015
 *      Author: wendell
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/ktime.h>
#include <linux/kthread.h>
#include <linux/sched.h>

#include "../gmtp.h"
#include "gmtp-inter.h"
#include "ucc.h"

extern struct gmtp_inter gmtp_inter;

void gmtp_ucc_equation_callback(unsigned long data)
{
	/*unsigned int next = min(gmtp_inter.worst_rtt, gmtp_inter.h_user);
	if(next <= 0)
		next = GMTP_DEFAULT_RTT;*/

	unsigned int next = GMTP_DEFAULT_RTT;
	gmtp_pr_func();
	gmtp_ucc_equation(GMTP_UCC_NONE);
	mod_timer(&gmtp_inter.gmtp_ucc_timer, jiffies + msecs_to_jiffies(next));
}

unsigned int gmtp_relay_queue_size()
{
	return gmtp_inter.total_bytes_rx;
}

static inline unsigned long gmtp_ucc_interval(unsigned int rtt)
{
	unsigned long interval;
	if(unlikely(rtt <= 0))
		return (jiffies + GMTP_ACK_INTERVAL);

	interval = (unsigned long)(rtt);
	return (jiffies + msecs_to_jiffies(interval));
}

void gmtp_inter_ack_timer_callback(unsigned long data)
{
	struct gmtp_inter_entry *entry = (struct gmtp_inter_entry*) data;
	struct sk_buff *skb;

//	gmtp_ucc_equation(GMTP_UCC_NONE);
	gmtp_ucc_equation(GMTP_UCC_ALL);
	skb = gmtp_inter_build_ack(entry);
	if(skb != NULL)
		gmtp_inter_send_pkt(skb);

	mod_timer(&entry->ack_timer, gmtp_ucc_interval(gmtp_inter.worst_rtt));
}

void gmtp_inter_register_timer_callback(unsigned long data)
{
	struct gmtp_inter_entry *entry = (struct gmtp_inter_entry*) data;
	struct sk_buff *skb = gmtp_inter_build_register(entry);

	if(skb != NULL)
		gmtp_inter_send_pkt(skb);
	mod_timer(&entry->register_timer, jiffies + HZ);
}

/**
 * Get TX rate, via RCP
 *
 * FIXME Work with MSEC in RTT and TX.
 * After convert to SEC...
 */
void gmtp_ucc_equation(enum gmtp_ucc_log_level log_level)
{
	int r = 0, H, h;
	s64 up, mult, delta;

	int r_prev = gmtp_inter.ucc_rx;
	int C = gmtp_inter.capacity;
	s64 q = gmtp_inter.buffer_len * 1000;
   	int y = gmtp_inter.total_rx;
   	int rcv_bytes = gmtp_inter.ucc_bytes;

   	s64 alpha_cy, beta_qh;

	unsigned long current_time = ktime_to_ms(ktime_get_real());
	unsigned long elapsed = current_time - gmtp_inter.ucc_rx_tstamp;

	if(elapsed != 0)
		y = DIV_ROUND_CLOSEST(gmtp_inter.ucc_bytes * MSEC_PER_SEC, elapsed);
		/*y = DIV_ROUND_CLOSEST(gmtp_inter.ucc_bytes, elapsed);*/ /* B/ms */

	/* FIXME Sane RTT before using it (in server and relays) */
	h = gmtp_inter.worst_rtt;
	if(h <= 0) {
		gmtp_pr_error("Error: h = %u. Assuming h = 64 ms", h);
		h = GMTP_DEFAULT_RTT;
	}
	H = min(h, gmtp_inter.h_user);

	alpha_cy = (s64) GMTP_ALPHA(GMTP_GHAMA(C)-y);
	beta_qh = (s64) GMTP_BETA(q / h);
	up = DIV_ROUND_CLOSEST(H, h) * (alpha_cy - beta_qh);
	/*up = DIV_ROUND_CLOSEST(H, h) * (GMTP_ALPHA(GMTP_GHAMA(C)-y) - GMTP_BETA(q / h));*/
	/*delta = DIV_ROUND_CLOSEST(r_prev * up, GMTP_GHAMA(C));*/

	mult = up * (s64)r_prev; /* s64 */
	delta = DIV_ROUND_CLOSEST(mult, (s64) GMTP_GHAMA(C));

	/*delta = (r_prev * up) / GMTP_GHAMA(C);*/

	/**
	 * r = r_prev * (1 + up/GHAMA(C)) =>
	 * r = r_prev + [(r_prev * up) / GMTP_GHAMA(C)] =>
	 * r = r_prev + delta
	 */
	r = r_prev + (int) delta;
	r = max(r, 1000);
	r = min(r, GMTP_GHAMA(C));
	gmtp_inter.ucc_rx = r;

	/* Reset GMTP-UCC variables */
	gmtp_inter.ucc_bytes = 0;
	gmtp_inter.ucc_rx_tstamp = ktime_to_ms(ktime_get_real());
	gmtp_inter.total_rx = y;

	if(log_level > GMTP_UCC_NONE) {

		if(log_level > GMTP_UCC_OUTPUT) {
			pr_info("------------------------------------------\n");
			gmtp_pr_debug("r_prev: %d B/s", r_prev);
			gmtp_pr_debug("Received: %d B\n", rcv_bytes);
			gmtp_pr_debug("h_user: %d ms", gmtp_inter.h_user);
			gmtp_pr_debug("h0: %d ms", h);
			gmtp_pr_debug("H: %d ms", H);
			gmtp_pr_debug("C: %d B/s", C);
			gmtp_pr_debug("y(t): %d B/s", y);
			gmtp_pr_debug("buffer(t): %u B", gmtp_inter.buffer_len);
			gmtp_pr_debug("q(t): %lld B", q);
			gmtp_pr_debug("H/h0: %d\n", DIV_ROUND_CLOSEST(H, h));

			if(log_level > GMTP_UCC_DEBUG) {
				gmtp_pr_debug("GHAMA(C): %d", GMTP_GHAMA(C));
				gmtp_pr_debug("GHAMA(C)-y: %d", GMTP_GHAMA(C)-y);
				gmtp_pr_debug("ALPHA(GHAMA(C)-y): %lld", alpha_cy);
				gmtp_pr_debug("up = ((H / h) * [ALPHA(GHAMA(C)-y) " "- BETA(q / h)])");
				gmtp_pr_debug("up = %d * [%lld - %lld]", H / h,
						alpha_cy, beta_qh);
				gmtp_pr_debug("up = %lld\n", up);
				gmtp_pr_debug("delta = (r_prev * up)/GHAMA(C)");
				gmtp_pr_debug("delta = (%d * %lld)/%d", r_prev,
						up, GMTP_GHAMA(C));
				gmtp_pr_debug("delta = %lld/%d", mult,
						GMTP_GHAMA(C));

			}

			gmtp_pr_debug("delta = %lld", delta);
			gmtp_pr_debug("new_r = (int)(r_prev) + delta");
			gmtp_pr_debug("new_r = %d + %lld\n", r_prev, delta);
		}
		gmtp_pr_debug("new_r = %d B/s", r);

		if(log_level > GMTP_UCC_OUTPUT)
			pr_info("------------------------------------------\n");
	}

}
EXPORT_SYMBOL_GPL(gmtp_ucc_equation);

int gmtp_inter_build_ucc(struct gmtp_inter_ucc_protocol *ucc,
		enum gmtp_ucc_type ucc_type)
{
	switch(ucc_type) {
	case GMTP_DELAY_UCC:
		ucc->congestion_control = gmtp_inter_delay_cc;
		break;
	case GMTP_MEDIA_ADAPT_UCC:
		ucc->congestion_control = gmtp_inter_media_adapt_cc;
		break;
	default:
		return 1;
		break;
	}

	return 0;
}

struct gmtp_delay_cc_data {
	struct sk_buff *skb;
	struct gmtp_inter_entry *entry;
	struct gmtp_relay *relay;
	struct timer_list *delay_cc_timer;
};

void gmtp_inter_xmit_timer(unsigned long data)
{
	struct gmtp_delay_cc_data *cc_data = (struct gmtp_delay_cc_data *) data;
	gmtp_inter_delay_cc(cc_data->skb, cc_data->entry, cc_data->relay);
	del_timer(cc_data->delay_cc_timer);
	kfree(cc_data->delay_cc_timer);
}

void gmtp_inter_delay_cc(struct sk_buff *skb, struct gmtp_inter_entry *entry,
		struct gmtp_relay *relay)
{
	struct iphdr *iph = ip_hdr(skb);
	struct gmtp_hdr *gh = gmtp_hdr(skb);

	unsigned long elapsed = 0;
	long delay = 0, delay2 = 0, delay_budget = 0;
	unsigned long tx_rate = relay->tx_rate;
	int len;

	/** TODO Continue tests with different scales... */
	static const int scale = 1;
	/*static const int scale = HZ/100;*/

	if(tx_rate == UINT_MAX)
		goto send;

	elapsed = jiffies - entry->last_data_tstamp;
	len = skb->len;
	if(relay->tx_byte_budget >= mult_frac(len, 3, 4)) {
		goto send;
	} else if(relay->tx_byte_budget != INT_MIN) {
		delay_budget = scale;
		goto wait;
	}

	delay = DIV_ROUND_CLOSEST((HZ * len), tx_rate);
	delay2 = delay - elapsed;

	/*if(delay2 > 0)
		delay2 += mult_frac(delay2, get_rate_gap(gp, 1), 100);*/

wait:
	delay2 += delay_budget;

	if(delay <= 0)
		relay->tx_byte_budget =	mult_frac(scale, tx_rate, HZ) /*-
			mult_frac(relay->tx_byte_budget,
					(int) get_rate_gap(gp, 0), 100)*/;
	else
		relay->tx_byte_budget = INT_MIN;

	/* Subcaminhos GMTP-UCC... */
	 if(delay2 > 0) {
		struct gmtp_delay_cc_data *cc_data;
		cc_data = kmalloc(sizeof(struct gmtp_delay_cc_data), GFP_KERNEL);
		cc_data->skb = skb;
		cc_data->entry = entry;
		cc_data->relay = relay;
		cc_data->delay_cc_timer = kmalloc(sizeof(struct timer_list),
				GFP_KERNEL);

		setup_timer(cc_data->delay_cc_timer, gmtp_inter_xmit_timer,
				(unsigned long) cc_data);
		mod_timer(cc_data->delay_cc_timer, jiffies + delay2);

		schedule_timeout(delay2);
		return;
	}

send:
	gmtp_inter_build_and_send_pkt(skb, iph->saddr, relay->addr, gh,
			GMTP_INTER_FORWARD);
	gmtp_ucc_buffer_dequeue(skb);

}

void gmtp_inter_media_adapt_cc(struct sk_buff *skb, struct gmtp_inter_entry *entry,
		struct gmtp_relay *relay)
{
	struct iphdr *iph = ip_hdr(skb);
	struct gmtp_hdr *gh = gmtp_hdr(skb);

	unsigned int len, hdrlen, datalen, datalen20, datalen40, datalen80;
	unsigned int rate, new_datalen;
	unsigned int rx_rate = entry->current_rx <= 0 ?
			(unsigned int) entry->transm_r : entry->current_rx;

	hdrlen = gmtp_data_hdr_len() + ip_hdrlen(skb);
	new_datalen = datalen = skb->len - hdrlen;

	if(relay->tx_rate >= rx_rate)
		goto send;

	rate = DIV_ROUND_CLOSEST(1000 * rx_rate,
			(unsigned int) relay->tx_rate);

	if(rate == 0)
		goto send;

	datalen20 = DIV_ROUND_CLOSEST(200 * skb->len, 1000) - hdrlen;
	datalen40 = DIV_ROUND_CLOSEST(400 * skb->len, 1000) - hdrlen;
	datalen80 = DIV_ROUND_CLOSEST(800 * skb->len, 1000) - hdrlen;

	new_datalen = DIV_ROUND_CLOSEST(skb->len * 1000, rate) - hdrlen;

	char label[90];

	if(new_datalen >= datalen80)
		new_datalen = datalen80;
	else if(new_datalen >= datalen40)
		new_datalen = datalen40;
	else if(new_datalen >= datalen20)
		new_datalen = datalen20;
	else {
		goto drop;
	}

	if(new_datalen < 0) {
		goto drop;

	}

	sprintf(label, "Cur_RX: %u B/s, TX: %lu B/s. Data to %pI4, reducing to %u B (-%u B): ",
			rx_rate, relay->tx_rate, &relay->addr, new_datalen,
			(datalen - new_datalen));
	print_gmtp_data(skb, label);

send:
	gmtp_inter_build_and_send_pkt_len(skb, iph->saddr, relay->addr,
			gh, new_datalen, GMTP_INTER_FORWARD);
	return;
drop:
	sprintf(label, "Cur_RX: %u B/s, TX: %lu B/s. Dropping packet to %pI4: ",
			rx_rate, relay->tx_rate, &relay->addr);
	print_gmtp_data(skb, label);
}



