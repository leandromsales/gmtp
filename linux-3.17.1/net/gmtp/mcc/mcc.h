#ifndef _MCC_H_
#define _MCC_H_
/*
 *  Copyright (c) 2007   The University of Aberdeen, Scotland, UK
 *  Copyright (c) 2005-6 The University of Waikato, Hamilton, New Zealand.
 *  Copyright (c) 2005-6 Ian McDonald <ian.mcdonald@jandi.co.nz>
 *  Copyright (c) 2005   Arnaldo Carvalho de Melo <acme@conectiva.com.br>
 *  Copyright (c) 2003   Nils-Erik Mattsson, Joacim Haggmark, Magnus Erixzon
 *
 *  Adapted to GMTP by
 *  Copyright (c) 2015   Federal University of Alagoas, Maceió, Brazil
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 */
#include <linux/types.h>
#include <linux/math64.h>
#include "../gmtp.h"

/* internal includes that this library exports: */
#include "loss_interval.h"
#include "packet_history.h"

#define MCC_DEBUG "[GMTP-MCC] %s:%d - "
#define mccc_pr_debug(format, args...) pr_info(MCC_DEBUG format \
		"\n", __FUNCTION__, __LINE__, ##args)

/* integer-arithmetic divisions of type (a * 1000000)/b */
static inline u64 scaled_div(u64 a, u64 b)
{
	BUG_ON(b == 0);
	return div64_u64(a * 1000000, b);
}

static inline u32 scaled_div32(u64 a, u64 b)
{
	u64 result = scaled_div(a, b);

	if (result > UINT_MAX) {
		gmtp_print_error("Overflow: %llu/%llu > UINT_MAX",
			  (unsigned long long)a, (unsigned long long)b);
		return UINT_MAX;
	}
	return result;
}

/**
 * tfrc_ewma  -  Exponentially weighted moving average
 * @weight: Weight to be used as damping factor, in units of 1/10
 */
static inline u32 mccc_ewma(const u32 avg, const u32 newval, const u8 weight)
{
	return avg ? (weight * avg + (10 - weight) * newval) / 10 : newval;
}

u32 mcc_calc_x(u16 s, u32 R, u32 p);
u32 mcc_calc_x_reverse_lookup(u32 fvalue);
u32 mcc_invert_loss_event_rate(u32 loss_event_rate);

int mcc_tx_packet_history_init(void);
void mcc_tx_packet_history_exit(void);
int mcc_rx_packet_history_init(void);
void mcc_rx_packet_history_exit(void);

int mcc_li_init(void);
void mcc_li_exit(void);

/*
#define mcc_lib_init() (0)
#define mcc_lib_exit()
*/

#endif /* _MCC_H_ */
