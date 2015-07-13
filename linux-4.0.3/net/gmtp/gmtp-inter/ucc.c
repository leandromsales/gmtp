/*
 * gmtp-ucc.c
 *
 *  Created on: 08/03/2015
 *      Author: wendell
 */

#include <linux/kernel.h>
#include <linux/module.h>

#include "gmtp-inter.h"
#include "ucc.h"

#include <linux/ktime.h>

extern struct gmtp_inter gmtp_inter;

unsigned int gmtp_rtt_average()
{
	return 1;
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
void gmtp_ucc(unsigned int h_user, unsigned char debug)
{
	int up, delta;
	unsigned int r = 0, H, h;

	unsigned int r_prev = gmtp_inter.ucc_rx;
	unsigned int C = gmtp_inter.capacity;
	unsigned int q = gmtp_inter.buffer_len;
   	unsigned int y = gmtp_inter.total_rx;

	unsigned long current_time = ktime_to_ms(ktime_get_real());
	unsigned long elapsed = current_time - gmtp_inter.ucc_rx_tstamp;

	if(!!debug) {
		pr_info("\n");
		gmtp_pr_func();
	}

	if(elapsed != 0)
		y = DIV_ROUND_CLOSEST(gmtp_inter.ucc_bytes * MSEC_PER_SEC, elapsed);

	h = gmtp_rtt_average();
	H = (h < h_user) ? h : h_user;
	up = (H / h) * (GMTP_ALPHA(GMTP_GHAMA(C)-y) - GMTP_BETA(q / h));
	delta = ((int)(r_prev) * up) / GMTP_GHAMA(C);

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

	if(!!debug) {
		pr_info("r_prev: %d bytes/s\n", r_prev);
		gmtp_pr_debug("Current time: %lu ms", current_time);
		gmtp_pr_debug("Stamp: %lu ms", gmtp_inter.ucc_rx_tstamp);
		gmtp_pr_debug("Elapsed: %lu ms", elapsed);
		gmtp_pr_debug("Received bytes at interval: %u bytes\n",
				gmtp_inter.ucc_bytes);
		gmtp_pr_debug("h_user: %u", h_user);
		gmtp_pr_debug("h0: %u", h);
		gmtp_pr_debug("H: %u\n", H);
		gmtp_pr_debug("C: %u bytes/s", C);
		gmtp_pr_debug("y(t): %u bytes/s", y);
		gmtp_pr_debug("q(t): %u bytes\n", q);
		gmtp_pr_debug("H/h0: %u", (H / h));
		gmtp_pr_debug("GHAMA(C): %u", GMTP_GHAMA(C));
		gmtp_pr_debug("ALPHA(GHAMA(C)-y): %u", GMTP_ALPHA(GMTP_GHAMA(C)-y));
		gmtp_pr_debug("q/h: %u", q / h);
		gmtp_pr_debug("BETA(q/h): %u", GMTP_BETA(q/h));
		gmtp_pr_debug("ALPHA(GHAMA(C)-y) - BETA(q/h):");
		gmtp_pr_debug("%u - %u: %d\n", GMTP_ALPHA(GMTP_GHAMA(C)-y), GMTP_BETA(q/h),
				(GMTP_ALPHA(GMTP_GHAMA(C)-y) - GMTP_BETA(q/h)));

		gmtp_pr_debug("up = ((H / h) * [ALPHA(GHAMA(C)-y) - BETA(q / h)])");
		gmtp_pr_debug("up = %u * [%u - %u]", H / h, GMTP_ALPHA(GMTP_GHAMA(C)-y),
				GMTP_BETA(q / h));
		gmtp_pr_debug("up = %d\n", up);
		gmtp_pr_debug("delta = ((r_prev * up)/GHAMA(C))");
		gmtp_pr_debug("delta = (%d * %u)/%u", (int )(r_prev), up,
				GMTP_GHAMA(C));
		gmtp_pr_debug("delta = %d\n", delta);

		gmtp_pr_debug("new_r = (int)(r_prev) + delta");
		gmtp_pr_debug("new_r = %d + %d", r_prev, delta);
		gmtp_pr_debug("new_r = %d bytes/s\n", r);

		pr_info("-----------------------\n");
		pr_info("GMTP-UCC execution time: %lu ms\n",
				gmtp_inter.ucc_rx_tstamp - current_time);
		pr_info("-----------------------\n");
	}

}
EXPORT_SYMBOL_GPL(gmtp_ucc);

