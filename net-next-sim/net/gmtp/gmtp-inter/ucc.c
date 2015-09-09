/*
 * gmtp-ucc.c
 *
 *  Created on: 08/03/2015
 *      Author: wendell
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/ktime.h>

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
	gmtp_ucc_equation(GMTP_UCC_DEBUG);
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

int gmtp_inter_build_ucc(struct gmtp_ucc_protocol *ucc,
		enum gmtp_ucc_type ucc_type)
{
	switch(ucc_type) {
	case GMTP_DELAY_UCC:
		ucc->congestion_control = gmtp_delay_cc;
		break;
	case GMTP_MEDIA_ADAPT_UCC:
		ucc->congestion_control = gmtp_media_adapt_cc;
		break;
	default:
		return 1;
		break;
	}

	return 0;
}


void gmtp_inter_xmit_timer(unsigned long data)
{
	struct gmtp_inter_ucc_info *info = (struct gmtp_inter_ucc_info*) data;
	gmtp_delay_cc(info->skb, info->entry, info->relay);
	del_timer_sync(&info->relay->xmit_timer);
	kfree(info);
}
EXPORT_SYMBOL_GPL(gmtp_inter_xmit_timer);

int gmtp_delay_cc(struct sk_buff *skb, struct gmtp_inter_entry *entry,
		struct gmtp_relay *relay)
{
	unsigned long elapsed = 0;
	long delay = 0, delay2 = 0, delay_budget = 0;
	unsigned long tx_rate = relay->tx_rate;
	struct gmtp_inter_ucc_info *info;
	int len;

	/** TODO Continue tests with different scales... */
	static const int scale = 1;
	/*static const int scale = HZ/100;*/

	if(unlikely(skb == NULL))
		return NF_DROP;

	if(tx_rate == UINT_MAX)
		goto send;

	info = kmalloc(sizeof(struct gmtp_inter_ucc_info), GFP_KERNEL);
	info->skb = skb;
	info->entry = entry;
	info->relay = relay;
	elapsed = jiffies - entry->last_data_tstamp;

	len = skb->len + (gmtp_data_hdr_len() + 20 + ETH_HLEN);
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

	/*
	 * FIXME More tests with byte_budgets...
	 */
	if(delay <= 0)
		relay->tx_byte_budget =	mult_frac(scale, tx_rate, HZ) /*-
			mult_frac(relay->tx_byte_budget,
					(int) get_rate_gap(gp, 0), 100)*/;
	else
		relay->tx_byte_budget = INT_MIN;

	if(delay2 > 0) {
		setup_timer(&relay->xmit_timer, gmtp_inter_xmit_timer,
				(unsigned long ) info);
		mod_timer(&relay->xmit_timer, jiffies + delay2);
		schedule_timeout(delay2);
		return NF_ACCEPT;
	}

send:
	return NF_ACCEPT;

}

int gmtp_media_adapt_cc(struct sk_buff *skb, struct gmtp_inter_entry *entry,
			struct gmtp_relay *relay)
{
	return NF_ACCEPT;
}

