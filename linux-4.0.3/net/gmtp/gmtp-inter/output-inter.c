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

static int gmtp_inter_close_from_client(struct sk_buff *skb,
		struct gmtp_relay_entry *entry)
{
	struct iphdr *iph = ip_hdr(skb);
	struct gmtp_hdr *gh = gmtp_hdr(skb);
	struct gmtp_flow_info *info = entry->info;

	struct gmtp_client *client, *temp;

	gmtp_pr_func();

	pr_info("N Clients: %u\n", info->nclients);


	list_for_each_entry_safe(client, temp, &info->clients->list, list)
	{
		if(iph->saddr == client->addr && gh->sport == client->port) {
			pr_info("Deleting client: %pI4@%-5d\n", &client->addr,
					ntohs(client->port));
			info->nclients--;
			list_del(&client->list);
			kfree(client);
		}
	}


	if(info->nclients == 0) {
		pr_info("0 Clients...\n");

		gh->sport = info->my_port;
		iph->saddr = info->my_addr;
		ip_send_check(iph);

		/*pr_info("Calling 'gmtp_inter_build_and_send_pkt'\n");
		gmtp_inter_build_and_send_pkt(skb, info->my_addr, iph->daddr,
				gh, false);*/

		gmtp_inter_del_entry(gmtp_inter.hashtable, entry->flowname);

		pr_info("Going to ACCEPT:\n");
		print_gmtp_packet(iph, gh);

		return NF_ACCEPT;
	}
	return NF_DROP;
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

	entry = gmtp_inter_lookup_media(gmtp_inter.hashtable, gh->flowname);
	if(entry == NULL)
		return NF_ACCEPT;

	switch(entry->state) {
	case GMTP_INTER_TRANSMITTING:
		return gmtp_inter_close_from_client(skb, entry);
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
