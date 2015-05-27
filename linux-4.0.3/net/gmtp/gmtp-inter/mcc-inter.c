/*
 * mcc.h
 *
 *  Created on: 13/04/2015
 *      Author: wendell
 */

#include "gmtp-inter.h"
#include "mcc-inter.h"

void gmtp_inter_mcc_delay(struct gmtp_flow_info *info, struct sk_buff *skb,
		u64 server_tx)
{
	u64 delay;
	s64 elapsed, delay2;
	u64 tx = info->current_tx;
	unsigned int len = skb->len + ETH_HLEN;

	if(tx == 0 || tx >= server_tx) {
		/*pr_info("tx: %lld >= %lld || 0\n", tx, server_tx);*/
		goto out;
	}

	delay = DIV_ROUND_CLOSEST(len * USEC_PER_SEC, tx);
	elapsed = ktime_us_delta(skb->tstamp, info->last_rx_tstamp);
	delay2 = delay - elapsed;

	/*pr_info("delay = (1000000 * %u)/%llu = %llu us\n", len, tx, delay);*/
	/*pr_info("elapsed = %lld\n", elapsed);*/
	pr_info("delay2 = %lld us (%lld ms)\n", delay2, (delay2/1000));

	/* if delay2 <= 0, pass way... */
	if(delay2 > 0)
		gmtp_inter_wait_us(delay2);

out:
	info->last_rx_tstamp = skb->tstamp;
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
