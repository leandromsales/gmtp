#include <linux/kernel.h>
#include <linux/skbuff.h>
#include <linux/slab.h>
#include <linux/gmtp.h>

#include <net/inet_sock.h>
#include <net/sock.h>
#include "gmtp.h"

/*
 * Do all connect socket setups that can be done AF independent.
 */
int gmtp_connect(struct sock *sk)
{
	//TODO Implement gmtp_connect
	gmtp_print_debug("gmtp_connect");
	return 0;
}
EXPORT_SYMBOL_GPL(gmtp_connect);


