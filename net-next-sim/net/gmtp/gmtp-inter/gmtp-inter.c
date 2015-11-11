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

static struct nf_hook_ops nfho_pre_routing;
static struct nf_hook_ops nfho_local_in;
static struct nf_hook_ops nfho_local_out;
static struct nf_hook_ops nfho_post_routing;
struct gmtp_inter gmtp_inter;

/* FIXME This fails at NS-3-DCE */
unsigned char *gmtp_build_md5(unsigned char *buf)
{
	struct scatterlist sg;
	struct crypto_hash *tfm;
	struct hash_desc desc;
	unsigned char *output;
	size_t buf_size = sizeof(buf) - 1;
	 __u8 md5[21];

	gmtp_print_function();

	output = kmalloc(MD5_LEN * sizeof(unsigned char), GFP_KERNEL);
	tfm = crypto_alloc_hash("md5", 0, CRYPTO_ALG_ASYNC);

	if(output == NULL || IS_ERR(tfm)) {
		gmtp_pr_warning("Allocation failed...");
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
	u8 *str[30];

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

	if(dev == NULL)
		return 0;

	in_dev = (struct in_device *)dev->ip_ptr;
	if_info = in_dev->ifa_list;
	for(; if_info; if_info = if_info->ifa_next) {
		/* just return the first entry for now */
		return if_info->ifa_address;
	}

	return 0;
}

bool gmtp_inter_ip_local(__be32 ip)
{
	struct socket *sock = NULL;
	struct net_device *dev = NULL;
	struct net *net;

	int i, length = 0;
	char mac_address[6];

	char buffer[50];
	u8 *str[30];

	bool ret = false;

	sock_create(AF_INET, SOCK_STREAM, 0, &sock);
	net = sock_net(sock->sk);

	for(i = 2; (dev = dev_get_by_index_rcu(net, i)) != NULL; ++i) {
		__be32 dev_ip = gmtp_inter_device_ip(dev);
		if(ip == dev_ip) {
			ret = true;
			break;
		}
	}

	sock_release(sock);
	return ret;
}


__be32 get_mcst_v4_addr(void)
{
	__be32 mcst_addr;
	unsigned char *base_channel = "\xe0\xc0\x00\x00"; /* 224.192.0.0 */
	unsigned char *channel = kmalloc(4 * sizeof(unsigned char), GFP_KERNEL);

	gmtp_print_function();

	if(channel == NULL) {
		gmtp_pr_error("NULL channel: Cannot assign requested address");
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
		int i;
		for(i = 0; i < 4; ++i)
			pr_info("gmtp_inter.mcst[%d] = %u\n", i,
					gmtp_inter.mcst[i]);
		gmtp_pr_error("Cannot assign requested multicast address");
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

void gmtp_buffer_add(struct gmtp_inter_entry *info, struct sk_buff *newskb)
{
	skb_queue_tail(info->buffer, skb_copy(newskb, GFP_ATOMIC));
	info->buffer_len += newskb->len + ETH_HLEN;
}

struct sk_buff *gmtp_buffer_dequeue(struct gmtp_inter_entry *info)
{
	struct sk_buff *skb = skb_dequeue(info->buffer);
	if(skb != NULL) {
		info->buffer_len -= (skb->len + ETH_HLEN);
	}
	return skb;
}

unsigned int hook_func_pre_routing(unsigned int hooknum, struct sk_buff *skb,
		const struct net_device *in, const struct net_device *out,
		int (*okfn)(struct sk_buff *))
{
	int ret = NF_ACCEPT;
	struct iphdr *iph = ip_hdr(skb);

	gmtp_inter.buffer_len += skb->len + ETH_HLEN;

	if(gmtp_info->relay_enabled == 0)
		return ret;

	if(iph->protocol == IPPROTO_GMTP) {

		struct gmtp_hdr *gh = gmtp_hdr(skb);
		struct gmtp_inter_entry *entry;
		struct gmtp_relay *relay;

		if(gh->type == GMTP_PKT_REQUEST) {
			if(gmtp_inter_ip_local(iph->saddr)
					&& iph->saddr != iph->daddr)
				return NF_ACCEPT;

			if(iph->ttl == 1) {
				print_packet(skb, true);
				print_gmtp_packet(iph, gh);
				return gmtp_inter_request_rcv(skb);
			}
		}

		entry = gmtp_inter_lookup_media(gmtp_inter.hashtable,
				gh->flowname);
		if(entry == NULL)
			return NF_ACCEPT;

		switch(gh->type) {
		case GMTP_PKT_REGISTER:
			if(!gmtp_inter_ip_local(iph->daddr))
				ret = gmtp_inter_register_rcv(skb);
			break;
		case GMTP_PKT_REGISTER_REPLY:
			ret = gmtp_inter_register_reply_rcv(skb, entry,
					GMTP_INTER_BACKWARD);
			break;
		case GMTP_PKT_ROUTE_NOTIFY:
			ret = gmtp_inter_route_rcv(skb, entry);
			break;
		case GMTP_PKT_ACK:
			ret = gmtp_inter_ack_rcv(skb, entry);
			break;
		case GMTP_PKT_FEEDBACK:
			ret = gmtp_inter_feedback_rcv(skb, entry);
			break;
		case GMTP_PKT_DELEGATE:
			ret = gmtp_inter_delegate_rcv(skb, entry);
			break;
		default:
			relay = gmtp_get_relay(&entry->relays->list,
					iph->daddr, gh->dport);
			if(!gmtp_inter_ip_local(iph->daddr) && (relay == NULL))
				return NF_ACCEPT;
		}

		switch(gh->type) {
		case GMTP_PKT_DATA:
			ret = gmtp_inter_data_rcv(skb, entry);
			break;
		case GMTP_PKT_ELECT_RESPONSE:
			ret = gmtp_inter_elect_resp_rcv(skb, entry);
			break;
		case GMTP_PKT_RESET:
		case GMTP_PKT_CLOSE:
			ret = gmtp_inter_close_rcv(skb, entry, true);
			break;
		}

	}

	return ret;
}


unsigned int hook_func_local_in(unsigned int hooknum, struct sk_buff *skb,
		const struct net_device *in, const struct net_device *out,
		int (*okfn)(struct sk_buff *))
{
	int ret = NF_ACCEPT;
	struct iphdr *iph = ip_hdr(skb);

	if(gmtp_info->relay_enabled == 0)
		return ret;

	if(iph->protocol == IPPROTO_GMTP) {

		struct gmtp_hdr *gh = gmtp_hdr(skb);

		struct gmtp_inter_entry *entry = gmtp_inter_lookup_media(
				gmtp_inter.hashtable, gh->flowname);
		if(entry == NULL)
			return NF_ACCEPT;

		if(!gmtp_inter_lookup_media(gmtp_inter.hashtable, gh->flowname))
			return NF_ACCEPT;

		switch(gh->type) {
		case GMTP_PKT_REGISTER:
			/* Here. We need to trick the server,
			to avoid data packets destined to 'lo' */
			ret = gmtp_inter_register_local_in(skb, entry);
			break;
		}
	}

	return ret;
}

unsigned int hook_func_local_out(unsigned int hooknum, struct sk_buff *skb,
		const struct net_device *in, const struct net_device *out,
		int (*okfn)(struct sk_buff *))
{
	int ret = NF_ACCEPT;
	struct iphdr *iph = ip_hdr(skb);

	if(gmtp_info->relay_enabled == 0)
		return ret;

	if(iph->protocol == IPPROTO_GMTP) {

		struct gmtp_hdr *gh = gmtp_hdr(skb);

		pr_info("LOCAL_OUT: ");
		print_gmtp_packet(iph, gh);

		struct gmtp_inter_entry *entry = gmtp_inter_lookup_media(
				gmtp_inter.hashtable, gh->flowname);
		if(entry == NULL)
			return NF_ACCEPT;

		switch(gh->type) {
		case GMTP_PKT_RESET:
		case GMTP_PKT_CLOSE:
			ret = gmtp_inter_close_rcv(skb, entry, false);
			break;
		}
	}

	return ret;
}

static int gmtp_inter_has_clients(struct sk_buff *skb,
		struct gmtp_inter_entry *entry)
{
	struct iphdr *iph = ip_hdr(skb);
	struct gmtp_hdr *gh = gmtp_hdr(skb);

	struct gmtp_relay *relay = gmtp_get_relay(&entry->relays->list,
			iph->daddr, gh->dport);
	struct gmtp_reporter *reporter = gmtp_get_client(&entry->clients->list,
			iph->daddr, gh->dport);

	if(relay == NULL && reporter == NULL)
		return 1;
	return 0;
}

unsigned int hook_func_post_routing(unsigned int hooknum, struct sk_buff *skb,
		const struct net_device *in, const struct net_device *out,
		int (*okfn)(struct sk_buff *))
{
	int ret = NF_ACCEPT;
	struct iphdr *iph = ip_hdr(skb);

	gmtp_inter.buffer_len -= skb->len + ETH_HLEN;
	if(gmtp_inter.buffer_len < 0)
		gmtp_inter.buffer_len = 0;

	if(gmtp_info->relay_enabled == 0)
		return ret;

	if(iph->protocol == IPPROTO_GMTP) {

		struct gmtp_hdr *gh = gmtp_hdr(skb);
		struct gmtp_inter_entry *entry;

		entry = gmtp_inter_lookup_media(gmtp_inter.hashtable,
				gh->flowname);
		if(entry == NULL)
			return NF_ACCEPT;

		if(gh->type == GMTP_PKT_DATA) {
			if(!gmtp_inter_ip_local(iph->daddr) ||
			    GMTP_SKB_CB(skb)->jumped) {
				if(gmtp_inter_has_clients(skb, entry))
					return NF_ACCEPT;
			}
		}

		switch(gh->type) {
		case GMTP_PKT_REGISTER:
			ret = gmtp_inter_register_out(skb, entry);
			break;
		case GMTP_PKT_REGISTER_REPLY:
			ret = gmtp_inter_register_reply_out(skb, entry);
			break;
		case GMTP_PKT_DATA:
			if(gmtp_inter_ip_local(iph->daddr)
					|| GMTP_SKB_CB(skb)->jumped) {
				ret = gmtp_inter_data_out(skb, entry);
			}
			break;
		case GMTP_PKT_ACK:
			ret = gmtp_inter_ack_out(skb, entry);
			break;
		case GMTP_PKT_RESET:
		case GMTP_PKT_CLOSE:
			ret = gmtp_inter_close_out(skb, entry);
			break;
		}
	}

	return ret;
}

static void register_hooks(void)
{
	nfho_pre_routing.hook = hook_func_pre_routing;
	nfho_pre_routing.hooknum = NF_INET_PRE_ROUTING;
	nfho_pre_routing.pf = PF_INET;
	nfho_pre_routing.priority = NF_IP_PRI_FIRST;
	nf_register_hook(&nfho_pre_routing);

	nfho_local_in.hook = hook_func_local_in;
	nfho_local_in.hooknum = NF_INET_LOCAL_IN;
	nfho_local_in.pf = PF_INET;
	nfho_local_in.priority = NF_IP_PRI_FIRST;
	nf_register_hook(&nfho_local_in);

	nfho_local_out.hook = hook_func_local_out;
	nfho_local_out.hooknum = NF_INET_LOCAL_OUT;
	nfho_local_out.pf = PF_INET;
	nfho_local_out.priority = NF_IP_PRI_FIRST;
	nf_register_hook(&nfho_local_out);

	nfho_post_routing.hook = hook_func_post_routing;
	nfho_post_routing.hooknum = NF_INET_POST_ROUTING;
	nfho_post_routing.pf = PF_INET;
	nfho_post_routing.priority = NF_IP_PRI_FIRST;
	nf_register_hook(&nfho_post_routing);
}

int init_module()
{
	int ret = 0;
	__u8 relay_id[21];
	unsigned char *rid;

	gmtp_pr_func();
	gmtp_print_debug("Starting GMTP-Inter");

	if(gmtp_info == NULL) {
		gmtp_print_error("gmtp_info is NULL...");
		ret = -ENOBUFS;
		goto out;
	}

	gmtp_inter.capacity = CAPACITY_DEFAULT;
	gmtp_inter.buffer_len = 0;
	gmtp_inter.kreporter = GMTP_REPORTER_DEFAULT_PROPORTION - 1;

	/* TODO Why initial rx per flow is 5% of capacity of channel? */
	gmtp_inter.ucc_rx = DIV_ROUND_CLOSEST(gmtp_inter.capacity * 10, 100);

	gmtp_inter.total_bytes_rx = 0;
	gmtp_inter.total_rx = 0;
	gmtp_inter.ucc_bytes = 0;
	gmtp_inter.ucc_rx_tstamp = 0;
	gmtp_inter.rx_rate_wnd = 100;
	memset(&gmtp_inter.mcst, 0, 4 * sizeof(unsigned char));

	rid = gmtp_inter_build_relay_id();
	if(rid == NULL) {
		gmtp_pr_error("Relay ID build failed. Creating a random id.");
		get_random_bytes(gmtp_inter.relay_id, GMTP_FLOWNAME_LEN);
	} else
		memcpy(gmtp_inter.relay_id, rid, GMTP_FLOWNAME_LEN);

	flowname_strn(relay_id, gmtp_inter.relay_id, MD5_LEN);
	pr_info("Relay ID = %s\n", relay_id);

	gmtp_inter.hashtable = gmtp_inter_create_hashtable(64);
	if(gmtp_inter.hashtable == NULL) {
		gmtp_print_error("Cannot create hashtable...");
		ret = -ENOMEM;
		goto out;
	}

	gmtp_info->relay_enabled = 1; /* Enabling gmtp-inter */

	gmtp_inter.h_user = UINT_MAX; /* TODO Make it user defined */
	gmtp_inter.worst_rtt = GMTP_MIN_RTT_MS;

	pr_info("Configuring GMTP-UCC timer...\n");
	setup_timer(&gmtp_inter.gmtp_ucc_timer, gmtp_ucc_equation_callback, 0);
	mod_timer(&gmtp_inter.gmtp_ucc_timer, jiffies + 1);

	register_hooks();

out:
	return ret;
}

static void unregister_hooks(void)
{
	nf_unregister_hook(&nfho_pre_routing);
	nf_unregister_hook(&nfho_local_in);
	nf_unregister_hook(&nfho_post_routing);
}

void cleanup_module()
{
	gmtp_pr_func();
	gmtp_print_debug("Finishing GMTP-inter");

	gmtp_info->relay_enabled = 0;
	kfree_gmtp_inter_hashtable(gmtp_inter.hashtable);

	unregister_hooks();
	del_timer(&gmtp_inter.gmtp_ucc_timer);
}

module_init(init_module);
module_exit(cleanup_module);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Mário André Menezes <mariomenezescosta@gmail.com>");
MODULE_AUTHOR("Wendell Silva Soares <wss@ic.ufal.br>");
MODULE_DESCRIPTION("GMTP - Global Media Transmission Protocol");
