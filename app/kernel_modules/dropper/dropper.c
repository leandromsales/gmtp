#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/netfilter.h>
#include <linux/netfilter_ipv4.h>
#include <linux/skbuff.h>

static struct nf_hook_ops nfho;
char seq = 0;

unsigned int hook_func(unsigned int hooknum, struct sk_buff *skb,
		const struct net_device *in, const struct net_device *out,
		int (*okfn)(struct sk_buff *))
{
	int ret = NF_ACCEPT;

	if(in == NULL)
		goto exit;
        
    ++seq;
    
    /* Dropping packets */
	if(seq == 10) {
        /*pr_info("Discarding... %ld\n", seq);*/
        seq = 0;
        ret = NF_DROP;
    }

exit:
	return ret;
}

int init_module()
{
    pr_info("Initing Dropper Module\n");

	nfho.hook = hook_func;
	nfho.hooknum = NF_INET_PRE_ROUTING;
	nfho.pf = PF_INET;
	nfho.priority = NF_IP_PRI_FIRST;
	nf_register_hook(&nfho);
    
    return 0;
}

void cleanup_module()
{
	pr_info("Finishing Dropper Module\n");
	nf_unregister_hook(&nfho);
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Wendell Silva Soares <wss@ic.ufal.br>");
MODULE_DESCRIPTION("Dropper - A module to drop packets");
