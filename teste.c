#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kthread.h>

#include <linux/errno.h>
#include <linux/types.h>

#include <linux/netdevice.h>
#include <linux/inetdevice.h>
#include <linux/ip.h>
#include <linux/in.h>
//#include <net/ipv4/devinet.h>
//
//
#include <linux/socket.h>
#include <asm/uaccess.h>
#include <asm/cacheflush.h>
#include <linux/syscalls.h>
#include <linux/delay.h>

struct socket *udpsock; 
u32 get_address(char *ifname) 
{ 
        struct net *net;
        struct net_device *dev; 
        u32 addr; 
        if ((dev = dev_get_by_name(net, ifname)) == NULL) 
                return -ENODEV; 
        else 
                printk("Found device\n"); 
        addr = inet_select_addr(dev, 0, RT_SCOPE_UNIVERSE); 
        if (!addr) 
              printk("Specify IP address on multicast interface.\n"); 
        
    printk(KERN_EMERG"TESTANDO essa merda %s\n",addr);
    return addr; 
}

int init_module(void) 
{ 
        int error; 
        struct sockaddr_in sin; 
         
        //struct msghdr   udpmsg; 
        //mm_segment_t    oldfs; 
       // struct iovec    iov; 
        char buffer[] = "A small test msg"; 
        printk("Elvis has entered the building\n"); 
        error = sock_create(PF_INET, SOCK_DGRAM, 0, &udpsock); 
     
        if (error<0) 
        { 
                printk("Error during creation of socket; terminating\n"); 
                return -1; 
        } 
        memset(&sin, 0, sizeof(sin)); 
                sin.sin_family = AF_INET; 
                        sin.sin_port = htons(9090); 
                                sin.sin_addr.s_addr = get_address("enp4s0"); 
                                         
               error =  udpsock->ops->bind(udpsock,(struct sockaddr *)&sin,sizeof(sin)); 
               if (error<0) { 
              printk("Error binding socket"); 
              return -1; 
               }

/*iov.iov_base    = (void *)buffer; 
        iov.iov_len = strlen(buffer); 
        udpmsg.msg_name = 0; 
        udpmsg.msg_namelen  = 0; 
        udpmsg.msg_iov  = &iov; 
        udpmsg.msg_iovlen   = 1; 
        udpmsg.msg_control  = NULL; 
        udpmsg.msg_controllen = 0; 
        udpmsg.msg_flags    = MSG_DONTWAIT | MSG_NOSIGNAL; 
oldfs = get_fs(); set_fs(KERNEL_DS); 
        error = sock_sendmsg(udpsock, &udpmsg, strlen(buffer)); 
        set_fs(oldfs); 
        if(error < 0) 
                printk("Error when sendimg msg\n"); */
         
        return 0; 
} 

void cleanup_module(void) 
{ 
        sys_close(udpsock); 
        printk("Elvis has left the building\n"); 
}  
