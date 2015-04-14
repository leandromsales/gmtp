/*
 * mcc.h
 *
 *  Created on: 13/04/2015
 *      Author: wendell
 */

#ifndef MCC_H_
#define MCC_H_

#include <net/sock.h>
#include <linux/compiler.h>
#include <linux/gmtp.h>
#include <linux/list.h>
#include <linux/module.h>

#include "mcc/mcc_proto.h"

static void mcc_rx_packet_recv(struct sock *sk, struct sk_buff *skb);



#endif /* MCC_H_ */
