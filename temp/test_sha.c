/*
 * sha1_mod.c
 * A kernel module using SHA1 and Linux kernel crypto API
 * Reference: http://stackoverflow.com/questions/3869028/how-to-use-cryptoapi-in-the-linux-kernel-2-6
 * Jun 9, 2014
 * root@davejingtian.org
 * http://davejingtian.org
 */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/crypto.h>
#include <linux/err.h>
#include <linux/scatterlist.h>
 
#define SHA1_LENGTH     20
 
static int __init sha1_init(void)
{
    struct scatterlist sg;
    struct crypto_hash *tfm;
    struct hash_desc desc;
    unsigned char output[SHA1_LENGTH];
    unsigned char buf[10];
    int i, j;
 
    printk(KERN_INFO "sha1: %s\n", __FUNCTION__);
 
    memset(buf, 'A', 10);
    memset(output, 0x00, SHA1_LENGTH);
 
    tfm = crypto_alloc_hash("sha1", 0, CRYPTO_ALG_ASYNC);
    if (IS_ERR(tfm)) {
    printk(KERN_ERR "daveti: tfm allocation failed\n");
    return 0;
    }
 
    desc.tfm = tfm;
    desc.flags = 0;
 
    //crypto_hash_init(&desc);
    //daveti: NOTE, crypto_hash_init is needed
    //for every new hasing!
 
    for (j = 0; j < 3; j++) {
    crypto_hash_init(&desc);
    sg_init_one(&sg, buf, 10);
    crypto_hash_update(&desc, &sg, 10);
    crypto_hash_final(&desc, output);
 
    for (i = 0; i < 20; i++) {
        printk(KERN_ERR "%02x", output[i]);
    }
    printk(KERN_INFO "\n---------------\n");
    memset(output, 0x00, SHA1_LENGTH);
    }
 
    crypto_free_hash(tfm);
 
    return 0;
}
 
static void __exit sha1_exit(void)
{
    printk(KERN_INFO "sha1: %s\n", __FUNCTION__);
}
 
module_init(sha1_init);
module_exit(sha1_exit);
 
MODULE_LICENSE("GPL");
MODULE_AUTHOR("daveti");
