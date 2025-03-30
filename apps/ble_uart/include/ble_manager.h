#ifndef __BLE_MANAGER_H
#define __BLE_MANAGER_H

/****************************************************************************************************/
/*                                           INCLUDE                                                */
/****************************************************************************************************/
#include "list.h"
#include "uart.h"
/****************************************************************************************************/
/*                                           DEFINES                                                */
/****************************************************************************************************/
#define MAX_BUFFER_SIZE             (1024)
#define MAX_PATH_LEN                (32)
#define MIN_DATA_LEN                (2)
#define DFT_UART_BAUD_RATE          (115200)
#define DFT_UART_DEV_PATH           "/dev/ttyUSB0"

#define BLE_UART_LOG(lvl, fmt, ...) do \
{ \
	if (lvl >= g_ble_uart_dbglvl) \
	{ \
		printf("[BLE_UART][%s:%d]"fmt, __func__, __LINE__, ##__VA_ARGS__); \
	} \
} while (0);

typedef enum
{
    BLE_STATE_INIT,
    BLE_STATE_CONN,
    BLE_STATE_DISCONN,
} BLE_STATE;

typedef enum 
{
    BLE_ROLE_MASTER,
    BLE_ROLE_SLAVE,
}BLE_ROLE;

typedef struct __ble_manager
{
    char uart_path[MAX_PATH_LEN];
    int uart_baud_rate;
    struct uart uart;
    int self_fd;
    BLE_STATE state;
    BLE_ROLE role;
    struct list_head  cmd_queue;
    struct list_head send_data_queue;
    struct list_head recv_data_queuq;
} BLE_MANAGER;

typedef enum
{
	DEBUG,
	INFO,
	WARN,
	ERROR,
	FATAL,
	OFF
} DBGLVL;
/****************************************************************************************************/
/*                                           VARIABLES                                              */
/****************************************************************************************************/
extern DBGLVL g_ble_uart_dbglvl;
/****************************************************************************************************/
/*                                       STATIC FUNCTIONS                                           */
/****************************************************************************************************/

/****************************************************************************************************/
/*                                       PUBLIC FUNCTIONS                                           */
/****************************************************************************************************/
#endif /* __BLE_MANAGER_H */