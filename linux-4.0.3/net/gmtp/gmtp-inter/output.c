/*
 * output.c
 *
 *  Created on: 27/02/2015
 *      Author: wendell
 */

#include <linux/phy.h>
#include <linux/etherdevice.h>
#include <net/ip.h>

#include <uapi/linux/gmtp.h>
#include <linux/gmtp.h>
#include "../gmtp.h"

#include "gmtp-inter.h"
#include "mcc-inter.h"

void gmtp_inter_add_relayid(struct sk_buff *skb)
{
	struct gmtp_hdr_register_reply *gh_rply = gmtp_hdr_register_reply(skb);
	struct gmtp_relay relay;

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
	gh_cpy->dport = gh->sport;
	gh_cpy->sport = gh->dport;

	gh_rn = (struct gmtp_hdr_route *)(transport_header
			+ sizeof(struct gmtp_hdr));
	gh_reply = gmtp_hdr_register_reply(skb);
	memcpy(gh_rn, gh_reply,	sizeof(*gh_reply));

	return gh_cpy;
}

struct gmtp_hdr *gmtp_inter_make_request_notify_hdr(struct sk_buff *skb,
		struct gmtp_relay_entry *entry, __be16 new_sport,
		__be16 new_dport, __u8 code)
{
	struct gmtp_hdr *gh = gmtp_hdr(skb);
	__u8 *transport_header;

	struct gmtp_hdr *gh_cpy;
	struct gmtp_hdr_reqnotify *gh_rnotify;

	int gmtp_hdr_len = sizeof(struct gmtp_hdr)
			+ sizeof(struct gmtp_hdr_reqnotify);

	gmtp_print_function();

	transport_header = kmalloc(gmtp_hdr_len, gfp_any());
	memset(transport_header, 0, gmtp_hdr_len);

	gh_cpy = (struct gmtp_hdr *)transport_header;
	memcpy(gh_cpy, gh, sizeof(struct gmtp_hdr));

	gh_cpy->version = GMTP_VERSION;
	gh_cpy->type = GMTP_PKT_REQUESTNOTIFY;
	gh_cpy->hdrlen = gmtp_hdr_len;
	gh_cpy->sport = new_sport;
	gh_cpy->dport = new_dport;

	gh_rnotify = (struct gmtp_hdr_reqnotify*)(transport_header
			+ sizeof(struct gmtp_hdr));

	gh_rnotify->error_code = code;

	if(entry != NULL && code != GMTP_REQNOTIFY_CODE_ERROR) {
		gh_rnotify->mcst_addr = entry->channel_addr;
		gh_rnotify->mcst_port = entry->channel_port;

		gmtp_print_debug("ReqNotify => Channel: %pI4@%-5d | Code: %d",
				&gh_rnotify->mcst_addr,
				ntohs(gh_rnotify->mcst_port),
				gh_rnotify->error_code);
	} else {
		gmtp_print_debug("ReqNotify => Channel: NULL | Code: %d",
				gh_rnotify->error_code);
	}

	return gh_cpy;
}


int gmtp_inter_make_request_notify(struct sk_buff *skb, __be32 new_saddr,
		__be16 new_sport, __be32 new_daddr, __be16 new_dport,
		__u8 code)
{
	int ret = NF_ACCEPT;

	struct gmtp_hdr *gh = gmtp_hdr(skb);
	struct iphdr *iph = ip_hdr(skb);
	struct gmtp_hdr_reqnotify *gh_rnotify;
	struct gmtp_relay_entry *entry;
	unsigned int skb_len = skb->len;
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

	gh->version = GMTP_VERSION;
	gh->type = GMTP_PKT_REQUESTNOTIFY;
	gh->hdrlen = gmtp_hdr_len;
	gh->sport = new_sport;
	gh->dport = new_dport;

	gh_rnotify = (struct gmtp_hdr_reqnotify*)
			skb_put(skb, sizeof(struct gmtp_hdr_reqnotify));

	if(gh_rnotify == NULL)
		goto fail;

	if(entry != NULL && code != GMTP_REQNOTIFY_CODE_ERROR) {
		gh_rnotify->error_code = code;
		gh_rnotify->mcst_addr = entry->channel_addr;
		gh_rnotify->mcst_port = entry->channel_port;

		gmtp_print_debug("ReqNotify => Channel: %pI4@%-5d | Code: %d",
				&gh_rnotify->mcst_addr,
				ntohs(gh_rnotify->mcst_port),
				gh_rnotify->error_code);
	} else {
		gmtp_print_debug("ReqNotify => Channel: NULL | Code: %d",
				gh_rnotify->error_code);
	}

	iph->saddr = new_saddr;
	iph->daddr = new_daddr;
	iph->tot_len = htons(skb->len);
	ip_send_check(iph);

	return ret;

fail:
	ret = NF_DROP;
	return ret;
}

/*
 * Add an ip header to a skbuff and send it out.
 * Based on netpoll_send_udp(...) (netpoll.c)
 */
struct sk_buff *gmtp_inter_build_pkt(struct sk_buff *skb_src, __be32 saddr,
		__be32 daddr, struct gmtp_hdr *gh_ref, bool backward)
{
	struct net_device *dev = skb_src->dev;
	struct ethhdr *eth_src = eth_hdr(skb_src);

	struct sk_buff *skb = alloc_skb(gh_ref->hdrlen, gfp_any());
	struct ethhdr *eth;
	struct iphdr *iph;
	struct gmtp_hdr *gh;
	int total_len, ip_len, gmtp_len, data_len = 0;

	if(eth_src == NULL) {
		gmtp_print_warning("eth_src is NULL!");
		return NULL;
	}

	if (skb == NULL) {
		gmtp_print_warning("skb is null");
		return NULL;
	}

	if(gh_ref->type == GMTP_PKT_DATA)
		data_len = gmtp_data_len(skb_src);

	gmtp_len = gh_ref->hdrlen + data_len;
	ip_len = gmtp_len + sizeof(*iph);
	total_len = ip_len + LL_RESERVED_SPACE(dev);

	skb_reserve(skb, total_len);

	/* Build GMTP data */
	if(data_len > 0) {
		unsigned char *data = skb_push(skb, data_len);
		memcpy(data, gmtp_data(skb_src), data_len);
	}

	/* Build GMTP header */
	skb_push(skb, gh_ref->hdrlen);
	skb_reset_transport_header(skb);
	gh = gmtp_hdr(skb);
	memcpy(gh, gh_ref, gh_ref->hdrlen);

	/* Build the IP header. */
	skb_push(skb, sizeof(struct iphdr));
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
	if(backward)
		ether_addr_copy(eth->h_dest, eth_src->h_source);
	else
		ether_addr_copy(eth->h_dest, eth_src->h_dest);

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
		__be32 daddr, struct gmtp_hdr *gh_ref, bool backward)
{
	struct sk_buff *skb = gmtp_inter_build_pkt(skb_src, saddr, daddr,
			gh_ref, backward);

	if(skb != NULL)
		gmtp_inter_send_pkt(skb);
}

void gmtp_inter_build_and_send_skb(struct sk_buff *skb)
{
	struct iphdr *iph = ip_hdr(skb);
	struct gmtp_hdr *gh = gmtp_hdr(skb);

	gmtp_inter_build_and_send_pkt(skb, iph->saddr, iph->daddr, gh, 0);
}

void gmtp_copy_hdr(struct sk_buff *skb, struct sk_buff *src_skb)
{
	struct gmtp_hdr *gh = gmtp_hdr(skb);
	struct gmtp_hdr *gh_src = gmtp_hdr(src_skb);

	if(gh->type == gh_src->type)
		memcpy(gh, gh_src, gh_src->hdrlen);
}

int gmtp_inter_register_out(struct sk_buff *skb)
{
	struct iphdr *iph = ip_hdr(skb);
	struct gmtp_hdr *gh = gmtp_hdr(skb);
	struct gmtp_relay_entry *entry;

	gmtp_pr_func();

	entry = gmtp_inter_lookup_media(gmtp_inter.hashtable, gh->flowname);
	if(entry == NULL)
		return NF_DROP;

	/* FIXME Get a valid and unused port */
	entry->info->my_addr = gmtp_inter_device_ip(skb->dev);
	entry->info->my_port = gh->sport;

	iph->saddr = entry->info->my_addr;
	ip_send_check(iph);

	return NF_ACCEPT;
}

int gmtp_inter_request_notify_out(struct sk_buff *skb)
{
	struct gmtp_hdr *gh = gmtp_hdr(skb);
	struct iphdr *iph = ip_hdr(skb);
	struct gmtp_hdr *gh_req_n;
	struct gmtp_relay_entry *entry;

	struct gmtp_client *client;
	__u8 code = GMTP_REQNOTIFY_CODE_OK;

	entry = gmtp_inter_lookup_media(gmtp_inter.hashtable, gh->flowname);
	if(entry == NULL)
		return NF_ACCEPT;

	list_for_each_entry(client, &entry->info->clients->list, list)
	{
		struct sk_buff *copy = skb_copy(skb, gfp_any());

		pr_info("Client: %pI4@%-5d, Rep: %d\n", &client->addr,
				client->port, client->reporter);

		code = client->reporter == 1 ?
				GMTP_REQNOTIFY_CODE_OK_REPORTER :
				GMTP_REQNOTIFY_CODE_OK;
		if(copy != NULL) {
			struct iphdr *iph_copy = ip_hdr(copy);
			struct gmtp_hdr *gh_copy = gmtp_hdr(copy);

			iph_copy->daddr = client->addr;
			ip_send_check(iph_copy);
			gh_copy->dport = client->port;

			gh_req_n = gmtp_inter_make_request_notify_hdr(copy,
					entry, gh->sport, client->port, code);
			if(gh_req_n != NULL)
				gmtp_inter_build_and_send_pkt(skb, iph->saddr,
						client->addr, gh_req_n, false);
		}
	}

	return NF_ACCEPT;
}

void gmtp_copy_data(struct sk_buff *skb, struct sk_buff *src_skb)
{
	__u8* data = gmtp_data(skb);
	__u32 data_len = gmtp_data_len(skb);

	__u8* data_src = gmtp_data(src_skb);
	__u32 data_src_len = gmtp_data_len(src_skb);

	int diff = (int)data_len - (int)data_src_len;

	if(diff > 0)
		skb_trim(skb, diff);
	else if(diff < 0)
		skb_put(skb, -diff);

	memcpy(data, data_src, data_src_len);
}

/**
 * Read p in buffer
 * P = p.flowname
 * If P != NULL:
 *     p.dest_address = get_multicast_channel(P)
 *     p.port = get_multicast_port(P)
 *     send(p)
 */
int gmtp_inter_data_out(struct sk_buff *skb)
{
	struct gmtp_hdr *gh = gmtp_hdr(skb);
	struct iphdr *iph = ip_hdr(skb);
	struct gmtp_relay_entry *entry;
	struct gmtp_flow_info *info;

	entry = gmtp_inter_lookup_media(gmtp_inter.hashtable, gh->flowname);
	if(entry == NULL) /* Failed to lookup media info in table... */
		goto out;

	info = entry->info;
	if(entry->state == GMTP_INTER_TRANSMITTING) {
		if(info->buffer_len > info->buffer_min) {
			struct sk_buff *buffered = gmtp_buffer_dequeue(info);
			if(buffered != NULL) {
				info->data_pkt_tx++;
				gmtp_copy_hdr(skb, buffered);
				gmtp_copy_data(skb, buffered);
			}
		} else
			return NF_DROP;
	}

	iph->daddr = entry->channel_addr;
	ip_send_check(iph);
	gh->dport = entry->channel_port;

	/** TODO We really trust in declared tx rate from server ? */
	gmtp_inter_mcc_delay(info, skb, (u64) gh->transm_r);

out:
	return NF_ACCEPT;
}

/*
 * FIXME Send close to multicast (or foreach client) and delete entry later...
 * Treat close from servers
 */
int gmtp_inter_close_out(struct sk_buff *skb)
{
	struct gmtp_hdr *gh = gmtp_hdr(skb);
	struct gmtp_relay_entry *entry;

	gmtp_pr_func();

	/**
	 * If destiny is not me, just let it go!
	 */
	entry = gmtp_inter_lookup_media(gmtp_inter.hashtable, gh->flowname);
	if(entry == NULL)
		return NF_ACCEPT;

	switch(entry->state) {
	case GMTP_INTER_TRANSMITTING:
		return NF_ACCEPT;
	case GMTP_INTER_CLOSED:
		pr_info("CLOSED\n");
		gmtp_inter_del_entry(gmtp_inter.hashtable, gh->flowname);
		return NF_ACCEPT;
	case GMTP_INTER_CLOSE_RECEIVED:
		pr_info("CLOSE_RECEIVED\n");
		entry->state = GMTP_INTER_CLOSED;
		break;
	}

	while(entry->info->buffer_len > 0) {
		struct sk_buff *buffered = gmtp_buffer_dequeue(entry->info);
		if(buffered == NULL) {
			gmtp_pr_error("Buffered is NULL...");
			return NF_ACCEPT;
		}
		buffered->dev = skb->dev;
		gmtp_inter_build_and_send_skb(buffered);
	}

	entry->state = GMTP_INTER_CLOSED;

	return NF_STOLEN;
}
