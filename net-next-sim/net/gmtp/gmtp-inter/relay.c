#include <linux/list.h>
#include <linux/etherdevice.h>

#include "../gmtp.h"
#include "../hash.h"
#include "hash-inter.h"

struct gmtp_relay *gmtp_create_relay(__be32 addr, __be16 port)
{
	struct gmtp_relay *new = kmalloc(sizeof(struct gmtp_relay),
			GFP_ATOMIC);

	if(new != NULL) {
		new->addr = addr;
		new->port = port;
		new->state = GMTP_CLOSED;
	}
	return new;
}

/**
 * Create and add a relay in the list of relays
 */
struct gmtp_relay *gmtp_list_add_relay(__be32 addr, __be16 port,
		struct list_head *head)
{
	struct gmtp_relay *newr = gmtp_create_relay(addr, port);

	if(newr == NULL) {
		gmtp_pr_error("Error while creating new relay...");
		return NULL;
	}

	INIT_LIST_HEAD(&newr->list);
	list_add_tail(&newr->list, head);
	return newr;
}

struct gmtp_relay *gmtp_inter_create_relay(struct sk_buff *skb,
		struct gmtp_inter_entry *entry)
{
	struct iphdr *iph = ip_hdr(skb);
	struct gmtp_hdr *gh = gmtp_hdr(skb);
	struct ethhdr *eth = eth_hdr(skb);
	struct gmtp_relay *relay;

	relay = gmtp_list_add_relay(iph->saddr, gh->sport, &entry->relays->list);
	if(relay != NULL) {
		++entry->nrelays;
		ether_addr_copy(relay->mac_addr, eth->h_source);
		relay->state = GMTP_CLOSED;
	}
	return relay;
}

void init_relay_buffer(struct gmtp_relay *relay)
{
	setup_timer(&relay->xmit_timer, gmtp_inter_xmit_timer,
			(unsigned long ) relay);
	relay->buffer = kmalloc(sizeof(struct sk_buff_head), GFP_KERNEL);
	skb_queue_head_init(relay->buffer);
}
EXPORT_SYMBOL_GPL(init_relay_buffer);

struct gmtp_relay* gmtp_get_relay(struct list_head *head,
		__be32 addr, __be16 port)
{
	struct gmtp_relay *relay;
	list_for_each_entry(relay, head, list)
	{
		if(relay->addr == addr && relay->port == port)
			return relay;
	}
	return NULL;
}
EXPORT_SYMBOL_GPL(gmtp_get_relay);

int gmtp_delete_relays(struct list_head *list, __be32 addr, __be16 port)
{
	struct gmtp_relay *relay, *temp;
	int ret = 0;

	pr_info("Searching for %pI4@%-5d\n", &addr, ntohs(port));
	pr_info("List of relays:\n");
	list_for_each_entry_safe(relay, temp, list, list)
	{
		pr_info("Relay: %pI4@%-5d\n", &relay->addr,
						ntohs(relay->port));
		if(addr == relay->addr && port == relay->port) {
			pr_info("Deleting relay: %pI4@%-5d\n", &relay->addr,
					ntohs(relay->port));
			list_del(&relay->list);
			kfree(relay);
			++ret;
		}
	}
	if(ret == 0)
		gmtp_pr_warning("Relay %pI4@%-5d was not found!", &addr,
				ntohs(port));

	return ret;
}

void print_gmtp_relay(struct gmtp_relay *r)
{
	pr_info("R: %pI4@%-5d\n", &r->addr, ntohs(r->port));
}
EXPORT_SYMBOL_GPL(print_gmtp_relay);

