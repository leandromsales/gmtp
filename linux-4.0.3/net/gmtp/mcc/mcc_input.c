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
	struct gmtp_sock *gp = gmtp_sk(sk);
	ktime_t now = ktime_get_real();
	u32 sample;
	s64 delta = 0;
	u32 p;

	switch (fbtype) {
	case MCC_FBACK_INITIAL:
		gp->rx_x_recv = 0;
		gp->rx_pinv   = ~0U;   /* see RFC 4342, 8.5 */
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
		if (gp->rx_x_recv > 0)
			break;
		/* fall through */
	case MCC_FBACK_PERIODIC:
		delta = ktime_us_delta(now, gp->rx_tstamp_last_feedback);
		if (delta <= 0)
			gmtp_pr_error("delta (%ld) <= 0", (long)delta);
		else
			gp->rx_x_recv = scaled_div32(gp->rx_bytes_recv, delta);
		break;
	default:
		return;
	}

	now = ktime_get_real();
	gp->rx_tstamp_last_feedback = now;
	gp->rx_bytes_recv	    = 0;
	sample = gp->rx_rtt * USEC_PER_MSEC;

	if(sample != 0)
		gp->rx_avg_rtt = mcc_ewma(gp->rx_avg_rtt, sample, 9);

	if(gp->rx_avg_rtt <= 0)
		gp->rx_avg_rtt = GMTP_SANE_RTT_MIN;

	p = mcc_invert_loss_event_rate(gp->rx_pinv);
	if(p > 0) {
		u32 new_rate = mcc_calc_x(gp->rx_s, gp->rx_avg_rtt, p);
		/*
		 * Change only if the value is valid!
		 */
		if(new_rate > 0)
			gp->rx_max_rate = new_rate;
	} else {
		/*
		 * No loss events. Returning max X_calc.
		 * Sender will ignore feedback and keep last TX
		 */
		gp->rx_max_rate = ~0U;
	}

	if(likely(gp->role == GMTP_ROLE_REPORTER)) {

		mcc_pr_debug("REPORT: RTT=%u us (sample=%u us), s=%u, "
			       "p=%u, X_calc=%u bytes/s, X_recv=%u bytes/s",
			       gp->rx_avg_rtt, sample,
			       gp->rx_s, p,
			       gp->rx_max_rate,
			       gp->rx_x_recv);

		gmtp_send_feedback(sk, skb);
	}
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
		gmtp_pr_warning("No RTT estimate available, using fallback RTT");
		/* FIXME Use server RTT */
		hc->rx_rtt = GMTP_FALLBACK_RTT;
	}

	delta  = ktime_to_us(net_timedelta(hc->rx_tstamp_last_feedback));
	x_recv = scaled_div32(hc->rx_bytes_recv, delta);
	if (x_recv == 0) {		/* would also trigger divide-by-zero */
		gmtp_pr_warning("X_recv==0\n");
		if (hc->rx_x_recv == 0) {
			gmtp_pr_error("stored value of X_recv is zero");
			return ~0U;
		}
		x_recv = hc->rx_x_recv;
	}

	fval = scaled_div(hc->rx_s, hc->rx_rtt);
	fval = scaled_div32(fval, x_recv);
	p = mcc_calc_x_reverse_lookup(fval);

	mcc_pr_debug("%s receive rate=%u bytes/s, implied "
		       "loss rate=%u\n", gmtp_role_name(sk), x_recv, p);

	return p == 0 ? ~0U : scaled_div(1, p);
}


void mcc_rx_packet_recv(struct sock *sk, struct sk_buff *skb)
{
	struct gmtp_sock *gp = gmtp_sk(sk);
	enum mcc_fback_type do_feedback = MCC_FBACK_NONE;
	const __be32 ndp = gmtp_sk(sk)->ndp_count;
	const bool is_data_packet = gmtp_data_packet(skb);
	ktime_t now = ktime_get_real();
	s64 delta = 0;

	if(unlikely(gp->rx_state == MCC_RSTATE_NO_DATA)) {
		if(is_data_packet) {
			const u32 payload = skb->len
					- gmtp_hdr(skb)->hdrlen * 4;
			do_feedback = MCC_FBACK_INITIAL;
			mcc_rx_set_state(sk, MCC_RSTATE_DATA);
			gp->rx_s = payload;
			/*
			 * Not necessary to update rx_bytes_recv here,
			 * since X_recv = 0 for the first feedback packet (cf.
			 * RFC 3448, 6.3) -- gerrit
			 */
		}
		goto update_records;
	}

	if(mcc_rx_hist_duplicate(&gp->rx_hist, skb))
		return;  /* done receiving */

	if(is_data_packet) {
		const u32 payload = skb->len - gmtp_hdr(skb)->hdrlen;
		struct gmtp_hdr_data *dh = gmtp_hdr_data(skb);

		/*
		 * Update moving-average of s and the sum of received payload bytes
		 */
		gp->rx_s = mcc_ewma(gp->rx_s, payload, 9);
		gp->rx_bytes_recv += payload;
		GMTP_SKB_CB(skb)->server_tstamp = dh->tstamp;
	}

	/*
	 * Perform loss detection and handle pending losses
	 */
	if(mcc_rx_handle_loss(&gp->rx_hist, &gp->rx_li_hist,
			skb, ndp, mcc_first_li, sk)) {
		do_feedback = MCC_FBACK_PARAM_CHANGE;
		goto done_receiving;
	}

	if(mcc_rx_hist_loss_pending(&gp->rx_hist))
		return;  /* done receiving */

	/*
	 * Handle data packets: RTT sampling and monitoring p
	 */
	if(unlikely(!is_data_packet))
		goto update_records;

	if(!mcc_lh_is_initialised(&gp->rx_li_hist)) {
		/* ms to us */
		const u32 sample = jiffies_to_usecs(msecs_to_jiffies(gp->rx_rtt));
		/*
		 * Empty loss history: no loss so far, hence p stays 0.
		 * Sample RTT values, since an RTT estimate is required for the
		 * computation of p when the first loss occurs; RFC 3448, 6.3.1.
		 */
		if(sample != 0)
			gp->rx_avg_rtt = mcc_ewma(gp->rx_avg_rtt, sample, 9);

	} else if(mcc_lh_update_i_mean(&gp->rx_li_hist, skb)) {
		/*
		 * Step (3) of [RFC 3448, 6.1]: Recompute I_mean and, if I_mean
		 * has decreased (resp. p has increased), send feedback now.
		 */
		do_feedback = MCC_FBACK_PARAM_CHANGE;
	}

	/*
	 * Check if the periodic once-per-RTT feedback is due; RFC 4342, 10.3
	 */
	delta = ktime_us_delta(now, gp->rx_tstamp_last_feedback);
	if(delta <= 0)
		gmtp_pr_error("delta (%ld) <= 0", (long )delta);
	else if(delta >= gp->rx_avg_rtt)
		do_feedback = MCC_FBACK_PERIODIC;

update_records:
	mcc_rx_hist_add_packet(&gp->rx_hist, skb, ndp);

done_receiving:
	if(do_feedback)
		mcc_rx_send_feedback(sk, skb, do_feedback);
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



