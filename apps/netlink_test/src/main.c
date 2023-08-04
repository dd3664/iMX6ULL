#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <linux/netlink.h>

#define NETLINK_CUSTOM 17
#define MAX_PAYLOAD 128

int main()
{
    int nl_sock;
    struct sockaddr_nl src_addr, dest_addr;
    struct nlmsghdr *nlh;
    struct msghdr msg;
    struct iovec iov;
    int ret;

    //Create a netlink socket
    nl_sock = socket(AF_NETLINK, SOCK_RAW, NETLINK_CUSTOM);
    if (nl_sock < 0)
    {
        perror("socket");
        exit(EXIT_FAILURE);
    }
    memset(&src_addr, 0, sizeof(src_addr));
    src_addr.nl_family = AF_NETLINK;
    src_addr.nl_pid = getpid(); //PID of the current process

    bind(nl_sock, (struct sockaddr *)&src_addr, sizeof(src_addr));

    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.nl_family = AF_NETLINK;
    dest_addr.nl_pid = 0; //For kernel
    dest_addr.nl_groups = 0; //Unicast

    //Allcate memory for the netlink message
    nlh = (struct nlmsghdr *) malloc(NLMSG_SPACE(MAX_PAYLOAD));
    memset(nlh, 0, NLMSG_SPACE(MAX_PAYLOAD));
    nlh->nlmsg_len = NLMSG_SPACE(MAX_PAYLOAD);
    nlh->nlmsg_pid = getpid();
    nlh->nlmsg_flags = 0;

    //Copy the message to be sent
    strcpy(NLMSG_DATA(nlh), "Hello from user space!");

    //Setup the message structure
    memset(&iov, 0, sizeof(iov));
    iov.iov_base = (void *)nlh;
    iov.iov_len = nlh->nlmsg_len;

    memset(&msg, 0, sizeof(msg));
    msg.msg_name = (void *)&dest_addr;
    msg.msg_namelen = sizeof(dest_addr);
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    //Send the message to the kernel
    ret =  sendmsg(nl_sock, &msg, 0);
    if (ret == -1)
    {
        perror("sendmsg");
        close(nl_sock);
        exit(EXIT_FAILURE);
    }

    //Receive the response from the kernel
    recvmsg(nl_sock, &msg, 0);
    printf("Received message from kernel: %s\n", (char *)NLMSG_DATA(nlh));

    //Close the socket
    close(nl_sock);
    
    return 0;
}

