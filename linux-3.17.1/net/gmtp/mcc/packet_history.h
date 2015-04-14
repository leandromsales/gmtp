/*
 *  Packet RX/TX history data structures and routines for TFRC-based protocols.
 *
 *  Copyright (c) 2007   The University of Aberdeen, Scotland, UK
 *  Copyright (c) 2005-6 The University of Waikato, Hamilton, New Zealand.
 *
 *  Adapted to GMTP by
 *  Copyright (c) 2015   Federal University of Alagoas, Maceió, Brazil
 *
 *  This code has been developed by the University of Waikato WAND
 *  research group. For further information please see http://www.wand.net.nz/
 *  or e-mail Ian McDonald - ian.mcdonald@jandi.co.nz
 *
 *  This code also uses code from Lulea University, rereleased as GPL by its
 *  authors:
 *  Copyright (c) 2003 Nils-Erik Mattsson, Joacim Haggmark, Magnus Erixzon
 *
 *  Changes to meet Linux coding standards, to make it meet latest ccid3 draft
 *  and to make it work as a loadable module in the DCCP stack written by
 *  Arnaldo Carvalho de Melo <acme@conectiva.com.br>.
 *
 *  Copyright (c) 2005 Arnaldo Carvalho de Melo <acme@conectiva.com.br>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef _GMTP_PKT_HIST_
#define _GMTP_PKT_HIST_

#include <linux/list.h>
#include <linux/slab.h>
#include "mcc_proto.h"

/**
 *  mcc_tx_hist_entry  -  Simple singly-linked TX history list
 *  @next:  next oldest entry (LIFO order)
 *  @seqno: sequence number of this entry
 *  @stamp: send time of packet with sequence number @seqno
 */
struct mcc_tx_hist_entry {
	struct mcc_tx_hist_entry *next;
	u64			  seqno;
	ktime_t			  stamp;
};

static inline struct mcc_tx_hist_entry *
	mcc_tx_hist_find_entry(struct mcc_tx_hist_entry *head, u64 seqno)
{
	while (head != NULL && head->seqno != seqno)
		head = head->next;
	return head;
}

int mcc_tx_hist_add(struct mcc_tx_hist_entry **headp, u64 seqno);
void mcc_tx_hist_purge(struct mcc_tx_hist_entry **headp);

/* Subtraction a-b modulo-16, respects circular wrap-around */
#define SUB16(a, b) (((a) + 16 - (b)) & 0xF)i

/**
 * mcc_rx_hist_entry - Store information about a single received packet
 * @seqno:	DCCP packet sequence number
 * @ccval:	window counter value of packet (RFC 4342, 8.1)
 * @ndp:	the NDP count (if any) of the packet
 * @tstamp:	actual receive time of packet
 */
struct mcc_rx_hist_entry {
	u64		 seqno:48,
			 ccval:4,
			 type:4;
	u64		 ndp:48;
	ktime_t		 tstamp;
};

/**
 * mcc_rx_hist_index - index to reach n-th entry after loss_start
 */
static inline u8 mcc_rx_hist_index(const struct mcc_rx_hist *h, const u8 n)
{
	return (h->loss_start + n) & MCC_NDUPACK;
}

/**
 * mcc_rx_hist_last_rcv - entry with highest-received-seqno so far
 */
static inline struct mcc_rx_hist_entry *
			mcc_rx_hist_last_rcv(const struct mcc_rx_hist *h)
{
	return h->ring[mcc_rx_hist_index(h, h->loss_count)];
}

/**
 * mcc_rx_hist_entry - return the n-th history entry after loss_start
 */
static inline struct mcc_rx_hist_entry *
			mcc_rx_hist_entry(const struct mcc_rx_hist *h, const u8 n)
{
	return h->ring[mcc_rx_hist_index(h, n)];
}

/**
 * mcc_rx_hist_loss_prev - entry with highest-received-seqno before loss was detected
 */
static inline struct mcc_rx_hist_entry *
			mcc_rx_hist_loss_prev(const struct mcc_rx_hist *h)
{
	return h->ring[h->loss_start];
}

/* indicate whether previously a packet was detected missing */
static inline bool mcc_rx_hist_loss_pending(const struct mcc_rx_hist *h)
{
	return h->loss_count > 0;
}

void mcc_rx_hist_add_packet(struct mcc_rx_hist *h, const struct sk_buff *skb,
			     const u64 ndp);

int mcc_rx_hist_duplicate(struct mcc_rx_hist *h, struct sk_buff *skb);

struct mcc_loss_hist;
int mcc_rx_handle_loss(struct mcc_rx_hist *h, struct mcc_loss_hist *lh,
			struct sk_buff *skb, const u64 ndp,
			u32 (*first_li)(struct sock *sk), struct sock *sk);
u32 mcc_rx_hist_sample_rtt(struct mcc_rx_hist *h, const struct sk_buff *skb);
int mcc_rx_hist_alloc(struct mcc_rx_hist *h);
void mcc_rx_hist_purge(struct mcc_rx_hist *h);

#endif /* _GMTP_PKT_HIST_ */
