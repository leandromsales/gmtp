/*
 * mcc.h
 *
 *  Created on: 13/04/2015
 *      Author: wendell
 */

#include "gmtp-inter.h"
#include "mcc-inter.h"

struct timer_list mcc_timer;

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
	/*pr_info("delay2 = %lld us (%lld ms)\n", delay2, (delay2/1000));*/

	/* if delay2 <= 0, pass way... */
	if(delay2 > 0)
		gmtp_inter_wait_us(delay2);

out:
	info->last_rx_tstamp = ktime_to_ms(skb->tstamp);
}

/**
 * FIXME Don't start timer if we are Waiting Register Reply...
 *
 * TODO update_stats RTT through feedbacks
 *
 * If the GMTP-MCC sender receives no reports from the Reporters for (4 RTTs)*,
 * the sending rate is cut in half.
 * TODO In addition, if the sender receives no reports from the Reporter for at
 * least (12 RTTs)*, it assumes that the Reporter crashed or left the group.
 * A new reporter is selected, sending an elect-request to control channel.
 * The first client to respond will be the new Reporter.
 * If no one respond... There no clients... Close-Connection
 *
 * TODO Undo temporally changes:
 *   * Change from 4 RTTs to GMTP_ACK_INTERVAL
 *   ** Change from 10 RTTs to GMTP_ACK_TIMEOUT
 *
 */
void mcc_timer_callback(unsigned long data)
{
	struct gmtp_flow_info *info = (struct gmtp_flow_info*) data;
	struct gmtp_client *reporter, *temp;

	/* FIXME FIX MCC on reporters to generate correct TX_rate... */
/*	if(likely(info->nfeedbacks > 0))
		info->required_tx = DIV_ROUND_CLOSEST(info->sum_feedbacks,
				info->nfeedbacks);
	else
		info->required_tx /= 2;*/

	/*pr_info("n_feedbacks: %u\n", info->nfeedbacks);
	pr_info("Required_tx=%u bytes/s\n", info->required_tx);*/

	list_for_each_entry_safe(reporter, temp, &info->clients->list, list)
	{
		unsigned int now = jiffies_to_msecs(jiffies);
		int interval = (int)(now - reporter->ack_rx_tstamp);

		/** Deleting non-reporters */
		if(unlikely(reporter->max_nclients <= 0)) {
			list_del(&reporter->list);
			kfree(reporter);
			continue;
		}

		if(unlikely(interval > jiffies_to_msecs(GMTP_ACK_TIMEOUT))) {
			pr_info("Timeout: Reporter %pI4@%-5d\n",
					&reporter->addr, ntohs(reporter->port));
			pr_info("TODO: select new reporter and delete this.\n");
		}
	}

	info->nfeedbacks = 0;
	info->sum_feedbacks = 0;

	/*pr_info("RTT: %u ms, Next interval: %lu ms\n", info->rtt,
			(gmtp_mcc_interval(info->rtt) - jiffies));*/

	/* TODO Send here an ack to server? */

	mod_timer(&info->mcc_timer, gmtp_mcc_interval(info->flow_avg_rtt));
}

