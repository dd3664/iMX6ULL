/****************************************************************************************************/
/*                                           INCLUDE                                                */
/****************************************************************************************************/
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/netfilter.h>
#include <linux/netfilter_ipv4.h>
#include <linux/ip.h>
#include <linux/if_ether.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/inet.h>
#include "dump_ip_mac.h"
/****************************************************************************************************/
/*                                           DEFINES                                                */
/****************************************************************************************************/

/****************************************************************************************************/
/*                                           VARIABLES                                              */
/****************************************************************************************************/
static struct nf_hook_ops hook_ops; // Netfilter hook options structure
int g_dbg_cnt = 0;
int g_dbg_switch = 0;
int g_dbg_cnt1 = 0;
int g_dbg_switch1 = 0;
/****************************************************************************************************/
/*                                       STATIC FUNCTIONS                                           */
/****************************************************************************************************/
static unsigned int hook_func(void *priv, struct sk_buff *skb, const struct nf_hook_state *state)
{
	unsigned char match_src_mac[ETH_ALEN] = {0x8c, 0x32, 0x23, 0x1a, 0xe7, 0xe0};
	unsigned char match_dest_mac[ETH_ALEN] = {0x08, 0x90, 0x90, 0x90, 0x90, 0x90};
	match_skb_by_ip(skb, 0x6400a8c0, 0xc800a8c0, &g_dbg_cnt, 1, &g_dbg_switch); /* 0x6400a8c0: 192.168.0.100的十六进制，网络字节序 */
	match_skb_by_mac(skb, match_src_mac, match_dest_mac, &g_dbg_cnt1, 1, &g_dbg_switch1);
	dump_ip(skb, g_dbg_switch, __func__, __LINE__);
	dump_mac(skb, g_dbg_switch1, __func__, __LINE__);
    return NF_ACCEPT;
}

// Initialize the Netfilter hook
static int __init nf_module_init(void)
{
    hook_ops.hook = (nf_hookfn*)hook_func;            // Hook function
    hook_ops.hooknum = NF_INET_PRE_ROUTING; // Hook point (Pre-routing)
    hook_ops.pf = PF_INET;                // Protocol Family (IPv4)
    hook_ops.priority = NF_IP_PRI_FIRST;  // Priority

    nf_register_hook(&hook_ops); // Register the hook

    printk(KERN_INFO "Netfilter module loaded.\n");
    return 0;
}

static void __exit nf_module_exit(void)
{
    nf_unregister_hook(&hook_ops); 
    printk(KERN_INFO "Netfilter module unloaded.\n");
}
/****************************************************************************************************/
/*                                       PUBLIC FUNCTIONS                                           */
/****************************************************************************************************/

module_init(nf_module_init);
module_exit(nf_module_exit);
MODULE_LICENSE("GPL");
