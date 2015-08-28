/*
 * mcc.h
 *
 *  Created on: 13/04/2015
 *      Author: wendell
 */

#include "gmtp-inter.h"
#include "mcc-inter.h"

struct timer_list mcc_timer;

void gmtp_inter_mcc_delay(struct gmtp_inter_entry *info, struct sk_buff *skb,
		unsigned int server_tx)
{
	long server_delay, server_delay2, elapsed, req_delay, final_delay;
	unsigned int len = skb->len + ETH_HLEN;
	unsigned long now = jiffies_to_msecs(jiffies);

	if(info->required_tx <= 0 || info->required_tx >= server_tx)
		return;

	server_delay = (long) DIV_ROUND_CLOSEST(HZ * len, server_tx);
	elapsed = now - info->last_rx_tstamp;
	server_delay2 = server_delay - elapsed;
	req_delay = (long) DIV_ROUND_CLOSEST(HZ * len, info->required_tx);
	final_delay = req_delay - server_delay2;

	/* if delay2 <= 0, pass way... */
	if(final_delay > 0)
		gmtp_inter_wait_ms(final_delay);

}

/**
 * FIXME Dont start timer if we are Waiting Register Reply...
 *
 * TODO update_stats RTT through feedbacks
 *
 * If the GMTP-MCC sender receives no reports from the Reporters for (4 RTTs),
 * the sending rate is cut in half.
 * TODO In addition, if the sender receives no reports from the Reporter for at
 * least (12 RTTs), it assumes that the Reporter crashed or left the group.
 * A new reporter is selected, sending an elect-request to control channel.
 * The first client to respond will be the new Reporter.
 * If no one respond... There no clients... Close-Connection
 *
 * TODO Undo temporally changes:
 *   Change from 4 RTTs to GMTP_ACK_INTERVAL
 *
 */
void mcc_timer_callback(unsigned long data)
{
	struct gmtp_inter_entry *info = (struct gmtp_inter_entry*) data;
	struct gmtp_client *reporter, *temp;
	unsigned int new_tx = info->required_tx;

	if(info->nclients <= 0)
		goto out;

	if(likely(info->nfeedbacks > 0))
		new_tx = DIV_ROUND_CLOSEST(info->sum_feedbacks,
				info->nfeedbacks);
	else
		new_tx = info->required_tx / 2;

	/* Avoid super TX reduction */
	if(new_tx < DIV_ROUND_CLOSEST(info->transm_r, 8))
		new_tx = DIV_ROUND_CLOSEST(info->transm_r, 8);

	info->required_tx = new_tx;

	pr_info("n=%u, req_tx=%u B/s\n", info->nfeedbacks, info->required_tx);

	/* FIXME Colocar isso em outro timer? */
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

		/*if(unlikely(interval > jiffies_to_msecs(GMTP_ACK_TIMEOUT))) {
			pr_info("Timeout: Reporter %pI4@%-5d\n",
					&reporter->addr, ntohs(reporter->port));
			pr_info("TODO: select new reporter and delete this.\n");
		}*/
	}

out:
	info->nfeedbacks = 0;
	info->sum_feedbacks = 0;

	/*pr_info("RTT: %u ms, Next interval: %lu ms\n", info->rtt,
			(gmtp_mcc_interval(info->rtt) - jiffies));*/

	/* TODO Send here an ack to server? */
	if(likely(info->state != GMTP_INTER_CLOSE_RECEIVED
					&& info->state != GMTP_INTER_CLOSED))
		mod_timer(&info->mcc_timer,
				gmtp_mcc_interval(info->server_rtt
							+ info->clients_rtt));
}

