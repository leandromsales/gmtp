/*
 * ucc.h
 *
 *  Created on: 14/05/2015
 *      Author: wendell
 */

#ifndef UCC_H_
#define UCC_H_

#define GMTP_ALPHA(X) DIV_ROUND_CLOSEST((X) * 30, 100) /* X*0.3 */
#define GMTP_BETA(X)  DIV_ROUND_CLOSEST((X) * 60, 100) /* X*0.6 */
#define GMTP_GHAMA(X) (X) /* DIV_ROUND_CLOSEST(X * 100, 100)*/ /* X*1.0 */
#define GMTP_THETA(X) DIV_ROUND_CLOSEST((X) * 2000, 100000) /* X*0.02 */
#define GMTP_ONE_MINUS_THETA(X) DIV_ROUND_CLOSEST((X) * 98000, 100000) /* X*(1-0.02) */

#include "gmtp-inter.h"

enum gmtp_ucc_log_level {
	GMTP_UCC_NONE,
	GMTP_UCC_OUTPUT,
	GMTP_UCC_DEBUG,
	GMTP_UCC_ALL
};

/** gmtp-ucc. */
void gmtp_ucc_equation_callback(unsigned long);
unsigned int gmtp_relay_queue_size(void);
void gmtp_inter_ack_timer_callback(unsigned long data);
void gmtp_inter_register_timer_callback(unsigned long data);

void gmtp_ucc_equation(enum gmtp_ucc_log_level log_level);


struct gmtp_relay;
struct gmtp_inter_entry;

struct gmtp_inter_ucc_protocol {
	void (*congestion_control)(struct sk_buff *skb,
			struct gmtp_inter_entry *entry,
			struct gmtp_relay *relay);
};

int gmtp_inter_build_ucc(struct gmtp_inter_ucc_protocol *ucc,
		enum gmtp_ucc_type ucc_type);

void gmtp_inter_delay_cc(struct sk_buff *skb, struct gmtp_inter_entry *entry,
		struct gmtp_relay *relay);
void gmtp_inter_media_adapt_cc(struct sk_buff *skb,
		struct gmtp_inter_entry *entry, struct gmtp_relay *relay);

struct gmtp_inter_ucc_info {
	struct sk_buff *skb;
	struct gmtp_inter_entry *entry;
	struct gmtp_relay *relay;
	struct timer_list *timer;
};

#endif /* UCC_H_ */
