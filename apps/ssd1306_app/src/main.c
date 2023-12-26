/****************************************************************************************************/
/*                                           INCLUDE                                                */
/****************************************************************************************************/
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <stdlib.h>
#include <poll.h>
#include <string.h>
#include <sys/select.h>
#include <sys/time.h>
#include <signal.h>
#include <fcntl.h>
#include "ssd1306_ioctl.h"
#include "image.h"
/****************************************************************************************************/
/*                                           DEFINES                                                */
/****************************************************************************************************/
#define MAX_CMD_LEN          (16)
typedef void (*operation_func)(int fd);
typedef struct _opration
{
	char cmd[MAX_CMD_LEN];
	operation_func cb;
} OPERTAION;
/****************************************************************************************************/
/*                                           VARIABLES                                              */
/****************************************************************************************************/


/****************************************************************************************************/
/*                                       STATIC FUNCTIONS                                           */
/****************************************************************************************************/
/*
 *@description: 设备初始化
 *@param-fd:    设备描述符 
 *@return: NULL
*/
static void dev_init(int fd)
{
	int ret = 0;
	SSD1306_IOCTL_DATA ioctl_data = {0};
	ret = ioctl(fd, IOCTL_INIT, &ioctl_data);
	return;
}

/*
 *@description: 发送命令
 *@param-fd:    设备描述符 
 *@param-data:  命令内容
 *@return: NULL
*/
static void write_iic_command(int fd, unsigned char data)
{
	int ret = 0;
	SSD1306_IOCTL_DATA ioctl_data = {0};
	memcpy(&ioctl_data.data, &data, sizeof(data));
	ret = ioctl(fd, IOCTL_WRITE_CMD, &ioctl_data);	
	return;
}

/*
 *@description: 发送数据
 *@param-fd:    设备描述符 
 *@param-data:  数据内容
 *@return: NULL
*/
static void write_iic_data(int fd, unsigned char data)
{
	int ret = 0;
	SSD1306_IOCTL_DATA ioctl_data = {0};
	memcpy(&ioctl_data.data, &data, sizeof(data));
	ret = ioctl(fd, IOCTL_WRITE_DATA, &ioctl_data);
	return;
}

/*
 *@description: 绘制图形
 *@param-fd:    设备描述符 
 *@return: NULL
*/
static void picture(int fd)
{
	unsigned char x, y;
	unsigned int i = 0;
	for (y = 0; y < 8; y++)
	{
		write_iic_command(fd, 0xb0 + y);
		write_iic_command(fd, 0x0);
		write_iic_command(fd, 0x10);
		for (x = 0; x < 132; x++)
		{
			write_iic_data(fd, show[i++]);
		}
	}
	return;
}

/*
 *@description: 绘制全亮
 *@param-fd:    设备描述符 
 *@return: NULL
*/
static void picture_one(int fd)
{
	unsigned char x, y;
	for (y = 0; y < 8; y++)
	{
		write_iic_command(fd, 0xb0 + y);
		write_iic_command(fd, 0x0);
		write_iic_command(fd, 0x10);
		for (x = 0; x < 132; x++)
		{
			write_iic_data(fd, 0xFF);
		}
	}
	return;
}

/*
 *@description: 熄灭屏幕
 *@param-fd:    设备描述符 
 *@return: NULL
*/
static void picture_zero(int fd)
{
	unsigned char x, y;
	for (y = 0; y < 8; y++)
	{
		write_iic_command(fd, 0xb0 + y);
		write_iic_command(fd, 0x0);
		write_iic_command(fd, 0x10);
		for (x = 0; x < 132; x++)
		{
			write_iic_data(fd, 0x00);
		}
	}
	return;
}

//操作回调函数
static OPERTAION oprations[] = 
{
	{"init", dev_init},
	{"picture", picture},
	{"on", picture_one},
	{"off", picture_zero},
};
/****************************************************************************************************/
/*                                       PUBLIC FUNCTIONS                                           */
/****************************************************************************************************/
/*
 *@description: main主程序
 *@param-argc: argv数组元素个数
 *@param-argv: 具体参数
 *@return: 0 成功； 其它 失败
*/
int main(int argc, char *argv[])
{	
	int i;
	int fd;
	char *filename;
	char *operation;
	if (3 != argc)
	{
		printf("Please specify the filename of the ssd1306 device and the operation\n");
		return -1;
	}

	filename = argv[1];
	fd = open(filename, O_RDWR);
	if (fd < 0)
	{
		printf("Open %s failed\n", filename);
		return -1;
	}

	operation = argv[2];
	for (i = 0; i < sizeof(oprations)/sizeof(OPERTAION); i++)
	{
		if (0 == strcmp(operation, oprations[i].cmd))
		{
			oprations[i].cb(fd);
			break;
		}
	}

	close(fd);
	return 0;
}

