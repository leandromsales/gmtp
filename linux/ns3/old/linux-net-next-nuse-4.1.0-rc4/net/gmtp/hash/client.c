/*
 * client.c
 *
 *  Created on: 20/08/2015
 *      Author: wendell
 */

#include <linux/list.h>

#include "../gmtp.h"
#include "hash.h"

struct gmtp_client *gmtp_create_client(__be32 addr, __be16 port,
		__u8 max_nclients)
{
	struct gmtp_client *new = kmalloc(sizeof(struct gmtp_client),
	GFP_ATOMIC);

	if(new != NULL) {
		new->addr = addr;
		new->port = port;
		new->max_nclients = max_nclients;
		new->nclients = 0;
		new->ack_rx_tstamp = 0;

		new->clients = 0;
		new->reporter = 0;
		new->rsock = 0;
		new->mysock = 0;
	}
	return new;
}

/**
 * Create and add a client in the list of clients
 */
struct gmtp_client *gmtp_list_add_client(u32 id, __be32 addr, __be16 port,
		__u8 max_nclients, struct list_head *head)
{
	struct gmtp_client *newc = gmtp_create_client(addr, port, max_nclients);

	if(newc == NULL) {
		gmtp_pr_error("Error while creating new gmtp_client...");
		return NULL;
	}

	newc->id = id;
	gmtp_pr_info("New client (%u): ADDR=%pI4@%-5d",
			newc->id, &addr, ntohs(port));

	INIT_LIST_HEAD(&newc->list);
	list_add_tail(&newc->list, head);
	return newc;
}

struct gmtp_client* gmtp_get_first_client(struct list_head *head)
{
	struct gmtp_client *client;
	list_for_each_entry(client, head, list)
	{
		return client;
	}
	return NULL;
}

struct gmtp_client* gmtp_get_client(struct list_head *head,
		__be32 addr, __be16 port)
{
	struct gmtp_client *client;
	list_for_each_entry(client, head, list)
	{
		if(client->addr == addr && client->port == port)
			return client;
	}
	return NULL;
}

struct gmtp_client* gmtp_get_client_by_id(struct list_head *head,
		unsigned int id)
{
	struct gmtp_client *client;
	list_for_each_entry(client, head, list)
	{
		if(client->id == id)
			return client;
	}
	return NULL;
}

int gmtp_delete_clients(struct list_head *list, __be32 addr, __be16 port)
{
	struct gmtp_client *client, *temp;
	int ret = 0;

	pr_info("Searching for %pI4@%-5d\n", &addr, ntohs(port));
	pr_info("List of clients:\n");
	list_for_each_entry_safe(client, temp, list, list)
	{
		pr_info("Client: %pI4@%-5d\n", &client->addr,
							ntohs(client->port));
		if(addr == client->addr && port == client->port) {
			pr_info("Deleting client: %pI4@%-5d\n", &client->addr,
					ntohs(client->port));
			list_del(&client->list);
			kfree(client);
			++ret;
		}
	}
	if(ret == 0)
		gmtp_pr_warning("Client %pI4@%-5d was not found!", &addr,
				ntohs(port));

	return ret;
}

void print_gmtp_client(struct gmtp_client *c)
{
	pr_info("C: %pI4@%-5d\n", &c->addr, ntohs(c->port));
}
EXPORT_SYMBOL_GPL(print_gmtp_client);

