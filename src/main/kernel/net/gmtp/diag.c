/*
 *  net/gmtp/diag.c
 *
 *  An implementation of the GMTP protocol
 *  Arnaldo Carvalho de Melo <acme@mandriva.com>
 *
 *	This program is free software; you can redistribute it and/or modify it
 *	under the terms of the GNU General Public License version 2 as
 *	published by the Free Software Foundation.
 */


#include <linux/module.h>
#include <linux/inet_diag.h>

#include "ccid.h"
#include "gmtp.h"

static void gmtp_get_info(struct sock *sk, struct tcp_info *info)
{
	struct gmtp_sock *dp = gmtp_sk(sk);
	const struct inet_connection_sock *icsk = inet_csk(sk);

	memset(info, 0, sizeof(*info));

	info->tcpi_state	= sk->sk_state;
	info->tcpi_retransmits	= icsk->icsk_retransmits;
	info->tcpi_probes	= icsk->icsk_probes_out;
	info->tcpi_backoff	= icsk->icsk_backoff;
	info->tcpi_pmtu		= icsk->icsk_pmtu_cookie;

	if (dp->gmtps_hc_rx_ackvec != NULL)
		info->tcpi_options |= TCPI_OPT_SACK;

	if (dp->gmtps_hc_rx_ccid != NULL)
		ccid_hc_rx_get_info(dp->gmtps_hc_rx_ccid, sk, info);

	if (dp->gmtps_hc_tx_ccid != NULL)
		ccid_hc_tx_get_info(dp->gmtps_hc_tx_ccid, sk, info);
}

static void gmtp_diag_get_info(struct sock *sk, struct inet_diag_msg *r,
			       void *_info)
{
	r->idiag_rqueue = r->idiag_wqueue = 0;

	if (_info != NULL)
		gmtp_get_info(sk, _info);
}

static void gmtp_diag_dump(struct sk_buff *skb, struct netlink_callback *cb,
		struct inet_diag_req_v2 *r, struct nlattr *bc)
{
	inet_diag_dump_icsk(&gmtp_hashinfo, skb, cb, r, bc);
}

static int gmtp_diag_dump_one(struct sk_buff *in_skb, const struct nlmsghdr *nlh,
		struct inet_diag_req_v2 *req)
{
	return inet_diag_dump_one_icsk(&gmtp_hashinfo, in_skb, nlh, req);
}

static const struct inet_diag_handler gmtp_diag_handler = {
	.dump		 = gmtp_diag_dump,
	.dump_one	 = gmtp_diag_dump_one,
	.idiag_get_info	 = gmtp_diag_get_info,
	.idiag_type	 = IPPROTO_GMTP,
};

static int __init gmtp_diag_init(void)
{
	return inet_diag_register(&gmtp_diag_handler);
}

static void __exit gmtp_diag_fini(void)
{
	inet_diag_unregister(&gmtp_diag_handler);
}

module_init(gmtp_diag_init);
module_exit(gmtp_diag_fini);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Arnaldo Carvalho de Melo <acme@mandriva.com>");
MODULE_DESCRIPTION("GMTP inet_diag handler");
MODULE_ALIAS_NET_PF_PROTO_TYPE(PF_NETLINK, NETLINK_SOCK_DIAG, 2-33 /* AF_INET - IPPROTO_GMTP */);
