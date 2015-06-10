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

#define ALPHA(X) (((X)*3)/10)  /* X*0.3 */
#define BETA(X)  (((X)*6)/10)  /* X*0.6 */
#define GHAMA(X) (((X)*10)/10) /* X*1.0 */
#define THETA(X) (((X)*2)/100) /* X*0.02 */

#define MOD(X)   ((X > 0) ? (X) : -(X))

#define C 1250000 /* bytes/s => 10 Mbit/s */

extern struct gmtp_inter gmtp_inter;

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
	return gmtp_inter.total_bytes_rx;
}

/**
 * Get the last calculated TX
 */
unsigned int gmtp_get_current_rx_rate()
{
	return gmtp_inter.total_rx;
}
EXPORT_SYMBOL_GPL(gmtp_get_current_rx_rate);

/**
 * Get TX rate, via RCP
 */
void gmtp_update_rx_rate(unsigned int h_user,struct gmtp_flow_info *info)
{
   	unsigned int r = 0;
	unsigned int H, h, y, q, Nt;
	unsigned int rt;
   	unsigned int r_prev = gmtp_get_current_rx_rate();
    unsigned long elapsed;
    ktime_t stamp;
    ktime_t current_time; 
    unsigned int total_bytes;
	int up, delta;
	int flag_aux = 0;

    q = info->buffer->qlen;
    
    stamp = info->last_rx_tstamp;     
    current_time = ktime_get_real();
    elapsed = ktime_ms_delta(current_time, stamp);

    total_bytes = info->total_bytes;

    y = total_bytes/elapsed;
    
    /*atualiza o valor da variavel para a proxima chamada da funcao*/
    info->last_rx_tstamp = current_time;

    /*depois a variavel de total bytes sera zerada*/
    info->total_bytes = 0;
   
	gmtp_pr_func();

	gmtp_print_debug("r_prev: %d", r_prev);

	h = gmtp_rtt_average();
	//y = gmtp_rx_rate();
	//q = gmtp_relay_queue_size();

	gmtp_print_debug("h_user: %u", h_user);
	gmtp_print_debug("h0: %u", h);

	H = (h < h_user) ? h : h_user;
	gmtp_print_debug("H: %u\n", H);

	gmtp_print_debug("C: %u", C);
	gmtp_print_debug("y(t): %u", y);
	gmtp_print_debug("q(t): %u\n", q);

	up = (H / h) * (ALPHA(GHAMA(C)-y) - BETA(q / h));

	gmtp_print_debug("H/h0: %u", (H / h));
	gmtp_print_debug("GHAMA(C): %u", GHAMA(C));
	gmtp_print_debug("ALPHA(GHAMA(C)-y): %u", ALPHA(GHAMA(C)-y));
	gmtp_print_debug("q/h: %u", q/h);
	gmtp_print_debug("BETA(q/h): %u", BETA(q/h));
	gmtp_print_debug("ALPHA(GHAMA(C)-y) - BETA(q/h):");
	gmtp_print_debug("%u - %u: %d\n", ALPHA(GHAMA(C)-y), BETA(q/h), (ALPHA(GHAMA(C)-y) - BETA(q/h)));

	gmtp_print_debug("up = ((H / h) * [ALPHA(GHAMA(C)-y) - BETA(q / h)])");
	gmtp_print_debug("up = %u * [%u - %u]", H/h, ALPHA(GHAMA(C)-y),  BETA(q / h));
	gmtp_print_debug("up = %d\n", up);

	delta = ((int)(r_prev) * up) / GHAMA(C);

	gmtp_print_debug("delta = ((r_prev * up)/GHAMA(C))");
	gmtp_print_debug("delta = (%d * %u)/%u", (int)(r_prev), up, GHAMA(C));
	gmtp_print_debug("delta = %d\n", delta);

	/**
	 * r = r_prev * (1 + up/GHAMA(C)) =>
	 * r = r_prev + r_pc
	 * rev * up/GHAMA(C) =>
	 * r = r_prev + delta
	 */
	r = (int)(r_prev) + delta;

	gmtp_print_debug("new_r = (int)(r_prev) + delta");
	gmtp_print_debug("new_r = %d + %d", r_prev, delta);
	gmtp_print_debug("new_r = %d\n", r);
	gmtp_inter.total_rx = r;

	pr_info("-----------------------\n");

	if(flag_aux){
		/* 4.1 */
		Nt = C*r_prev;
		rt = r_prev + (up/Nt);
		gmtp_print_debug("rt equacao 4.1 = %d", rt);
	} else {
		/* 4.2 */
		rt = r_prev*(1 + ((H/h)*(ALPHA(GHAMA(C) - y) - BETA(q/h)))/GHAMA(C));
		gmtp_print_debug("rt equacao 4.2 = %d", rt);
	}
	pr_info("-----------------------\n");

}
EXPORT_SYMBOL_GPL(gmtp_update_rx_rate);

