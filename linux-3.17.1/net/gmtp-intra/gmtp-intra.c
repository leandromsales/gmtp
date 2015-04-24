#define GET_MAC_ADDRESS  0
#define GET_IP_ADDRESS   1

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/netfilter.h>
#include <linux/netfilter_ipv4.h>
#include <linux/skbuff.h>
#include <linux/ip.h>

#include <linux/if.h>
#include <linux/ioctl.h>

#include <linux/inet.h>
#include <linux/dirent.h>
#include <linux/inetdevice.h>

#include <net/ip.h>
#include <net/sock.h>
#include <net/sch_generic.h>
#include <uapi/linux/gen_stats.h>
#include <uapi/linux/types.h>

#include <uapi/linux/gmtp.h>
#include <linux/gmtp.h>
#include "../gmtp/gmtp.h"

#include "gmtp-intra.h"

extern const char *gmtp_packet_name(const int);
extern const char *gmtp_state_name(const int);
extern void flowname_str(__u8* str, const __u8* flowname);

static struct nf_hook_ops nfho_in;
static struct nf_hook_ops nfho_out;

struct gmtp_intra gmtp;

struct gmtp_intra_hashtable* relay_hashtable;
EXPORT_SYMBOL_GPL(relay_hashtable);

void gmtp_intra_relay_get_devices (int option)
{
    printk(KERN_INFO"\n\n\n");
    
    struct socket *sock = NULL;
    struct net_device *dev = NULL;

    struct in_device *in_dev;
    struct in_ifaddr *if_info;

    struct net *net;
    int i, retval;
    char mac_address[6];
    char *ip_address;
    
    retval = sock_create(AF_INET, SOCK_STREAM, 0, &sock);
    net = sock_net (sock->sk);
    
            for(i = 2; (dev = dev_get_by_index_rcu(net,i)) != NULL; ++i){
            
                if(option == GET_MAC_ADDRESS){
   
                    memcpy(&mac_address, dev->dev_addr, 6);
                    printk(KERN_DEBUG"size of dev_addr = %d\n",sizeof(dev->dev_addr));
                    printk(KERN_DEBUG"Interface[%d] MAC = %x:%x:%x:%x:%x:%x\n",i,
                              mac_address[0],mac_address[1],
                              mac_address[2],mac_address[3], 
                              mac_address[4],mac_address[5]);
                }

                if(option ==  GET_IP_ADDRESS){    
                    
                    in_dev = (struct in_device * )dev->ip_ptr;

                    if(in_dev == NULL)
                        printk("in_dev == NULL\n");

                    if_info = in_dev->ifa_list;
                    for(;if_info;if_info = if_info->ifa_next){
                        if(if_info != NULL){
                            printk("if_info->ifa_address=%032x\n",if_info->ifa_address);
                            break;    
                        }
                    }   

                }   
            }
    
    sock_release(sock);

}

const __u8 *gmtp_intra_relay_id()
{
	gmtp_intra_relay_get_devices(GET_MAC_ADDRESS);

    /*just to keep the same return of previous implementation*/
	return "777777777777777777777";

}
__be32 gmtp_intra_relay_ip()
{
	gmtp_intra_relay_get_devices(GET_IP_ADDRESS);
   
    /*just to keep the same return of previous implementation*/
	unsigned char *ip = "\xc0\xa8\x02\x01"; /* 192.168.2.1 */
	return *(unsigned int *)ip;

}

static inline void increase_stats(struct iphdr *iph)
{
	gmtp.seq++;
	gmtp.npackets++;
	gmtp.nbytes += ntohs(iph->tot_len);
}

static inline void decrease_stats(struct iphdr *iph)
{
	if(gmtp.npackets > 0)
		gmtp.npackets--;

	if(gmtp.nbytes <= ntohs(iph->tot_len))
		gmtp.nbytes = 0;
	else
		gmtp.nbytes -= ntohs(iph->tot_len);
}

__be32 get_mcst_v4_addr(void)
{
	__be32 mcst_addr;
	unsigned char *base_channel = "\xe0\x00\x00\x01"; /* 224.0.0.1 */
	unsigned char *channel = kmalloc(4 * sizeof(unsigned char), GFP_KERNEL);

	gmtp_print_function();

	if(channel == NULL) {
		gmtp_print_error("Cannot assign requested multicast address");
		return -EADDRNOTAVAIL;
	}

	memcpy(channel, base_channel, 4 * sizeof(unsigned char));

	channel[3] += gmtp.mcst[3]++;

	/**
	 * From: base_channel (224. 0 . 0 . 1 )
	 * to:   max_channel  (239.255.255.255)
	 *                     L0  L1  L2  L3
	 */
	if(gmtp.mcst[3] > 254) {  /* L3 starts with 1 */
		gmtp.mcst[3] = 0;
		gmtp.mcst[2]++;
	}
	if(gmtp.mcst[2] > 255) {
		gmtp.mcst[2] = 0;
		gmtp.mcst[1]++;
	}
	if(gmtp.mcst[1] > 255) {
		gmtp.mcst[1] = 0;
		gmtp.mcst[0]++;
	}
	if(gmtp.mcst[0] > 15) {  /* 239 - 224 */
		gmtp_print_error("Cannot assign requested multicast address");
		return -EADDRNOTAVAIL;
	}
	channel[2] += gmtp.mcst[2];
	channel[1] += gmtp.mcst[1];
	channel[0] += gmtp.mcst[0];

	mcst_addr = *(unsigned int *)channel;
	gmtp_print_debug("Channel addr: %pI4", &mcst_addr);
	return mcst_addr;
}
EXPORT_SYMBOL_GPL(get_mcst_v4_addr);

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

/* Treat close from servers */
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
	int ret = NF_ACCEPT;
	struct gmtp_hdr *gh = gmtp_hdr(skb);
	struct iphdr *iph = ip_hdr(skb);
	struct gmtp_relay_entry *flow_info;

	/*gmtp_print_function();*/

	/**
	 * FIXME If destiny is not me, just let it go!
	 */
	flow_info = gmtp_intra_lookup_media(relay_hashtable, gh->flowname);
	if(flow_info == NULL) {
		gmtp_print_warning("Failed to lookup media info in table...");
		ret = NF_DROP;
		goto out;
	}

	iph->daddr = flow_info->channel_addr;
	ip_send_check(iph);
	gh->dport = flow_info->channel_port;

	/*skb_queue_tail(gmtp.buffer, skb_copy(skb, GFP_ATOMIC));

	pr_info("qlen: %u\n", gmtp.buffer->qlen);
	pr_info("skb (%s): %p\n", skb->dev->name, skb);

	if(gmtp.buffer->qlen >= 5) {
		int i;
		for(i=0; i<gmtp.data_buffer->qlen; ++i) {

			struct sk_buff *buf_skb = skb_dequeue(gmtp.buffer);

			struct gmtp_hdr *gh2 = gmtp_hdr(buf_skb);
			struct iphdr *iph2 = ip_hdr(buf_skb);

			iph2->daddr = flow_info->channel_addr;
			ip_send_check(iph2);
			gh2->dport = flow_info->channel_port;

			pr_info("buf_skb (%s): %p\n", buf_skb->dev->name, buf_skb);
			print_gmtp_packet(iph2, gh2);
			gmtp_intra_build_and_send_pkt(buf_skb, iph2->saddr,
					iph2->daddr, gh2, 0);
		}

	}*/

out:
	return ret;
}

unsigned int hook_func_in(unsigned int hooknum, struct sk_buff *skb,
		const struct net_device *in, const struct net_device *out,
		int (*okfn)(struct sk_buff *))
{
	int ret = NF_ACCEPT;
	struct iphdr *iph = ip_hdr(skb);
	struct gmtp_hdr *gh;

	if(in == NULL)
		goto out;

	increase_stats(iph);

	if(iph->protocol == IPPROTO_GMTP) {

		gh = gmtp_hdr(skb);

		if(gh->type != GMTP_PKT_DATA) {
			gmtp_print_debug("GMTP packet: %s (%d)",
					gmtp_packet_name(gh->type), gh->type);
			print_packet(iph, true);
			print_gmtp_packet(iph, gh);
		}

		switch(gh->type) {
		case GMTP_PKT_REQUEST:
			ret = gmtp_intra_request_rcv(skb);
			break;
		case GMTP_PKT_REGISTER_REPLY:
			ret = gmtp_intra_register_reply_rcv(skb);
			break;
		case GMTP_PKT_ACK:
			ret = gmtp_intra_ack_rcv(skb);
			break;
		case GMTP_PKT_FEEDBACK:
			ret = gmtp_intra_feedback_rcv(skb);
			break;
		case GMTP_PKT_CLOSE:
			ret = gmtp_intra_close_rcv(skb);
			break;
		}
	}

out:
	return ret;
}

unsigned int hook_func_out(unsigned int hooknum, struct sk_buff *skb,
		const struct net_device *in, const struct net_device *out,
		int (*okfn)(struct sk_buff *))
{
	int ret = NF_ACCEPT;
	struct iphdr *iph = ip_hdr(skb);
	struct gmtp_hdr *gh;

	if(out == NULL)
		goto out;

	decrease_stats(iph);

	if(iph->protocol == IPPROTO_GMTP) {

		gh = gmtp_hdr(skb);

		switch(gh->type) {
		case GMTP_PKT_DATA:
			/* Artificial loss (only for tests)
			if(gh->seq % 2)
				return NF_DROP;*/

			ret = gmtp_intra_data_rcv(skb);
			break;
		default:
			gmtp_print_debug("GMTP packet: %s (%d)",
					gmtp_packet_name(gh->type), gh->type);
			print_packet(iph, false);
		}

	}

out:
	return ret;
}

int init_module()
{
	int ret = 0;
	gmtp_print_function();
	gmtp_print_debug("Starting GMTP-Intra");

	memset(&gmtp.mcst, 0, 4*sizeof(unsigned char));
	gmtp.npackets = 0;
	gmtp.nbytes = 0;
	gmtp.current_tx = 1;
	gmtp.seq = 0;
	gmtp.buffer = kmalloc(sizeof(struct sk_buff_head), GFP_ATOMIC);
	skb_queue_head_init(gmtp.buffer);
	gmtp.buffer_size = 0;

	relay_hashtable = gmtp_intra_create_hashtable(64);
	if(relay_hashtable == NULL) {
		gmtp_print_error("Cannot create hashtable...");
		ret = -ENOMEM;
		goto out;
	}

	nfho_in.hook = hook_func_in;
	nfho_in.hooknum = NF_INET_PRE_ROUTING;
	nfho_in.pf = PF_INET;
	nfho_in.priority = NF_IP_PRI_FIRST;
	nf_register_hook(&nfho_in);

	nfho_out.hook = hook_func_out;
	nfho_out.hooknum = NF_INET_POST_ROUTING;
	nfho_out.pf = PF_INET;
	nfho_out.priority = NF_IP_PRI_FIRST;
	nf_register_hook(&nfho_out);

out:
	return ret;
}

void cleanup_module()
{
	gmtp_print_debug("Finishing GMTP-Intra");

	skb_queue_purge(gmtp.buffer);

	kfree_gmtp_intra_hashtable(relay_hashtable);

	nf_unregister_hook(&nfho_in);
	nf_unregister_hook(&nfho_out);
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Mário André Menezes <mariomenezescosta@gmail.com>");
MODULE_AUTHOR("Wendell Silva Soares <wss@ic.ufal.br>");
MODULE_DESCRIPTION("GMTP - Global Media Transmission Protocol");
