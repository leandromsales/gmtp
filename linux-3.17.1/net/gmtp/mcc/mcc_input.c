/*
 * mcc_input.c
 *
 *  Created on: 13/04/2015
 *      Author: wendell
 *
 *  GMTP-MCC receiver routines
 */

#include <linux/gmtp.h>

#include "mcc_proto.h"

/* GMTP-MCC feedback types */
enum mcc_fback_type {
	MCC_FBACK_NONE = 0,
	MCC_FBACK_INITIAL,
	MCC_FBACK_PERIODIC,
	MCC_FBACK_PARAM_CHANGE
};

static const char *mcc_rx_state_name(enum mcc_rx_states state)
{
	static const char *const mcc_rx_state_names[] = {
	[MCC_RSTATE_NO_DATA] = "NO_DATA",
	[MCC_RSTATE_DATA]    = "DATA",
	};

	return mcc_rx_state_names[state];
}

static void mcc_rx_set_state(struct sock *sk, enum mcc_rx_states state)
{
	struct gmtp_sock *hc = gmtp_sk(sk);
	enum mcc_rx_states oldstate = hc->rx_state;

	gmtp_pr_func();
	mcc_pr_debug("%s(%p) %-8.8s -> %s\n",
			gmtp_role_name(sk), sk, mcc_rx_state_name(oldstate),
		       mcc_rx_state_name(state));
	WARN_ON(state == oldstate);
	hc->rx_state = state;
}


static void mcc_rx_send_feedback(struct sock *sk,
				      const struct sk_buff *skb,
				      enum mcc_fback_type fbtype)
{
	struct gmtp_sock *hc = gmtp_sk(sk);
	ktime_t now = ktime_get_real();
	s64 delta = 0;

	gmtp_pr_func();

	switch (fbtype) {
	case MCC_FBACK_INITIAL:
		hc->rx_x_recv = 0;
		hc->rx_pinv   = ~0U;   /* see RFC 4342, 8.5 */
		break;
	case MCC_FBACK_PARAM_CHANGE:
		/*
		 * When parameters change (new loss or p > p_prev), we do not
		 * have a reliable estimate for R_m of [RFC 3448, 6.2] and so
		 * need to  reuse the previous value of X_recv. However, when
		 * X_recv was 0 (due to early loss), this would kill X down to
		 * s/t_mbi (i.e. one packet in 64 seconds).
		 * To avoid such drastic reduction, we approximate X_recv as
		 * the number of bytes since last feedback.
		 * This is a safe fallback, since X is bounded above by X_calc.
		 */
		if (hc->rx_x_recv > 0)
			break;
		/* fall through */
	case MCC_FBACK_PERIODIC:
		delta = ktime_us_delta(now, hc->rx_tstamp_last_feedback);
		if (delta <= 0)
			gmtp_pr_error("delta (%ld) <= 0", (long)delta);
		else
			hc->rx_x_recv = scaled_div32(hc->rx_bytes_recv, delta);
		break;
	default:
		return;
	}

	mcc_pr_debug("Interval %ld usec | X_recv: %u | 1/p: %u", (long)delta,
		       hc->rx_x_recv, hc->rx_pinv);

	hc->rx_tstamp_last_feedback = now;
	/* FIXME Change ccval by tstamp or other method */
	/*hc->rx_last_counter	    = dccp_hdr(skb)->dccph_ccval;*/
	hc->rx_bytes_recv	    = 0;

	if(hc->px_inv > 0)
		hc->rx_max_rate = mcc_calc_x(hc->rx_s, hc->rx_rtt,
				mcc_invert_loss_event_rate(hc->rx_pinv));

	gmtp_pr_info("Now, we should send a feedback... Testing send ack");
	/* FIXME Choose a packet type to send Feedback */
	gmtp_send_ack(sk, GMTP_ACK_MCC_FEEDBACK);
}



/**
 * mcc_first_li  -  Implements [RFC 5348, 6.3.1]
 *
 * Determine the length of the first loss interval via inverse lookup.
 * Assume that X_recv can be computed by the throughput equation
 *		    s
 *	X_recv = --------
 *		 R * fval
 * Find some p such that f(p) = fval; return 1/p (scaled).
 */
static u32 mcc_first_li(struct sock *sk)
{
	struct gmtp_sock *hc = gmtp_sk(sk);
	u32 x_recv, p, delta;
	u64 fval;

	if (hc->rx_rtt == 0) {
		gmtp_print_warning("No RTT estimate available, using fallback RTT\n");
		/* FIXME Use server RTT */
		hc->rx_rtt = GMTP_FALLBACK_RTT;
	}

	delta  = ktime_to_us(net_timedelta(hc->rx_tstamp_last_feedback));
	x_recv = scaled_div32(hc->rx_bytes_recv, delta);
	if (x_recv == 0) {		/* would also trigger divide-by-zero */
		gmtp_print_warning("X_recv==0\n");
		if (hc->rx_x_recv == 0) {
			gmtp_print_error("stored value of X_recv is zero");
			return ~0U;
		}
		x_recv = hc->rx_x_recv;
	}

	fval = scaled_div(hc->rx_s, hc->rx_rtt);
	fval = scaled_div32(fval, x_recv);
	p = mcc_calc_x_reverse_lookup(fval);

	mcc_pr_debug("%s(%p), receive rate=%u bytes/s, implied "
		       "loss rate=%u\n", gmtp_role_name(sk), sk, x_recv, p);

	return p == 0 ? ~0U : scaled_div(1, p);
}


void mcc_rx_packet_recv(struct sock *sk, struct sk_buff *skb)
{
	struct gmtp_sock *hc = gmtp_sk(sk);
	enum mcc_fback_type do_feedback = MCC_FBACK_NONE;
	const __be32 ndp = gmtp_sk(sk)->ndp_count;
	const bool is_data_packet = gmtp_data_packet(skb);

	gmtp_pr_func();

	if(unlikely(hc->rx_state == MCC_RSTATE_NO_DATA)) {
		if(is_data_packet) {
			const u32 payload = skb->len
					- gmtp_hdr(skb)->hdrlen * 4;
			do_feedback = MCC_FBACK_INITIAL;
			mcc_rx_set_state(sk, MCC_RSTATE_DATA);
			hc->rx_s = payload;
			/*
			 * Not necessary to update rx_bytes_recv here,
			 * since X_recv = 0 for the first feedback packet (cf.
			 * RFC 3448, 6.3) -- gerrit
			 */
		}
		goto update_records;
	}

	if(mcc_rx_hist_duplicate(&hc->rx_hist, skb))
		return;  /* done receiving */

	if(is_data_packet) {
		const u32 payload = skb->len - gmtp_hdr(skb)->hdrlen;
		/*
		 * Update moving-average of s and the sum of received payload bytes
		 */
		hc->rx_s = mcc_ewma(hc->rx_s, payload, 9);
		hc->rx_bytes_recv += payload;
	}

	/* Perform loss detection and handle pending losses */
	if(mcc_rx_handle_loss(&hc->rx_hist, &hc->rx_li_hist, skb, ndp,
			mcc_first_li, sk)) {
		do_feedback = MCC_FBACK_PARAM_CHANGE;
		goto done_receiving;
	}

	if(mcc_rx_hist_loss_pending(&hc->rx_hist))
		return;  /* done receiving */

	/* Handle data packets: RTT sampling and monitoring p */
	if(unlikely(!is_data_packet))
		goto update_records;

	if(!mcc_lh_is_initialised(&hc->rx_li_hist)) {
		const u32 sample = hc->rtt;
		/*
		 * Empty loss history: no loss so far, hence p stays 0.
		 * Sample RTT values, since an RTT estimate is required for the
		 * computation of p when the first loss occurs; RFC 3448, 6.3.1.
		 */
		if(sample != 0)
			hc->rx_rtt = mcc_ewma(hc->rx_rtt, sample, 9);

	} else if(mcc_lh_update_i_mean(&hc->rx_li_hist, skb)) {
		/*
		 * Step (3) of [RFC 3448, 6.1]: Recompute I_mean and, if I_mean
		 * has decreased (resp. p has increased), send feedback now.
		 */
		do_feedback = MCC_FBACK_PARAM_CHANGE;
	}

	/*
	 * FIXME
	 * Check if the periodic once-per-RTT feedback is due; RFC 4342, 10.3
	 */
	/*if(SUB16(dccp_hdr(skb)->dccph_ccval, hc->rx_last_counter) > 3)
		do_feedback = MCC_FBACK_PERIODIC;*/

update_records:
	mcc_rx_hist_add_packet(&hc->rx_hist, skb, ndp);

done_receiving:
	if(do_feedback)
		mcc_rx_send_feedback(sk, skb, do_feedback);
	return;
}
EXPORT_SYMBOL_GPL(mcc_rx_packet_recv);

int mcc_rx_init(struct sock *sk)
{
	struct gmtp_sock *hc = gmtp_sk(sk);

	gmtp_pr_func();

	hc->rx_state = MCC_RSTATE_NO_DATA;
	hc->ndp_count = 0;
	mcc_lh_init(&hc->rx_li_hist);
	return mcc_rx_hist_alloc(&hc->rx_hist);
}

void mcc_rx_exit(struct sock *sk)
{
	/*struct gmtp_sock *hc = gmtp_sk(sk);*/

	gmtp_pr_func();

	/* FIXME this breaks the modules  */
	/*
	mcc_rx_hist_purge(&hc->rx_hist);
	mcc_lh_cleanup(&hc->rx_li_hist);
	*/
}



