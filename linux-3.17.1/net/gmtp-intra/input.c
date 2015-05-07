#include <net/ip.h>

#include <uapi/linux/gmtp.h>
#include <linux/gmtp.h>
#include "../gmtp/gmtp.h"

#include "gmtp-intra.h"
#include "mcc-intra.h"

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
 *        gmtp_intra_register_reply_received
 *        if state(P) != GMTP_INTRA_WAITING_REGISTER_REPLY:
 *            state(P) = GMTP_INTRA_WAITING_REGISTER_REPLY
 *            sendToServer(GMTP_PKT_REGISTER, P)
 *        else:
 *            // Ask client to wait registration reply for P
 *            respondToClients(GMTPRequestReply(P, "waiting_registration"));
 *        endif
 *        return 0
 *    endif
 *    if state(P) != GMTP_INTRA_WAITING_REGISTER_REPLY:
 *            state(P) = GMTP_INTRA_WAITING_REGISTER_REPLY
 *            sendToServer(GMTP_PKT_REGISTER)
 *    endif
 * endif
 * return 0
 * end
 */
int gmtp_intra_request_rcv(struct sk_buff *skb)
{
	int ret = NF_ACCEPT;
	struct iphdr *iph = ip_hdr(skb);
	struct gmtp_hdr *gh = gmtp_hdr(skb);
	struct gmtp_hdr *gh_reqnotify;
	struct gmtp_relay_entry *entry;

	__u8 code = GMTP_REQNOTIFY_CODE_ERROR;
	__u8 reporter = 0;

	gmtp_pr_func();

	entry = gmtp_intra_lookup_media(gmtp.hashtable, gh->flowname);
	if(entry != NULL) {
		gmtp_pr_info("Media found. Sending RequestNotify.");
		reporter = new_reporter(entry);
		switch(entry->state) {
		case GMTP_INTRA_WAITING_REGISTER_REPLY:
			code = reporter ?
					GMTP_REQNOTIFY_CODE_WAIT_REPORTER :
					GMTP_REQNOTIFY_CODE_WAIT;
			break;
		case GMTP_INTRA_REGISTER_REPLY_RECEIVED:
		case GMTP_INTRA_TRANSMITTING:
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
		int err = gmtp_intra_add_entry(gmtp.hashtable, gh->flowname,
				iph->daddr,
				NULL,
				gh->sport,
				mcst_addr,
				gh->dport); /* Mcst port <- server port */

		if(!err) {
			entry = gmtp_intra_lookup_media(gmtp.hashtable,
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
	gh_reqnotify = gmtp_intra_make_request_notify_hdr(skb, entry,
			gh->dport, gh->sport, code);

	if(gh_reqnotify != NULL)
		gmtp_intra_build_and_send_pkt(skb, iph->daddr,
				iph->saddr, gh_reqnotify, true);

	return ret;
}

/**
 * Algorithm 2:: onReceiveGMTPRegisterReply(p.type == GMTP-Register-Reply)
 *
 * The node r (relay) executes this algorithm when receives a packet of type
 * GMTP-Register-Reply, as response for a registration of participation
 * sent to a s(server) node.
 *
 * begin
 * state(P) = GMTP_INTRA_REGISTER_REPLY_RECEIVED
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
int gmtp_intra_register_reply_rcv(struct sk_buff *skb)
{
	int ret = NF_ACCEPT;
	struct gmtp_hdr *gh = gmtp_hdr(skb);
	struct iphdr *iph = ip_hdr(skb);
	struct gmtp_hdr *gh_rn;
	struct gmtp_relay_entry *data;
	/*unsigned int r;*/

	gmtp_print_function();

	/* Add relay information in REGISTER-REPLY packet) */
	gmtp_intra_add_relayid(skb);

	/* Update transmission rate (GMTP-UCC) */
	/* gmtp_print_debug("UPDATING Tx Rate");
	 gmtp_update_tx_rate(H_USER);
	 r = gmtp_get_current_tx_rate();
	 if(r < gh->transm_r)
	 gh->transm_r = r;
	 */

	/*
	 * FIXME
	 * If RegisterReply is destined others, just let it go...
	 */
	data = gmtp_intra_lookup_media(gmtp.hashtable, gh->flowname);
	pr_info("gmtp_lookup_media returned: %p\n", data);
	if(data == NULL)
		return NF_ACCEPT;

	/** Send ack back to server */
	gh_rn = gmtp_intra_make_route_hdr(skb);
	if(gh_rn != NULL)
		gmtp_intra_build_and_send_pkt(skb, iph->daddr, iph->saddr,
				gh_rn, true);

	data->state = GMTP_INTRA_REGISTER_REPLY_RECEIVED;

	/*
	 * FIXME
	 * (1) Send ReqNotify to every waiting client.
	 * (2) Clear waiting client list
	 *
	 * Here, the first client is a REPORTER
	 */
	ret = gmtp_intra_make_request_notify(skb,
			iph->saddr, gh->sport,
			iph->daddr, gh->dport,
			GMTP_REQNOTIFY_CODE_OK_REPORTER);

	return ret;
}

/* FIXME Treat acks from clients */
int gmtp_intra_ack_rcv(struct sk_buff *skb)
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

int gmtp_intra_feedback_rcv(struct sk_buff *skb)
{
	int ret = NF_DROP;

	struct gmtp_hdr *gh = gmtp_hdr(skb);
	struct gmtp_relay_entry *entry;

	entry = gmtp_intra_lookup_media(gmtp.hashtable, gh->flowname);
	if(entry == NULL)
		goto out;

	/*{
		struct iphdr *iph = ip_hdr(skb);
		gmtp_pr_info("%s (%d) src=%pI4@%-5d dst=%pI4@%-5d transm_r: %u",
				gmtp_packet_name(gh->type), gh->type,
				&iph->saddr, ntohs(gh->sport), &iph->daddr,
				ntohs(gh->dport), gh->transm_r);
	}*/

	/* Discard early feedbacks */
	if((entry->info->data_pkt_tx/entry->info->buffer_max) < 100)
		goto out;

	if(gh->transm_r > 0)
		entry->info->current_tx = (u64) gh->transm_r;

out:
	return ret;
}

/**
 * Update GMTP-Intra statistics
 */
static inline void gmtp_update_stats(struct gmtp_flow_info *info,
		struct sk_buff *skb, struct gmtp_hdr *gh)
{
	if(info->iseq == 0)
		info->iseq = gh->seq;
	if(gh->seq > info->seq)
		info->seq = gh->seq;
	info->nbytes += skb->len + ETH_HLEN;
	gmtp.total_bytes_rx += skb->len + ETH_HLEN;
}

/**
 * begin
 * P = p.flowname
 * If P != NULL:
 *  in:
 *     AddToBuffer(P)
 *  out:
 *     p.dest_address = get_multicast_channel(P)
 *     p.port = get_multicast_port(P)
 *     send(P)
 */
int gmtp_intra_data_rcv(struct sk_buff *skb)
{
	struct gmtp_hdr *gh = gmtp_hdr(skb);
	struct gmtp_relay_entry *entry;
	struct gmtp_flow_info *info;

	entry = gmtp_intra_lookup_media(gmtp.hashtable, gh->flowname);
	if(entry == NULL) {
		gmtp_pr_info("Failed to lookup media info in table...");
		goto out;
	}

	info = entry->info;
	if(entry->state == GMTP_INTRA_REGISTER_REPLY_RECEIVED)
		entry->state = GMTP_INTRA_TRANSMITTING;

	/* Do not add repeated packets
	 * Do not add packets if state != Transmitting
	 */
	if((gh->seq > info->seq) && entry->state == GMTP_INTRA_TRANSMITTING)
		gmtp_buffer_add(info, skb);

	gmtp_update_stats(info, skb, gh);

out:
	return NF_ACCEPT;
}
