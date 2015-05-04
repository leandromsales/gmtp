/*
 * gmtp-ucc.c
 *
 *  Created on: 08/03/2015
 *      Author: wendell
 */

#include <linux/kernel.h>
#include <linux/module.h>

#include "gmtp-intra.h"

#define ALPHA(X) (((X)*3)/10)  /* X*0.3 */
#define BETA(X)  (((X)*6)/10)  /* X*0.6 */
#define GHAMA(X) (((X)*10)/10) /* X*1.0 */
#define THETA(X) (((X)*2)/100) /* X*0.02 */

#define MOD(X)   ((X > 0) ? (X) : -(X))

#define C 1250 /* bytes/ms => 10 Mbit/s */

extern struct gmtp_intra gmtp;

unsigned int gmtp_rtt_average()
{
	return 1;
}

unsigned int gmtp_rx_rate()
{
	return 0;
}

unsigned int gmtp_relay_queue_size()
{
	return gmtp.total_bytes_rx;
}

/**
 * Get the last calculated TX
 */
unsigned int gmtp_get_current_rx_rate()
{
	return gmtp.total_rx;
}
EXPORT_SYMBOL_GPL(gmtp_get_current_rx_rate);

/**
 * Get TX rate, via RCP
 */
void gmtp_update_rx_rate(unsigned int  h_user)
{
	unsigned int r = 0;
	unsigned int H, h, y, q;
	unsigned int r_prev = gmtp_get_current_rx_rate();
	int up, delta;

	gmtp_pr_func();

	gmtp_print_debug("r_prev: %d", r_prev);

	h = gmtp_rtt_average();
	y = gmtp_rx_rate();
	q = gmtp_relay_queue_size();

	gmtp_print_debug("h_user: %u", h_user);
	gmtp_print_debug("h0: %u", h);
	gmtp_print_debug("y(t): %u", y);
	gmtp_print_debug("q(t): %u", q);

	H = (h < h_user) ? h : h_user;

	up = (H/h) * (ALPHA(GHAMA(C)-y) - BETA(q/h));

	gmtp_print_debug("H/h0: %u", (H/h));
	gmtp_print_debug("ALPHA(GHAMA(C)-y): %u", ALPHA(GHAMA(C)-y));
	gmtp_print_debug("BETA(q/h): %u", BETA(q/h));
	gmtp_print_debug("ALPHA(GHAMA(C)-y) - BETA(q/h): %d", ALPHA(GHAMA(C)-y) - BETA(q/h));

	gmtp_print_debug("up: %d", up);

	delta = ((int)(r_prev) * up)/GHAMA(C);

	gmtp_print_debug("delta = ((r_prev * up)/GHAMA(C)): %d", delta);

	/**
	 * r = r_prev * (1 + up/GHAMA(C)) =>
	 * r = r_prev + r_prev * up/GHAMA(C) =>
	 * r = r_prev + delta
	 */
	r = (int)(r_prev) + delta;

	gmtp_print_debug("new_r: %d", r);
	gmtp.total_rx = r;
}
EXPORT_SYMBOL_GPL(gmtp_update_rx_rate);

