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
static void match_skb_by_ip(struct sk_buff *skb, const unsigned int match_src_ip, const unsigned int match_dest_ip, int *dbg_cnt, int dbg_freq, int *dbg_switch)
{
	struct iphdr *iph;
	unsigned int src_ip;
	unsigned int dest_ip;
	
	if (!skb)
	{
		goto out;
	}

	iph = ip_hdr(skb);
	if (!iph)
	{
		goto out;
	}

	src_ip = iph->saddr;
	dest_ip = iph->daddr;

	if ((0 != match_src_ip) && (src_ip != match_src_ip))
	{
		goto out;
	}

	if ((0 != match_dest_ip) && (dest_ip != match_dest_ip))
	{
		goto out;
	}
	
	if (*dbg_cnt < dbg_freq - 1)
	{
		(*dbg_cnt)++;
		goto out;
	}
	else
	{
		*dbg_cnt = 0;
	}

	*dbg_switch = 1;
	return;

out:
	*dbg_switch = 0;
	return;
}

static void match_skb_by_mac(struct sk_buff *skb, unsigned char *match_src_mac, unsigned match_dest_mac, int *dbg_cnt, int dbg_freq, int *dbg_switch)
{
	struct ethhdr *eth_header;
	unsigned char *src_mac;
    unsigned char *dest_mac;

	if (!skb)
	{
        goto out;
	}

    eth_header = eth_hdr(skb);
    if (!eth_header)
	{
        goto out;
	}

    src_mac = eth_header->h_source;
    dest_mac = eth_header->h_dest;

	if ((NULL != match_src_mac) && (0 != memcmp(src_mac, match_src_mac, ETH_ALEN)))
	{
		goto out;
	}

	if ((NULL != match_dest_mac) && (0 != memcmp(dest_mac, match_dest_mac, ETH_ALEN)))
	{
		goto out;
	}

	if (*dbg_cnt < dbg_freq - 1)
	{
		(*dbg_cnt)++;
		goto out;
	}
	else
	{
		*dbg_cnt = 0;
	}

	*dbg_switch = 1;
	return;

out:
	*dbg_switch = 0;
	return;
}

static void dump_ip(struct sk_buff *skb, int dbg_switch, const char *call_func, const int call_line)
{
	struct iphdr *iph;
	unsigned int src_ip;
	unsigned int dest_ip;

	if (!dbg_switch)
	{
		return;
	}
	
	if (!skb)
	{
		return;
	}

	iph = ip_hdr(skb);
	if (!iph)
	{
		return;
	}

	src_ip = iph->saddr;
	dest_ip = iph->daddr;

	printk("[%s:%d] Source IP:%pI4, Destination IP:%pI4, skb->dev:%s\n", call_func, call_line, &src_ip, &dest_ip, skb->dev->name);
	return;
}

static void dump_mac(struct sk_buff *skb, int dbg_switch, const char *call_func, const int call_line)
{
	struct ethhdr *eth_header;
	unsigned char *src_mac;
    unsigned char *dest_mac;

	if (!dbg_switch)
	{
		return;
	}

	if (!skb)
	{
		return;
	}

	eth_header = eth_hdr(skb);
	if (!eth_header)
	{
		return;
	}

	src_mac = eth_header->h_source;
    dest_mac = eth_header->h_dest;

	printk("[%s:%d] Source Mac:%pM, Destination Mac:%pM, skb->dev:%s\n", call_func, call_line, src_mac, dest_mac, skb->dev->name);
	return;
}

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
    hook_ops.hook = hook_func;            // Hook function
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
