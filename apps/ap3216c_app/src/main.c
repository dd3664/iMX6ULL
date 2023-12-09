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
/*
 *@description: main主程序
 *@param-argc: argv数组元素个数
 *@param-argv: 具体参数
 *@return: 0 成功； 其它 失败
*/
int main(int argc, char *argv[])
{
	int fd;
	char *filename;
	unsigned short databuf[3];
	unsigned short ir, als, ps;
	int ret = 0;

	if (2 != argc)
	{
		printf("Please specify the filename of the ap3216c device\n");
		return -1;
	}

	filename = argv[1];
	fd = open(filename, O_RDWR);
	if (fd < 0)
	{
		printf("Open %s failed\n", filename);
		return -1;
	}

	while(1)
	{
		ret = read(fd, databuf, sizeof(databuf));
		if (0 == ret) /* 数据读取成功 */
		{
			ir = databuf[0]; /* ir传感器数据 */
			als = databuf[1]; /* als传感器数据 */
			ps = databuf[2]; /* ps传感器数据 */
			printf("ir=%d, als=%d, ps=%d\n", ir, als, ps);
		}
		usleep(200000);
	}
	close(fd);
	return 0;
}

