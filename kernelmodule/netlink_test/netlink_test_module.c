#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/netlink.h>
#include <net/sock.h>

#define NETLINK_CUSTOM 17
#define MAX_PAYLOAD 128

struct sock *nl_sock = NULL;

static void nl_test_recv_msg(struct sk_buff *skb)
{
    struct nlmsghdr *nlh;
    int pid, msg_size;
    char *msg;
    struct sk_buff *skb_out;
    int msg_type = 0;
    char *resp_msg = "Hello from kenel!";

    nlh = (struct nlmsghdr *)skb->data;
    msg = (char *)nlmsg_data(nlh);
    msg_size = nlh->nlmsg_len - NLMSG_HDRLEN;
    pid = nlh->nlmsg_pid;
    printk(KERN_INFO "Received message from PID %d: %s\n", pid, msg);

    //create response message
    skb_out = nlmsg_new(MAX_PAYLOAD, 0);
    if (!skb_out)
    {
        printk(KERN_ERR "Failed to allocate new skb\n");
        return;
    }
    nlh = nlmsg_put(skb_out, 0, 0, NLMSG_DONE, MAX_PAYLOAD, 0);
    NETLINK_CB(skb_out).dst_group = 0;
    strncpy(nlmsg_data(nlh), resp_msg, strlen(resp_msg));

    //Send the message back to the user space process
    nlmsg_unicast(nl_sock, skb_out, pid);
}

static int __init nl_test_init(void)
{
    struct netlink_kernel_cfg cfg = {
        .input = nl_test_recv_msg,
    };
    printk(KERN_INFO "Netlink test module init\n");

    nl_sock = netlink_kernel_create(&init_net, NETLINK_CUSTOM, &cfg);
    if (!nl_sock)
    {
        printk(KERN_ERR "Failed to create netlink socket\n");
        return -ENOMEM;
    }
    return 0;
}

static int __exit nl_test_exit(void)
{
    printk(KERN_INFO "Netlink test module exit\n");
    if (nl_sock)
    {
        netlink_kernel_release(nl_sock);
    }
}

module_init(nl_test_init);
module_exit(nl_test_exit);
MODULE_LICENSE("GPL");