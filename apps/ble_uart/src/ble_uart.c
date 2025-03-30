/****************************************************************************************************/
/*                                           INCLUDE                                                */
/****************************************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "eloop.h"
#include "ble_manager.h"
#include "unix_sock.h"
/****************************************************************************************************/
/*                                           DEFINES                                                */
/****************************************************************************************************/
#define WAIT_TIME		(2)
/****************************************************************************************************/
/*                                           VARIABLES                                              */
/****************************************************************************************************/
DBGLVL g_ble_uart_dbglvl = DEBUG;
/****************************************************************************************************/
/*                                       STATIC FUNCTIONS                                           */
/****************************************************************************************************/
static void ble_dump_data(char *data, int len)
{
    int i = 0;
	
    for (i = 0; i < len; i++)
    {
        printf("%02x ", (unsigned char)data[i]);
    }
    printf("\n");
}


static void ble_uart_recv_handle(int sock, void *eloop_ctx, void *sock_ctx)
{
	BLE_MANAGER *ble_mnger = (BLE_MANAGER *)eloop_ctx;
	char buffer[512];
	int len;

	len = uart_recv(&ble_mnger->uart, buffer, sizeof(buffer));
	if (len <= MIN_DATA_LEN)
	{
		/* 串口干扰 */
		return;
	}

	buffer[len] = '\0';
	BLE_UART_LOG(DEBUG, "buffer:%s\n", buffer);
	ble_dump_data(buffer, len);

	return;
}

static void ble_uart_init(BLE_MANAGER *ble_mnger)
{
	ble_mnger->uart.uart_path = strlen(ble_mnger->uart_path) ? ble_mnger->uart_path : DFT_UART_DEV_PATH;
	ble_mnger->uart.baud_rate = ble_mnger->uart_baud_rate ? ble_mnger->uart_baud_rate : DFT_UART_BAUD_RATE;
	ble_mnger->uart.blocking_mode = 1; /* 阻塞模式 */

	if (0 == uart_open(&ble_mnger->uart))
	{
		exit(-1);
	}
}

static BLE_MANAGER* ble_mnger_init(void)
{
	BLE_MANAGER *ble_mnger = NULL;

	ble_mnger = malloc(sizeof(BLE_MANAGER));
	if (NULL == ble_mnger)
	{
		exit(-1);
	}
	memset(ble_mnger, 0, sizeof(BLE_MANAGER));
	return ble_mnger;
}

static int ble_parse_param(int argc, char *argv[], BLE_MANAGER *ble_mnger)
{
	int opt;
	const char opt_set[] = "d:b:";
	while ((opt = getopt(argc, argv, opt_set)) != -1)
	{
		switch (opt)
		{
			case 'd':
				strncpy(ble_mnger->uart_path, optarg, strlen(optarg) + 1);
				break;
			case 'b':
				ble_mnger->uart_baud_rate = atoi(optarg);
				break;
			default:
				printf("Usage: %s [-d dev_path] [-b baud_rate]\n", argv[0]);
				break;
		}
	}
	return 0;
}

static int ble_self_fd_init(BLE_MANAGER *ble_mnger)
{
	ble_mnger->self_fd = unix_sock_init_server(BLE_UNIX_SRV_PATH);
	if (-1 == ble_mnger->self_fd)
	{
		exit(-1);
	}
	return 0;
}

static void ble_process_at_cmd(int sock, BLE_MANAGER *ble_mnger, char *at_cmd)
{
	char buffer[MAX_AT_CMD_LEN + 2] = {0}; /* \r\n */
	fd_set read_fds;
	struct timeval ti;
	int ret;
	UNIX_MSG resp_msg;

	snprintf(buffer, sizeof(buffer), "%s\r\n", at_cmd);
	uart_send(&ble_mnger->uart, buffer, sizeof(buffer));

again:
	FD_ZERO(&read_fds);
	FD_SET(ble_mnger->uart.uart_fd, &read_fds);
	ti.tv_sec = WAIT_TIME;
	ti.tv_usec = 0;
	ret = select(ble_mnger->uart.uart_fd + 1, &read_fds, NULL, NULL, &ti);
	if (-1 == ret)
	{

	}
	else if (0 == ret)
	{

	}
	else
	{
		if (FD_ISSET(ble_mnger->uart.uart_fd, &read_fds))
		{
			memset(&resp_msg, 0, sizeof(UNIX_MSG));
			ret = uart_recv(&ble_mnger->uart, resp_msg.value.at_resp, MAX_AT_RESP_LEN);
			if (ret <= 0)
			{
				return;
			}
			if (ret <= MIN_DATA_LEN)
			{
				/* 串口干扰 */
				goto again;
			}
			resp_msg.header.type = MSG_TYPE_AT_RESP;
			resp_msg.header.len = ret;
			unix_sock_send_msg(sock, &resp_msg);
		}
	}
	return;
}

static void ble_process_tx_data(int sock, BLE_MANAGER *ble_mnger, char *data)
{	
	uart_send(&ble_mnger->uart, data, strlen(data));
}

static void ble_cli_recv_handle(int sock, void *eloop_ctx, void *sock_ctx)
{
	BLE_MANAGER *ble_mnger = (BLE_MANAGER *)eloop_ctx;
	UNIX_MSG msg;
	int ret;

	memset(&msg, 0, sizeof(UNIX_MSG));
	ret = unix_sock_recv_msg(sock, &msg);
	if (0 == ret)
	{
		/* socket closed */
		eloop_unregister_read_sock(sock);
		close(sock);
		return;
	}

	switch (msg.header.type)
	{
	case MSG_TYPE_AT_CMD:
		ble_process_at_cmd(sock, ble_mnger, msg.value.at_cmd);
		break;
	case MSG_TYPE_TX_DATA:
		ble_process_tx_data(sock, ble_mnger, msg.value.tx_data);
	default:
		break;
	}

	return;
}

static void ble_self_fd_recv_handle(int sock, void *eloop_ctx, void *sock_ctx)
{
	BLE_MANAGER *ble_mnger = (BLE_MANAGER *)eloop_ctx;
	int client_fd = 0;
	struct sockaddr_un client_addr;
	socklen_t client_addr_len;

	memset(&client_addr, 0, sizeof(struct sockaddr_un));
	client_addr_len = sizeof(struct sockaddr_un);
	client_fd = accept(sock, (struct sockaddr*)&client_addr, &client_addr_len);
	if (-1 == client_fd)
	{
		perror("accept");
		return;
	}
	eloop_register_read_sock(client_fd, ble_cli_recv_handle, ble_mnger, NULL);
	return;
}
/****************************************************************************************************/
/*                                       PUBLIC FUNCTIONS                                           */
/****************************************************************************************************/
int main(int argc, char *argv[])
{
	BLE_MANAGER *ble_mnger = NULL;

	ble_mnger = ble_mnger_init();
	ble_parse_param(argc, argv, ble_mnger);
	ble_uart_init(ble_mnger);
	ble_self_fd_init(ble_mnger);
	eloop_init(NULL);
	eloop_register_read_sock(ble_mnger->uart.uart_fd, ble_uart_recv_handle, ble_mnger, NULL);
	eloop_register_read_sock(ble_mnger->self_fd, ble_self_fd_recv_handle, ble_mnger, NULL);
	eloop_run();
	eloop_destroy();
	return 0;
}
