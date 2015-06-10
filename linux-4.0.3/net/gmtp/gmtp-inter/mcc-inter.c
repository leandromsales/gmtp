/*
 * mcc.h
 *
 *  Created on: 13/04/2015
 *      Author: wendell
 */

#include "gmtp-inter.h"
#include "mcc-inter.h"

void gmtp_inter_mcc_delay(struct gmtp_flow_info *info, struct sk_buff *skb,
		unsigned int server_tx)
{
	u64 delay;
	s64 elapsed, delay2;

	unsigned int len = skb->len + ETH_HLEN;
	ktime_t rx_tstamp = ms_to_ktime(info->last_rx_tstamp);

	if(info->required_tx <= 0 || info->required_tx >= server_tx)
		goto out;

	delay = (u64) DIV_ROUND_CLOSEST(len * USEC_PER_SEC, info->required_tx);
	elapsed = ktime_us_delta(skb->tstamp, rx_tstamp);
	delay2 = delay - elapsed;

	/*pr_info("delay = (1000000 * %u)/%llu = %llu us\n", len, tx, delay);*/
	/*pr_info("elapsed = %lld\n", elapsed);*/
	/*pr_info("delay2 = %lld us (%lld ms)\n", delay2, (delay2/1000)); */

	/* if delay2 <= 0, pass way... */
	if(delay2 > 0)
		gmtp_inter_wait_us(delay2);

out:
	info->last_rx_tstamp = ktime_to_ms(skb->tstamp);
}

/*static struct timer_list my_timer;
void my_timer_callback(unsigned long data)
{
	pr_info("GMTP-inter TIMER CALLBACK (%ld).\n", jiffies);
	mod_timer(&my_timer, jiffies + msecs_to_jiffies(1000));
}*/
/* setup your timer to call my_timer_callback */
/*setup_timer(&my_timer, my_timer_callback, 0);*/
/* setup timer interval to 1000 msecs */
/*mod_timer(&my_timer, jiffies + msecs_to_jiffies(1000));*/
/*del_timer_sync(&my_timer);*/
