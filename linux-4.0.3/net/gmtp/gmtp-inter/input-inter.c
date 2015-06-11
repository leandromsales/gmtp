#include <net/ip.h>

#include <uapi/linux/gmtp.h>
#include <linux/gmtp.h>
#include "../gmtp.h"

#include "gmtp-inter.h"
#include "mcc-inter.h"
#include "ucc.h"


/**
 * Algorithm 1: registerRelay
 *
 * The node r (relay) executes this algorithm to send a register of
 * participation to a  * given node s (server).
 * If p (packet) is given, node c wants to receive the flow P, so notify s.
 *
 * begin
 * P = p.flowname
 * If P != NULL:
 *    client = p.client (ip:port)
 *    Check if P is already being received (via reception table)
 *    // Add client to an waiting list
 *    addClientWaitingFlow(c, P)
 *    If multicast channel exists:
 *        respondToClients(GMTPRequestReply(channel))
 *        return 0
 *    else:
 *        Send request to server and wait registration reply.
 *        When GMTP-Register-Reply is received, executes
 *        gmtp_inter_register_reply_received
 *        if state(P) != GMTP_inter_WAITING_REGISTER_REPLY:
 *            state(P) = GMTP_inter_WAITING_REGISTER_REPLY
 *            sendToServer(GMTP_PKT_REGISTER, P)
 *        else:
 *            // Ask client to wait registration reply for P
 *            respondToClients(GMTPRequestReply(P, "waiting_registration"));
 *        endif
 *        return 0
 *    endif
 *    if state(P) != GMTP_inter_WAITING_REGISTER_REPLY:
 *            state(P) = GMTP_inter_WAITING_REGISTER_REPLY
 *            sendToServer(GMTP_PKT_REGISTER)
 *    endif
 * endif
 * return 0
 * end
 */
int gmtp_inter_request_rcv(struct sk_buff *skb)
{
	int ret = NF_ACCEPT;
	struct iphdr *iph = ip_hdr(skb);
	struct gmtp_hdr *gh = gmtp_hdr(skb);
	struct gmtp_hdr *gh_reqnotify;
	struct gmtp_relay_entry *entry;

	__u8 code = GMTP_REQNOTIFY_CODE_ERROR;
	__u8 reporter = 0;

	gmtp_pr_func();

	entry = gmtp_inter_lookup_media(gmtp_inter.hashtable, gh->flowname);
	if(entry != NULL) {
		gmtp_pr_info("Media found. Sending RequestNotify.");
		reporter = new_reporter(entry);
		switch(entry->state) {
		case GMTP_INTER_WAITING_REGISTER_REPLY:
			code = reporter ?
					GMTP_REQNOTIFY_CODE_WAIT_REPORTER :
					GMTP_REQNOTIFY_CODE_WAIT;
			break;
		case GMTP_INTER_REGISTER_REPLY_RECEIVED:
		case GMTP_INTER_TRANSMITTING:
			code = reporter ?
					GMTP_REQNOTIFY_CODE_OK_REPORTER:
					GMTP_REQNOTIFY_CODE_OK;
			ret = NF_DROP;
			break;
		default:
			gmtp_pr_error("Inconsistent state: %d", entry->state);
			ret = NF_DROP;
			goto out;
		}

	} else {
		__be32 mcst_addr = get_mcst_v4_addr();
		int err = gmtp_inter_add_entry(gmtp_inter.hashtable, gh->flowname,
				iph->daddr,
				NULL,
				gh->sport,
				mcst_addr,
				gh->dport); /* Mcst port <- server port */

		if(!err) {
			entry = gmtp_inter_lookup_media(gmtp_inter.hashtable,
					gh->flowname);
			reporter = new_reporter(entry);
			code = reporter ?
					GMTP_REQNOTIFY_CODE_WAIT_REPORTER :
					GMTP_REQNOTIFY_CODE_WAIT;
			gh->type = GMTP_PKT_REGISTER;
		} else {
			gmtp_pr_error("Failed to insert flow in table (%d)", err);
			ret = NF_DROP;
			goto out;
		}
	}

	gmtp_list_add_client(++entry->info->nclients, iph->saddr, gh->sport,
			reporter, &entry->info->clients->list);

out:
	gh_reqnotify = gmtp_inter_make_request_notify_hdr(skb, entry,
			gh->dport, gh->sport, code);

	if(gh_reqnotify != NULL)
		gmtp_inter_build_and_send_pkt(skb, iph->daddr,
				iph->saddr, gh_reqnotify, true);

	return ret;
}

/* A little trick...
 * Just get any client IP to pass over GMTP-Intra.
 */
static inline struct gmtp_client *jump_over_gmtp_intra(struct sk_buff *skb,
		struct list_head *list_head)
{
	struct iphdr *iph = ip_hdr(skb);
	struct gmtp_hdr *gh = gmtp_hdr(skb);
	struct gmtp_client *client = gmtp_get_first_client(list_head);

	gh->dport = client->port;
	iph->daddr = client->addr;
	ip_send_check(iph);
	return client;
}

/**
 * Algorithm 2:: onReceiveGMTPRegisterReply(p.type == GMTP-Register-Reply)
 *
 * The node r (relay) executes this algorithm when receives a packet of type
 * GMTP-Register-Reply, as response for a registration of participation
 * sent to a s(server) node.
 *
 * begin
 * state(P) = GMTP_inter_REGISTER_REPLY_RECEIVED
 * if (packet p is OK):
 *     W = p.way
 *     s = p.server (ip:port)
 *     P = p.flowname
 *     if P != NULL:
 *         if s enabled security layer:
 *             getAndStoreServerPublicKey(s)
 *         endif
 *         channel = createMulticastChannel(s, P)
 *         Update flow reception table with channel
 *         respondToClients(GMTPRequestReply(channel))
 *         startRelay(channel);
 *      endif
 *      updateFlowReceptionTable(s)
 *      Send ack to server (ack.w = W) //Send way back to server
 * else
 *      // s refused to accept the registration of participation.
 *      errorCode = p.error
 *      respondToClients(GMTPRequestReply(errorCode, P))
 * endif
 * end
 */
int gmtp_inter_register_reply_rcv(struct sk_buff *skb)
{
	int ret = NF_ACCEPT;
	struct gmtp_hdr *gh = gmtp_hdr(skb);
	struct iphdr *iph = ip_hdr(skb);
	struct gmtp_hdr *gh_route_n;
	struct gmtp_relay_entry *entry;
	struct gmtp_hdr *gh_req_n;
	struct gmtp_client *client, *first_client;
	__u8 code = GMTP_REQNOTIFY_CODE_OK;

	gmtp_print_function();

	/* Add relay information in REGISTER-REPLY packet) */
	gmtp_inter_add_relayid(skb);

	gmtp_print_debug("UPDATING Tx Rate");
	gmtp_ucc(UINT_MAX);
	if(gmtp_inter.ucc_rx < gh->transm_r)
		gh->transm_r = (__be32) gmtp_inter.ucc_rx;

	entry = gmtp_inter_lookup_media(gmtp_inter.hashtable, gh->flowname);
	if(entry == NULL)
		return NF_ACCEPT;

	gh_route_n = gmtp_inter_make_route_hdr(skb);
	if(gh_route_n != NULL)
		gmtp_inter_build_and_send_pkt(skb, iph->daddr, iph->saddr,
				gh_route_n, true);

	/* Convert packet to RequestNotify and send it to all clients */
	ret = gmtp_inter_make_request_notify(skb, iph->saddr, gh->sport,
			iph->daddr, gh->dport, GMTP_REQNOTIFY_CODE_OK_REPORTER);
	entry->state = GMTP_INTER_REGISTER_REPLY_RECEIVED;

	/* Send to first client */
	first_client = jump_over_gmtp_intra(skb, &entry->info->clients->list);

	/* Send to others */
	list_for_each_entry(client, &entry->info->clients->list, list)
	{
		struct sk_buff *copy = skb_copy(skb, gfp_any());
		if(client == first_client)
			continue;

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

/* FIXME Treat acks from clients */
int gmtp_inter_ack_rcv(struct sk_buff *skb)
{
	int ret = NF_DROP;

	struct gmtp_hdr *gh = gmtp_hdr(skb);
	struct iphdr *iph = ip_hdr(skb);

	gmtp_print_function();

	gmtp_pr_info("%s (%d) src=%pI4@%-5d dst=%pI4@%-5d transm_r: %u",
			gmtp_packet_name(gh->type), gh->type,
			&iph->saddr, ntohs(gh->sport),
			&iph->daddr, ntohs(gh->dport),
			gh->transm_r);

	return ret;
}

int gmtp_inter_feedback_rcv(struct sk_buff *skb)
{
	int ret = NF_DROP;

	struct gmtp_hdr *gh = gmtp_hdr(skb);
	struct gmtp_relay_entry *entry;


	entry = gmtp_inter_lookup_media(gmtp_inter.hashtable, gh->flowname);
	if(entry == NULL)
		goto out;

	/* Discard early feedbacks */
	if((entry->info->data_pkt_out/entry->info->buffer_min) < 100)
		goto out;

	if(gh->transm_r > 0)
		entry->info->required_tx = (unsigned int) gh->transm_r;

out:
	return ret;
}

#define skblen(skb) ((*skb).len) + ETH_HLEN

/**
 * Update GMTP-inter statistics
 */
static inline void gmtp_update_stats(struct gmtp_flow_info *info,
		struct sk_buff *skb, struct gmtp_hdr *gh)
{
	info->total_bytes += skblen(skb);
	info->recent_bytes += skblen(skb);
	info->seq = (unsigned int) gh->seq;

	if(gh->seq % gmtp_inter.rx_rate_wnd == 0) {
		unsigned long current_time = ktime_to_ms(ktime_get_real());
		unsigned long elapsed = current_time - info->recent_rx_tstamp;
		if(elapsed != 0)
			info->current_rx = DIV_ROUND_CLOSEST(
					info->recent_bytes * MSEC_PER_SEC,
					elapsed);

		info->recent_rx_tstamp = ktime_to_ms(skb->tstamp);
		info->recent_bytes = 0;

		pr_info("Flow RX: %u bytes/s\n", info->current_rx);
	}

	gmtp_inter.total_bytes_rx += skblen(skb);
	gmtp_inter.ucc_bytes += skblen(skb);
}

/**
 * P = p.flowname
 * If P != NULL:
 *  in:
 *     AddToBuffer(P)
 *  out:
 *     p.dest_address = get_multicast_channel(P)
 *     p.port = get_multicast_port(P)
 *     send(P)
 */
int gmtp_inter_data_rcv(struct sk_buff *skb)
{
	struct gmtp_hdr *gh = gmtp_hdr(skb);
	struct gmtp_relay_entry *entry;
	struct gmtp_flow_info *info;

	entry = gmtp_inter_lookup_media(gmtp_inter.hashtable, gh->flowname);
	if(entry == NULL) /* Failed to lookup media info in table... */
		return NF_ACCEPT;

	if(entry->state == GMTP_INTER_REGISTER_REPLY_RECEIVED)
		entry->state = GMTP_INTER_TRANSMITTING;

	info = entry->info;
	if(info->buffer->qlen >= info->buffer_max)
		goto out; /* Dont add it to buffer (equivalent to drop) */

	jump_over_gmtp_intra(skb, &info->clients->list);
	if((gh->seq > info->seq) && entry->state == GMTP_INTER_TRANSMITTING)
		gmtp_buffer_add(info, skb);

	gmtp_update_stats(info, skb, gh);

	if(gh->seq % 1000 == 0)
		gmtp_ucc(UINT_MAX);

out:
	return NF_ACCEPT;
}

int gmtp_inter_close_rcv(struct sk_buff *skb)
{
	struct gmtp_hdr *gh = gmtp_hdr(skb);
	struct iphdr *iph = ip_hdr(skb);
	struct gmtp_relay_entry *entry;
	struct gmtp_flow_info *info;

	struct gmtp_client *client;

	gmtp_pr_func();

	entry = gmtp_inter_lookup_media(gmtp_inter.hashtable, gh->flowname);
	if(entry == NULL)
		goto out;
	info = entry->info;

	if(iph->saddr == entry->server_addr) {
		if(entry->state == GMTP_INTER_TRANSMITTING) {
			entry->state = GMTP_INTER_CLOSE_RECEIVED;

			jump_over_gmtp_intra(skb, &info->clients->list);
			list_for_each_entry(client, &info->clients->list, list)
			{
				struct sk_buff *copy = skb_copy(skb, gfp_any());
				if(copy != NULL) {
					struct iphdr *iph_copy = ip_hdr(copy);
					struct gmtp_hdr *gh_copy = gmtp_hdr(
							copy);

					iph_copy->daddr = client->addr;
					ip_send_check(iph_copy);
					gh_copy->dport = client->port;

					gmtp_buffer_add(info, copy);
				}
			}
		}
	}

out:
	return NF_ACCEPT;
}
