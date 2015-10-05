#include <net/ip.h>
#include <asm-generic/unaligned.h>
#include <linux/etherdevice.h>

#include <uapi/linux/gmtp.h>
#include <linux/gmtp.h>
#include "../gmtp.h"

#include "gmtp-inter.h"
#include "mcc-inter.h"
#include "ucc.h"

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
		struct gmtp_client *cl;
		gmtp_pr_info("Media found. Sending RequestNotify.");
		switch(entry->state) {

		case GMTP_INTER_WAITING_REGISTER_REPLY:
			gmtp_pr_info("Waiting Register-Reply from server...");
			/* FIXME Make a timer to send register again... */
			code = GMTP_REQNOTIFY_CODE_WAIT;
			/*
			gh->type = GMTP_PKT_REGISTER;  Request shall not pass
			iph->ttl = 64;
			ip_send_check(iph);*/
			ret = NF_DROP;
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
		cl = gmtp_get_client(&entry->clients->list, iph->saddr,
				gh->sport);
		if(cl != NULL) {
			max_nclients = cl->max_nclients;
			goto out;
		}
		max_nclients = new_reporter(entry);

	} else {
		__be32 mcst_addr = get_mcst_v4_addr();
		int err = gmtp_inter_add_entry(gmtp_inter.hashtable,
				gh->flowname,
				iph->daddr,
				NULL,
				gh->dport, /* Server port */
				mcst_addr,
				gh->dport); /* Mcst port <- server port */

		if(err) {
			gmtp_pr_error("Failed to insert flow in table (%d)", err);
			ret = NF_DROP;
			goto out;
		}

		entry = gmtp_inter_lookup_media(gmtp_inter.hashtable,
				gh->flowname);
		max_nclients = new_reporter(entry);

		entry->dev_out = skb->dev;
		entry->my_addr = gmtp_inter_device_ip(skb->dev);
		pr_info("My addr: %pI4\n", &entry->my_addr);
		entry->my_port = gh->sport;
		code = GMTP_REQNOTIFY_CODE_WAIT;

		gh->type = GMTP_PKT_REGISTER;
		iph->ttl = 64;
		ip_send_check(iph);
	}

	if(max_nclients > 0)
		entry->cur_reporter = gmtp_list_add_client(
				++entry->nclients, iph->saddr, gh->sport,
				max_nclients, &entry->clients->list);
	else
		gmtp_list_add_client(++entry->nclients, iph->saddr, gh->sport,
				max_nclients, &entry->clients->list);

out:
	gh_reqnotify = gmtp_inter_make_request_notify_hdr(skb, entry,
			gh->dport, gh->sport, entry->cur_reporter,
			max_nclients, code);

	if(gh_reqnotify != NULL)
		gmtp_inter_build_and_send_pkt(skb, iph->daddr,
				iph->saddr, gh_reqnotify, GMTP_INTER_BACKWARD);

	return ret;
}

/** Register to localhost only */
int gmtp_inter_register_rcv(struct sk_buff *skb)
{
	int ret = NF_DROP;
	struct iphdr *iph = ip_hdr(skb);
	struct gmtp_hdr *gh = gmtp_hdr(skb);
	struct ethhdr *eth = eth_hdr(skb);
	struct gmtp_inter_entry *entry;
	struct gmtp_relay *relay;

	gmtp_pr_func();
	print_packet(skb, true);
	print_gmtp_packet(iph, gh);

	entry = gmtp_inter_lookup_media(gmtp_inter.hashtable, gh->flowname);
	if(entry != NULL) {
		relay = gmtp_get_relay(&entry->relays->list, iph->saddr,
				gh->sport);

		if(relay == NULL)
			relay = gmtp_inter_create_relay(skb, entry);

		switch(entry->state) {
		case GMTP_INTER_WAITING_REGISTER_REPLY:
			gmtp_pr_info("Waiting RegisterReply...");
			goto out;
		case GMTP_INTER_REGISTER_REPLY_RECEIVED:
		case GMTP_INTER_TRANSMITTING:
			gmtp_pr_info("Media found. Sending RegisterReply.");
			break;
		case GMTP_INTER_CLOSE_RECEIVED:
		case GMTP_INTER_CLOSED:
			/* TODO Send a reset to requester */
			gmtp_pr_warning("Server is already closed! "
					"(TODO: send a reset to requester)\n");
			break;
		default:
			gmtp_pr_error("Inconsistent state: %d", entry->state);
		}
		if(relay != NULL) {
			struct gmtp_hdr *ghreply;
			ghreply = gmtp_inter_make_register_reply_hdr(skb, entry,
					gh->dport, gh->sport);
			gmtp_inter_build_and_send_pkt(skb, iph->daddr,
					iph->saddr, ghreply,
					GMTP_INTER_BACKWARD);
		}
	} else { /* I am not the server */

		__be32 mcst_addr = get_mcst_v4_addr();
		int err = gmtp_inter_add_entry(gmtp_inter.hashtable,
				gh->flowname, iph->daddr,
				NULL, gh->dport, /* Media port */
				mcst_addr, gh->dport); /* Mcst port <- server port */
		if(err) {
			gmtp_pr_error("Failed to insert flow in table (%d)", err);
			return NF_DROP;
		}
		entry = gmtp_inter_lookup_media(gmtp_inter.hashtable,
				gh->flowname);

		entry->dev_out = skb->dev;
		entry->my_addr = gmtp_inter_device_ip(skb->dev);
		entry->my_port = gh->sport;
		relay = gmtp_inter_create_relay(skb, entry);

		if(relay != NULL)
			ret = NF_ACCEPT;
	}

out:
	return ret;
}

int gmtp_inter_register_local_in(struct sk_buff *skb,
		struct gmtp_inter_entry *entry)
{
	struct iphdr *iph = ip_hdr(skb);
	struct gmtp_hdr *gh = gmtp_hdr(skb);
	struct ethhdr *eth = eth_hdr(skb);

	gmtp_pr_func();

	if(entry->state != GMTP_INTER_WAITING_REGISTER_REPLY)
		return NF_DROP;

	entry->my_addr = gmtp_inter_device_ip(skb->dev);
	entry->my_port = gh->sport;
	ether_addr_copy(entry->request_mac_addr, eth->h_source);

	iph->ttl = 64;
	ip_send_check(iph);

	ether_addr_copy(eth->h_source, eth->h_dest);

	return NF_ACCEPT;
}


/* A little trick...
 * Just get any client IP to pass over GMTP-Intra.
 */
struct gmtp_client *jump_over_gmtp_intra(struct sk_buff *skb,
		struct list_head *list_head)
{
	struct iphdr *iph = ip_hdr(skb);
	struct gmtp_hdr *gh = gmtp_hdr(skb);
	struct gmtp_client *client = gmtp_get_first_client(list_head);

	if(client != NULL) {
		gh->dport = client->port;
		iph->daddr = client->addr;
		ip_send_check(iph);
		GMTP_SKB_CB(skb)->jumped = 1;
	} else
		pr_info("There are no clients anymore!\n");
	return client;
}

static int gmtp_inter_transmitting_register_reply_rcv(struct sk_buff *skb,
		struct gmtp_inter_entry *entry)
{
	struct gmtp_hdr *gh = gmtp_hdr(skb);
	struct iphdr *iph = ip_hdr(skb);
	struct ethhdr *eth = eth_hdr(skb);
	struct gmtp_hdr *gh_route_n;

	/* TODO write my ucc tx_rate here... */

	gmtp_inter_add_relayid(skb);

	if(!gmtp_inter_ip_local(iph->daddr))
		return NF_ACCEPT;

	gh_route_n = gmtp_inter_make_route_hdr(skb);

	ether_addr_copy(entry->server_mac_addr, eth->h_source);

	if(gh_route_n != NULL)
		gmtp_inter_build_and_send_pkt(skb, iph->daddr, iph->saddr,
				gh_route_n, GMTP_INTER_BACKWARD);

	return NF_DROP;
}

static void gmtp_inter_send_reply_to_relay(struct sk_buff *skb,
		struct gmtp_inter_entry *entry, struct gmtp_relay *relay)
{
	struct gmtp_hdr *gh = gmtp_hdr(skb);
	struct iphdr *iph = ip_hdr(skb);

	struct sk_buff *copy = skb_copy(skb, gfp_any());

	if(copy != NULL) {
		struct iphdr *iph_copy = ip_hdr(copy);
		struct gmtp_hdr *gh_copy = gmtp_hdr(copy);
		struct gmtp_hdr *gh_reply;

		iph_copy->daddr = relay->addr;
		ip_send_check(iph_copy);
		gh_copy->dport = relay->port;

		gh_reply = gmtp_inter_make_register_reply_hdr(copy, entry,
				gh->sport, relay->port);
		if(gh_reply != NULL)
			gmtp_inter_build_and_send_pkt(skb, iph->saddr,
					relay->addr, gh_reply,
					GMTP_INTER_FORWARD);
	}
}
static void gmtp_inter_send_reply_to_relays(struct sk_buff *skb,
		struct gmtp_inter_entry *entry)
{
	struct gmtp_relay *relay, *tempr;

	list_for_each_entry_safe(relay, tempr, &entry->relays->list, list)
	{
		gmtp_inter_send_reply_to_relay(skb, entry, relay);
	}

}

static void gmtp_inter_send_reqnotify_to_client(struct sk_buff *skb,
		struct gmtp_inter_entry *entry, struct gmtp_client *client,
		struct gmtp_client *cur_reporter, __u8 code)
{
	struct gmtp_hdr *gh = gmtp_hdr(skb);
	struct iphdr *iph = ip_hdr(skb);

	struct sk_buff *copy = skb_copy(skb, gfp_any());

	if(copy != NULL) {
		struct iphdr *iph_copy = ip_hdr(copy);
		struct gmtp_hdr *gh_copy = gmtp_hdr(copy);
		struct gmtp_hdr *gh_req_n;

		iph_copy->daddr = client->addr;
		ip_send_check(iph_copy);
		gh_copy->dport = client->port;

		gh_req_n = gmtp_inter_make_request_notify_hdr(copy, entry,
				gh->sport, client->port, cur_reporter,
				client->max_nclients, code);
		if(gh_req_n != NULL)
			gmtp_inter_build_and_send_pkt(skb, iph->saddr,
					client->addr, gh_req_n,
					GMTP_INTER_FORWARD);
	}
}

static int gmtp_inter_send_reqnotify_to_clients(struct sk_buff *skb,
		struct gmtp_inter_entry *entry)
{
	int ret = NF_ACCEPT;
	struct gmtp_hdr *gh = gmtp_hdr(skb);
	struct iphdr *iph = ip_hdr(skb);
	struct gmtp_client *client, *tempc, *first_client, *cur_reporter = NULL;
	__u8 code = GMTP_REQNOTIFY_CODE_OK;

	/* Send it to first client */
	first_client = gmtp_get_first_client(&entry->clients->list);
	if(first_client == NULL) {
		pr_info("No clients registered.\n");
		return NF_ACCEPT;
	}

	if(first_client->max_nclients > 0)
		cur_reporter = first_client;

	ret = gmtp_inter_make_request_notify(skb, iph->saddr, gh->sport,
			first_client->addr, first_client->port, cur_reporter,
			first_client->max_nclients, code);

	/* Clean list of clients and keep only reporters */
	list_for_each_entry_safe(client, tempc, &entry->clients->list, list)
	{
		/* Send to other clients (not first) */
		if(client == first_client)
			continue;

		if(client->max_nclients > 0)
			cur_reporter = client;

		gmtp_inter_send_reqnotify_to_client(skb, entry, client,
				cur_reporter, code);

		/** Deleting non-reporters */
		if(client->max_nclients <= 0) {
			list_del(&client->list);
			kfree(client);
		}
	}

	return ret;
}

int gmtp_inter_register_reply_rcv(struct sk_buff *skb,
		struct gmtp_inter_entry *entry,
		enum gmtp_inter_direction direction)
{
	int ret = NF_ACCEPT;
	struct gmtp_hdr *gh = gmtp_hdr(skb);
	struct iphdr *iph = ip_hdr(skb);
	struct ethhdr *eth = eth_hdr(skb);
	struct gmtp_hdr *gh_route_n;

	__u8 code = GMTP_REQNOTIFY_CODE_OK;

	gmtp_print_function();

	print_packet(skb, true);
	print_gmtp_packet(iph, gh);

	if(entry->state == GMTP_INTER_REGISTER_REPLY_RECEIVED)
		return NF_DROP;
	else if(entry->state == GMTP_INTER_TRANSMITTING)
		return gmtp_inter_transmitting_register_reply_rcv(skb, entry);

	entry->transm_r =  gh->transm_r;
	entry->rcv_tx_rate = gh->transm_r;
	entry->server_rtt = (unsigned int)gh->server_rtt;
	entry->ucc_type = gmtp_hdr_register_reply(skb)->ucc_type;
	gmtp_inter_build_ucc(&entry->ucc, entry->ucc_type);
	entry->route_pending = true;

	if(direction != GMTP_INTER_LOCAL) {
		pr_info("Direction: %u\n", direction);

		entry->dev_in = skb->dev;

		/* Add relay information in REGISTER-REPLY packet) */
		gmtp_inter_add_relayid(skb);

		pr_info("UPDATING Tx Rate...\n");
		gmtp_inter.worst_rtt = max(gmtp_inter.worst_rtt,
				(unsigned int ) gh->server_rtt);

		pr_info("Server RTT: %u ms\n", (unsigned int ) gh->server_rtt);
		pr_info("Worst RTT: %u ms\n", gmtp_inter.worst_rtt);

		if(gmtp_inter.ucc_rx < gh->transm_r)
			gh->transm_r = (__be32) gmtp_inter.ucc_rx;

		if(entry->state != GMTP_INTER_WAITING_REGISTER_REPLY)
			return NF_ACCEPT;

		ether_addr_copy(entry->server_mac_addr, eth->h_source);

		gh_route_n = gmtp_inter_make_route_hdr(skb);
		if(gh_route_n != NULL)
			gmtp_inter_build_and_send_pkt(skb, iph->daddr,
					iph->saddr, gh_route_n, direction);
		mod_timer(&entry->ack_timer,
				jiffies + msecs_to_jiffies(2*GMTP_DEFAULT_RTT));

		/** FIXME This causes congestion... ! Why? */
		mod_timer(&entry->register_timer, jiffies + HZ);
	} else {
		pr_info("Direction: LOCAL\n");
		ether_addr_copy(entry->server_mac_addr, skb->dev->dev_addr);
		ether_addr_copy(eth->h_dest, entry->request_mac_addr);
	}

	entry->state = GMTP_INTER_REGISTER_REPLY_RECEIVED;

	if(entry->nrelays==0) {
		pr_info("No relays registered.\n");
		goto send_to_clients;
	}

	gmtp_inter_send_reply_to_relays(skb, entry);

send_to_clients:
	ret = gmtp_inter_send_reqnotify_to_clients(skb,	entry);

	return ret;
}

/* TODO Separate functions to clients and relays */
int gmtp_inter_ack_rcv(struct sk_buff *skb, struct gmtp_inter_entry *entry)
{
	struct gmtp_hdr *gh = gmtp_hdr(skb);
	struct iphdr *iph = ip_hdr(skb);
	struct gmtp_client *reporter;
	struct gmtp_relay *relay;

	reporter = gmtp_get_client(&entry->clients->list, iph->saddr,
			gh->sport);
	if(reporter != NULL)
		reporter->ack_rx_tstamp = jiffies_to_msecs(jiffies);

	relay = gmtp_get_relay(&entry->relays->list, iph->saddr, gh->sport);
	if(relay != NULL) {
		entry->rcv_tx_rate = gh->transm_r;
		relay->tx_rate = gh->transm_r;
	}

	if(!gmtp_inter_ip_local(iph->daddr)) {
		if(reporter == NULL && relay == NULL)
			return NF_ACCEPT;
	}

	if(gmtp_inter_ip_local(iph->daddr)) {
		if(entry->route_pending) {
			entry->route_pending = false;
			return NF_ACCEPT;
		} else if(relay != NULL) {
			return NF_ACCEPT;
		}
	}

	return NF_DROP;
}

/* Treat route_notify from relays */
int gmtp_inter_route_rcv(struct sk_buff *skb, struct gmtp_inter_entry *entry)
{
	struct gmtp_hdr *gh = gmtp_hdr(skb);
	struct gmtp_hdr_route *route = gmtp_hdr_route(skb);
	struct iphdr *iph = ip_hdr(skb);
	struct ethhdr *eth = eth_hdr(skb);
	struct gmtp_relay *relay;

	print_gmtp_packet(iph, gh);
	print_route_from_skb(skb);

	relay = gmtp_get_relay(&entry->relays->list, iph->saddr, gh->sport);
	pr_info("Relay: %p\n", relay);
	if(relay == NULL)
		return NF_ACCEPT;

	relay->state = GMTP_OPEN;
	ether_addr_copy(relay->mac_addr, eth->h_source);
	relay->dev = skb->dev;

	if(gmtp_inter_ip_local(iph->daddr)) { /* I am the server itself */
		if(route->nrelays > 0)
			/*gmtp_add_server_entry(server_hashtable, gh->flowname,
					route)*/;
		pr_info("ROUTE_RCV: entry->route_pending = %d\n",
				entry->route_pending);
		if(entry->route_pending) {
			entry->route_pending = false;
			return NF_ACCEPT;
		}
	}

	pr_info("Dropping Route...\n");
	return NF_DROP;
}

int gmtp_inter_feedback_rcv(struct sk_buff *skb, struct gmtp_inter_entry *entry)
{
	struct gmtp_hdr *gh = gmtp_hdr(skb);
	struct gmtp_hdr_feedback *ghf = gmtp_hdr_feedback(skb);
	struct iphdr *iph = ip_hdr(skb);
	struct gmtp_client *reporter;
	unsigned int now, rep_rtt;

	reporter = gmtp_get_client(&entry->clients->list, iph->saddr, gh->sport);
	if(reporter == NULL)
		return NF_DROP;

	if(likely(gh->transm_r > 0)) {
		entry->nfeedbacks++;
		entry->sum_feedbacks += (unsigned int) gh->transm_r;
		reporter->ack_rx_tstamp = jiffies_to_msecs(jiffies);
	}

	now = jiffies_to_msecs(jiffies);
	rep_rtt = now - ghf->orig_tstamp;
	entry->clients_rtt = rtt_ewma(entry->clients_rtt, rep_rtt, 900);

	return NF_DROP;
}

/**
 * FIXME This works only with auto promoted reporters
 */
int gmtp_inter_elect_resp_rcv(struct sk_buff *skb,
		struct gmtp_inter_entry *entry)
{
	struct iphdr *iph = ip_hdr(skb);
	struct gmtp_hdr *gh = gmtp_hdr(skb);

	print_packet(skb, true);
	print_gmtp_packet(iph, gh);

	gmtp_list_add_client(++entry->nclients, iph->saddr,
					gh->sport, gmtp_inter.kreporter,
					&entry->clients->list);

	return NF_DROP;
}

#define skblen(skb) ((*skb).len) + ETH_HLEN

/**
 * Update GMTP-inter statistics
 */
static inline void gmtp_update_stats(struct gmtp_inter_entry *info,
		struct sk_buff *skb, struct gmtp_hdr *gh)
{
	gmtp_inter.total_bytes_rx += skblen(skb);
	gmtp_inter.ucc_bytes += skblen(skb);

	info->total_bytes += skblen(skb);
	info->recent_bytes += skblen(skb);
	info->seq = (unsigned int)gh->seq;
	info->server_rtt = (unsigned int)gh->server_rtt;
	info->last_data_tstamp = gmtp_hdr_data(skb)->tstamp;
	info->last_rx_tstamp = jiffies_to_msecs(jiffies);

	info->transm_r = gh->transm_r;

	/* FIXME Make a timer to update this */
	/*info->rcv_tx_rate = gh->transm_r;
	gh->transm_r = min(info->rcv_tx_rate, gmtp_inter.ucc_rx);*/

	if(++info->data_pkt_in % gmtp_inter.rx_rate_wnd == 0) {
		unsigned long current_time = ktime_to_ms(ktime_get_real());
		unsigned long elapsed = current_time - info->recent_rx_tstamp;

		if(elapsed != 0) {
			info->current_rx = DIV_ROUND_CLOSEST(
					info->recent_bytes * MSEC_PER_SEC, elapsed);
		}
		info->recent_rx_tstamp = ktime_to_ms(ktime_get_real());

		info->recent_bytes = 0;
		gmtp_inter.worst_rtt = GMTP_MIN_RTT_MS;
	}
	gmtp_inter.worst_rtt = max(gmtp_inter.worst_rtt,
				(unsigned int ) gh->server_rtt);
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
int gmtp_inter_data_rcv(struct sk_buff *skb, struct gmtp_inter_entry *entry)
{
	struct iphdr *iph = ip_hdr(skb);
	struct gmtp_hdr *gh = gmtp_hdr(skb);

	if(!gmtp_inter_ip_local(iph->daddr)) /* Data is not to me */
		return NF_ACCEPT;

	if(unlikely(entry->state == GMTP_INTER_REGISTER_REPLY_RECEIVED)) {
		entry->state = GMTP_INTER_TRANSMITTING;
		mod_timer(&entry->mcc_timer, gmtp_mcc_interval(entry->server_rtt));
	}

	if(entry->buffer->qlen >= entry->buffer_max) {
		pr_info("GMTP-Inter: dropping pkt (to %pI4, seq=%u, data=%s)\n",
				&iph->daddr, gh->seq, gmtp_data(skb));
		goto out;
		/* Dont add it to buffer (equivalent to drop) */
	}

	if((gh->seq > entry->seq) && entry->state == GMTP_INTER_TRANSMITTING)
		gmtp_buffer_add(entry, skb);

out:
	if(iph->daddr == entry->my_addr)
		jump_over_gmtp_intra(skb, &entry->clients->list);
	else
		skb->dev = entry->dev_out;

	gmtp_update_stats(entry, skb, gh);

	return NF_ACCEPT;
}

int gmtp_inter_close_rcv(struct sk_buff *skb, struct gmtp_inter_entry *entry,
		bool in)
{
	struct gmtp_hdr *gh = gmtp_hdr(skb);
	struct iphdr *iph = ip_hdr(skb);

	gmtp_pr_func();

	print_packet(skb, in);
	print_gmtp_packet(iph, gh);
	gmtp_pr_info("State: %u", entry->state);

	if(!in) {
		pr_info("Local out!\n");
		entry->state = GMTP_INTER_CLOSE_RECEIVED;
		return NF_ACCEPT;
	}

	if(iph->saddr != entry->server_addr
			|| entry->state == GMTP_INTER_CLOSED)
		return NF_ACCEPT;


	if(entry->state == GMTP_INTER_TRANSMITTING) {
		struct gmtp_hdr *gh_reset;
		entry->state = GMTP_INTER_CLOSE_RECEIVED;

		pr_info("Deleting timers...\n");
		del_timer_sync(&entry->mcc_timer);
		del_timer_sync(&entry->ack_timer);
		del_timer_sync(&entry->register_timer);

		gh_reset = gmtp_inter_make_reset_hdr(skb, GMTP_RESET_CODE_CLOSED);

		if(gh_reset != NULL) {
			gmtp_inter_build_and_send_pkt(skb, iph->daddr,
					iph->saddr, gh_reset,
					GMTP_INTER_BACKWARD);

			jump_over_gmtp_intra(skb, &entry->clients->list);
			gmtp_buffer_add(entry, skb);
		}
	}

	return NF_ACCEPT;
}
