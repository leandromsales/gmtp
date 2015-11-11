/*
 * output.c
 *
 *  Created on: 27/02/2015
 *      Author: wendell
 */

#include <net/ip.h>
#include <linux/phy.h>
#include <linux/etherdevice.h>
#include <linux/list.h>

#include <uapi/linux/gmtp.h>
#include <linux/gmtp.h>
#include "../gmtp.h"

#include "gmtp-inter.h"
#include "mcc-inter.h"

int gmtp_inter_register_out(struct sk_buff *skb, struct gmtp_inter_entry *entry)
{
	struct iphdr *iph = ip_hdr(skb);
	struct gmtp_hdr *gh = gmtp_hdr(skb);
	struct gmtp_client* cl;

	cl = gmtp_get_client(&entry->clients->list, iph->saddr, gh->sport);
	if(cl == NULL)
		return NF_ACCEPT;

	if(entry->state != GMTP_INTER_WAITING_REGISTER_REPLY)
		return NF_DROP;

	/* FIXME Get a valid and unused port */
	entry->my_addr = gmtp_inter_device_ip(skb->dev);
	ether_addr_copy(entry->request_mac_addr, skb->dev->dev_addr);

	gh->sport = entry->my_port;
	iph->saddr = entry->my_addr;
	pr_info("My addr: %pI4\n", &entry->my_addr);
	iph->ttl = 64;
	ip_send_check(iph);

	print_packet(skb, false);
	print_gmtp_packet(iph, gh);

	return NF_ACCEPT;
}

/** XXX HOOK LOCAL OUT Does not works for RegisterReply (skb->dev == NULL) */
int gmtp_inter_register_reply_out(struct sk_buff *skb,
		struct gmtp_inter_entry *entry)
{
	struct iphdr *iph = ip_hdr(skb);
	struct gmtp_hdr *gh = gmtp_hdr(skb);

	gmtp_pr_func();

	if(gmtp_inter_ip_local(iph->saddr)) {
		return gmtp_inter_register_reply_rcv(skb, entry,
				GMTP_INTER_LOCAL);
	}

	print_packet(skb, false);
	print_gmtp_packet(iph, gh);

	return NF_ACCEPT;
}

/* In some cases, turn ACK into a DELEGATE_REPLY */
int gmtp_inter_ack_out(struct sk_buff *skb, struct gmtp_inter_entry *entry)
{
	struct gmtp_hdr *gh = gmtp_hdr(skb);
	struct iphdr *iph = ip_hdr(skb);
	struct gmtp_relay *relay;

	pr_info("Out: Ack from %pI4\n", &iph->saddr);

	relay = gmtp_get_relay(&entry->relays->list, iph->saddr, gh->sport);
	if(relay != NULL) {
		pr_info("relay: %pI4 (%s)\n", &relay->addr,
				gmtp_state_name(relay->state));

		if(relay->state == GMTP_CLOSED) {
			gmtp_inter_make_delegate_reply(skb, relay, entry);
			relay->state = GMTP_OPEN;
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

	/* This way does not work... */
	/*skb_trim(skb, data_len);
	skb_put(skb, data_src_len);*/

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
int gmtp_inter_data_out(struct sk_buff *skb, struct gmtp_inter_entry *entry)
{
	struct gmtp_hdr *gh = gmtp_hdr(skb);
	struct gmtp_hdr_data *ghd = gmtp_hdr_data(skb);
	struct iphdr *iph = ip_hdr(skb);

	unsigned int server_tx;
	struct gmtp_relay *relay, *temp;

	if(unlikely(entry->state == GMTP_INTER_REGISTER_REPLY_RECEIVED))
		entry->state = GMTP_INTER_TRANSMITTING;

	/** TODO Verify close from server... */
	if(entry->state != GMTP_INTER_TRANSMITTING)
		return NF_DROP;

	if(gmtp_inter_ip_local(iph->saddr))
		goto send;

	/*if(entry->buffer->qlen > entry->buffer_min) {
		struct sk_buff *buffered = gmtp_buffer_dequeue(entry);
		if(buffered != NULL) {
			entry->data_pkt_out++;
			skb = skb_copy(buffered, gfp_any());
		}
	} else {
		return NF_DROP;
	}*/

send:
	entry->data_pkt_out++;
	GMTP_SKB_CB(skb)->jumped = 0;
	list_for_each_entry_safe(relay, temp, &entry->relays->list, list)
	{
		if(relay->state == GMTP_OPEN) {
		/*	struct ethhdr *eth = eth_hdr(skb);
			gh->dport = relay->port;
			ether_addr_copy(eth->h_dest, relay->mac_addr);
			skb->dev = relay->dev;

			pr_info("Sending to %pI4:%d (%u)\n", &relay->addr,
					htons(relay->port), gh->seq);
			entry->ucc.congestion_control(skb, entry, relay);*/

			struct sk_buff *buffered = skb_copy(skb, gfp_any());
			struct ethhdr *eth = eth_hdr(buffered);
			struct gmtp_hdr *buffgh = gmtp_hdr(buffered);

			buffgh->dport = relay->port;
			ether_addr_copy(eth->h_dest, relay->mac_addr);
			buffered->dev = relay->dev;

			pr_info("Sending to %pI4:%d (%u)\n", &relay->addr,
								htons(relay->port),
								buffgh->seq);
			entry->ucc.congestion_control(buffered, entry, relay);

			/*gmtp_inter_build_and_send_pkt(skb, iph->saddr,
					relay->addr, gh, GMTP_INTER_FORWARD);*/
		}
	}

	iph->daddr = entry->channel_addr;
	ip_send_check(iph);
	gh->relay = 1;
	gh->dport = entry->channel_port;
	gh->server_rtt = entry->server_rtt + entry->clients_rtt;

	if(entry->nclients > 0) {
		server_tx = entry->current_rx <= 0 ?
				(unsigned int)gh->transm_r : entry->current_rx;
		gmtp_inter_mcc_delay(entry, skb, (u64)server_tx);
	}
	ghd->tstamp = jiffies_to_msecs(jiffies);

	return NF_ACCEPT;
}

static int gmtp_inter_close_from_client(struct sk_buff *skb,
		struct gmtp_inter_entry *entry)
{
	struct iphdr *iph = ip_hdr(skb);
	struct gmtp_hdr *gh = gmtp_hdr(skb);
	struct gmtp_hdr *gh_reset;
	unsigned int skb_len = skb->len;
	__u8 *transport_header = NULL;
	int del = 0;

	gmtp_pr_func();

	pr_info("entry->nclients: %u\n", entry->nclients);
	del = gmtp_delete_clients(&entry->clients->list, iph->saddr, gh->sport);
	if(del == 0)
		return NF_ACCEPT;
	pr_info("del: %d\n", del);
	entry->nclients -= del;
	pr_info("entry->nclients - del: %u\n", entry->nclients);

	gh_reset = gmtp_inter_make_reset_hdr(skb, GMTP_RESET_CODE_CLOSED);
	if(gh_reset == NULL)
		return NF_DROP;

	/*
	 * Here, we have no clients.
	 * So, we can send 'close' to server
	 * and send a 'reset' to client.
	 */
	if(entry->nclients <= 0) {

		/* Build and send back 'reset' */
		pr_info("FIXME: Sending RESET back to client");
		/*gmtp_inter_build_and_send_pkt(skb, iph->daddr, iph->saddr,
				gh_reset, GMTP_INTER_BACKWARD);*/

		/* FIXME Transform 'reset' into 'close' before forwarding */
		/*if(gh->type == GMTP_PKT_RESET) {
			gh_close = gmtp_inter_make_close_hdr(skb);
			skb_trim(skb, (skb_len - gh->hdrlen));
			skb_reset_transport_header(skb);
			pr_info("gh: %p\n", gh);
			pr_info("gh->hdrlen: %u\n", gh->hdrlen);
			skb_put(skb, gh_close->hdrlen);
			gh = gmtp_hdr(skb);
			memcpy(gh, gh_close, gh_close->hdrlen);
			iph = ip_hdr(skb);
			iph->tot_len = ntohs(skb->len);
		}*/
		gh->relay = 1;
		gh->sport = entry->my_port;
		iph->saddr = entry->my_addr;
		ip_send_check(iph);

		gmtp_inter_del_entry(gmtp_inter.hashtable, entry->flowname);

	} else {
		/* Some clients still alive.
		 * Only send 'reset' to clients, discarding 'close'
		 * */
		pr_info("Some clients still alive.\n");
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

	print_gmtp_packet(iph, gh);

	return NF_ACCEPT;
}

int gmtp_inter_close_out(struct sk_buff *skb, struct gmtp_inter_entry *entry)
{
	struct iphdr *iph = ip_hdr(skb);
	struct gmtp_hdr *gh = gmtp_hdr(skb);
	struct gmtp_relay *relay, *temp;

	gmtp_pr_func();
	print_packet(skb, false);
	print_gmtp_packet(iph, gh);
	gmtp_pr_info("State: %u", entry->state);

	switch(entry->state) {
	case GMTP_INTER_TRANSMITTING:
		return gmtp_inter_close_from_client(skb, entry);
	case GMTP_INTER_CLOSED:
		pr_info("GMTP_CLOSED\n");
		list_for_each_entry_safe(relay, temp,
				&entry->relays->list, list)
		{
			/* FIXME Wait send all data pending */
			pr_info("FORWARDING close to %pI4:%d\n",
					&relay->addr,
					ntohs(relay->port));
			if(relay->state == GMTP_OPEN) {
				struct sk_buff *new_skb = skb_copy(skb, gfp_any());
				struct ethhdr *eth = eth_hdr(new_skb);
				gh->dport = relay->port;
				ether_addr_copy(eth->h_dest, relay->mac_addr);
				skb->dev = relay->dev;
				gmtp_inter_build_and_send_pkt(new_skb, iph->saddr,
						relay->addr, gh,
						GMTP_INTER_FORWARD);
				relay->state = GMTP_CLOSED;
			}
		}

		gh->relay = 1;
		gh->dport = entry->channel_port;
		iph->daddr = entry->channel_addr;
		ip_send_check(iph);

		server_hashtable->hash_ops.del_entry(server_hashtable,
				gh->flowname);
		gmtp_inter_del_entry(gmtp_inter.hashtable, gh->flowname);

		return NF_ACCEPT;

	case GMTP_INTER_CLOSE_RECEIVED:
		pr_info("GMTP_INTER_CLOSE_RECEIVED -> GMTP_INTER_CLOSED\n");
		entry->state = GMTP_INTER_CLOSED;
		pr_info("nclients = %d\n", entry->nclients);
		if(entry->nclients == 0)
			return NF_REPEAT;
		break;
	}

	while(entry->buffer->qlen > 0) {
		struct sk_buff *buffered = gmtp_buffer_dequeue(entry);
		if(buffered == NULL) {
			gmtp_pr_error("Buffered is NULL...");
			return NF_ACCEPT;
		}

		gmtp_hdr(buffered)->relay = 1;

		if(iph->saddr == entry->my_addr) {
			skb->dev = entry->dev_out;
			gmtp_inter_build_and_send_skb(buffered, GMTP_INTER_LOCAL);
		} else {
			buffered->dev = skb->dev;
			gmtp_inter_build_and_send_skb(buffered, GMTP_INTER_FORWARD);
			return NF_REPEAT;
		}
	}

	entry->state = GMTP_INTER_CLOSED;

	return NF_STOLEN;
}
