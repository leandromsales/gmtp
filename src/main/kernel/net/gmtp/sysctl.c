/*
 *  net/gmtp/sysctl.c
 *
 *  An implementation of the GMTP protocol
 *  Arnaldo Carvalho de Melo <acme@mandriva.com>
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License v2
 *	as published by the Free Software Foundation.
 */

#include <linux/mm.h>
#include <linux/sysctl.h>
#include "gmtp.h"
#include "feat.h"

#ifndef CONFIG_SYSCTL
#error This file should not be compiled without CONFIG_SYSCTL defined
#endif

/* Boundary values */
static int		zero     = 0,
			u8_max   = 0xFF;
static unsigned long	seqw_min = GMTPF_SEQ_WMIN,
			seqw_max = 0xFFFFFFFF;		/* maximum on 32 bit */

static struct ctl_table gmtp_default_table[] = {
	{
		.procname	= "seq_window",
		.data		= &sysctl_gmtp_sequence_window,
		.maxlen		= sizeof(sysctl_gmtp_sequence_window),
		.mode		= 0644,
		.proc_handler	= proc_doulongvec_minmax,
		.extra1		= &seqw_min,		/* RFC 4340, 7.5.2 */
		.extra2		= &seqw_max,
	},
	{
		.procname	= "rx_ccid",
		.data		= &sysctl_gmtp_rx_ccid,
		.maxlen		= sizeof(sysctl_gmtp_rx_ccid),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= &zero,
		.extra2		= &u8_max,		/* RFC 4340, 10. */
	},
	{
		.procname	= "tx_ccid",
		.data		= &sysctl_gmtp_tx_ccid,
		.maxlen		= sizeof(sysctl_gmtp_tx_ccid),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= &zero,
		.extra2		= &u8_max,		/* RFC 4340, 10. */
	},
	{
		.procname	= "request_retries",
		.data		= &sysctl_gmtp_request_retries,
		.maxlen		= sizeof(sysctl_gmtp_request_retries),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= &zero,
		.extra2		= &u8_max,
	},
	{
		.procname	= "retries1",
		.data		= &sysctl_gmtp_retries1,
		.maxlen		= sizeof(sysctl_gmtp_retries1),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= &zero,
		.extra2		= &u8_max,
	},
	{
		.procname	= "retries2",
		.data		= &sysctl_gmtp_retries2,
		.maxlen		= sizeof(sysctl_gmtp_retries2),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= &zero,
		.extra2		= &u8_max,
	},
	{
		.procname	= "tx_qlen",
		.data		= &sysctl_gmtp_tx_qlen,
		.maxlen		= sizeof(sysctl_gmtp_tx_qlen),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= &zero,
	},
	{
		.procname	= "sync_ratelimit",
		.data		= &sysctl_gmtp_sync_ratelimit,
		.maxlen		= sizeof(sysctl_gmtp_sync_ratelimit),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_ms_jiffies,
	},

	{ }
};

static struct ctl_table_header *gmtp_table_header;

int __init gmtp_sysctl_init(void)
{
	gmtp_table_header = register_net_sysctl(&init_net, "net/gmtp/default",
			gmtp_default_table);

	return gmtp_table_header != NULL ? 0 : -ENOMEM;
}

void gmtp_sysctl_exit(void)
{
	if (gmtp_table_header != NULL) {
		unregister_net_sysctl_table(gmtp_table_header);
		gmtp_table_header = NULL;
	}
}
