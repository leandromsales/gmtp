/*
 * mcc.h
 *
 *  Created on: 13/04/2015
 *      Author: wendell
 */

#ifndef MCC_INTER_H_
#define MCC_INTER_H_

#include <uapi/linux/gmtp.h>
#include <linux/gmtp.h>
#include "../gmtp.h"
#include "gmtp-inter.h"


static inline unsigned long gmtp_mcc_interval(unsigned int rtt)
{
	/*unsigned long interval;
	if(unlikely(rtt <= 0))
		return (jiffies + HZ);

	interval = (unsigned long) (4 * rtt);
	return (jiffies + msecs_to_jiffies(interval));*/
	return (jiffies + GMTP_ACK_INTERVAL);
}

static inline int new_reporter(struct gmtp_relay_entry *entry)
{
	return (entry->info->nclients % GMTP_REPORTER_DEFAULT_PROPORTION) == 0 ?
			gmtp_inter.kreporter : 0;
}

void gmtp_inter_mcc_delay(struct gmtp_flow_info *info, struct sk_buff *skb,
		unsigned int server_tx);

void mcc_timer_callback(unsigned long data);


#endif /* MCC_INTER_H_ */
