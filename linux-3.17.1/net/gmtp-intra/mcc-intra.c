/*
 * mcc.h
 *
 *  Created on: 13/04/2015
 *      Author: wendell
 */

#include "gmtp-intra.h"
#include "mcc-intra.h"

ktime_t before;

void gmtp_intra_mcc_delay(struct sk_buff *skb)
{
	ktime_t ts, now;

	gmtp_pr_func();

	ts = skb->tstamp;
	now = ktime_get_real();

	pr_info("now->ts: %lld us\n", ktime_us_delta(now, ts));
	pr_info("now->ts: %lld ms\n", ktime_to_ms(ktime_sub(now, ts)));

	pr_info("delta: %lld us\n", ktime_us_delta(ts, before));
	pr_info("delta: %lld ms\n", ktime_to_ms(ktime_sub(ts, before)));

	before = ts;
}
