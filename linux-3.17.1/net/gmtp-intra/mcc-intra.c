/*
 * mcc.h
 *
 *  Created on: 13/04/2015
 *      Author: wendell
 */

#include "gmtp-intra.h"
#include "mcc-intra.h"

void gmtp_intra_mcc_delay(struct sk_buff *skb, u64 server_tx)
{
	u64 delay;
	s64 elapsed, delay2;
	u64 tx = gmtp.current_tx;

	gmtp_pr_func();

	if(tx == 0 || tx >= server_tx) {
		pr_info("tx: %lld >= %lld || 0\n", tx, server_tx);
		goto out;
	}

	delay = DIV_ROUND_CLOSEST(skb->len * USEC_PER_SEC, tx);
	elapsed = ktime_us_delta(skb->tstamp, gmtp.last_rx_tstamp);
	delay2 = delay - elapsed;

	pr_info("delay = (1000000 * %u)/%llu = %llu us\n", skb->len, tx, delay);
	pr_info("elapsed = %lld\n", elapsed);
	pr_info("delay2 = %lld\n", delay2);

	/* if delay2 <= 0, pass way... */
	if(delay2 > 0)
		gmtp_intra_wait_us(delay2);

out:
	gmtp.last_rx_tstamp = skb->tstamp;
}
