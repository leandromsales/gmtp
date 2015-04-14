/*
 * GMTP-MCC library initialisation
 *
 * Copyright (c) 2007 The University of Aberdeen, Scotland, UK
 * Copyright (c) 2007 Arnaldo Carvalho de Melo <acme@redhat.com>
 *
 * Adapted to GMTP by
 * Copyright (c) 2015 Federal University of Alagoas, Maceió, Brazil
 */
#include <linux/moduleparam.h>
#include "mcc_proto.h"

int __init mcc_lib_init(void)
{
	int rc = mcc_li_init();

	if (rc)
		goto out;

	rc = mcc_tx_packet_history_init();
	if (rc)
		goto out_free_loss_intervals;

	rc = mcc_rx_packet_history_init();
	if (rc)
		goto out_free_tx_history;
	return 0;

out_free_tx_history:
	mcc_tx_packet_history_exit();
out_free_loss_intervals:
	mcc_li_exit();
out:
	return rc;
}

void mcc_lib_exit(void)
{
	mcc_rx_packet_history_exit();
	mcc_tx_packet_history_exit();
	mcc_li_exit();
}
