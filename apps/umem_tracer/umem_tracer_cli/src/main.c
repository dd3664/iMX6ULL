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
#include <getopt.h>
#include "umem_tracer.h"
#include "unix_sock_msg.h"

/****************************************************************************************************/
/*                                           DEFINES                                                */
/****************************************************************************************************/
#define MAX_BUFFER_SIZE				(64)
#define MAX_WAIT_TIME				(2)
#define MAX_FDS						(5)
#define UMT_CLI_LOG(fmt, ...) printf("[UMT_CLI] [%s:%d]"fmt, __func__, __LINE__, ##__VA_ARGS__);

typedef struct __umt_cli
{
	int pid;
	UMT_MSG msg;
} UMT_CLI;
/****************************************************************************************************/
/*                                           VARIABLES                                              */
/****************************************************************************************************/

/****************************************************************************************************/
/*                                       STATIC FUNCTIONS                                           */
/****************************************************************************************************/
static void send_msg(UMT_CLI *umt_cli)
{
	int client_sock;
	struct sockaddr_un server_addr;
	struct timeval ti;
	fd_set read_fds;
	int ret;
	ssize_t size;
	char buffer[MAX_BUFFER_SIZE] = {0};

	client_sock = socket(AF_UNIX, SOCK_STREAM, 0);
	if (-1 == client_sock)
	{
		UMT_CLI_LOG("create client sock failed\n");
		exit(EXIT_FAILURE);
	}

	memset(&server_addr, 0, sizeof(struct sockaddr_un));
	server_addr.sun_family = AF_UNIX;
	snprintf(server_addr.sun_path, sizeof(server_addr.sun_path) - 1, "%s%d", SERVER_UNIX_PATH, umt_cli->pid);
	if (-1 == connect(client_sock, (struct sockaddr *)&server_addr, sizeof(struct sockaddr_un)))
	{
		UMT_CLI_LOG("connect to %s failed\n", server_addr.sun_path);
		exit(EXIT_FAILURE);
	}
	size = send(client_sock, &umt_cli->msg, sizeof(UMT_MSG), 0);

	FD_ZERO(&read_fds);
	FD_SET(client_sock, &read_fds);
	ti.tv_sec = MAX_WAIT_TIME;
	ti.tv_usec = 0;
	ret = select(MAX_FDS, &read_fds, NULL, NULL, &ti);
	if (-1 == ret)
	{
		UMT_CLI_LOG("select failed\n");
		goto out;
	}
	else if (0 == ret)
	{
		UMT_CLI_LOG("wait for resp timeout\n");
		goto out;
	}
	else
	{
		if (FD_ISSET(client_sock, &read_fds))
		{
			size = recv(client_sock, buffer, sizeof(buffer), 0);
			if (0 == size)
			{
				/* socket has been closed */
				UMT_CLI_LOG("Please see result in /tmp/umem_trace_data\n");
				goto out;
			}
			else
			{
				UMT_CLI_LOG("recv msg form server\n");
			}
		}
	}
	
out:
	close(client_sock);
	return;

}
/****************************************************************************************************/
/*                                       PUBLIC FUNCTIONS                                           */
/****************************************************************************************************/
int main(int argc, char *argv[])
{
	UMT_CLI utm_cli;
	int opt;
	const char opt_set[] = "p:a:s";
	while (-1 != (opt = getopt(argc, argv, opt_set)))
	{
		switch (opt)
		{
			case 'p':
				utm_cli.pid = atoi(optarg);
				break;
			case 's':
				utm_cli.msg.type = MSG_SHOW_STAT;
				break;
			case 'd':
				break;
			default:
				UMT_CLI_LOG("Usage: %s [-p pid] [-s] [-d dbglvl]\n", argv[0]);
				break;
		}
	}
	send_msg(&utm_cli);
	return 0;
}
