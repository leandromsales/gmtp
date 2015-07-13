#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/netfilter.h>
#include <linux/netfilter_ipv4.h>
#include <linux/skbuff.h>
#include <linux/ip.h>
#include <linux/inet.h>
#include <linux/dirent.h>
#include <linux/inetdevice.h>
#include <linux/crypto.h>
#include <linux/err.h>
#include <linux/scatterlist.h>
#include <linux/if.h>
#include <linux/ioctl.h>

#include <net/sock.h>
#include <net/sch_generic.h>

#include <uapi/linux/gen_stats.h>
#include <uapi/linux/types.h>

#include <uapi/linux/gmtp.h>
#include <linux/gmtp.h>
#include "../gmtp.h"

#include "gmtp-inter.h"
#include "mcc-inter.h"
#include "ucc.h"

static struct nf_hook_ops nfho_in;
static struct nf_hook_ops nfho_out;

struct gmtp_inter gmtp_inter;

unsigned char *gmtp_build_md5(unsigned char *buf)
{
	struct scatterlist sg;
	struct crypto_hash *tfm;
	struct hash_desc desc;
	unsigned char *output;
	 __u8 md5[21];
	size_t buf_size = sizeof(buf) - 1;

	gmtp_print_function();

	output = kmalloc(MD5_LEN * sizeof(unsigned char), GFP_KERNEL);
	tfm = crypto_alloc_hash("md5", 0, CRYPTO_ALG_ASYNC);
	if(output == NULL || IS_ERR(tfm)) {
		gmtp_pr_error("Allocation failed\n");
		return NULL;
	}

	desc.tfm = tfm;
	desc.flags = 0;

	crypto_hash_init(&desc);

	sg_init_one(&sg, buf, buf_size);
	crypto_hash_update(&desc, &sg, buf_size);
	crypto_hash_final(&desc, output);

	flowname_strn(md5, output, MD5_LEN);
	printk("Output md5 = %s\n", md5);

	crypto_free_hash(tfm);

	return output;
}

unsigned char *gmtp_inter_build_relay_id(void)
{
	struct socket *sock = NULL;
	struct net_device *dev = NULL;
	struct net *net;

	int i, retval, length = 0;
	char mac_address[6];

	char buffer[50];
	__u8 *str[30];

	gmtp_print_function();

	retval = sock_create(AF_INET, SOCK_STREAM, 0, &sock);
	net = sock_net(sock->sk);

	for(i = 2; (dev = dev_get_by_index_rcu(net, i)) != NULL; ++i) {
		memcpy(&mac_address, dev->dev_addr, 6);
		length += bytes_added(sprintf((char*)(buffer + length), str));
	}

	sock_release(sock);
	return gmtp_build_md5(buffer);
}

__be32 gmtp_inter_device_ip(struct net_device *dev)
{
	struct in_device *in_dev;
	struct in_ifaddr *if_info;

	gmtp_pr_func();

	if(dev == NULL)
		return 0;

	in_dev = (struct in_device *)dev->ip_ptr;
	if_info = in_dev->ifa_list;
	for(; if_info; if_info = if_info->ifa_next) {
		pr_info("if_info->ifa_address=%pI4\n", &if_info->ifa_address);
		/* just return the first entry for while */
		return if_info->ifa_address;
	}

	return 0;
}

__be32 get_mcst_v4_addr(void)
{
	__be32 mcst_addr;
	unsigned char *base_channel = "\xe0\xc0\x00\x00"; /* 224.192.0.0 */
	unsigned char *channel = kmalloc(4 * sizeof(unsigned char), GFP_KERNEL);

	gmtp_print_function();

	if(channel == NULL) {
		gmtp_print_error("Cannot assign requested multicast address");
		return -EADDRNOTAVAIL;
	}

	memcpy(channel, base_channel, 4 * sizeof(unsigned char));

	channel[3] += gmtp_inter.mcst[3]++;

	/**
	 * From: base_channel (224.192. 0 . 0 )
	 * to:   max_channel  (239.255.255.255)
	 *                     L0  L1  L2  L3
	 */
	if(gmtp_inter.mcst[3] > 255) {  /* L3 starts with 1 */
		gmtp_inter.mcst[3] = 0;
		gmtp_inter.mcst[2]++;
	}
	if(gmtp_inter.mcst[2] > 255) {
		gmtp_inter.mcst[2] = 0;
		gmtp_inter.mcst[1]++;
	}
	if(gmtp_inter.mcst[1] > 63) { /* 255 - 192 */
		gmtp_inter.mcst[1] = 0;
		gmtp_inter.mcst[0]++;
	}
	if(gmtp_inter.mcst[0] > 15) {  /* 239 - 224 */
		gmtp_print_error("Cannot assign requested multicast address");
		return -EADDRNOTAVAIL;
	}
	channel[2] += gmtp_inter.mcst[2];
	channel[1] += gmtp_inter.mcst[1];
	channel[0] += gmtp_inter.mcst[0];

	mcst_addr = *(unsigned int *)channel;
	gmtp_print_debug("Channel addr: %pI4", &mcst_addr);
	return mcst_addr;
}
EXPORT_SYMBOL_GPL(get_mcst_v4_addr);

void gmtp_buffer_add(struct gmtp_flow_info *info, struct sk_buff *newsk)
{
	skb_queue_tail(info->buffer, skb_copy(newsk, GFP_ATOMIC));
	info->buffer_len += newsk->len + ETH_HLEN;
	gmtp_inter.buffer_len += newsk->len + ETH_HLEN;
	info->last_rx_tstamp = ;
}

struct sk_buff *gmtp_buffer_dequeue(struct gmtp_flow_info *info)
{
	struct sk_buff *skb = skb_dequeue(info->buffer);
	if(skb != NULL) {
		info->buffer_len -= (skb->len + ETH_HLEN);
		gmtp_inter.buffer_len -= (skb->len + ETH_HLEN);
	}
	return skb;
}

unsigned int hook_func_in(unsigned int hooknum, struct sk_buff *skb,
		const struct net_device *in, const struct net_device *out,
		int (*okfn)(struct sk_buff *))

{
    	int ret = NF_ACCEPT;
	struct iphdr *iph = ip_hdr(skb);

	if((gmtp_info->relay_enabled == 0) || (in == NULL))
		return ret;

	if(iph->protocol == IPPROTO_GMTP) {
        
  		struct gmtp_hdr *gh = gmtp_hdr(skb);

		if(unlikely(gh->type != GMTP_PKT_DATA
					&& gh->type != GMTP_PKT_FEEDBACK)) {
			gmtp_pr_debug("GMTP packet: %s (%d)",
					gmtp_packet_name(gh->type), gh->type);
			print_packet(skb, true);
			print_gmtp_packet(iph, gh);
		}

		switch(gh->type) {
		case GMTP_PKT_REQUEST:
			ret = gmtp_inter_request_rcv(skb);
			break;
		case GMTP_PKT_REGISTER_REPLY:
			ret = gmtp_inter_register_reply_rcv(skb);
			break;
		case GMTP_PKT_ACK:
			ret = gmtp_inter_ack_rcv(skb);
			break;
		case GMTP_PKT_DATA:
			ret = gmtp_inter_data_rcv(skb);
			break;
		case GMTP_PKT_FEEDBACK:
			ret = gmtp_inter_feedback_rcv(skb);
			break;
		case GMTP_PKT_ELECT_RESPONSE:
			ret = gmtp_inter_elect_resp_rcv(skb);
			break;
		case GMTP_PKT_RESET:
		case GMTP_PKT_CLOSE:
			ret = gmtp_inter_close_rcv(skb);
			break;
		}

	}

	return ret;
}

unsigned int hook_func_out(unsigned int hooknum, struct sk_buff *skb,
		const struct net_device *in, const struct net_device *out,
		int (*okfn)(struct sk_buff *))
{
	int ret = NF_ACCEPT;
	struct iphdr *iph = ip_hdr(skb);


	if((gmtp_info->relay_enabled == 0) || (out == NULL))
		return ret;

	if(iph->protocol == IPPROTO_GMTP) {

		struct gmtp_hdr *gh = gmtp_hdr(skb);

		if(gh->type != GMTP_PKT_DATA) {
			gmtp_print_debug("GMTP packet: %s (%d)",
					gmtp_packet_name(gh->type), gh->type);
			print_packet(skb, false);
			print_gmtp_packet(iph, gh);
		}

		switch(gh->type) {
		case GMTP_PKT_REGISTER:
			ret = gmtp_inter_register_out(skb);
			break;
		case GMTP_PKT_DATA:
			ret = gmtp_inter_data_out(skb);
			break;
		case GMTP_PKT_RESET:
		case GMTP_PKT_CLOSE:
			ret = gmtp_inter_close_out(skb);
			break;
		}
	}

	return ret;
}

void gmtp_timer_callback(void)
{
	gmtp_ucc(UINT_MAX, 0);
	mod_timer(&gmtp_inter.gmtp_ucc_timer, jiffies + HZ);
    
}

int init_module()
{
	int ret = 0;
	gmtp_pr_func();
	gmtp_print_debug("Starting GMTP-inter");

	if(gmtp_info == NULL) {
		gmtp_print_error("gmtp_info is NULL...");
		ret = -ENOBUFS;
		goto out;
	}

	setup_timer(&gmtp_inter.gmtp_ucc_timer, gmtp_timer_callback, 0);
   	mod_timer(&gmtp_inter.gmtp_ucc_timer, jiffies + msecs_to_jiffies(1000)); 

	gmtp_inter.capacity = CAPACITY_DEFAULT;
	gmtp_inter.buffer_len = 0;
	gmtp_inter.kreporter = GMTP_REPORTER_DEFAULT_PROPORTION - 1;

	/* TODO Why initial rx per flow is 10% of capacity of channel? */
	gmtp_inter.ucc_rx = DIV_ROUND_CLOSEST(gmtp_inter.capacity * 10, 100);

	gmtp_inter.total_bytes_rx = 0;
	gmtp_inter.total_rx = 0;
	gmtp_inter.ucc_bytes = 0;
	gmtp_inter.ucc_rx_tstamp = 0;
	gmtp_inter.rx_rate_wnd = 1000;

	memcpy(gmtp_inter.relay_id, gmtp_inter_build_relay_id(),
			GMTP_RELAY_ID_LEN);
	memset(&gmtp_inter.mcst, 0, 4 * sizeof(unsigned char));

	gmtp_inter.hashtable = gmtp_inter_create_hashtable(64);
	if(gmtp_inter.hashtable == NULL) {
		gmtp_print_error("Cannot create hashtable...");
		ret = -ENOMEM;
		goto out;
	}

	gmtp_info->relay_enabled = 1; /* Enables gmtp-inter */

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
	gmtp_pr_func();
	gmtp_print_debug("Finishing GMTP-inter");

	gmtp_info->relay_enabled = 0;
	kfree_gmtp_inter_hashtable(gmtp_inter.hashtable);

	nf_unregister_hook(&nfho_in);
	nf_unregister_hook(&nfho_out);
	del_timer(&gmtp_inter.gmtp_ucc_timer);
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Mário André Menezes <mariomenezescosta@gmail.com>");
MODULE_AUTHOR("Wendell Silva Soares <wss@ic.ufal.br>");
MODULE_DESCRIPTION("GMTP - Global Media Transmission Protocol");
