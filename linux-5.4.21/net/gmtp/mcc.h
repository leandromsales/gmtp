// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef MCC_H_
#define MCC_H_
/*
 *  net/gmtp/mcc.h
 *
 *  Multicast Congestion Control for the GMTP protocol
 *  Wendell Silva Soares <wendell@ic.ufal.br>
 */

#include <linux/gmtp.h>

#include "mcc/mcc_proto.h"

int mcc_lib_init(void);
void mcc_lib_exit(void);

void mcc_rx_packet_recv(struct sock *sk, struct sk_buff *skb);
int mcc_rx_init(struct sock *sk);
void mcc_rx_exit(struct sock *sk);

#endif /* MCC_H_ */
