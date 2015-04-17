#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/netfilter_ipv4.h>
#include <linux/netfilter.h>
#include <linux/skbuff.h>
//#include <linux/tcp.h>
#include <linux/netdevice.h>
#include <net/sock.h>
#include <linux/types.h>
#include <linux/list.h>
#include <linux/if_addr.h>
#include <linux/inetdevice.h>

#define GET_MAC_ADDRESS 0
#define GET_IP_ADDRESS 1

struct __netdev_adjacent{
    struct net_device *dev;
bool master;

u16 ref_nr;
void *private;

struct list_head list;
struct rcu_head rcu; 
};

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("LINKED LIST TEST");
MODULE_AUTHOR("MARIO BOLADO");

static struct nf_hook_ops nfho;


unsigned int hook_func (unsigned int hooknum, struct sk_buff *skb, const struct
net_device *in, const struct net_device *out, int (*okfn)(struct sk_buff *))
{
//    int i = 0;

 //   struct net_device *aux = kmalloc(sizeof(struct net_device),GFP_KERNEL);

   //     printk(KERN_DEBUG"MAC interface= %x:%x:%x:%x:%x:%x\n",
     //         in->dev_addr[0],in->dev_addr[1],
       //       in->dev_addr[2],in->dev_addr[3],
         //     in->dev_addr[4],in->dev_addr[5]);
//    return NULL;
}
struct gmtp_net_device_addr {
	char dev_addr[MAX_ADDR_LEN];
	struct list_head list; //aqui eh kernel list
};
//create list head
struct gmtp_net_device_addr gmtp_list;

int init_module(){
    int  option = GET_IP_ADDRESS;

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
   // printk(KERN_DEBUG"INDEX DEV = %d\n",dev->ifindex);

//    struct __netdev_adjacent *iter;
    
  //  list_for_each_entry(iter, &dev->adj_list.upper, list){
 //       dev = dev_get_by_name_rcu(net, "eth0");
    //    if(iter == NULL)
      //     {
        //    printk(KERN_DEBUG"essa merda ta nula iter");
          //      return 1;
            //    }
//         if(iter->dev == NULL)
  //         {
    //        printk(KERN_DEBUG"essa merda ta nula dev");
      //      return 1;
        //    }
        
          //  memcpy(&mac_address, iter->dev->dev_addr, 6);
//        printk(KERN_DEBUG"ip address =%x:%x:%x:%x:%x:%x\n",
  //            mac_address[0],mac_address[1],
    //          mac_address[2],mac_address[3], 
      //        mac_address[4],mac_address[5]);

       sock_release(sock);

}

void cleanup_module(){
    nf_unregister_hook(&nfho);
}

