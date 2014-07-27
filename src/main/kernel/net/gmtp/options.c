#include "gmtp.h"

/*
 * On DCCP, the length of all options needs to be a multiple of 4 (5.8)
 * */
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
//	struct gmtp_sock *gp = gmtp_sk(sk);

	GMTP_SKB_CB(skb)->gmtpd_opt_len = 0;

	if (GMTP_SKB_CB(skb)->gmtpd_type != GMTP_PKT_DATA) {
		//TODO Treat it
	}

	gmtp_insert_option_padding(skb);
	return 0;
}
