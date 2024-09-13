/****************************************************************************************************/
/*                                           INCLUDE                                                */
/****************************************************************************************************/
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/time.h>
#include <signal.h>
#include <sys/stat.h>
#include "umem_tracer.h"
#include "unix_sock_msg.h"

/****************************************************************************************************/
/*                                           DEFINES                                                */
/****************************************************************************************************/
#define THREAD_STACK_SIZE				(8 * 1024 * 1024)
#define MAX_FDS							(32)
#define MAX_LISTEN_BACKLOG				(5)
#define LOOP_INTERVAL					(10)
/****************************************************************************************************/
/*                                           VARIABLES                                              */
/****************************************************************************************************/
extern UMEM_TRACER *g_tracer;
/****************************************************************************************************/
/*                                       STATIC FUNCTIONS                                           */
/****************************************************************************************************/
extern void save_data_to_file(void);

static void msg_callback(UMT_MSG *msg)
{
	switch (msg->type)
	{
		case MSG_SHOW_STAT:
			save_data_to_file();
			break;
		default:
			UMT_LOG(ERROR, "default msg\n");
			break;
	}
	return;
}

static void recv_msg(int fd) 
{
	UMT_LOG(DEBUG, "enter recv_msg\n");
	UMT_MSG msg = {0};
	int client_fd = -1;
	ssize_t size;

	client_fd = accept(fd, NULL, NULL);
	if (0 > client_fd)
	{
		UMT_LOG(ERROR, "accept failed\n");
		return;
	}
	size = recv(client_fd, &msg, sizeof(UMT_MSG), 0);
	if (0 > size)
	{
		UMT_LOG(ERROR, "recv msg failed\n");
		goto out;
	}
	else if (0 == size)
	{
		UMT_LOG(DEBUG, "client socket has been closed\n");
		goto out;
	}
	else
	{
		msg_callback(&msg);
	}
	
out:
	if (client_fd > 0)
	{
		close(client_fd);
	}
	return;
}

static void *ipc_loop(void)
{
	struct sockaddr_un server_addr;
	struct timeval ti;
	fd_set read_fds;
	int ret;

	snprintf(g_tracer->server_path, MAX_PATH_LEN, "%s%d", SERVER_UNIX_PATH, g_tracer->pid);
	if (0 > access(SERVER_UNIX_PATH, F_OK))
	{
		if (0 > mkdir(SERVER_UNIX_PATH, 0755))
		{
			UMT_LOG(FATAL, "create unix sock dir failed\n");
			exit(EXIT_FAILURE);
		}

	}

	g_tracer->server_sock = socket(AF_UNIX, SOCK_STREAM, 0);
	if (-1 == g_tracer->server_sock)
	{
		UMT_LOG(FATAL, "create umem_tracer socket failed\n");
		exit(EXIT_FAILURE);
	}
	memset(&server_addr, 0, sizeof(struct sockaddr_un));
	server_addr.sun_family = AF_UNIX;
	strncpy(server_addr.sun_path, g_tracer->server_path, sizeof(server_addr.sun_path) - 1);
	unlink(g_tracer->server_path); 

	if (-1 == bind(g_tracer->server_sock, (struct sockaddr *)&server_addr, sizeof(struct sockaddr_un)))
	{
		UMT_LOG(FATAL, "bind server socket failed\n");
		goto fail;
	}

	if (-1 == listen(g_tracer->server_sock, MAX_LISTEN_BACKLOG))
	{
		UMT_LOG(FATAL, "listen server socket failed\n");
		goto fail;
	}
	while (1)
	{
		FD_ZERO(&read_fds);
		FD_SET(g_tracer->server_sock, &read_fds);
		ti.tv_sec = LOOP_INTERVAL;
		ti.tv_usec = 0;
		ret = select(MAX_FDS, &read_fds, NULL, NULL, &ti);
		if (-1 == ret)
		{
			UMT_LOG(ERROR, "select error\n");
		}
		else if (0 == ret)
		{
			UMT_LOG(DEBUG, "select timeout\n");
			/* timeout, do nothing */
		}
		else
		{
			if (FD_ISSET(g_tracer->server_sock, &read_fds))
			{
				recv_msg(g_tracer->server_sock);
			}
		}
	}
	return NULL;

fail:
	close(g_tracer->server_sock);
	unlink(g_tracer->server_path);
	exit(EXIT_FAILURE);

}

/****************************************************************************************************/
/*                                       PUBLIC FUNCTIONS                                           */
/****************************************************************************************************/
void unix_sock_server_init(void)
{
	pthread_t tid;
	pthread_attr_t attr;

	pthread_attr_init(&attr);
	pthread_attr_setstacksize(&attr, THREAD_STACK_SIZE);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	pthread_create(&tid, &attr, (void *)ipc_loop, NULL);
	pthread_attr_destroy(&attr);
	return;
}

void unix_sock_server_exit(void)
{
	close(g_tracer->server_sock);
	unlink(g_tracer->server_path);
	return;
}
