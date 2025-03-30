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
#include <getopt.h>
#include "unix_sock.h"
/****************************************************************************************************/
/*                                           DEFINES                                                */
/****************************************************************************************************/
#define WAIT_TIME		(2)

#define BLE_CLI_LOG(lvl, fmt, ...) do \
{ \
	if (lvl >= g_ble_cli_dbglvl) \
	{ \
		printf("[BLE_CLI][%s:%d]"fmt, __func__, __LINE__, ##__VA_ARGS__); \
	} \
} while (0);

typedef enum
{
	DEBUG,
	INFO,
	WARN,
	ERROR,
	FATAL,
	OFF
} DBGLVL;

typedef enum
{
	AT_CMD,
	DATA
} OPT_VALUE;
/****************************************************************************************************/
/*                                           VARIABLES                                              */
/****************************************************************************************************/
DBGLVL g_ble_cli_dbglvl = DEBUG;

struct option l_opt_set[] = {
	{"at_cmd", required_argument, 0, AT_CMD},
	{"data", required_argument, 0, DATA},
	{0, 0, 0, 0} //结束标志
};
/****************************************************************************************************/
/*                                       STATIC FUNCTIONS                                           */
/****************************************************************************************************/
static void ble_cli_send_at_cmd(char *cmd)
{
	int cli_fd = 0;
	int ret;
	fd_set read_fds;
	struct timeval ti;
	UNIX_MSG msg;

	BLE_CLI_LOG(INFO, "AT CMD>>>%s\n", cmd);
	memset(&msg, 0, sizeof(UNIX_MSG));
	msg.header.type = MSG_TYPE_AT_CMD;
	msg.header.len = strlen(cmd);
	strncpy(msg.value.at_cmd, cmd, MAX_AT_CMD_LEN);

	cli_fd = unix_sock_init_client(NULL);
	ret = unix_sock_connect_server(cli_fd, BLE_UNIX_SRV_PATH);
	if (-1 == ret)
	{
		return;
	}

	ret = unix_sock_send_msg(cli_fd, &msg);
	if (0 >= ret)
	{
		BLE_CLI_LOG(ERROR, "send at_cmd failed\n");
		return;
	}
	
	FD_ZERO(&read_fds);
	FD_SET(cli_fd, &read_fds);
	ti.tv_sec = WAIT_TIME;
	ti.tv_usec = 0;
	ret = select(cli_fd + 1, &read_fds, NULL, NULL, &ti);
	if (-1 == ret)
	{

	}
	else if (0 == ret)
	{

	}
	else
	{
		if (FD_ISSET(cli_fd, &read_fds))
		{
			memset(&msg, 0, sizeof(UNIX_MSG));
			ret = unix_sock_recv_msg(cli_fd, &msg);
			if (0 == ret)
			{
				close(cli_fd);
				return;
			}
			BLE_CLI_LOG(INFO, "AT CMD<<<%s\n", msg.value.at_resp);
		}
	}
	close(cli_fd);
	return;
}

static void ble_cli_send_data(char *data)
{
	int cli_fd = 0;
	int ret;
	UNIX_MSG msg;

	BLE_CLI_LOG(INFO, "DATA>>>%s\n", data);
	memset(&msg, 0, sizeof(UNIX_MSG));
	msg.header.type = MSG_TYPE_TX_DATA;
	msg.header.len = strlen(data);
	strncpy(msg.value.tx_data, data, MAX_TX_DATA_LEN);

	cli_fd = unix_sock_init_client(NULL);
	ret = unix_sock_connect_server(cli_fd, BLE_UNIX_SRV_PATH);
	if (-1 == ret)
	{
		return;
	}

	ret = unix_sock_send_msg(cli_fd, &msg);
	if (0 >= ret)
	{
		BLE_CLI_LOG(ERROR, "send data failed\n");
	}

	return;
}
/****************************************************************************************************/
/*                                       PUBLIC FUNCTIONS                                           */
/****************************************************************************************************/
int main(int argc, char *argv[])
{
	int opt = 0;
	int opt_index = 0;

	while ((opt = getopt_long(argc, argv, "", l_opt_set, &opt_index)) != -1)
	{
		switch (opt)
		{
			case AT_CMD:
				ble_cli_send_at_cmd(optarg);
				break;
			case DATA:
				ble_cli_send_data(optarg);
			default:
				break;
		}
	}

	return 0;
}
