#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/netfilter_ipv4.h>
#include <linux/netfilter.h>
#include <linux/skbuff.h>
//#include <linux/tcp.h>
#include <linux/netdevice.h>
#include <net/sock.h>

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

    //create list node
   // INIT_LIST_HEAD(&gmtp_list.list);
    //struct gmtp_net_device_addr *new_entry;
    
    //new_entry = kmalloc(sizeof(struct gmtp_net_device_addr),GFP_KERNEL);

   // memcpy(new_entry->dev_addr,"testando",MAX_ADDR_LEN);
  
    //INIT_LIST_HEAD(&new_entry->list);

    //add a list node
    //list_add(&new_entry->list, &gmtp_list.list);

    //printk(KERN_INFO"Printing the list");

//    list_for_each_entry(new_entry, &gmtp_list.list, list){
  //          printk(KERN_INFO"Dev_ADDR = %s",new_entry->dev_addr);
//}
printk(KERN_INFO"\n\n\n");

    nfho.hook = hook_func;
    nfho.hooknum = 0; //NF_IP_PRE_ROUTING
    nfho.pf = PF_INET;
    nfho.priority = NF_IP_PRI_FIRST;
    nf_register_hook(&nfho);
    

    struct ifreq tmp;
    struct socket *sock = NULL;
    struct net_device *dev = NULL;
    struct net *net;
    int retval;
    char mac_address[6];
    retval = sock_create(AF_INET, SOCK_STREAM, 0, &sock);
    net = sock_net (sock->sk);

    dev = dev_get_by_name_rcu(net, "eth0");

    memcpy(&mac_address, dev->dev_addr, 6);
    printk(KERN_DEBUG"ip address =%x:%x:%x:%x:%x:%x\n",
              mac_address[0],mac_address[1],
              mac_address[2],mac_address[3], 
              mac_address[4],mac_address[5]);
 
    sock_release(sock);
    return 0; 
}

void cleanup_module(){
    nf_unregister_hook(&nfho);
}

