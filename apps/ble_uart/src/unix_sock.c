/****************************************************************************************************/
/*                                           INCLUDE                                                */
/****************************************************************************************************/
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/time.h>
#include "unix_sock.h"
/****************************************************************************************************/
/*                                           DEFINES                                                */
/****************************************************************************************************/

/****************************************************************************************************/
/*                                           VARIABLES                                              */
/****************************************************************************************************/

/****************************************************************************************************/
/*                                       STATIC FUNCTIONS                                           */
/****************************************************************************************************/

/****************************************************************************************************/
/*                                       PUBLIC FUNCTIONS                                           */
/****************************************************************************************************/
int unix_sock_init_server(char *path)
{
    int sockfd;
    struct sockaddr_un  addr;

	if (NULL == path)
	{
		return -1;
	}
	
	//创建UNIX域套接字
    sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (-1 == sockfd)
    {
        perror("socket");
        return -1;
    }

    unlink(path);

	//设置套接字地址
    memset(&addr, 0, sizeof(struct sockaddr_un));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);

	//将套接字绑定到地址
    if (-1 == bind(sockfd, (struct sockaddr *)&addr, sizeof(struct sockaddr_un)))
    {
        perror("bind");
		close(sockfd);
        return -1;
    }

	//监听套接字
    if (-1 == listen(sockfd, MAX_LISTEN_BACKLOG))
    {
        perror("listen");
        close(sockfd);
        return -1;
    }
    return sockfd;
}

int unix_sock_init_client(char *path)
{
    int sockfd;
	struct sockaddr_un addr;

    sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (-1 == sockfd)
    {
        perror("socket");
        return -1;
    }

	if (NULL == path)
    {
		return sockfd;
	}

	unlink(path);

	//设置套接字地址
    memset(&addr, 0, sizeof(struct sockaddr_un));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);

	//将套接字绑定到地址
    if (-1 == bind(sockfd, (struct sockaddr *)&addr, sizeof(struct sockaddr_un)))
    {
        perror("bind");
		close(sockfd);
        return -1;
    }
	return sockfd;
}

int unix_sock_connect_server(int sockfd, char *srv_path)
{
	struct sockaddr_un addr;

    memset(&addr, 0, sizeof(struct sockaddr_un));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, srv_path, sizeof(addr.sun_path) - 1);

    if (-1 == connect(sockfd, (struct sockaddr *)&addr, sizeof(struct sockaddr_un)))
    {
        perror("connect");
        return -1;
    }
    return 0;
}

int unix_sock_send_msg(int sockfd, UNIX_MSG *msg)
{
	int len;
	int total_len;

	for (len = 0, total_len = 0; total_len < sizeof(MSG_HEADER) + msg->header.len; )
	{
		len = send(sockfd, msg + total_len, sizeof(MSG_HEADER) + msg->header.len, 0);
		total_len += len;
	}

	return total_len;
}

int unix_sock_recv_msg(int sockfd, UNIX_MSG *msg)
{
	int len;
	int total_len;

	for (len = 0, total_len = 0; total_len < sizeof(MSG_HEADER); )
	{
		len = recv(sockfd, &msg->header + total_len, sizeof(MSG_HEADER), 0);
		if (0 == len)
		{
			/* socket closed */
			return 0;
		}
		total_len += len;
	}
	for (len = 0, total_len = 0; total_len < msg->header.len; )
	{
		len = recv(sockfd, &msg->value + total_len, msg->header.len, 0);
		total_len += len;
	}

	return total_len + sizeof(MSG_HEADER);
}