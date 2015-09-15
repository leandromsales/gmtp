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

/**
 * Get TX rate, via RCP
 *
 * FIXME Work with MSEC in RTT and TX.
 * After convert to SEC...
 */
void gmtp_ucc_equation(enum gmtp_ucc_log_level log_level)
{
	int up, delta;
	unsigned int r = 0, H, h;

	unsigned int r_prev = gmtp_inter.ucc_rx;
	unsigned int C = gmtp_inter.capacity;
	unsigned int q = gmtp_inter.buffer_len;
   	unsigned int y = gmtp_inter.total_rx;
   	unsigned int rcv_bytes = gmtp_inter.ucc_bytes;

	unsigned long current_time = ktime_to_ms(ktime_get_real());
	unsigned long elapsed = current_time - gmtp_inter.ucc_rx_tstamp;

	if(elapsed != 0)
		y = DIV_ROUND_CLOSEST(gmtp_inter.ucc_bytes * MSEC_PER_SEC, elapsed);

	/* FIXME Sane RTT before using it (in server and relays) */
	h = gmtp_inter.worst_rtt;
	if(h <= 0) {
		gmtp_pr_error("Error: h = %u. Assuming h = 64 ms", h);
		h = GMTP_DEFAULT_RTT;
	}
	H = min(h, gmtp_inter.h_user);
	up = DIV_ROUND_CLOSEST(H, h) * (GMTP_ALPHA(GMTP_GHAMA(C)-y) - GMTP_BETA(q / h));
	delta = DIV_ROUND_CLOSEST( ((int)(r_prev) * up), GMTP_GHAMA(C));

	/**
	 * r = r_prev * (1 + up/GHAMA(C)) =>
	 * r = r_prev + r_pc
	 * rev * up/GHAMA(C) =>
	 * r = r_prev + delta
	 */
	r = (int)(r_prev) + delta;
	gmtp_inter.ucc_rx = r;

	/* Reset GMTP-UCC variables */
	gmtp_inter.ucc_bytes = 0;
	gmtp_inter.ucc_rx_tstamp = ktime_to_ms(ktime_get_real());
	gmtp_inter.total_rx = y;

	if(log_level > GMTP_UCC_NONE) {

		if(log_level > GMTP_UCC_OUTPUT) {
			pr_info("------------------------------------------\n");
			gmtp_pr_debug("r_prev: %d B/s", r_prev);
			gmtp_pr_debug("Received: %u B\n", rcv_bytes);
			gmtp_pr_debug("h_user: %u ms", gmtp_inter.h_user);
			gmtp_pr_debug("h0: %u ms", h);
			gmtp_pr_debug("H: %u ms", H);
			gmtp_pr_debug("C: %u B/s", C);
			gmtp_pr_debug("y(t): %u B/s", y);
			gmtp_pr_debug("q(t): %u B", q);
			gmtp_pr_debug("H/h0: %u\n", DIV_ROUND_CLOSEST(H, h));

			if(log_level > GMTP_UCC_DEBUG) {
				gmtp_pr_debug("GHAMA(C): %u", GMTP_GHAMA(C));
				gmtp_pr_debug("ALPHA(GHAMA(C)-y): %u",
						GMTP_ALPHA(GMTP_GHAMA(C)-y));
				gmtp_pr_debug("q/h: %u", q / h);
				gmtp_pr_debug("BETA(q/h): %u", GMTP_BETA(q/h));
				gmtp_pr_debug("ALPHA(GHAMA(C)-y) - BETA(q/h):");
				gmtp_pr_debug("%u - %u: %d",
						GMTP_ALPHA(GMTP_GHAMA(C)-y),
						GMTP_BETA(q/h),
						(GMTP_ALPHA(GMTP_GHAMA(C)-y) -
								GMTP_BETA(q/h)));
				gmtp_pr_debug("up = ((H / h) * [ALPHA(GHAMA(C)-y) "
						"- BETA(q / h)])");
				gmtp_pr_debug("up = %u * [%u - %u]", H / h,
						GMTP_ALPHA(GMTP_GHAMA(C)-y),
						GMTP_BETA(q / h));
				gmtp_pr_debug("up = %d\n", up);
				gmtp_pr_debug("delta = ((r_prev * up)/GHAMA(C))");
				gmtp_pr_debug("delta = (%d * %u)/%u",
						(int )(r_prev), up,
						GMTP_GHAMA(C));

			}

			gmtp_pr_debug("delta = %d", delta);
			gmtp_pr_debug("new_r = (int)(r_prev) + delta");
			gmtp_pr_debug("new_r = %d + %d\n", r_prev, delta);
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

}

void gmtp_inter_media_adapt_cc(struct sk_buff *skb, struct gmtp_inter_entry *entry,
		struct gmtp_relay *relay)
{
	struct iphdr *iph = ip_hdr(skb);
	struct gmtp_hdr *gh = gmtp_hdr(skb);
	struct sk_buff *copy = skb_copy(skb, gfp_any());

	unsigned int len, hdrlen, datalen, datalen30, datalen60, datalen90;
	unsigned int rate, new_datalen, reduce;

	copy = skb_copy(skb, gfp_any());
	if(relay->tx_rate >= entry->transm_r)
		goto send;

	hdrlen = gmtp_data_hdr_len() + ip_hdrlen(skb) + ETH_HLEN;
	datalen = skb->len - hdrlen;

	rate = DIV_ROUND_CLOSEST(1000 * (unsigned int) entry->transm_r,
			(unsigned int) relay->tx_rate);

	if(rate == 0)
		goto send;

	datalen30 = DIV_ROUND_CLOSEST(300 * datalen, 1000);
	datalen60 = DIV_ROUND_CLOSEST(600 * datalen, 1000);
	datalen90 = DIV_ROUND_CLOSEST(900 * datalen, 1000);

	new_datalen = DIV_ROUND_CLOSEST(datalen * 1000, rate);

	if(new_datalen >= datalen90)
		new_datalen = datalen90;
	else if(new_datalen >= datalen60)
		new_datalen = datalen60;
	else if(new_datalen >= datalen30)
		new_datalen = datalen30;
	else
		return; /* drop */

	reduce = datalen - new_datalen;
	pr_info("\nNew len: %u B (reducing %u B)\n", new_datalen, reduce);
	print_gmtp_data(skb, "Reduced");

	skb_trim(copy, reduce);

send:
	gmtp_inter_build_and_send_pkt(copy, iph->saddr, relay->addr, gh,
				GMTP_INTER_FORWARD);

	/*gmtp_inter_build_and_send_pkt(skb, iph->saddr, relay->addr, gh,
				GMTP_INTER_FORWARD);*/
}



