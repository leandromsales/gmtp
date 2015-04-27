#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/netfilter.h>
#include <linux/netfilter_ipv4.h>
#include <linux/skbuff.h>
#include <linux/ip.h>

#include <net/ip.h>
#include <net/sock.h>
#include <net/sch_generic.h>
#include <uapi/linux/gen_stats.h>
#include <uapi/linux/types.h>

#include <uapi/linux/gmtp.h>
#include <linux/gmtp.h>
#include "../gmtp/gmtp.h"

#include "gmtp-intra.h"

extern const char *gmtp_packet_name(const int);
extern const char *gmtp_state_name(const int);
extern void flowname_str(__u8* str, const __u8* flowname);

static struct nf_hook_ops nfho_in;
static struct nf_hook_ops nfho_out;

struct gmtp_intra gmtp;

struct gmtp_intra_hashtable* relay_hashtable;
EXPORT_SYMBOL_GPL(relay_hashtable);

static inline void gmtp_update_stats(struct sk_buff *skb, struct gmtp_hdr *gh)
{
	if(gmtp.iseq == 0)
		gmtp.iseq = gh->seq;
	if(gh->seq > gmtp.seq)
		gmtp.seq = gh->seq;
	gmtp.nbytes += skb->len;
}

__be32 get_mcst_v4_addr()
{
	__be32 mcst_addr;
	unsigned char *base_channel = "\xe0\x00\x00\x01"; /* 224.0.0.1 */
	unsigned char *channel = kmalloc(4 * sizeof(unsigned char), GFP_KERNEL);

	gmtp_print_function();

	if(channel == NULL) {
		gmtp_print_error("Cannot assign requested multicast address");
		return -EADDRNOTAVAIL;
	}

	memcpy(channel, base_channel, 4 * sizeof(unsigned char));

	channel[3] += gmtp.mcst[3]++;

	/**
	 * From: base_channel (224. 0 . 0 . 1 )
	 * to:   max_channel  (239.255.255.255)
	 *                     L0  L1  L2  L3
	 */
	if(gmtp.mcst[3] > 254) {  /* L3 starts with 1 */
		gmtp.mcst[3] = 0;
		gmtp.mcst[2]++;
	}
	if(gmtp.mcst[2] > 255) {
		gmtp.mcst[2] = 0;
		gmtp.mcst[1]++;
	}
	if(gmtp.mcst[1] > 255) {
		gmtp.mcst[1] = 0;
		gmtp.mcst[0]++;
	}
	if(gmtp.mcst[0] > 15) {  /* 239 - 224 */
		gmtp_print_error("Cannot assign requested multicast address");
		return -EADDRNOTAVAIL;
	}
	channel[2] += gmtp.mcst[2];
	channel[1] += gmtp.mcst[1];
	channel[0] += gmtp.mcst[0];

	mcst_addr = *(unsigned int *)channel;
	gmtp_print_debug("Channel addr: %pI4", &mcst_addr);
	return mcst_addr;
}
EXPORT_SYMBOL_GPL(get_mcst_v4_addr);

void gmtp_buffer_add(struct sk_buff *newsk)
{
	skb_queue_tail(gmtp.buffer, skb_copy(newsk, GFP_ATOMIC));
	gmtp.buffer_size += newsk->len;
}

struct sk_buff *gmtp_buffer_dequeue()
{
	struct sk_buff *skb = skb_dequeue(gmtp.buffer);
	if(skb != NULL)
		gmtp.buffer_size -= skb->len;
	return skb;
}

unsigned int hook_func_in(unsigned int hooknum, struct sk_buff *skb,
		const struct net_device *in, const struct net_device *out,
		int (*okfn)(struct sk_buff *))
{
	int ret = NF_ACCEPT;
	struct iphdr *iph = ip_hdr(skb);
	struct gmtp_hdr *gh;

	if(in == NULL)
		goto exit;

	if(iph->protocol == IPPROTO_GMTP) {

		gh = gmtp_hdr(skb);

		if(gh->type != GMTP_PKT_DATA) {
			gmtp_print_debug("GMTP packet: %s (%d)",
					gmtp_packet_name(gh->type), gh->type);
			print_packet(iph, true);
			print_gmtp_packet(iph, gh);
		}

		switch(gh->type) {
		case GMTP_PKT_REQUEST:
			ret = gmtp_intra_request_rcv(skb);
			break;
		case GMTP_PKT_REGISTER_REPLY:
			ret = gmtp_intra_register_reply_rcv(skb);
			break;
		case GMTP_PKT_ACK:
			ret = gmtp_intra_ack_rcv(skb);
			break;
		case GMTP_PKT_DATA:
			/* Artificial loss (only for tests)
			 if(gh->seq % 2)
			 return NF_DROP;*/

			ret = gmtp_intra_data_rcv(skb);
			break;
		case GMTP_PKT_FEEDBACK:
			ret = gmtp_intra_feedback_rcv(skb);
			break;
		case GMTP_PKT_CLOSE:
			ret = gmtp_intra_close_rcv(skb);
			break;
		}

		gmtp_update_stats(iph, gh);
	}

exit:
	return ret;
}

unsigned int hook_func_out(unsigned int hooknum, struct sk_buff *skb,
		const struct net_device *in, const struct net_device *out,
		int (*okfn)(struct sk_buff *))
{
	int ret = NF_ACCEPT;
	struct iphdr *iph = ip_hdr(skb);
	struct gmtp_hdr *gh;

	if(out == NULL)
		goto exit;

	if(iph->protocol == IPPROTO_GMTP) {

		gh = gmtp_hdr(skb);

		switch(gh->type) {
		case GMTP_PKT_DATA:
			ret = gmtp_intra_data_out(skb);
			break;
		default:
			gmtp_print_debug("GMTP packet: %s (%d)",
					gmtp_packet_name(gh->type), gh->type);
			print_packet(iph, false);
		}

	}

exit:
	return ret;
}

int init_module()
{
	int ret = 0;
	gmtp_print_function();
	gmtp_print_debug("Starting GMTP-Intra");

	gmtp.iseq = 0;
	gmtp.seq = 0;
	gmtp.nbytes = 0;
	gmtp.current_tx = 1;
	memset(&gmtp.mcst, 0, 4*sizeof(unsigned char));
	gmtp.buffer = kmalloc(sizeof(struct sk_buff_head), GFP_ATOMIC);
	skb_queue_head_init(gmtp.buffer);
	gmtp.buffer_size = 0;

	relay_hashtable = gmtp_intra_create_hashtable(64);
	if(relay_hashtable == NULL) {
		gmtp_print_error("Cannot create hashtable...");
		ret = -ENOMEM;
		goto out;
	}

	nfho_in.hook = hook_func_in;
	nfho_in.hooknum = NF_INET_PRE_ROUTING;
	nfho_in.pf = PF_INET;
	nfho_in.priority = NF_IP_PRI_FIRST;
	nf_register_hook(&nfho_in);

	nfho_out.hook = hook_func_out;
	nfho_out.hooknum = NF_INET_POST_ROUTING;
	nfho_out.pf = PF_INET;
	nfho_out.priority = NF_IP_PRI_FIRST;
	nf_register_hook(&nfho_out);

out:
	return ret;
}

void cleanup_module()
{
	gmtp_print_debug("Finishing GMTP-Intra");

	skb_queue_purge(gmtp.buffer);

	kfree_gmtp_intra_hashtable(relay_hashtable);

	nf_unregister_hook(&nfho_in);
	nf_unregister_hook(&nfho_out);
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Mário André Menezes <mariomenezescosta@gmail.com>");
MODULE_AUTHOR("Wendell Silva Soares <wss@ic.ufal.br>");
MODULE_DESCRIPTION("GMTP - Global Media Transmission Protocol");
