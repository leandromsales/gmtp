#include <net/ip.h>

#include <uapi/linux/gmtp.h>
#include <linux/gmtp.h>
#include "../gmtp/gmtp.h"

#include "gmtp-intra.h"


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
	int err = 0;
	struct iphdr *iph = ip_hdr(skb);
	struct gmtp_hdr *gh = gmtp_hdr(skb);
	struct gmtp_hdr *gh_reqnotify;
	struct gmtp_relay_entry *media_info;
	__be32 mcst_addr;

	gmtp_print_function();

	/*set_current_state(TASK_INTERRUPTIBLE);
	schedule_timeout (3*HZ);*/

	media_info = gmtp_intra_lookup_media(relay_hashtable, gh->flowname);
	if(media_info != NULL) {

		gmtp_print_debug("Media found... ");

		switch(media_info->state) {
		case GMTP_INTRA_WAITING_REGISTER_REPLY:
			gh_reqnotify = gmtp_intra_make_request_notify_hdr(skb,
					media_info, gh->dport, gh->sport,
					GMTP_REQNOTIFY_CODE_WAIT);
			break;
		case GMTP_INTRA_REGISTER_REPLY_RECEIVED:
			gh_reqnotify = gmtp_intra_make_request_notify_hdr(skb,
					media_info, gh->dport, gh->sport,
					GMTP_REQNOTIFY_CODE_OK);
			ret = NF_DROP;
			break;
		default:
			gmtp_print_error("Inconsistent state at gmtp-intra: %d",
					media_info->state);

			gh_reqnotify = gmtp_intra_make_request_notify_hdr(skb,
					media_info, gh->dport, gh->sport,
					GMTP_REQNOTIFY_CODE_ERROR);

			ret = NF_DROP;
		}

	} else {
		gmtp_print_debug("Adding new entry in Relay Table...");
		mcst_addr = get_mcst_v4_addr();
		err = gmtp_intra_add_entry(relay_hashtable, gh->flowname,
				iph->daddr,
				NULL,
				gh->sport,
				mcst_addr,
				gh->dport); /* Mcst port <- server port */

		gh->type = GMTP_PKT_REGISTER;

		if(err) {
			gmtp_print_error("Failed to insert flow in table...");
			gh_reqnotify = gmtp_intra_make_request_notify_hdr(skb,
					media_info, gh->dport, gh->sport,
					GMTP_REQNOTIFY_CODE_ERROR);
			ret = NF_DROP;
		} else {
			gh_reqnotify = gmtp_intra_make_request_notify_hdr(skb,
					media_info, gh->dport, gh->sport,
					GMTP_REQNOTIFY_CODE_WAIT);
		}
	}

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
	data = gmtp_intra_lookup_media(relay_hashtable, gh->flowname);
	pr_info("gmtp_lookup_media returned: %p\n", data);
	if(data == NULL)
		goto out;

	/** Send ack back to server */
	gh_rn = gmtp_intra_make_route_hdr(skb);
	if(gh_rn != NULL)
		gmtp_intra_build_and_send_pkt(skb, iph->daddr, iph->saddr,
				gh_rn, true);

	data->state = GMTP_INTRA_REGISTER_REPLY_RECEIVED;

	/*
	 * FIXME
	 * (1) Send ReqNotify to every client.
	 * (2) Clear waiting client list
	 */
	ret = gmtp_intra_make_request_notify(skb,
			iph->saddr, gh->sport,
			iph->daddr, gh->dport, GMTP_REQNOTIFY_CODE_OK);

out:
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

/* FIXME Treat acks from clients */
int gmtp_intra_feedback_rcv(struct sk_buff *skb)
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

/*
 * FIXME Send close to multicast and delete entry later...
 * Treat close from servers
 */
int gmtp_intra_close_rcv(struct sk_buff *skb)
{
	int ret = NF_ACCEPT;
	struct gmtp_hdr *gh = gmtp_hdr(skb);

	gmtp_print_function();

	gmtp_intra_del_entry(relay_hashtable, gh->flowname);

	return ret;
}

/**
 * begin
 * P = p.flowname
 * If P != NULL:
 *     p.dest_address = get_multicast_channel(P)
 *     p.port = get_multicast_port(P)
 *     send(P)
 */
int gmtp_intra_data_rcv(struct sk_buff *skb)
{
	struct gmtp_hdr *gh = gmtp_hdr(skb);
	struct gmtp_relay_entry *flow_info;

	/**
	 * FIXME If destiny is not me, just let it go!
	 */
	flow_info = gmtp_intra_lookup_media(relay_hashtable, gh->flowname);
	if(flow_info == NULL) {
		gmtp_print_warning("Failed to lookup media info in table...");
		return NF_DROP;
	}

	pr_info("IN (%u) - %s\n", gh->seq, skb->dev->name);

	/* Dont add repeated packets */
	if(gh->seq > gmtp.seq)
		gmtp_buffer_add(skb);

	return NF_ACCEPT;
}
