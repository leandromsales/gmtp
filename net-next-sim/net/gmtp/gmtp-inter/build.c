/*
 * build.c
 *
 *  Created on: 27/05/2015
 *      Author: wendell
 */

#include <linux/phy.h>
#include <linux/etherdevice.h>
#include <net/ip.h>
#include <net/sch_generic.h>
#include <linux/kernel.h>

#include <uapi/linux/gmtp.h>
#include <linux/gmtp.h>
#include "../gmtp.h"

#include "gmtp-inter.h"

void gmtp_inter_add_relayid(struct sk_buff *skb)
{
	struct gmtp_hdr_register_reply *gh_rply = gmtp_hdr_register_reply(skb);
	struct gmtp_hdr_relay relay;

	gmtp_print_function();

	memcpy(relay.relay_id, gmtp_inter.relay_id, GMTP_RELAY_ID_LEN);
	relay.relay_ip =  gmtp_inter_device_ip(skb->dev);

	gh_rply->relay_list[gh_rply->nrelays] = relay;
	++gh_rply->nrelays;
}

struct gmtp_hdr *gmtp_inter_make_route_hdr(struct sk_buff *skb)
{
	struct gmtp_hdr *gh = gmtp_hdr(skb);
	__u8 *transport_header;

	struct gmtp_hdr *gh_cpy;
	struct gmtp_hdr_route *gh_rn;
	struct gmtp_hdr_register_reply *gh_reply;

	int gmtp_hdr_len = sizeof(struct gmtp_hdr) +
			sizeof(struct gmtp_hdr_route);

	gmtp_print_function();

	transport_header = kmalloc(gmtp_hdr_len, gfp_any());
	memset(transport_header, 0, gmtp_hdr_len);

	gh_cpy = (struct gmtp_hdr *) transport_header;
	memcpy(gh_cpy, gh, sizeof(struct gmtp_hdr));

	gh_cpy->version = GMTP_VERSION;
	gh_cpy->type = GMTP_PKT_ROUTE_NOTIFY;
	gh_cpy->hdrlen = gmtp_hdr_len;
	pr_info("Original gh->transm_r: %u B/s\n", gh->transm_r);
	/*gh_cpy->transm_r = entry->transm_r;*/
	gh_cpy->relay = 1;
	gh_cpy->dport = gh->sport;
	gh_cpy->sport = gh->dport;

	gh_rn = (struct gmtp_hdr_route *)(transport_header
			+ sizeof(struct gmtp_hdr));
	gh_reply = gmtp_hdr_register_reply(skb);
	memcpy(gh_rn, gh_reply,	sizeof(*gh_reply));

	return gh_cpy;
}

struct gmtp_hdr *gmtp_inter_make_request_notify_hdr(struct sk_buff *skb,
		struct gmtp_inter_entry *entry, __be16 new_sport,
		__be16 new_dport, struct gmtp_client *my_reporter,
		__u8 max_nclients, __u8 code)
{
	struct gmtp_hdr *gh = gmtp_hdr(skb);
	__u8 *transport_header;

	struct gmtp_hdr *gh_cpy;
	struct gmtp_hdr_reqnotify *gh_rnotify;

	int gmtp_hdr_len = sizeof(struct gmtp_hdr)
			+ sizeof(struct gmtp_hdr_reqnotify);

	transport_header = kmalloc(gmtp_hdr_len, gfp_any());
	memset(transport_header, 0, gmtp_hdr_len);

	gh_cpy = (struct gmtp_hdr *)transport_header;
	memcpy(gh_cpy, gh, sizeof(struct gmtp_hdr));

	gh_cpy->version = GMTP_VERSION;
	gh_cpy->type = GMTP_PKT_REQUESTNOTIFY;
	gh_cpy->hdrlen = gmtp_hdr_len;
	gh_cpy->relay = 1;
	gh_cpy->sport = new_sport;
	gh_cpy->dport = new_dport;

	gh_rnotify = (struct gmtp_hdr_reqnotify*)(transport_header
			+ sizeof(struct gmtp_hdr));

	gh_rnotify->rn_code = code;
	gh_rnotify->mcst_addr = entry->channel_addr;
	gh_rnotify->mcst_port = entry->channel_port;
	memcpy(gh_rnotify->relay_id, gmtp_inter.relay_id, GMTP_RELAY_ID_LEN);

	if(my_reporter != NULL) {
		gh_rnotify->reporter_addr = my_reporter->addr;
		gh_rnotify->reporter_port = my_reporter->port;
		gh_rnotify->max_nclients = max_nclients;
		my_reporter->nclients++;
	}

	pr_info("ReqNotify => Ch: %pI4@%-5d | Code: %u | max_nclients: %u\n",
					&gh_rnotify->mcst_addr,
					ntohs(gh_rnotify->mcst_port),
					gh_rnotify->rn_code,
					gh_rnotify->max_nclients);

	pr_info("Reporter: %pI4@%-5d\n", &gh_rnotify->reporter_addr,
			ntohs(gh_rnotify->reporter_port));

	return gh_cpy;
}


int gmtp_inter_make_request_notify(struct sk_buff *skb, __be32 new_saddr,
		__be16 new_sport, __be32 new_daddr, __be16 new_dport,
		struct gmtp_client *reporter, __u8 max_nclients, __u8 code)
{
	int ret = NF_ACCEPT;

	struct gmtp_hdr *gh = gmtp_hdr(skb);
	struct iphdr *iph = ip_hdr(skb);
	struct gmtp_inter_entry *entry;
	unsigned int skb_len = skb->len;
	struct gmtp_hdr *new_gh;
	int gmtp_hdr_len = sizeof(struct gmtp_hdr)
			+ sizeof(struct gmtp_hdr_reqnotify);

	gmtp_pr_func();

	entry = gmtp_inter_lookup_media(gmtp_inter.hashtable, gh->flowname);
	if(entry == NULL) {
		gmtp_print_warning("Failed to lookup media info in table...");
		goto fail;
	}

	/* Delete REQUEST or REGISTER-REPLY specific header */
	skb_trim(skb, (skb_len - gmtp_packet_hdr_variable_len(gh->type)));

	new_gh = kmalloc(gmtp_hdr_len, GFP_ATOMIC);
	new_gh = gmtp_inter_make_request_notify_hdr(skb, entry, new_sport,
			new_dport, reporter, max_nclients, code);

	skb_put(skb, sizeof(struct gmtp_hdr_reqnotify));
	memcpy(gh, new_gh, gmtp_hdr_len);

	iph->saddr = new_saddr;
	iph->daddr = new_daddr;
	iph->tot_len = htons(skb->len);
	ip_send_check(iph);

	return ret;

fail:
	ret = NF_DROP;
	return ret;
}

struct gmtp_hdr *gmtp_inter_make_register_reply_hdr(struct sk_buff *skb,
		struct gmtp_inter_entry *entry, __be16 new_sport,
		__be16 new_dport)
{
	struct gmtp_hdr *gh = gmtp_hdr(skb);
	struct gmtp_hdr *gh_cpy;
	__u8 *transport_header;

	int gmtp_hdr_len = sizeof(struct gmtp_hdr)
			+ sizeof(struct gmtp_hdr_register_reply);

	gmtp_pr_func();

	transport_header = kmalloc(gmtp_hdr_len, gfp_any());
	memset(transport_header, 0, gmtp_hdr_len);

	gh_cpy = (struct gmtp_hdr *)transport_header;
	memcpy(gh_cpy, gh, sizeof(struct gmtp_hdr));

	gh_cpy->version = GMTP_VERSION;
	gh_cpy->type = GMTP_PKT_REGISTER_REPLY;
	gh_cpy->hdrlen = gmtp_hdr_len;
	gh_cpy->transm_r = entry->transm_r;
	gh_cpy->server_rtt = entry->server_rtt;
	gh_cpy->relay = 1;
	gh_cpy->sport = new_sport;
	gh_cpy->dport = new_dport;

	return gh_cpy;
}
EXPORT_SYMBOL_GPL(gmtp_inter_make_register_reply_hdr);

struct gmtp_hdr *gmtp_inter_make_reset_hdr(struct sk_buff *skb, __u8 code)
{
	struct gmtp_hdr *gh = gmtp_hdr(skb);
	__u8 *transport_header;

	struct gmtp_hdr *gh_cpy;
	struct gmtp_hdr_reset *gh_reset;

	int gmtp_hdr_len = sizeof(struct gmtp_hdr)
			+ sizeof(struct gmtp_hdr_reset);

	gmtp_pr_func();

	transport_header = kmalloc(gmtp_hdr_len, gfp_any());
	memset(transport_header, 0, gmtp_hdr_len);

	gh_cpy = (struct gmtp_hdr *)transport_header;
	memcpy(gh_cpy, gh, sizeof(struct gmtp_hdr));

	gh_cpy->type = GMTP_PKT_RESET;
	gh_cpy->hdrlen = gmtp_hdr_len;
	gh_cpy->relay = 1;
	swap(gh_cpy->sport, gh_cpy->dport);

	gh_reset = (struct gmtp_hdr_reset*)(transport_header
			+ sizeof(struct gmtp_hdr));

	gh_reset->reset_code = code;

	switch (gh_reset->reset_code) {
	case GMTP_RESET_CODE_PACKET_ERROR:
		gh_reset->reset_data[0] = gh->type;
		break;
	default:
		break;
	}

	return gh_cpy;
}


int gmtp_inter_make_reset(struct sk_buff *skb, struct gmtp_hdr *gh_reset)
{
	struct gmtp_hdr *gh = gmtp_hdr(skb);
	struct iphdr *iph = ip_hdr(skb);
	int gmtp_hdr_len = sizeof(struct gmtp_hdr)
			+ sizeof(struct gmtp_hdr_reset);

	gmtp_pr_func();

	skb_put(skb, sizeof(struct gmtp_hdr_reset));
	memcpy(gh, gh_reset, gmtp_hdr_len);

	swap(iph->saddr, iph->daddr);
	iph->tot_len = htons(skb->len);
	ip_send_check(iph);

	return NF_ACCEPT;
}


struct gmtp_hdr *gmtp_inter_make_close_hdr(struct sk_buff *skb)
{
	struct gmtp_hdr *gh = gmtp_hdr(skb);
	__u8 *transport_header;

	struct gmtp_hdr *gh_cpy;

	int gmtp_hdr_len = sizeof(struct gmtp_hdr);

	gmtp_pr_func();

	transport_header = kmalloc(gmtp_hdr_len, gfp_any());
	memset(transport_header, 0, gmtp_hdr_len);

	gh_cpy = (struct gmtp_hdr *)transport_header;
	memcpy(gh_cpy, gh, gmtp_hdr_len);

	gh_cpy->type = GMTP_PKT_CLOSE;
	gh_cpy->hdrlen = gmtp_hdr_len;
	gh_cpy->relay = 1;

	pr_info("%s (%u)\n", gmtp_packet_name(gh_cpy->type), gh_cpy->type);

	return gh_cpy;
}

/*
 * Add an ip header to a skbuff and send it out.
 * Based on netpoll_send_udp(...) (netpoll.c)
 */
struct sk_buff *gmtp_inter_build_pkt(struct sk_buff *skb_src, __be32 saddr,
		__be32 daddr, struct gmtp_hdr *gh_ref,
		enum gmtp_inter_direction direction)
{
	struct net_device *dev = skb_src->dev;
	struct ethhdr *eth_src = eth_hdr(skb_src);
	struct iphdr *iph_src = ip_hdr(skb_src);
	struct sk_buff *skb;

	struct ethhdr *eth;
	struct iphdr *iph;
	struct gmtp_hdr *gh;
	int total_len, ip_len, gmtp_len, data_len = 0;

	if(eth_src == NULL) {
		gmtp_print_warning("eth_src is NULL!");
		return NULL;
	}

	if(gh_ref->type == GMTP_PKT_DATA)
		data_len = gmtp_data_len(skb_src);

	gmtp_len = data_len + gh_ref->hdrlen;
	ip_len = gmtp_len + sizeof(*iph_src);
	total_len = ip_len + LL_RESERVED_SPACE(dev);

	/*skb = skb_copy(skb_src, gfp_any());*/
	skb = alloc_skb(GMTP_MAX_HDR_LEN, GFP_ATOMIC);
	if(skb == NULL) {
		gmtp_print_warning("skb is null");
		return NULL;
	}

	/*skb_put(skb, data_len);*/
	/*skb_reset_tail_pointer(skb);*/
	skb_reserve(skb, GMTP_MAX_HDR_LEN);

	/* Build GMTP data */
	if(data_len > 0) {
		unsigned char *data = skb_push(skb, data_len);
		skb_reset_transport_header(skb);
		memcpy(data, gmtp_data(skb_src), data_len);
	}

	/* Build GMTP header */
	skb_push(skb, gh_ref->hdrlen);
	skb_reset_transport_header(skb);
	gh = gmtp_hdr(skb);
	memcpy(gh, gh_ref, gh_ref->hdrlen);

	/* Build the IP header. */
	skb_push(skb, sizeof(*iph));
	skb_reset_network_header(skb);
	iph = ip_hdr(skb);

	/* iph->version = 4; iph->ihl = 5; */
	put_unaligned(0x45, (unsigned char *)iph);
	iph->tos      = 0;
	iph->frag_off = 0;
	iph->ttl      = 64;
	iph->protocol = IPPROTO_GMTP;
	put_unaligned(saddr, &(iph->saddr));
	put_unaligned(daddr, &(iph->daddr));
	put_unaligned(htons(skb->len), &(iph->tot_len));
	ip_send_check(iph);

	eth = (struct ethhdr *) skb_push(skb, ETH_HLEN);
	skb_reset_mac_header(skb);
	skb->protocol = eth->h_proto = htons(ETH_P_IP);

	ether_addr_copy(eth->h_source, dev->dev_addr);
	switch(direction) {
	case GMTP_INTER_FORWARD:
		ether_addr_copy(eth->h_dest, eth_src->h_dest);
		break;
	case GMTP_INTER_BACKWARD:
		ether_addr_copy(eth->h_dest, eth_src->h_source);
		break;
	case GMTP_INTER_LOCAL:
		ether_addr_copy(eth->h_dest, eth->h_source);
		ether_addr_copy(eth->h_source, eth_src->h_dest);
		/*dev->qdisc->flags = 20;*/
		break;
	}
	skb->dev = dev;

	return skb;
}

/*
 * Add an ip header to a skbuff and send it out.
 * Based on netpoll_send_udp(...) (netpoll.c)
 */
struct sk_buff *gmtp_inter_build_multicast_pkt(struct sk_buff *skb_src,
		struct net_device *dev)
{
	struct sk_buff *skb;
	struct iphdr *iph_src = ip_hdr(skb_src);
	struct gmtp_hdr *gh_src = gmtp_hdr(skb_src);

	struct ethhdr *eth;
	struct iphdr *iph;
	struct gmtp_hdr *gh;
	int total_len, ip_len, gmtp_len, data_len = 0;

	if(gh_src->type == GMTP_PKT_DATA)
		data_len = gmtp_data_len(skb_src);
	gmtp_len = gh_src->hdrlen + data_len;
	ip_len = gmtp_len + sizeof(*iph_src);
	total_len = ip_len + LL_RESERVED_SPACE(dev);

	skb = skb_copy(skb_src, gfp_any());
	if(skb == NULL) {
		gmtp_print_warning("skb is null");
		return NULL;
	}

	skb_reserve(skb, total_len);

	/* Build GMTP data */
	if(data_len > 0) {
		unsigned char *data = skb_push(skb, data_len);
		memcpy(data, gmtp_data(skb_src), data_len);
	}

	/* Build GMTP header */
	skb_push(skb, gh_src->hdrlen);
	skb_reset_transport_header(skb);
	gh = gmtp_hdr(skb);
	memcpy(gh,  gh_src, gh_src->hdrlen);

	/* Build the IP header. */
	skb_push(skb, sizeof(struct iphdr));
	skb_reset_network_header(skb);
	iph = ip_hdr(skb);

	/* iph->version = 4; iph->ihl = 5; */
	put_unaligned(0x45, (unsigned char *)iph);
	iph->tos = 0;
	iph->frag_off = 0;
	iph->ttl = 64;
	iph->protocol = IPPROTO_GMTP;
	put_unaligned(iph_src->saddr, &(iph->saddr));
	put_unaligned(iph_src->daddr, &(iph->daddr));
	put_unaligned(htons(skb->len), &(iph->tot_len));
	ip_send_check(iph);

	eth = (struct ethhdr *)skb_push(skb, ETH_HLEN);
	skb_reset_mac_header(skb);
	skb->protocol = eth->h_proto = htons(ETH_P_IP);

	ether_addr_copy(eth->h_source, dev->dev_addr);
	ether_addr_copy(eth->h_dest, dev->dev_addr);

	u8 source_str[13];
	u8 dest_str[13];
	int i;
	for(i = 0; i < 6; ++i)
		sprintf(&source_str[i*2], "%02x", eth->h_source[i]);

	for(i = 0; i < 6; ++i)
		sprintf(&dest_str[i * 2], "%02x", eth->h_dest[i]);

	pr_info("src: %s, dst: %s\n", source_str, dest_str);

	/*dev->qdisc->flags = 20;*/

	dev->qdisc->flags = 20;
	skb->dev = dev;

	return skb;
}


void gmtp_inter_send_pkt(struct sk_buff *skb)
{
	int err = dev_queue_xmit(skb);
	if(err)
		gmtp_pr_error("Error %d trying send packet (%p)", err, skb);
}

/*
 * Add an ip header to a skbuff and send it out.
 * Based on netpoll_send_udp(...) (netpoll.c)
 */
void gmtp_inter_build_and_send_pkt(struct sk_buff *skb_src, __be32 saddr,
		__be32 daddr, struct gmtp_hdr *gh_ref,
		enum gmtp_inter_direction direction)
{
	struct sk_buff *skb = gmtp_inter_build_pkt(skb_src, saddr, daddr,
			gh_ref, direction);

	if(skb != NULL)
		gmtp_inter_send_pkt(skb);
}

void gmtp_inter_build_and_send_skb(struct sk_buff *skb,
		enum gmtp_inter_direction direction)
{
	struct iphdr *iph = ip_hdr(skb);
	struct gmtp_hdr *gh = gmtp_hdr(skb);

	gmtp_inter_build_and_send_pkt(skb, iph->saddr, iph->daddr, gh, direction);
}

void gmtp_copy_hdr(struct sk_buff *skb, struct sk_buff *src_skb)
{
	struct gmtp_hdr *gh = gmtp_hdr(skb);
	struct gmtp_hdr *gh_src = gmtp_hdr(src_skb);

	if(gh->type == gh_src->type)
		memcpy(gh, gh_src, gh_src->hdrlen);
}

struct sk_buff *gmtp_inter_build_ack(struct gmtp_inter_entry *entry)
{
	struct sk_buff *skb = alloc_skb(GMTP_MAX_HDR_LEN, GFP_ATOMIC);

	struct ethhdr *eth;
	struct iphdr *iph;
	struct gmtp_hdr *gh;
	struct gmtp_hdr_ack *gack;

	struct net_device *dev_entry = NULL;
	int gmtp_hdr_len = sizeof(struct gmtp_hdr) + sizeof(struct gmtp_hdr_ack);
	int total_len, ip_len = 0;

	dev_entry = entry->dev_in;

	ip_len = gmtp_hdr_len + sizeof(struct iphdr);
	total_len = ip_len + LL_RESERVED_SPACE(dev_entry);
	skb_reserve(skb, total_len);

	gh = gmtp_zeroed_hdr(skb, gmtp_hdr_len);

	gh->version = GMTP_VERSION;
	gh->type = GMTP_PKT_ACK;
	gh->hdrlen = gmtp_hdr_len;
	gh->relay = 1;
	gh->seq = entry->seq;
	gh->dport = entry->media_port;
	gh->sport = entry->my_port;
	gh->server_rtt = entry->server_rtt;
	gh->transm_r = min(gmtp_inter.ucc_rx, entry->rcv_tx_rate);
	memcpy(gh->flowname, entry->flowname, GMTP_FLOWNAME_LEN);

	gack = gmtp_hdr_ack(skb);
	gack->orig_tstamp = entry->last_data_tstamp;

	/* Build the IP header. */
	skb_push(skb, sizeof(struct iphdr));
	skb_reset_network_header(skb);
	iph = ip_hdr(skb);

	/* iph->version = 4; iph->ihl = 5; */
	put_unaligned(0x45, (unsigned char *)iph);
	iph->tos = 0;
	iph->frag_off = 0;
	iph->ttl = 64;
	iph->protocol = IPPROTO_GMTP;

	put_unaligned(entry->my_addr, &(iph->saddr));
	put_unaligned(entry->server_addr, &(iph->daddr));
	put_unaligned(htons(skb->len), &(iph->tot_len));
	ip_send_check(iph);

	/*print_gmtp_packet(iph, gh);*/

	skb_push(skb, ETH_HLEN);
	skb_reset_mac_header(skb);
	eth = eth_hdr(skb);
	skb->protocol = eth->h_proto = htons(ETH_P_IP);

	ether_addr_copy(eth->h_source, dev_entry->dev_addr);
	ether_addr_copy(eth->h_dest, entry->server_mac_addr);

	skb->dev = dev_entry;
	return skb;
}

