#ifndef __DUMP_IP_MAC_H_
#define __DUMP_IP_MAC_H_

/****************************************************************************************************/
/*                                           INCLUDE                                                */
/****************************************************************************************************/
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
extern int g_dbg_switch;   /* 打印开关变量，根据实际命名调整 */

/****************************************************************************************************/
/*                                       STATIC FUNCTIONS                                           */
/****************************************************************************************************/
static void inline match_skb_by_ip(struct sk_buff *skb, const unsigned int match_src_ip, const unsigned int match_dest_ip, int *dbg_cnt, int dbg_freq, int *dbg_switch)
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

static void inline match_skb_by_mac(struct sk_buff *skb, unsigned char *match_src_mac, unsigned char *match_dest_mac, int *dbg_cnt, int dbg_freq, int *dbg_switch)
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

static void inline dump_ip(struct sk_buff *skb, int dbg_switch, const char *call_func, const int call_line)
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

static void inline dump_mac(struct sk_buff *skb, int dbg_switch, const char *call_func, const int call_line)
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
/****************************************************************************************************/
/*                                       PUBLIC FUNCTIONS                                           */
/****************************************************************************************************/

#endif /* __DUMP_IP_MAC_H_ */