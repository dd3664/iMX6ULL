#ifndef __UNIX_SOCK_H_
#define __UNIX_SOCK_H_

/****************************************************************************************************/
/*                                           INCLUDE                                                */
/****************************************************************************************************/

/****************************************************************************************************/
/*                                           DEFINES                                                */
/****************************************************************************************************/
#define MAX_LISTEN_BACKLOG              (5)

#define BLE_UNIX_SRV_PATH               "/tmp/ble_unix_srv"
#define MAX_AT_CMD_LEN                  (32)
#define MAX_AT_RESP_LEN                 (512)
#define MAX_TX_DATA_LEN                    (512)

typedef enum __msg_type
{
    MSG_TYPE_AT_CMD,
    MSG_TYPE_AT_RESP,
    MSG_TYPE_TX_DATA
} MSG_TYPE;

typedef struct __msg_header
{
    MSG_TYPE type;
    unsigned int len;
} MSG_HEADER;

typedef struct __unix_msg
{
    MSG_HEADER header;
    union
    {
        char at_cmd[MAX_AT_CMD_LEN];
        char at_resp[MAX_AT_RESP_LEN];
        char tx_data[MAX_TX_DATA_LEN];
    } value;
} UNIX_MSG;
/****************************************************************************************************/
/*                                           VARIABLES                                              */
/****************************************************************************************************/

/****************************************************************************************************/
/*                                       STATIC FUNCTIONS                                           */
/****************************************************************************************************/
int unix_sock_init_server(char *path);
int unix_sock_init_client(char *path);
int unix_sock_connect_server(int sockfd, char *srv_path);
int unix_sock_send_msg(int sockfd, UNIX_MSG *msg);
int unix_sock_recv_msg(int sockfd, UNIX_MSG *msg);

/****************************************************************************************************/
/*                                       PUBLIC FUNCTIONS                                           */
/****************************************************************************************************/

#endif /* __UNIX_SOCK_H_ */