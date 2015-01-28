/*
 * minisocks.c
 *
 *  Created on: 21/01/2015
 *      Author: wendell
 */

#include <linux/gmtp.h>
#include <linux/kernel.h>

#include <net/sock.h>
#include <net/xfrm.h>
#include <net/inet_timewait_sock.h>

#include "gmtp.h"

int gmtp_reqsk_init(struct request_sock *req,
		    struct gmtp_sock const *gp, struct sk_buff const *skb)
{
	struct gmtp_request_sock *greq = gmtp_rsk(req);

	gmtp_print_function();

	inet_rsk(req)->ir_rmt_port = gmtp_hdr(skb)->sport;
	inet_rsk(req)->ir_num	   = ntohs(gmtp_hdr(skb)->dport);
	inet_rsk(req)->acked	   = 0;
//	dreq->dreq_timestamp_echo  = 0;

	return 0;
}

EXPORT_SYMBOL_GPL(gmtp_reqsk_init);

