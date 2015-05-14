#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/netfilter.h>
#include <linux/netfilter_ipv4.h>
#include <linux/skbuff.h>
#include <linux/ip.h>
#include <linux/timer.h>
#include <linux/inet.h>
#include <linux/dirent.h>
#include <linux/inetdevice.h>
#include <linux/crypto.h>
#include <linux/err.h>
#include <linux/scatterlist.h>
#include <linux/if.h>
#include <linux/ioctl.h>

#include <net/sock.h>
#include <net/sch_generic.h>

#include <uapi/linux/gen_stats.h>
#include <uapi/linux/types.h>

#include <uapi/linux/gmtp.h>
#include <linux/gmtp.h>
#include "../gmtp/gmtp.h"
#include "gmtp-intra.h"
 
#define SHA1_LENGTH    16
#define GET_MAC_ADDRESS  0
#define GET_IP_ADDRESS   1

extern const char *gmtp_packet_name(const int);
extern const char *gmtp_state_name(const int);
extern void flowname_str(__u8* str, const __u8* flowname);

static struct nf_hook_ops nfho_in;
static struct nf_hook_ops nfho_out;

struct gmtp_intra gmtp;
 
int bytes_added(int sprintf_return)
{
    return (sprintf_return > 0)? sprintf_return : 0;
}

void flowname_strn(__u8* str, const __u8 *buffer, int length)
{
	
    int i;
	for(i = 0; i < length; ++i){
		sprintf(&str[i*2], "%02x", buffer[i]);
        //printk("testando = %02x\n", buffer[i]);
    }
}

__u8 gmtp_interfaces_md5(unsigned char *buf)
{
    struct scatterlist sg;
    struct crypto_hash *tfm;
    struct hash_desc desc;
    unsigned char output[SHA1_LENGTH];
    //unsigned char buf[] = "README";
    size_t buf_size = sizeof(buf) - 1;
    int i;
    
    printk(KERN_INFO "sha1: %s\n\n\n\n\n", __FUNCTION__);
      
    printk(KERN_INFO "bUFFER = %s\n", buf);

    tfm = crypto_alloc_hash("md5", 0, CRYPTO_ALG_ASYNC);
    if (IS_ERR(tfm)) {
        printk(KERN_ERR"allocation failed\n");
        return 0;
    }
 
    desc.tfm = tfm;
    desc.flags = 0;
 
    crypto_hash_init(&desc);
       
    sg_init_one(&sg, buf, buf_size);
    crypto_hash_update(&desc, &sg, buf_size);
    crypto_hash_final(&desc, output);
 
    printk(KERN_INFO"resultado ------------------------- %d \n",buf_size);

    for (i = 0; i < SHA1_LENGTH; i++) {
        printk(KERN_INFO "%x", output[i]);
    }
    printk(KERN_INFO "\n\n\n\n\n---------------\n");

    __u8 md5[21];
    flowname_strn(md5, output, SHA1_LENGTH);
    printk("saida md5 = %s\n",md5);
 
    crypto_free_hash(tfm);
 
    return md5;
}

void gmtp_intra_relay_get_devices (int option)
{
    /*int  option = GET_IP_ADDRESS;*/
    /*int  option = GET_MAC_ADDRESS;*/

    printk(KERN_INFO"\n\n\n");
    
    struct socket *sock = NULL;
    struct net_device *dev = NULL;

    struct in_device *in_dev;
    struct in_ifaddr *if_info;

    struct net *net;
    int i, j, retval, length = 0;
    char mac_address[6];
    char *ip_address;
    char buffer[50];
    unsigned int hex = 0XABC123FF;
    __u8 *str[30];

    retval = sock_create(AF_INET, SOCK_STREAM, 0, &sock);
    net = sock_net (sock->sk);
            
            for(i = 2; (dev = dev_get_by_index_rcu(net,i)) != NULL; ++i){
            
                if(option == GET_MAC_ADDRESS){
   
                    memcpy(&mac_address, dev->dev_addr, 6);
                    printk(KERN_DEBUG"SIZE of dev_addr = %d\n",
                                        sizeof(dev->dev_addr));
                    printk(KERN_DEBUG"Interface[%d] MAC = %x:%x:%x:%x:%x:%x\n",i,
                              mac_address[0],mac_address[1],
                              mac_address[2],mac_address[3], 
                              mac_address[4],mac_address[5]);
                    for(j = 0; j < 6; ++j){

                        printk(KERN_DEBUG"testando boladao = %x \n",mac_address[j]);
                                        
                    }
                    flowname_strn(&str,mac_address,6);
                    printk(KERN_DEBUG"[%d]\ntestando string = %s \n",i,str);
                    length += bytes_added(sprintf(buffer+length, str));

                }

                /** TODO Option == IP dá merda... */
                if(option ==  GET_IP_ADDRESS){    
                    
                    in_dev = (struct in_device * )dev->ip_ptr;

                    if(in_dev == NULL)
                        printk(KERN_DEBUG"in_dev == NULL\n");

                    if_info = in_dev->ifa_list;
                    for(;if_info;if_info = if_info->ifa_next){
                        if(if_info != NULL){
                            printk(KERN_DEBUG"if_info->ifa_address=%pI4\n", &if_info->ifa_address);
                            break;    
                        }
                    }   

                }
                if(length != 0)
                    printk(KERN_DEBUG"Concatenado = %s \n\n",buffer);

   
            }

    sock_release(sock);
    return buffer;
}
/*{
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

}*/

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

__be32 get_mcst_v4_addr(void)
{
	__be32 mcst_addr;
	unsigned char *base_channel = "\xe0\xc0\x00\x00"; /* 224.192.0.0 */
	unsigned char *channel = kmalloc(4 * sizeof(unsigned char), GFP_KERNEL);

	gmtp_print_function();

	if(channel == NULL) {
		gmtp_print_error("Cannot assign requested multicast address");
		return -EADDRNOTAVAIL;
	}

	memcpy(channel, base_channel, 4 * sizeof(unsigned char));

	channel[3] += gmtp.mcst[3]++;

	/**
	 * From: base_channel (224.192. 0 . 0 )
	 * to:   max_channel  (239.255.255.255)
	 *                     L0  L1  L2  L3
	 */
	if(gmtp.mcst[3] > 255) {  /* L3 starts with 1 */
		gmtp.mcst[3] = 0;
		gmtp.mcst[2]++;
	}
	if(gmtp.mcst[2] > 255) {
		gmtp.mcst[2] = 0;
		gmtp.mcst[1]++;
	}
	if(gmtp.mcst[1] > 63) { /* 255 - 192 */
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

void gmtp_buffer_add(struct gmtp_flow_info *info, struct sk_buff *newsk)
{
	skb_queue_tail(info->buffer, skb_copy(newsk, GFP_ATOMIC));
	info->buffer_size += newsk->len;
}

struct sk_buff *gmtp_buffer_dequeue(struct gmtp_flow_info *info)
{
	struct sk_buff *skb = skb_dequeue(info->buffer);
	if(skb != NULL)
		info->buffer_size -= skb->len;
	return skb;
}

struct gmtp_flow_info *gmtp_intra_get_info(
		struct gmtp_intra_hashtable *hashtable, const __u8 *media)
{
	struct gmtp_relay_entry *entry =
			gmtp_intra_lookup_media(gmtp.hashtable, media);

	if(entry != NULL)
		return entry->info;

	return NULL;
}

unsigned int hook_func_in(unsigned int hooknum, struct sk_buff *skb,
		const struct net_device *in, const struct net_device *out,
		int (*okfn)(struct sk_buff *))
{
	int ret = NF_ACCEPT;
	struct iphdr *iph = ip_hdr(skb);

	if(in == NULL)
		goto exit;

	if(iph->protocol == IPPROTO_GMTP) {

		struct gmtp_hdr *gh = gmtp_hdr(skb);

		if(gh->type != GMTP_PKT_DATA && gh->type != GMTP_PKT_FEEDBACK) {
			gmtp_print_debug("GMTP packet: %s (%d)",
					gmtp_packet_name(gh->type), gh->type);
			print_packet(skb, true);
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
		case GMTP_PKT_DATA:
			ret = gmtp_intra_data_rcv(skb);
			break;
		case GMTP_PKT_FEEDBACK:
			ret = gmtp_intra_feedback_rcv(skb);
			break;
		case GMTP_PKT_CLOSE:
			ret = gmtp_intra_close_rcv(skb);
			break;
		}

	}

exit:
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
		goto exit;

	if(iph->protocol == IPPROTO_GMTP) {

		gh = gmtp_hdr(skb);
		if(gh->type != GMTP_PKT_DATA) {
			gmtp_print_debug("GMTP packet: %s (%d)",
					gmtp_packet_name(gh->type), gh->type);
			print_packet(skb, false);
			print_gmtp_packet(iph, gh);
		}

		switch(gh->type) {
		case GMTP_PKT_DATA:
			ret = gmtp_intra_data_out(skb);
			break;
		case GMTP_PKT_CLOSE:
			ret = gmtp_intra_close_out(skb);
			break;
		}
	}

exit:
	return ret;
}

int init_module()
{
	int ret = 0;
	gmtp_pr_func();
	gmtp_print_debug("Starting GMTP-Intra");

	gmtp.total_rx = 1;
	memset(&gmtp.mcst, 0, 4*sizeof(unsigned char));

	gmtp.hashtable = gmtp_intra_create_hashtable(64);
	if(gmtp.hashtable == NULL) {
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
	gmtp_pr_func();
	gmtp_print_debug("Finishing GMTP-Intra");

	kfree_gmtp_intra_hashtable(gmtp.hashtable);

	nf_unregister_hook(&nfho_in);
	nf_unregister_hook(&nfho_out);
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Mário André Menezes <mariomenezescosta@gmail.com>");
MODULE_AUTHOR("Wendell Silva Soares <wss@ic.ufal.br>");
MODULE_DESCRIPTION("GMTP - Global Media Transmission Protocol");
