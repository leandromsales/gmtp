/*
 * mcc.h
 *
 *  Created on: 13/04/2015
 *      Author: wendell
 */

#ifndef MCC_INTRA_H_
#define MCC_INTRA_H_

#include <linux/gmtp.h>
#include "gmtp-intra.h"

#define GMTP_REPORTER_PROPORTION 6

static inline int new_reporter(struct gmtp_relay_entry *entry)
{
	return (entry->info->nclients % GMTP_REPORTER_PROPORTION) == 0 ? 1 : 0;
}

void gmtp_intra_mcc_delay(struct gmtp_flow_info *info, struct sk_buff *skb,
		u64 server_tx);

#endif /* MCC_INTRA_H_ */
