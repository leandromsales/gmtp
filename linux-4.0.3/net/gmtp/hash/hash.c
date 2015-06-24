/*
 * hash.c
 *
 *  Created on: 27/02/2015
 *      Author: mario
 */

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/string.h>

#include "hash.h"

extern void gmtp_del_client_list(struct gmtp_client_entry *entry);

struct gmtp_hashtable *gmtp_create_hashtable(unsigned int size)
{
	int i;
	struct gmtp_hashtable *nt;

	gmtp_print_function();
	gmtp_print_debug("Size of gmtp_hashtable: %d", size);

	if(size < 1)
		return NULL;

	nt = kmalloc(sizeof(struct gmtp_hashtable), GFP_KERNEL);
	if(nt == NULL)
		return NULL;

	nt->client_table = kmalloc(sizeof(struct gmtp_client_entry*) * size,
			GFP_KERNEL);

	nt->server_table = kmalloc(sizeof(struct gmtp_server_entry*) * size,
			GFP_KERNEL);

	if((nt->client_table == NULL) || (nt->server_table == NULL))
		return NULL;

	for(i = 0; i < size; ++i) {
		nt->client_table[i] = NULL;
		nt->server_table[i] = NULL;
	}

	nt->size = size;
	return nt;
}

void kfree_gmtp_hashtable(struct gmtp_hashtable *hashtable)
{
	int i;
	struct gmtp_client_entry *c_entry, *ctmp;
	struct gmtp_server_entry *s_entry, *stmp;

	gmtp_print_function();

	if(hashtable == NULL)
		return;

	for(i = 0; i < hashtable->size; ++i) {
		c_entry = hashtable->client_table[i];
		while(c_entry != NULL) {
			ctmp = c_entry;
			gmtp_del_client_list(ctmp);
			c_entry = c_entry->next;
			kfree(ctmp);
		}
	}

	for(i = 0; i < hashtable->size; ++i) {
		s_entry = hashtable->server_table[i];
		while(s_entry != NULL) {
			stmp = s_entry;
			s_entry = s_entry->next;
			kfree(stmp);
		}
	}

	kfree(hashtable->client_table);
	kfree(hashtable->server_table);
	kfree(hashtable);
}
