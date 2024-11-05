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
/****************************************************************************************************/
/*                                       STATIC FUNCTIONS                                           */
/****************************************************************************************************/
static void print_ip_mac(struct sk_buff *skb, unsigned int match_src_ip, unsigned int match_dest_ip, unsigned char *match_src_mac, unsigned char *match_dest_mac, char *call_func, int call_line)
{
	struct iphdr *ip_header;
    struct ethhdr *eth_header;
    unsigned char *src_mac;
    unsigned char *dest_mac;
    unsigned int src_ip;
	unsigned int dest_ip;

	// Check if the skb is valid
    if (!skb)
	{
        return;
	}

    // Extract Ethernet header
    eth_header = eth_hdr(skb);
    if (!eth_header)
	{
        return;
	}

    // Extract source and destination MAC addresses
    src_mac = eth_header->h_source;
    dest_mac = eth_header->h_dest;

    // Extract IP header
    ip_header = ip_hdr(skb);
    if (!ip_header)
	{
        return;
	}

    // Get source IP address
    src_ip = ip_header->saddr;
	dest_ip = ip_header->daddr;

	if ((0 != match_src_ip) && (src_ip != match_src_ip))
	{
		return;
	}

	if ((0 != match_dest_ip) && (dest_ip != match_dest_ip))
	{
		return;
	}

	if ((NULL != match_src_mac) && (0 != memcmp(src_mac, match_src_mac, 6)))
	{
		return;
	}

	if ((NULL != match_dest_mac) && (0 != memcmp(dest_mac, match_dest_mac, 6)))
	{
		return;
	}

    printk("[%s:%d] Packet received: Source IP: %pI4, Destination IP: %pI4, Source MAC: %pM, Destination MAC: %pM\n", call_func, call_line, &src_ip, &dest_ip, src_mac, dest_mac);
	return;
}

static unsigned int hook_func(void *priv, struct sk_buff *skb, const struct nf_hook_state *state)
{
    unsigned int match_src_ip = 0x6400a8c0; /* 192.168.0.100的十六进制 */
	unsigned char match_src_mac[6] = {0x8c, 0x32, 0x23, 0x1a, 0xe7, 0xe0};
	print_ip_mac(skb, match_src_ip, 0, match_src_mac, NULL, __func__, __LINE__);
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
