/*
 * mcc.h
 *
 *  Created on: 13/04/2015
 *      Author: wendell
 */

#ifndef MCC_H_
#define MCC_H_

#include <linux/gmtp.h>

#include "mcc/mcc_proto.h"

int mcc_lib_init(void);
void mcc_lib_exit(void);

void mcc_rx_packet_recv(struct sock *sk, struct sk_buff *skb);
int mcc_rx_init(struct sock *sk);
void mcc_rx_exit(struct sock *sk);

#endif /* MCC_H_ */
