#include <net/ip.h>
#include <asm-generic/unaligned.h>
#include <linux/etherdevice.h>

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
	struct gmtp_inter_entry *entry;

	__u8 code = GMTP_REQNOTIFY_CODE_ERROR;
	__u8 max_nclients = 0;

	gmtp_pr_func();

	entry = gmtp_inter_lookup_media(gmtp_inter.hashtable, gh->flowname);
	if(entry != NULL) {
		gmtp_pr_info("Media found. Sending RequestNotify.");
		max_nclients = new_reporter(entry);
		switch(entry->state) {
		case GMTP_INTER_WAITING_REGISTER_REPLY:
			code = 	GMTP_REQNOTIFY_CODE_WAIT;
			gh->type = GMTP_PKT_REGISTER;
			iph->ttl = 64;
			ip_send_check(iph);
			break;
		case GMTP_INTER_REGISTER_REPLY_RECEIVED:
		case GMTP_INTER_TRANSMITTING:
			code = GMTP_REQNOTIFY_CODE_OK;
			ret = NF_DROP;
			break;
		default:
			gmtp_pr_error("Inconsistent state: %d", entry->state);
			ret = NF_DROP;
			goto out;
		}

	} else {
		__be32 mcst_addr = get_mcst_v4_addr();
		int err = gmtp_inter_add_entry(gmtp_inter.hashtable,
				gh->flowname,
				iph->daddr,
				NULL,
				gh->dport, /* Media port */
				mcst_addr,
				gh->dport); /* Mcst port <- server port */

		if(!err) {
			entry = gmtp_inter_lookup_media(gmtp_inter.hashtable,
					gh->flowname);
			max_nclients = new_reporter(entry);
			code = GMTP_REQNOTIFY_CODE_WAIT;

			gh->type = GMTP_PKT_REGISTER;
			iph->ttl = 64;
			ip_send_check(iph);
		} else {
			gmtp_pr_error("Failed to insert flow in table (%d)", err);
			ret = NF_DROP;
			goto out;
		}
	}

	if(max_nclients > 0)
		entry->info->cur_reporter = gmtp_list_add_client(
				++entry->info->nclients, iph->saddr, gh->sport,
				max_nclients, &entry->info->clients->list);
	else
		gmtp_list_add_client(++entry->info->nclients, iph->saddr,
				gh->sport, max_nclients,
				&entry->info->clients->list);

out:
	gh_reqnotify = gmtp_inter_make_request_notify_hdr(skb, entry,
			gh->dport, gh->sport, entry->info->cur_reporter,
			max_nclients, code);

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

	if(client != NULL) {
		gh->dport = client->port;
		iph->daddr = client->addr;
		ip_send_check(iph);
	} else
		pr_info("There are no clients anymore!\n");
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
	struct ethhdr *eth = eth_hdr(skb);
	struct gmtp_hdr *gh_route_n;
	struct gmtp_inter_entry *entry;
	struct gmtp_flow_info *info;
	struct gmtp_hdr *gh_req_n;
	struct gmtp_client *client, *temp, *first_client, *cur_reporter = NULL;
	__u8 code = GMTP_REQNOTIFY_CODE_OK;

	gmtp_print_function();

	/* Add relay information in REGISTER-REPLY packet) */
	gmtp_inter_add_relayid(skb);

	gmtp_print_debug("UPDATING Tx Rate");
	gmtp_inter.last_rtt = (unsigned int) gh->server_rtt;
	gmtp_inter.avg_rtt = rtt_ewma(gmtp_inter.avg_rtt, gmtp_inter.last_rtt,
			GMTP_RTT_WEIGHT);

	pr_info("Server RTT: %u ms\n", (unsigned int) gh->server_rtt);
	pr_info("Last RTT: %u ms\n", gmtp_inter.last_rtt);
	pr_info("RTT AVG: %u ms\n", gmtp_inter.avg_rtt);

	if(gmtp_inter.ucc_rx < gh->transm_r)
		gh->transm_r = (__be32) gmtp_inter.ucc_rx;

	entry = gmtp_inter_lookup_media(gmtp_inter.hashtable, gh->flowname);
	if(entry == NULL)
		return NF_ACCEPT;

	if(entry->state != GMTP_INTER_WAITING_REGISTER_REPLY)
		return NF_ACCEPT;

	print_packet(skb, true);
	print_gmtp_packet(iph, gh);

	info = entry->info;
	info->rcv_tx_rate = gh->transm_r;
	info->flow_rtt = (unsigned int) gh->server_rtt;
	info->flow_avg_rtt = rtt_ewma(info->flow_avg_rtt, info->flow_rtt,
			GMTP_RTT_WEIGHT);

	ether_addr_copy(entry->server_mac_addr, eth->h_source);

	gh_route_n = gmtp_inter_make_route_hdr(skb);
	if(gh_route_n != NULL)
		gmtp_inter_build_and_send_pkt(skb, iph->daddr, iph->saddr,
				gh_route_n, true);

	entry->state = GMTP_INTER_REGISTER_REPLY_RECEIVED;

	/* Send it to first client */
	first_client = gmtp_get_first_client(&info->clients->list);
	if(first_client->max_nclients > 0)
		cur_reporter = first_client;
	ret = gmtp_inter_make_request_notify(skb, iph->saddr, gh->sport,
			first_client->addr, first_client->port, cur_reporter,
			first_client->max_nclients, code);

	/* Clean list of clients and keep only reporters */
	list_for_each_entry_safe(client, temp, &info->clients->list, list)
	{
		struct sk_buff *copy = skb_copy(skb, gfp_any());

		/* Send to other clients (not first) */
		if(client == first_client)
			continue;

		if(client->max_nclients > 0)
			cur_reporter = client;

		if(copy != NULL) {
			struct iphdr *iph_copy = ip_hdr(copy);
			struct gmtp_hdr *gh_copy = gmtp_hdr(copy);

			iph_copy->daddr = client->addr;
			ip_send_check(iph_copy);
			gh_copy->dport = client->port;

			gh_req_n = gmtp_inter_make_request_notify_hdr(copy,
					entry, gh->sport, client->port,
					cur_reporter, client->max_nclients,
					code);
			if(gh_req_n != NULL)
				gmtp_inter_build_and_send_pkt(skb, iph->saddr,
						client->addr, gh_req_n, false);
		}

		/** Deleting non-reporters */
		if(client->max_nclients <= 0) {
			list_del(&client->list);
			kfree(client);
		}
	}

	return NF_ACCEPT;
}

/* Treat acks from clients */
int gmtp_inter_ack_rcv(struct sk_buff *skb)
{
	struct gmtp_hdr *gh = gmtp_hdr(skb);
	struct iphdr *iph = ip_hdr(skb);
	struct gmtp_inter_entry *entry;
	struct gmtp_flow_info *info;
	struct gmtp_client *reporter;

	entry = gmtp_inter_lookup_media(gmtp_inter.hashtable, gh->flowname);
	if(entry == NULL)
		return NF_ACCEPT;

	gmtp_print_function();
	print_packet(skb, true);
	print_gmtp_packet(iph, gh);

	info = entry->info;
	reporter = gmtp_get_client(&info->clients->list, iph->saddr, gh->sport);
	gmtp_pr_debug("Reporter: %pI4:%d", &iph->saddr, gh->sport);
	if(reporter != NULL) {
		pr_info("Reporter is not NULL!\n");
		reporter->ack_rx_tstamp = jiffies_to_msecs(jiffies);
	}

	return NF_DROP;
}

/**
 * FIXME Take the average of feedbacks in an window
 */
int gmtp_inter_feedback_rcv(struct sk_buff *skb)
{
	struct gmtp_hdr *gh = gmtp_hdr(skb);
	struct iphdr *iph = ip_hdr(skb);
	struct gmtp_inter_entry *entry;
	struct gmtp_flow_info *info;
	struct gmtp_client *reporter;

	entry = gmtp_inter_lookup_media(gmtp_inter.hashtable, gh->flowname);
	if(entry == NULL)
		return NF_ACCEPT;
	info = entry->info;

	/*print_gmtp_packet(iph, gh);*/

	reporter = gmtp_get_client(&info->clients->list, iph->saddr, gh->sport);
	if(reporter == NULL)
		return NF_DROP;

	if(likely(gh->transm_r > 0)) {
		info->nfeedbacks++;
		info->sum_feedbacks += (unsigned int) gh->transm_r;
		reporter->ack_rx_tstamp = jiffies_to_msecs(jiffies);
	}

	return NF_DROP;
}

/**
 * FIXME This works only with auto promoted reporters
 */
int gmtp_inter_elect_resp_rcv(struct sk_buff *skb)
{
	struct iphdr *iph = ip_hdr(skb);
	struct gmtp_hdr *gh = gmtp_hdr(skb);
	struct gmtp_inter_entry *entry;

	entry = gmtp_inter_lookup_media(gmtp_inter.hashtable, gh->flowname);
	if(entry == NULL)
		return NF_ACCEPT;

	print_packet(skb, true);
	print_gmtp_packet(iph, gh);

	gmtp_list_add_client(++entry->info->nclients, iph->saddr,
					gh->sport, gmtp_inter.kreporter,
					&entry->info->clients->list);

	return NF_DROP;
}

#define skblen(skb) ((*skb).len) + ETH_HLEN

/**
 * Update GMTP-inter statistics
 */
static inline void gmtp_update_stats(struct gmtp_flow_info *info,
		struct sk_buff *skb, struct gmtp_hdr *gh)
{
	gmtp_inter.total_bytes_rx += skblen(skb);
	gmtp_inter.ucc_bytes += skblen(skb);
	gmtp_inter.last_rtt = (unsigned int)gh->server_rtt;
	gmtp_inter.avg_rtt = rtt_ewma(gmtp_inter.avg_rtt, gmtp_inter.last_rtt,
	GMTP_RTT_WEIGHT);

	info->total_bytes += skblen(skb);
	info->recent_bytes += skblen(skb);
	info->seq = (unsigned int)gh->seq;
	info->flow_rtt = (unsigned int)gh->server_rtt;
	info->flow_avg_rtt = rtt_ewma(info->flow_avg_rtt, info->flow_rtt,
	GMTP_RTT_WEIGHT);
	info->last_data_tstamp = gmtp_hdr_data(skb)->tstamp;

	info->rcv_tx_rate = gh->transm_r;
	gh->transm_r = min(info->rcv_tx_rate, gmtp_inter.ucc_rx);

	if(gh->seq % gmtp_inter.rx_rate_wnd == 0) {
		unsigned long current_time = ktime_to_ms(ktime_get_real());
		unsigned long elapsed = current_time - info->recent_rx_tstamp;
		if(elapsed != 0)
			info->current_rx = DIV_ROUND_CLOSEST(
					info->recent_bytes * MSEC_PER_SEC,
					elapsed);

		info->recent_rx_tstamp = ktime_to_ms(skb->tstamp);
		info->recent_bytes = 0;
		pr_info("Last RTT: %u ms\n", (unsigned int) gh->server_rtt);
		pr_info("Last RTT: %u ms\n", gmtp_inter.last_rtt);
		pr_info("RTT AVG: %u ms\n", gmtp_inter.avg_rtt);
	}
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
	struct gmtp_inter_entry *entry;
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

out:
	return NF_ACCEPT;
}

int gmtp_inter_close_rcv(struct sk_buff *skb)
{
	struct gmtp_hdr *gh = gmtp_hdr(skb);
	struct iphdr *iph = ip_hdr(skb);
	struct gmtp_inter_entry *entry;

	entry = gmtp_inter_lookup_media(gmtp_inter.hashtable, gh->flowname);
	if(entry == NULL)
		return NF_ACCEPT;

	gmtp_pr_func();
	print_packet(skb, true);
	print_gmtp_packet(iph, gh);

	if(iph->saddr != entry->server_addr
			|| entry->state == GMTP_INTER_CLOSED)
		return NF_ACCEPT;


	if(entry->state == GMTP_INTER_TRANSMITTING) {
		struct gmtp_hdr *gh_reset;
		entry->state = GMTP_INTER_CLOSE_RECEIVED;

		gh_reset = gmtp_inter_make_reset_hdr(skb,
				GMTP_RESET_CODE_CLOSED);
		if(gh_reset != NULL) {

			/* FIXME Crashing ns-3 */
			gmtp_inter_build_and_send_pkt(skb, iph->daddr,
					iph->saddr, gh_reset, true);
			gmtp_pr_debug("Reset: src=%pI4@%-5d, dst=%pI4@%-5d",
					&iph->daddr, ntohs(gh_reset->sport),
					&iph->saddr, ntohs(gh_reset->dport));
		}

		jump_over_gmtp_intra(skb, &entry->info->clients->list);
		print_gmtp_packet(iph, gh);
		gmtp_buffer_add(entry->info, skb);
	}

	return NF_ACCEPT;
}
