/*
 *  net/gmtp/options.c
 *
 *  An implementation of the GMTP protocol
 *  Copyright (c) 2005 Aristeu Sergio Rozanski Filho <aris@cathedrallabs.org>
 *  Copyright (c) 2005 Arnaldo Carvalho de Melo <acme@ghostprotocols.net>
 *  Copyright (c) 2005 Ian McDonald <ian.mcdonald@jandi.co.nz>
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 */
#include <linux/gmtp.h>
#include <linux/module.h>
#include <linux/types.h>
#include <asm/unaligned.h>
#include <linux/kernel.h>
#include <linux/skbuff.h>

#include "ackvec.h"
#include "ccid.h"
#include "gmtp.h"
#include "feat.h"

u64 gmtp_decode_value_var(const u8 *bf, const u8 len)
{
	u64 value = 0;

	if (len >= GMTP_OPTVAL_MAXLEN)
		value += ((u64)*bf++) << 40;
	if (len > 4)
		value += ((u64)*bf++) << 32;
	if (len > 3)
		value += ((u64)*bf++) << 24;
	if (len > 2)
		value += ((u64)*bf++) << 16;
	if (len > 1)
		value += ((u64)*bf++) << 8;
	if (len > 0)
		value += *bf;

	return value;
}

/**
 * gmtp_parse_options  -  Parse GMTP options present in @skb
 * @sk: client|server|listening gmtp socket (when @dreq != NULL)
 * @dreq: request socket to use during connection setup, or NULL
 */
int gmtp_parse_options(struct sock *sk, struct gmtp_request_sock *dreq,
		       struct sk_buff *skb)
{
	struct gmtp_sock *dp = gmtp_sk(sk);
	const struct gmtp_hdr *dh = gmtp_hdr(skb);
	const u8 pkt_type = GMTP_SKB_CB(skb)->gmtpd_type;
	unsigned char *options = (unsigned char *)dh + gmtp_hdr_len(skb);
	unsigned char *opt_ptr = options;
	const unsigned char *opt_end = (unsigned char *)dh +
					(dh->gmtph_doff * 4);
	struct gmtp_options_received *opt_recv = &dp->gmtps_options_received;
	unsigned char opt, len;
	unsigned char *uninitialized_var(value);
	u32 elapsed_time;
	__be32 opt_val;
	int rc;
	int mandatory = 0;

	memset(opt_recv, 0, sizeof(*opt_recv));

	opt = len = 0;
	while (opt_ptr != opt_end) {
		opt   = *opt_ptr++;
		len   = 0;
		value = NULL;

		/* Check if this isn't a single byte option */
		if (opt > GMTPO_MAX_RESERVED) {
			if (opt_ptr == opt_end)
				goto out_nonsensical_length;

			len = *opt_ptr++;
			if (len < 2)
				goto out_nonsensical_length;
			/*
			 * Remove the type and len fields, leaving
			 * just the value size
			 */
			len	-= 2;
			value	= opt_ptr;
			opt_ptr += len;

			if (opt_ptr > opt_end)
				goto out_nonsensical_length;
		}

		/*
		 * CCID-specific options are ignored during connection setup, as
		 * negotiation may still be in progress (see RFC 4340, 10.3).
		 * The same applies to Ack Vectors, as these depend on the CCID.
		 */
		if (dreq != NULL && (opt >= GMTPO_MIN_RX_CCID_SPECIFIC ||
		    opt == GMTPO_ACK_VECTOR_0 || opt == GMTPO_ACK_VECTOR_1))
			goto ignore_option;

		switch (opt) {
		case GMTPO_PADDING:
			break;
		case GMTPO_MANDATORY:
			if (mandatory)
				goto out_invalid_option;
			if (pkt_type != GMTP_PKT_DATA)
				mandatory = 1;
			break;
		case GMTPO_NDP_COUNT:
			if (len > 6)
				goto out_invalid_option;

			opt_recv->gmtpor_ndp = gmtp_decode_value_var(value, len);
			gmtp_pr_debug("%s opt: NDP count=%llu\n", gmtp_role(sk),
				      (unsigned long long)opt_recv->gmtpor_ndp);
			break;
		case GMTPO_CHANGE_L ... GMTPO_CONFIRM_R:
			if (pkt_type == GMTP_PKT_DATA)      /* RFC 4340, 6 */
				break;
			if (len == 0)
				goto out_invalid_option;
			rc = gmtp_feat_parse_options(sk, dreq, mandatory, opt,
						    *value, value + 1, len - 1);
			if (rc)
				goto out_featneg_failed;
			break;
		case GMTPO_TIMESTAMP:
			if (len != 4)
				goto out_invalid_option;
			/*
			 * RFC 4340 13.1: "The precise time corresponding to
			 * Timestamp Value zero is not specified". We use
			 * zero to indicate absence of a meaningful timestamp.
			 */
			opt_val = get_unaligned((__be32 *)value);
			if (unlikely(opt_val == 0)) {
				GMTP_WARN("Timestamp with zero value\n");
				break;
			}

			if (dreq != NULL) {
				dreq->dreq_timestamp_echo = ntohl(opt_val);
				dreq->dreq_timestamp_time = gmtp_timestamp();
			} else {
				opt_recv->gmtpor_timestamp =
					dp->gmtps_timestamp_echo = ntohl(opt_val);
				dp->gmtps_timestamp_time = gmtp_timestamp();
			}
			gmtp_pr_debug("%s rx opt: TIMESTAMP=%u, ackno=%llu\n",
				      gmtp_role(sk), ntohl(opt_val),
				      (unsigned long long)
				      GMTP_SKB_CB(skb)->gmtpd_ack_seq);
			/* schedule an Ack in case this sender is quiescent */
			inet_csk_schedule_ack(sk);
			break;
		case GMTPO_TIMESTAMP_ECHO:
			if (len != 4 && len != 6 && len != 8)
				goto out_invalid_option;

			opt_val = get_unaligned((__be32 *)value);
			opt_recv->gmtpor_timestamp_echo = ntohl(opt_val);

			gmtp_pr_debug("%s rx opt: TIMESTAMP_ECHO=%u, len=%d, "
				      "ackno=%llu", gmtp_role(sk),
				      opt_recv->gmtpor_timestamp_echo,
				      len + 2,
				      (unsigned long long)
				      GMTP_SKB_CB(skb)->gmtpd_ack_seq);

			value += 4;

			if (len == 4) {		/* no elapsed time included */
				gmtp_pr_debug_cat("\n");
				break;
			}

			if (len == 6) {		/* 2-byte elapsed time */
				__be16 opt_val2 = get_unaligned((__be16 *)value);
				elapsed_time = ntohs(opt_val2);
			} else {		/* 4-byte elapsed time */
				opt_val = get_unaligned((__be32 *)value);
				elapsed_time = ntohl(opt_val);
			}

			gmtp_pr_debug_cat(", ELAPSED_TIME=%u\n", elapsed_time);

			/* Give precedence to the biggest ELAPSED_TIME */
			if (elapsed_time > opt_recv->gmtpor_elapsed_time)
				opt_recv->gmtpor_elapsed_time = elapsed_time;
			break;
		case GMTPO_ELAPSED_TIME:
			if (gmtp_packet_without_ack(skb))   /* RFC 4340, 13.2 */
				break;

			if (len == 2) {
				__be16 opt_val2 = get_unaligned((__be16 *)value);
				elapsed_time = ntohs(opt_val2);
			} else if (len == 4) {
				opt_val = get_unaligned((__be32 *)value);
				elapsed_time = ntohl(opt_val);
			} else {
				goto out_invalid_option;
			}

			if (elapsed_time > opt_recv->gmtpor_elapsed_time)
				opt_recv->gmtpor_elapsed_time = elapsed_time;

			gmtp_pr_debug("%s rx opt: ELAPSED_TIME=%d\n",
				      gmtp_role(sk), elapsed_time);
			break;
		case GMTPO_MIN_RX_CCID_SPECIFIC ... GMTPO_MAX_RX_CCID_SPECIFIC:
			if (ccid_hc_rx_parse_options(dp->gmtps_hc_rx_ccid, sk,
						     pkt_type, opt, value, len))
				goto out_invalid_option;
			break;
		case GMTPO_ACK_VECTOR_0:
		case GMTPO_ACK_VECTOR_1:
			if (gmtp_packet_without_ack(skb))   /* RFC 4340, 11.4 */
				break;
			/*
			 * Ack vectors are processed by the TX CCID if it is
			 * interested. The RX CCID need not parse Ack Vectors,
			 * since it is only interested in clearing old state.
			 * Fall through.
			 */
		case GMTPO_MIN_TX_CCID_SPECIFIC ... GMTPO_MAX_TX_CCID_SPECIFIC:
			if (ccid_hc_tx_parse_options(dp->gmtps_hc_tx_ccid, sk,
						     pkt_type, opt, value, len))
				goto out_invalid_option;
			break;
		default:
			GMTP_CRIT("GMTP(%p): option %d(len=%d) not "
				  "implemented, ignoring", sk, opt, len);
			break;
		}
ignore_option:
		if (opt != GMTPO_MANDATORY)
			mandatory = 0;
	}

	/* mandatory was the last byte in option list -> reset connection */
	if (mandatory)
		goto out_invalid_option;

out_nonsensical_length:
	/* RFC 4340, 5.8: ignore option and all remaining option space */
	return 0;

out_invalid_option:
	GMTP_INC_STATS_BH(GMTP_MIB_INVALIDOPT);
	rc = GMTP_RESET_CODE_OPTION_ERROR;
out_featneg_failed:
	GMTP_WARN("GMTP(%p): Option %d (len=%d) error=%u\n", sk, opt, len, rc);
	GMTP_SKB_CB(skb)->gmtpd_reset_code = rc;
	GMTP_SKB_CB(skb)->gmtpd_reset_data[0] = opt;
	GMTP_SKB_CB(skb)->gmtpd_reset_data[1] = len > 0 ? value[0] : 0;
	GMTP_SKB_CB(skb)->gmtpd_reset_data[2] = len > 1 ? value[1] : 0;
	return -1;
}

EXPORT_SYMBOL_GPL(gmtp_parse_options);

void gmtp_encode_value_var(const u64 value, u8 *to, const u8 len)
{
	if (len >= GMTP_OPTVAL_MAXLEN)
		*to++ = (value & 0xFF0000000000ull) >> 40;
	if (len > 4)
		*to++ = (value & 0xFF00000000ull) >> 32;
	if (len > 3)
		*to++ = (value & 0xFF000000) >> 24;
	if (len > 2)
		*to++ = (value & 0xFF0000) >> 16;
	if (len > 1)
		*to++ = (value & 0xFF00) >> 8;
	if (len > 0)
		*to++ = (value & 0xFF);
}

static inline u8 gmtp_ndp_len(const u64 ndp)
{
	if (likely(ndp <= 0xFF))
		return 1;
	return likely(ndp <= USHRT_MAX) ? 2 : (ndp <= UINT_MAX ? 4 : 6);
}

int gmtp_insert_option(struct sk_buff *skb, const unsigned char option,
		       const void *value, const unsigned char len)
{
	unsigned char *to;

	if (GMTP_SKB_CB(skb)->gmtpd_opt_len + len + 2 > GMTP_MAX_OPT_LEN)
		return -1;

	GMTP_SKB_CB(skb)->gmtpd_opt_len += len + 2;

	to    = skb_push(skb, len + 2);
	*to++ = option;
	*to++ = len + 2;

	memcpy(to, value, len);
	return 0;
}

EXPORT_SYMBOL_GPL(gmtp_insert_option);

static int gmtp_insert_option_ndp(struct sock *sk, struct sk_buff *skb)
{
	struct gmtp_sock *dp = gmtp_sk(sk);
	u64 ndp = dp->gmtps_ndp_count;

	if (gmtp_non_data_packet(skb))
		++dp->gmtps_ndp_count;
	else
		dp->gmtps_ndp_count = 0;

	if (ndp > 0) {
		unsigned char *ptr;
		const int ndp_len = gmtp_ndp_len(ndp);
		const int len = ndp_len + 2;

		if (GMTP_SKB_CB(skb)->gmtpd_opt_len + len > GMTP_MAX_OPT_LEN)
			return -1;

		GMTP_SKB_CB(skb)->gmtpd_opt_len += len;

		ptr = skb_push(skb, len);
		*ptr++ = GMTPO_NDP_COUNT;
		*ptr++ = len;
		gmtp_encode_value_var(ndp, ptr, ndp_len);
	}

	return 0;
}

static inline int gmtp_elapsed_time_len(const u32 elapsed_time)
{
	return elapsed_time == 0 ? 0 : elapsed_time <= 0xFFFF ? 2 : 4;
}

/* FIXME: This function is currently not used anywhere */
int gmtp_insert_option_elapsed_time(struct sk_buff *skb, u32 elapsed_time)
{
	const int elapsed_time_len = gmtp_elapsed_time_len(elapsed_time);
	const int len = 2 + elapsed_time_len;
	unsigned char *to;

	if (elapsed_time_len == 0)
		return 0;

	if (GMTP_SKB_CB(skb)->gmtpd_opt_len + len > GMTP_MAX_OPT_LEN)
		return -1;

	GMTP_SKB_CB(skb)->gmtpd_opt_len += len;

	to    = skb_push(skb, len);
	*to++ = GMTPO_ELAPSED_TIME;
	*to++ = len;

	if (elapsed_time_len == 2) {
		const __be16 var16 = htons((u16)elapsed_time);
		memcpy(to, &var16, 2);
	} else {
		const __be32 var32 = htonl(elapsed_time);
		memcpy(to, &var32, 4);
	}

	return 0;
}

EXPORT_SYMBOL_GPL(gmtp_insert_option_elapsed_time);

static int gmtp_insert_option_timestamp(struct sk_buff *skb)
{
	__be32 now = htonl(gmtp_timestamp());
	/* yes this will overflow but that is the point as we want a
	 * 10 usec 32 bit timer which mean it wraps every 11.9 hours */

	return gmtp_insert_option(skb, GMTPO_TIMESTAMP, &now, sizeof(now));
}

static int gmtp_insert_option_timestamp_echo(struct gmtp_sock *dp,
					     struct gmtp_request_sock *dreq,
					     struct sk_buff *skb)
{
	__be32 tstamp_echo;
	unsigned char *to;
	u32 elapsed_time, elapsed_time_len, len;

	if (dreq != NULL) {
		elapsed_time = gmtp_timestamp() - dreq->dreq_timestamp_time;
		tstamp_echo  = htonl(dreq->dreq_timestamp_echo);
		dreq->dreq_timestamp_echo = 0;
	} else {
		elapsed_time = gmtp_timestamp() - dp->gmtps_timestamp_time;
		tstamp_echo  = htonl(dp->gmtps_timestamp_echo);
		dp->gmtps_timestamp_echo = 0;
	}

	elapsed_time_len = gmtp_elapsed_time_len(elapsed_time);
	len = 6 + elapsed_time_len;

	if (GMTP_SKB_CB(skb)->gmtpd_opt_len + len > GMTP_MAX_OPT_LEN)
		return -1;

	GMTP_SKB_CB(skb)->gmtpd_opt_len += len;

	to    = skb_push(skb, len);
	*to++ = GMTPO_TIMESTAMP_ECHO;
	*to++ = len;

	memcpy(to, &tstamp_echo, 4);
	to += 4;

	if (elapsed_time_len == 2) {
		const __be16 var16 = htons((u16)elapsed_time);
		memcpy(to, &var16, 2);
	} else if (elapsed_time_len == 4) {
		const __be32 var32 = htonl(elapsed_time);
		memcpy(to, &var32, 4);
	}

	return 0;
}

static int gmtp_insert_option_ackvec(struct sock *sk, struct sk_buff *skb)
{
	struct gmtp_sock *dp = gmtp_sk(sk);
	struct gmtp_ackvec *av = dp->gmtps_hc_rx_ackvec;
	struct gmtp_skb_cb *dcb = GMTP_SKB_CB(skb);
	const u16 buflen = gmtp_ackvec_buflen(av);
	/* Figure out how many options do we need to represent the ackvec */
	const u8 nr_opts = DIV_ROUND_UP(buflen, GMTP_SINGLE_OPT_MAXLEN);
	u16 len = buflen + 2 * nr_opts;
	u8 i, nonce = 0;
	const unsigned char *tail, *from;
	unsigned char *to;

	if (dcb->gmtpd_opt_len + len > GMTP_MAX_OPT_LEN) {
		GMTP_WARN("Lacking space for %u bytes on %s packet\n", len,
			  gmtp_packet_name(dcb->gmtpd_type));
		return -1;
	}
	/*
	 * Since Ack Vectors are variable-length, we can not always predict
	 * their size. To catch exception cases where the space is running out
	 * on the skb, a separate Sync is scheduled to carry the Ack Vector.
	 */
	if (len > GMTPAV_MIN_OPTLEN &&
	    len + dcb->gmtpd_opt_len + skb->len > dp->gmtps_mss_cache) {
		GMTP_WARN("No space left for Ack Vector (%u) on skb (%u+%u), "
			  "MPS=%u ==> reduce payload size?\n", len, skb->len,
			  dcb->gmtpd_opt_len, dp->gmtps_mss_cache);
		dp->gmtps_sync_scheduled = 1;
		return 0;
	}
	dcb->gmtpd_opt_len += len;

	to   = skb_push(skb, len);
	len  = buflen;
	from = av->av_buf + av->av_buf_head;
	tail = av->av_buf + GMTPAV_MAX_ACKVEC_LEN;

	for (i = 0; i < nr_opts; ++i) {
		int copylen = len;

		if (len > GMTP_SINGLE_OPT_MAXLEN)
			copylen = GMTP_SINGLE_OPT_MAXLEN;

		/*
		 * RFC 4340, 12.2: Encode the Nonce Echo for this Ack Vector via
		 * its type; ack_nonce is the sum of all individual buf_nonce's.
		 */
		nonce ^= av->av_buf_nonce[i];

		*to++ = GMTPO_ACK_VECTOR_0 + av->av_buf_nonce[i];
		*to++ = copylen + 2;

		/* Check if buf_head wraps */
		if (from + copylen > tail) {
			const u16 tailsize = tail - from;

			memcpy(to, from, tailsize);
			to	+= tailsize;
			len	-= tailsize;
			copylen	-= tailsize;
			from	= av->av_buf;
		}

		memcpy(to, from, copylen);
		from += copylen;
		to   += copylen;
		len  -= copylen;
	}
	/*
	 * Each sent Ack Vector is recorded in the list, as per A.2 of RFC 4340.
	 */
	if (gmtp_ackvec_update_records(av, dcb->gmtpd_seq, nonce))
		return -ENOBUFS;
	return 0;
}

/**
 * gmtp_insert_option_mandatory  -  Mandatory option (5.8.2)
 * Note that since we are using skb_push, this function needs to be called
 * _after_ inserting the option it is supposed to influence (stack order).
 */
int gmtp_insert_option_mandatory(struct sk_buff *skb)
{
	if (GMTP_SKB_CB(skb)->gmtpd_opt_len >= GMTP_MAX_OPT_LEN)
		return -1;

	GMTP_SKB_CB(skb)->gmtpd_opt_len++;
	*skb_push(skb, 1) = GMTPO_MANDATORY;
	return 0;
}

/**
 * gmtp_insert_fn_opt  -  Insert single Feature-Negotiation option into @skb
 * @type: %GMTPO_CHANGE_L, %GMTPO_CHANGE_R, %GMTPO_CONFIRM_L, %GMTPO_CONFIRM_R
 * @feat: one out of %gmtp_feature_numbers
 * @val: NN value or SP array (preferred element first) to copy
 * @len: true length of @val in bytes (excluding first element repetition)
 * @repeat_first: whether to copy the first element of @val twice
 *
 * The last argument is used to construct Confirm options, where the preferred
 * value and the preference list appear separately (RFC 4340, 6.3.1). Preference
 * lists are kept such that the preferred entry is always first, so we only need
 * to copy twice, and avoid the overhead of cloning into a bigger array.
 */
int gmtp_insert_fn_opt(struct sk_buff *skb, u8 type, u8 feat,
		       u8 *val, u8 len, bool repeat_first)
{
	u8 tot_len, *to;

	/* take the `Feature' field and possible repetition into account */
	if (len > (GMTP_SINGLE_OPT_MAXLEN - 2)) {
		GMTP_WARN("length %u for feature %u too large\n", len, feat);
		return -1;
	}

	if (unlikely(val == NULL || len == 0))
		len = repeat_first = false;
	tot_len = 3 + repeat_first + len;

	if (GMTP_SKB_CB(skb)->gmtpd_opt_len + tot_len > GMTP_MAX_OPT_LEN) {
		GMTP_WARN("packet too small for feature %d option!\n", feat);
		return -1;
	}
	GMTP_SKB_CB(skb)->gmtpd_opt_len += tot_len;

	to    = skb_push(skb, tot_len);
	*to++ = type;
	*to++ = tot_len;
	*to++ = feat;

	if (repeat_first)
		*to++ = *val;
	if (len)
		memcpy(to, val, len);
	return 0;
}

/* The length of all options needs to be a multiple of 4 (5.8) */
static void gmtp_insert_option_padding(struct sk_buff *skb)
{
	int padding = GMTP_SKB_CB(skb)->gmtpd_opt_len % 4;

	if (padding != 0) {
		padding = 4 - padding;
		memset(skb_push(skb, padding), 0, padding);
		GMTP_SKB_CB(skb)->gmtpd_opt_len += padding;
	}
}

int gmtp_insert_options(struct sock *sk, struct sk_buff *skb)
{
	struct gmtp_sock *dp = gmtp_sk(sk);

	GMTP_SKB_CB(skb)->gmtpd_opt_len = 0;

	if (dp->gmtps_send_ndp_count && gmtp_insert_option_ndp(sk, skb))
		return -1;

	if (GMTP_SKB_CB(skb)->gmtpd_type != GMTP_PKT_DATA) {

		/* Feature Negotiation */
		if (gmtp_feat_insert_opts(dp, NULL, skb))
			return -1;

		if (GMTP_SKB_CB(skb)->gmtpd_type == GMTP_PKT_REQUEST) {
			/*
			 * Obtain RTT sample from Request/Response exchange.
			 * This is currently used for TFRC initialisation.
			 */
			if (gmtp_insert_option_timestamp(skb))
				return -1;

		} else if (gmtp_ackvec_pending(sk) &&
			   gmtp_insert_option_ackvec(sk, skb)) {
				return -1;
		}
	}

	if (dp->gmtps_hc_rx_insert_options) {
		if (ccid_hc_rx_insert_options(dp->gmtps_hc_rx_ccid, sk, skb))
			return -1;
		dp->gmtps_hc_rx_insert_options = 0;
	}

	if (dp->gmtps_timestamp_echo != 0 &&
	    gmtp_insert_option_timestamp_echo(dp, NULL, skb))
		return -1;

	gmtp_insert_option_padding(skb);
	return 0;
}

int gmtp_insert_options_rsk(struct gmtp_request_sock *dreq, struct sk_buff *skb)
{
	GMTP_SKB_CB(skb)->gmtpd_opt_len = 0;

	if (gmtp_feat_insert_opts(NULL, dreq, skb))
		return -1;

	/* Obtain RTT sample from Response/Ack exchange (used by TFRC). */
	if (gmtp_insert_option_timestamp(skb))
		return -1;

	if (dreq->dreq_timestamp_echo != 0 &&
	    gmtp_insert_option_timestamp_echo(NULL, dreq, skb))
		return -1;

	gmtp_insert_option_padding(skb);
	return 0;
}
