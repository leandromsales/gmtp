#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/netfilter.h>
#include <linux/netfilter_ipv4.h>
#include <linux/skbuff.h>
#include <linux/ip.h>
#include <linux/inet.h>
#include <linux/inetdevice.h>
#include <linux/err.h>
#include <linux/if.h>
#include <linux/ioctl.h>
#include <linux/rtnetlink.h>

#include <net/sock.h>
#include <net/sch_generic.h>

#include <uapi/linux/gen_stats.h>
#include <uapi/linux/types.h>

#include <uapi/linux/gmtp.h>
#include <linux/gmtp.h>
#include "../gmtp.h"

#include "gmtp-inter.h"
#include "mcc-inter.h"
#include "ucc.h"

/****
static struct nf_hook_ops nfho_pre_routing;
static struct nf_hook_ops nfho_local_in;
static struct nf_hook_ops nfho_local_out;
static struct nf_hook_ops nfho_post_routing;
****/

struct gmtp_inter gmtp_inter;

__be32 get_mcst_v4_addr(void)
{
    __be32 mcst_addr;
    unsigned char *base_channel = "\xe0\xc0\x00\x00"; /* 224.192.0.0 */
    unsigned char *channel = kmalloc(4 * sizeof(unsigned char), GFP_KERNEL);

    gmtp_print_function();

    if(channel == NULL) {
        gmtp_pr_error("NULL channel: Cannot assign requested address");
        return -EADDRNOTAVAIL;
    }
    memcpy(channel, base_channel, 4 * sizeof(unsigned char));

    channel[3] += gmtp_inter.mcst[3]++;

    /**
     * From: base_channel (224.192. 0 . 0 )
     * to:   max_channel  (239.255.255.255)
     *                     L0  L1  L2  L3
     */
    if(gmtp_inter.mcst[3] > 255) {  /* L3 starts with 1 */
        gmtp_inter.mcst[3] = 0;
        gmtp_inter.mcst[2]++;
    }
    if(gmtp_inter.mcst[2] > 255) {
        gmtp_inter.mcst[2] = 0;
        gmtp_inter.mcst[1]++;
    }
    if(gmtp_inter.mcst[1] > 63) { /* 255 - 192 */
        gmtp_inter.mcst[1] = 0;
        gmtp_inter.mcst[0]++;
    }
    if(gmtp_inter.mcst[0] > 15) {  /* 239 - 224 */
        int i;
        for(i = 0; i < 4; ++i)
            pr_info("gmtp_inter.mcst[%d] = %u\n", i,
                    gmtp_inter.mcst[i]);
        gmtp_pr_error("Cannot assign requested multicast address");
        return -EADDRNOTAVAIL;
    }
    channel[2] += gmtp_inter.mcst[2];
    channel[1] += gmtp_inter.mcst[1];
    channel[0] += gmtp_inter.mcst[0];

    mcst_addr = *(unsigned int *)channel;
    gmtp_print_debug("Channel addr: %pI4", &mcst_addr);
    return mcst_addr;
}
EXPORT_SYMBOL_GPL(get_mcst_v4_addr);

void gmtp_buffer_add(struct gmtp_inter_entry *info, struct sk_buff *newskb)
{
    skb_queue_tail(info->buffer, skb_copy(newskb, GFP_ATOMIC));
    info->buffer_len += newskb->len + ETH_HLEN;
}

void gmtp_ucc_buffer_add(struct sk_buff *newskb)
{
    if(newskb != NULL) {
        gmtp_inter.buffer_len += skblen(newskb);
        gmtp_inter.total_bytes_rx += skblen(newskb);
        gmtp_inter.ucc_bytes += skblen(newskb);
    }
}

struct sk_buff *gmtp_buffer_dequeue(struct gmtp_inter_entry *info)
{
    struct sk_buff *skb = skb_dequeue(info->buffer);
    if(skb != NULL) {
        info->buffer_len -= (skb->len + ETH_HLEN);
    }
    return skb;
}

void gmtp_ucc_buffer_dequeue(struct sk_buff *newskb)
{
    if(newskb != NULL) {
        gmtp_inter.buffer_len -= skblen(newskb);
        if(gmtp_inter.buffer_len < 0)
            gmtp_inter.buffer_len = 0;
    }
}


unsigned int hook_func_pre_routing(unsigned int hooknum, struct sk_buff *skb,
        const struct net_device *in, const struct net_device *out,
        int (*okfn)(struct sk_buff *))
{
    int ret = NF_ACCEPT;
    struct iphdr *iph = ip_hdr(skb);

    gmtp_ucc_buffer_add(skb);

    /*if(skb->dev != NULL) {
        struct netdev_rx_queue *rx = skb->dev->_rx;
        pr_info("skb->dev->ingress_queue3: %p\n", rx);
        if(rx != NULL) {
            pr_info("in->ingress_queue->qdisc: %p\n", ndq->qdisc);
            if(ndq->qdisc != NULL) {
                pr_info("q.qlen: %u\n", ndq->qdisc->q.qlen);
                pr_info("qstats.qlen: %u\n",
                        ndq->qdisc->qstats.qlen);
                pr_info("qstats.drops: %u\n",
                        ndq->qdisc->qstats.drops);
                pr_info("qstats.overlimits: %u\n",
                        ndq->qdisc->qstats.overlimits);
            }
        }
    }*/

    /*int i;
    for(i = 0; i < NR_CPUS; i++) {
        struct softnet_data *queue;
        queue = &per_cpu(softnet_data, i);

        struct sk_buff_head *input = &queue->input_pkt_queue;
        struct sk_buff_head *process = &queue->process_queue;

        pr_info("input_queue: %u\n", input->qlen);
        pr_info("process_queue: %u\n", process->qlen);
        pr_info("processed: %u\n", queue->processed);
        pr_info("time_squeeze: %u\n", queue->time_squeeze);
        pr_info("cpu_collision: %u\n", queue->cpu_collision);
        pr_info("received_rps: %u\n", queue->received_rps);
        pr_info("dropped: %u\n", queue->dropped);
    }*/

    if(gmtp_info->relay_enabled == 0)
        return ret;

    if(iph->protocol == IPPROTO_GMTP) {

        struct gmtp_hdr *gh = gmtp_hdr(skb);
        struct gmtp_inter_entry *entry;
        struct gmtp_relay *relay;

        if(gh->type == GMTP_PKT_REQUEST) {
            if(gmtp_local_ip(iph->saddr)
                    && iph->saddr != iph->daddr) {
                goto out;
            }

            if(iph->ttl == 1) {
                print_ipv4_packet(skb, true);
                print_gmtp_packet(iph, gh);
                ret = gmtp_inter_request_rcv(skb);
                goto out;
            }
        }

        entry = gmtp_inter_lookup_media(gmtp_inter.hashtable,
                gh->flowname);
        if(entry == NULL)
            goto out;

        switch(gh->type) {
        case GMTP_PKT_REGISTER:
            if(!gmtp_local_ip(iph->daddr))
                ret = gmtp_inter_register_rcv(skb);
            break;
        case GMTP_PKT_REGISTER_REPLY:
            ret = gmtp_inter_register_reply_rcv(skb, entry,
                    GMTP_INTER_BACKWARD);
            break;
        case GMTP_PKT_ROUTE_NOTIFY:
            ret = gmtp_inter_route_rcv(skb, entry);
            break;
        case GMTP_PKT_ACK:
            ret = gmtp_inter_ack_rcv(skb, entry);
            break;
        case GMTP_PKT_FEEDBACK:
            ret = gmtp_inter_feedback_rcv(skb, entry);
            break;
        case GMTP_PKT_DELEGATE:
            ret = gmtp_inter_delegate_rcv(skb, entry);
            break;
        default:
            relay = gmtp_get_relay(&entry->relays->list,
                    iph->daddr, gh->dport);
            if(!gmtp_local_ip(iph->daddr) && (relay == NULL))
                goto out;
        }

        switch(gh->type) {
        case GMTP_PKT_DATA:
            ret = gmtp_inter_data_rcv(skb, entry);
            break;
        case GMTP_PKT_ELECT_RESPONSE:
            ret = gmtp_inter_elect_resp_rcv(skb, entry);
            break;
        case GMTP_PKT_RESET:
        case GMTP_PKT_CLOSE:
            ret = gmtp_inter_close_rcv(skb, entry, true);
            break;
        }

    }

out:
    if(ret == NF_DROP)
        gmtp_ucc_buffer_dequeue(skb);

    return ret;
}


unsigned int hook_func_local_in(unsigned int hooknum, struct sk_buff *skb,
        const struct net_device *in, const struct net_device *out,
        int (*okfn)(struct sk_buff *))
{
    int ret = NF_ACCEPT;
    struct iphdr *iph = ip_hdr(skb);

    if(gmtp_info->relay_enabled == 0)
        goto in;

    if(iph->protocol == IPPROTO_GMTP) {

        struct gmtp_hdr *gh = gmtp_hdr(skb);

        struct gmtp_inter_entry *entry = gmtp_inter_lookup_media(
                gmtp_inter.hashtable, gh->flowname);
        if(entry == NULL)
            goto in;

        if(!gmtp_inter_lookup_media(gmtp_inter.hashtable, gh->flowname))
            goto in;

        switch(gh->type) {
        case GMTP_PKT_REGISTER:
            /* Here. We need to trick the server,
            to avoid data packets destined to 'lo' */
            ret = gmtp_inter_register_local_in(skb, entry);
            break;
        }
    }

in:
    /*gmtp_ucc_buffer_dequeue(skb);*/
    return ret;
}

unsigned int hook_func_local_out(unsigned int hooknum, struct sk_buff *skb,
        const struct net_device *in, const struct net_device *out,
        int (*okfn)(struct sk_buff *))
{
    int ret = NF_ACCEPT;
    struct iphdr *iph = ip_hdr(skb);

    if(gmtp_info->relay_enabled == 0)
        return ret;

    if(iph->protocol == IPPROTO_GMTP) {

        struct gmtp_hdr *gh = gmtp_hdr(skb);

        struct gmtp_inter_entry *entry = gmtp_inter_lookup_media(
                gmtp_inter.hashtable, gh->flowname);
        if(entry == NULL)
            return NF_ACCEPT;

        switch(gh->type) {
        case GMTP_PKT_RESET:
        case GMTP_PKT_CLOSE:
            ret = gmtp_inter_close_rcv(skb, entry, false);
            break;
        }
    }

    return ret;
}

static int gmtp_inter_has_clients(struct sk_buff *skb,
        struct gmtp_inter_entry *entry)
{
    struct iphdr *iph = ip_hdr(skb);
    struct gmtp_hdr *gh = gmtp_hdr(skb);

    struct gmtp_relay *relay = gmtp_get_relay(&entry->relays->list,
            iph->daddr, gh->dport);
    // struct gmtp_reporter *reporter = gmtp_get_client(&entry->clients->list,
    //         iph->daddr, gh->dport);
    struct gmtp_client *reporter = gmtp_get_client(&entry->clients->list,
             iph->daddr, gh->dport);

    if(relay == NULL && reporter == NULL)
        return 1;
    return 0;
}

unsigned int hook_func_post_routing(unsigned int hooknum, struct sk_buff *skb,
        const struct net_device *in, const struct net_device *out,
        int (*okfn)(struct sk_buff *))
{
    int ret = NF_ACCEPT;
    struct iphdr *iph = ip_hdr(skb);

    if(gmtp_info->relay_enabled == 0)
        goto out;

    if(iph->protocol == IPPROTO_GMTP) {

        struct gmtp_hdr *gh = gmtp_hdr(skb);
        struct gmtp_inter_entry *entry;

        entry = gmtp_inter_lookup_media(gmtp_inter.hashtable,
                gh->flowname);
        if(entry == NULL)
            return NF_ACCEPT;

        if(gh->type == GMTP_PKT_DATA) {
            if(!gmtp_local_ip(iph->daddr) ||
                GMTP_SKB_CB(skb)->jumped) {
                if(gmtp_inter_has_clients(skb, entry))
                    goto out;
            }
        }

        switch(gh->type) {
        case GMTP_PKT_REGISTER:
            ret = gmtp_inter_register_out(skb, entry);
            break;
        case GMTP_PKT_REGISTER_REPLY:
            ret = gmtp_inter_register_reply_out(skb, entry);
            break;
        case GMTP_PKT_DATA:
            if(gmtp_local_ip(iph->daddr)
                    || GMTP_SKB_CB(skb)->jumped) {
                ret = gmtp_inter_data_out(skb, entry);
            }
            break;
        case GMTP_PKT_ACK:
            ret = gmtp_inter_ack_out(skb, entry);
            break;
        case GMTP_PKT_RESET:
        case GMTP_PKT_CLOSE:
            ret = gmtp_inter_close_out(skb, entry);
            break;
        }
    }

out:
    return ret;
}

static void register_hooks(void)
{
    /****
    nfho_pre_routing.hook = hook_func_pre_routing;
    nfho_pre_routing.hooknum = NF_INET_PRE_ROUTING;
    nfho_pre_routing.pf = PF_INET;
    nfho_pre_routing.priority = NF_IP_PRI_FIRST;
    nf_register_hook(&nfho_pre_routing);

    nfho_local_in.hook = hook_func_local_in;
    nfho_local_in.hooknum = NF_INET_LOCAL_IN;
    nfho_local_in.pf = PF_INET;
    nfho_local_in.priority = NF_IP_PRI_FIRST;
    nf_register_hook(&nfho_local_in);

    nfho_local_out.hook = hook_func_local_out;
    nfho_local_out.hooknum = NF_INET_LOCAL_OUT;
    nfho_local_out.pf = PF_INET;
    nfho_local_out.priority = NF_IP_PRI_FIRST;
    nf_register_hook(&nfho_local_out);

    nfho_post_routing.hook = hook_func_post_routing;
    nfho_post_routing.hooknum = NF_INET_POST_ROUTING;
    nfho_post_routing.pf = PF_INET;
    nfho_post_routing.priority = NF_IP_PRI_FIRST;
    nf_register_hook(&nfho_post_routing);
    ****/
}

static int __init gmtp_inter_init(void)
{
    int ret = 0;

    gmtp_pr_func();

    if(gmtp_info == NULL) {
        gmtp_print_error("gmtp_info is NULL...");
        ret = -ENOBUFS;
        goto out;
    }

    gmtp_inter.capacity = CAPACITY_DEFAULT;
    gmtp_inter.buffer_len = 0;
    gmtp_inter.kreporter = GMTP_REPORTER_DEFAULT_PROPORTION - 1;

    /* TODO Why initial rx per flow is 5% of capacity of channel? */
    gmtp_inter.ucc_rx = DIV_ROUND_CLOSEST(gmtp_inter.capacity * 10, 100);

    gmtp_inter.total_bytes_rx = 0;
    gmtp_inter.total_rx = 0;
    gmtp_inter.ucc_bytes = 0;
    gmtp_inter.ucc_rx_tstamp = 0;
    gmtp_inter.rx_rate_wnd = 100;
    memset(&gmtp_inter.mcst, 0, 4 * sizeof(unsigned char));

    gmtp_inter.hashtable = gmtp_inter_create_hashtable(64);
    if(gmtp_inter.hashtable == NULL) {
        gmtp_print_error("Cannot create hashtable...");
        ret = -ENOMEM;
        goto out;
    }

    gmtp_info->relay_enabled = 1; /* Enabling gmtp-inter */

    gmtp_inter.h_user = INT_MAX; /* TODO Make it user defined */
    gmtp_inter.worst_rtt = GMTP_MIN_RTT_MS;

    pr_info("Configuring GMTP-UCC timer...\n");
    timer_setup(&gmtp_inter.gmtp_ucc_timer, gmtp_ucc_equation_callback, 0);
    mod_timer(&gmtp_inter.gmtp_ucc_timer, jiffies + 1);

    register_hooks();

out:
    return ret;
}

static void unregister_hooks(void)
{
    /****
    nf_unregister_hook(&nfho_pre_routing);
    nf_unregister_hook(&nfho_local_in);
    nf_unregister_hook(&nfho_post_routing);
    ****/
}

static void __exit gmtp_inter_exit(void)
{
    gmtp_pr_func();

    gmtp_info->relay_enabled = 0;
    kfree_gmtp_inter_hashtable(gmtp_inter.hashtable);

    unregister_hooks();
    del_timer(&gmtp_inter.gmtp_ucc_timer);
}

module_init(gmtp_inter_init);
module_exit(gmtp_inter_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Mário André Menezes <mariomenezescosta@gmail.com>");
MODULE_AUTHOR("Wendell Silva Soares <wss@ic.ufal.br>");
MODULE_AUTHOR("Leandro Melo de Sales <leandro@ic.ufal.br>");
MODULE_DESCRIPTION("GMTP - Global Media Transmission Protocol");
