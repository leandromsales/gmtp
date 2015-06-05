/*
 * output.c
 *
 *  Created on: 27/02/2015
 *      Author: wendell
 */

#include <net/ip.h>

#include <uapi/linux/gmtp.h>
#include <linux/gmtp.h>
#include "../gmtp.h"

#include "gmtp-inter.h"
#include "mcc-inter.h"

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
	gh->relay = 1;
	gh->dport = entry->channel_port;

	/** FIXME We really trust in declared tx rate from server ? */
	gmtp_inter_mcc_delay(info, skb, (u64) gh->transm_r);

out:
	return NF_ACCEPT;
}

static void gmtp_intra_convert_to_close(struct sk_buff *skb)
{
	struct gmtp_hdr *gh = gmtp_hdr(skb);
	unsigned int skb_len = skb->len;

	skb_trim(skb, (skb_len - gmtp_packet_hdr_variable_len(gh->type)));

	gh->type = GMTP_PKT_CLOSE;
	gh->hdrlen = __gmtp_hdr_len(gh);
}

static int gmtp_inter_close_from_client(struct sk_buff *skb,
		struct gmtp_relay_entry *entry)
{
	struct iphdr *iph = ip_hdr(skb);
	struct gmtp_hdr *gh = gmtp_hdr(skb);
	struct gmtp_flow_info *info = entry->info;
	struct gmtp_hdr *gh_reset;
	unsigned int skb_len = skb->len;
	int del = 0;

	gmtp_pr_func();

	del = gmtp_delete_clients(&info->clients->list, iph->saddr, gh->sport);
	info->nclients -= del;

	gh_reset = gmtp_inter_make_reset_hdr(skb, GMTP_RESET_CODE_CLOSED);
	if(gh_reset == NULL)
		return NF_DROP;

	/*
	 * Here, we have no clients.
	 * So, we can send 'close' to server
	 * and send a 'reset' to client.
	 */
	if(info->nclients <= 0) {

		/* Build and send back 'reset' */
		pr_info("Sending RESET back to client");
		gmtp_inter_build_and_send_pkt(skb, iph->daddr, iph->saddr,
				gh_reset, true);

		/* Forwarding 'close' */
		if(gh->type == GMTP_PKT_RESET)
			gmtp_intra_convert_to_close(skb);
		gh->relay = 1;
		gh->sport = info->my_port;
		iph->saddr = info->my_addr;
		ip_send_check(iph);

		gmtp_inter_del_entry(gmtp_inter.hashtable, entry->flowname);

	} else {
		__u8 *transport_header;
		/* Only send 'reset' to clients, discarding 'close' */
		skb_trim(skb, (skb_len - sizeof(struct gmtp_hdr)));
		transport_header = skb_put(skb, gh_reset->hdrlen);
		skb_reset_transport_header(skb);
		memcpy(transport_header, gh_reset, gh_reset->hdrlen);

		gh = gmtp_hdr(skb);
		gh->relay = 1;
		iph = ip_hdr(skb);
		swap((iph->saddr), (iph->daddr));
		ip_send_check(iph);
	}

	gmtp_pr_debug("%s: src=%pI4@%-5d, dst=%pI4@%-5d",
			gmtp_packet_name(gh->type),
			&iph->saddr, ntohs(gh_reset->sport),
			&iph->daddr, ntohs(gh_reset->dport));

	return NF_ACCEPT;
}

/*
 * FIXME Send close to multicast (or foreach reporter) and delete entry later...
 */
int gmtp_inter_close_out(struct sk_buff *skb)
{
	struct gmtp_hdr *gh = gmtp_hdr(skb);
	struct gmtp_relay_entry *entry;

	gmtp_pr_func();

	entry = gmtp_inter_lookup_media(gmtp_inter.hashtable, gh->flowname);
	if(entry == NULL)
		return NF_ACCEPT;

	switch(entry->state) {
	case GMTP_INTER_TRANSMITTING:
		return gmtp_inter_close_from_client(skb, entry);
	case GMTP_INTER_CLOSED:
		gmtp_inter_del_entry(gmtp_inter.hashtable, gh->flowname);
		return NF_ACCEPT;
	case GMTP_INTER_CLOSE_RECEIVED:
		entry->state = GMTP_INTER_CLOSED;
		break;
	}

	while(entry->info->buffer_len > 0) {
		struct sk_buff *buffered = gmtp_buffer_dequeue(entry->info);
		if(buffered == NULL) {
			gmtp_pr_error("Buffered is NULL...");
			return NF_ACCEPT;
		}

		gmtp_hdr(buffered)->relay = 1;
		buffered->dev = skb->dev;
		gmtp_inter_build_and_send_skb(buffered);
	}

	entry->state = GMTP_INTER_CLOSED;

	return NF_STOLEN;
}
