/*
 * mcc.h
 *
 *  Created on: 13/04/2015
 *      Author: wendell
 */

#ifndef MCC_INTRA_H_
#define MCC_INTRA_H_

#include <linux/gmtp.h>

void gmtp_intra_mcc_delay(struct sk_buff *skb);

#endif /* MCC_INTRA_H_ */
