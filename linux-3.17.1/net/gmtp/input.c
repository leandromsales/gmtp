#include <linux/kernel.h>
#include <linux/skbuff.h>
#include <linux/slab.h>
#include <linux/gmtp.h>
#include <linux/types.h>

#include <net/inet_sock.h>
#include <net/sock.h>
#include "gmtp.h"

static int gmtp_check_seqno(struct sock *sk, struct sk_buff *skb)
{
	gmtp_print_debug("gmtp_check_seqno");
	return 0;
}

static int __gmtp_rcv_established(struct sock *sk, struct sk_buff *skb,
				  const struct gmtp_hdr *dh, const unsigned int len)
{
	struct gmtp_sock *dp = gmtp_sk(sk);

	gmtp_print_debug("__gmtp_rcv_established");

	switch (gmtp_hdr(skb)->type) {
	case GMTP_PKT_DATAACK:
	case GMTP_PKT_DATA:
		return 0;
	case GMTP_PKT_ACK:
		goto discard;
	}

	return 0;
discard:
	__kfree_skb(skb);
	return 0;
}

/**
 * gmtp_parse_options  -  Parse GMTP options present in @skb
 * @sk: client|server|listening gmtp socket (when @dreq != NULL)
 * @dreq: request socket to use during connection setup, or NULL
 *
 * TODO Verificar se a fc eh necessaria
 */
int gmtp_parse_options(struct sock *sk, struct gmtp_request_sock *dreq,
		       struct sk_buff *skb)
{
	gmtp_print_debug("gmtp_parse_options");
	return 0;
}
EXPORT_SYMBOL_GPL(gmtp_parse_options);

int gmtp_rcv_established(struct sock *sk, struct sk_buff *skb,
			 const struct gmtp_hdr *dh, const unsigned int len)
{
	gmtp_print_debug("gmtp_rcv_established");

	//Check sequence numbers...
	if (gmtp_check_seqno(sk, skb))
		goto discard;

	if (gmtp_parse_options(sk, NULL, skb))
		return 1;

	//TODO verificar acks..s
//	dccp_handle_ackvec_processing(sk, skb);

	return __gmtp_rcv_established(sk, skb, dh, len);
discard:
	__kfree_skb(skb);
	return 0;
}
EXPORT_SYMBOL_GPL(gmtp_rcv_established);

