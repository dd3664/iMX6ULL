/****************************************************************************************************/
/*                                           INCLUDE                                                */
/****************************************************************************************************/
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/ide.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/gpio.h>
#include <linux/cdev.h>
#include <linux/of_gpio.h>
#include <linux/semaphore.h>
#include <linux/timer.h>
#include <linux/i2c.h>
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include "ssd1306.h"
#include "ssd1306_ioctl.h"
/****************************************************************************************************/
/*                                           DEFINES                                                */
/****************************************************************************************************/
#define SSD1306_CNT          1
#define SSD1306_NAME         "ssd1306"

typedef struct _Ssd1306Dev
{
	dev_t devid; /* 设备号 */
	struct cdev cdev; /* cdev */
	struct class *class; /* 类 */
	struct device *device; /* 设备 */
	int major; /* 主设备号 */
	void *private_data; /* 私有数据 */
} Ssd1306Dev;

/****************************************************************************************************/
/*                                           VARIABLES                                              */
/****************************************************************************************************/
static Ssd1306Dev *ssd1306dev;
/****************************************************************************************************/
/*                                       STATIC FUNCTIONS                                           */
/****************************************************************************************************/
/*
 *@description: 发送命令
 *@param-dev: ssd1306设备
 *@param-cmd: 命令内容 
 *@return: 0 成功
*/
static int ssd1306_write_cmd(Ssd1306Dev *dev, unsigned char cmd)
{
	unsigned char b[2] = {0};
	struct i2c_msg msg;
	struct i2c_client *client = (struct i2c_client*)dev->private_data;

	b[0] = REG_CMD;
	b[1] = cmd;

	msg.addr = client->addr;
	msg.flags = 0;
	msg.buf = b;
	msg.len = 2;
	return i2c_transfer(client->adapter, &msg, 1);
}

/*
 *@description: 发送数据
 *@param-dev: ssd1306设备
 *@param-cmd: 数据内容 
 *@return: 0 成功
*/
static int ssd1306_write_data(Ssd1306Dev *dev, unsigned char data)
{
	unsigned char b[2] = {0};
	struct i2c_msg msg;
	struct i2c_client *client = (struct i2c_client*)dev->private_data;

	b[0] = REG_DATA;
	b[1] = data;

	msg.addr = client->addr;
	msg.flags = 0;
	msg.buf = b;
	msg.len = 2;

	return i2c_transfer(client->adapter, &msg, 1);

}

/*
 *@description: 设备初始化
 *@param-dev: ssd1306设备
 *@return: 0 成功
*/
static int ssd1306_dev_init(Ssd1306Dev *dev)
{
	ssd1306_write_cmd(dev, 0xAE);   //display off
	ssd1306_write_cmd(dev, 0x20);	//Set Memory Addressing Mode	
	ssd1306_write_cmd(dev, 0x10);	//00,Horizontal Addressing Mode;01,Vertical Addressing Mode;10,Page Addressing Mode (RESET);11,Invalid
	ssd1306_write_cmd(dev, 0xb0);	//Set Page Start Address for Page Addressing Mode,0-7
	ssd1306_write_cmd(dev, 0xc8);	//Set COM Output Scan Direction
	ssd1306_write_cmd(dev, 0x00);//---set low column address
	ssd1306_write_cmd(dev, 0x10);//---set high column address
	ssd1306_write_cmd(dev, 0x40);//--set start line address
	ssd1306_write_cmd(dev, 0x81);//--set contrast control register
	ssd1306_write_cmd(dev, 0xdf);
	ssd1306_write_cmd(dev, 0xa1);//--set segment re-map 0 to 127
	ssd1306_write_cmd(dev, 0xa6);//--set normal display
	ssd1306_write_cmd(dev, 0xa8);//--set multiplex ratio(1 to 64)
	ssd1306_write_cmd(dev, 0x3F);//
	ssd1306_write_cmd(dev, 0xa4);//0xa4,Output follows RAM content;0xa5,Output ignores RAM content
	ssd1306_write_cmd(dev, 0xd3);//-set display offset
	ssd1306_write_cmd(dev, 0x00);//-not offset
	ssd1306_write_cmd(dev, 0xd5);//--set display clock divide ratio/oscillator frequency
	ssd1306_write_cmd(dev, 0xf0);//--set divide ratio
	ssd1306_write_cmd(dev, 0xd9);//--set pre-charge period
	ssd1306_write_cmd(dev, 0x22); //
	ssd1306_write_cmd(dev, 0xda);//--set com pins hardware configuration
	ssd1306_write_cmd(dev, 0x12);
	ssd1306_write_cmd(dev, 0xdb);//--set vcomh
	ssd1306_write_cmd(dev, 0x20);//0x20,0.77xVcc
	ssd1306_write_cmd(dev, 0x8d);//--set DC-DC enable
	ssd1306_write_cmd(dev, 0x14);//
	ssd1306_write_cmd(dev, 0xaf);//--turn on oled panel 
	return 0;
}

/*
 *@description: 打开设备
 *@param-inode: 传递给驱动的inode
 *@param-filp: 设备文件，file结构体有个叫做private_data的成员变量，一般在open时将其指向设备结构体
 *@return: 0 成功
*/
static int ssd1306_open(struct inode *inode, struct file *filp)
{
	filp->private_data = ssd1306dev;

	return 0;
}


/*
 *@description: 从设备读取数据
 *@param-filp: 要打开的设备文件（文件描述符）
 *@param-buf: 要返回给用户空间的数据缓冲区
 *@param-cnt: 要读取的数据长度
 *@param-offt: 相对于文件首地址的偏移
 *@return: 读取的字节数，如果为负值，表示读取失败
*/
static ssize_t ssd1306_read(struct file *filp, char __user *buf, size_t size, loff_t *offt)
{
	char data[] = "Nothing to read!";
	int err;

	err = copy_to_user(buf, data, sizeof(data));

	return sizeof(data);
}

static ssize_t ssd1306_write(struct file *filp, char __user *buf, size_t size, loff_t *offt)
{
	char *kbuf = kmalloc(size, GFP_KERNEL);
	Ssd1306Dev *dev = filp->private_data;
	struct i2c_msg msg;
	struct i2c_client *client = (struct i2c_client*)dev->private_data;
	if (copy_from_user(kbuf, buf, size) != 0)
	{
		printk("[%s], copy from user error\n", __func__);
		return -1;
	}
	msg.addr = client->addr;
	msg.flags = 0;
	msg.buf = kbuf;
	msg.len = size;

	i2c_transfer(client->adapter, &msg, 1);
	kfree(kbuf);

	return size;
}
/*
 *@description: 关闭/释放设备,若在open中有申请资源需要在将其释放
 *@param-inode: 传递给驱动的inode
 *@param-filp： 设备文件描述符
 *return: 0 成功; 其它 失败
*/
static int ssd1306_release(struct inode *inode, struct file *filp)
{
	return 0;
}

/*
 *@description: ioctl入口函数
 *@param-filp： 设备文件描述符
 *param-cmd:    命令
 *param-arg:    参数
 *return: 0 成功; 其它 失败
*/
static int ssd1306_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	Ssd1306Dev *dev = filp->private_data;
	SSD1306_IOCTL_DATA data = {0};
	if (0 != copy_from_user(&data, (SSD1306_IOCTL_DATA *)arg, sizeof(SSD1306_IOCTL_DATA)))
	{
		printk("[%s], copy from user error\n", __func__);
		return -1;
	}
	switch (cmd)
	{	
		case IOCTL_INIT:
			ssd1306_dev_init(dev);
			break;
		case IOCTL_WRITE_CMD:
			ssd1306_write_cmd(dev, data.data);
			break;
		case IOCTL_WRITE_DATA:
			ssd1306_write_data(dev, data.data);
			break;
	}
	return 0;
}
/* ssd1306操作函数 */
static const struct file_operations ssd1306_ops = {
	.owner = THIS_MODULE,
	.open = ssd1306_open,
	.read = ssd1306_read, 
	.write = ssd1306_write,
	.release = ssd1306_release,
	.unlocked_ioctl = ssd1306_ioctl,
};

/*
 *@description: 驱动与设备匹配后的probe函数
 *@param-client: i2c设备
 *@param-id: i2cp设备id
 *@return: 0 成功；其他负值，失败
*/
static int ssd1306_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	ssd1306dev = kmalloc(sizeof(Ssd1306Dev), GFP_KERNEL);
	if (ssd1306dev == NULL)
	{
		printk("[%s], kmalloc ssd1306dev failed\n", __func__);
		return -1;
	}
	
	/* 构建设备号 */
	if (ssd1306dev->major)
	{
		ssd1306dev->devid = MKDEV(ssd1306dev->major, 0);
		register_chrdev_region(&ssd1306dev->devid, SSD1306_CNT, SSD1306_NAME);
	}
	else
	{
		alloc_chrdev_region(&ssd1306dev->devid, 0, SSD1306_CNT, SSD1306_NAME);
		ssd1306dev->major = MAJOR(ssd1306dev->devid);
	}

	/* 注册设备 */
	cdev_init(&ssd1306dev->cdev, &ssd1306_ops);
	cdev_add(&ssd1306dev->cdev, ssd1306dev->devid, SSD1306_CNT);

	/* 创建类 */
	ssd1306dev->class = class_create(THIS_MODULE, SSD1306_NAME);
	if (IS_ERR(ssd1306dev->class))
	{
		return PTR_ERR(ssd1306dev->class);
	}

	/* 创建设备 */
	ssd1306dev->device = device_create(ssd1306dev->class, NULL, ssd1306dev->devid, NULL, SSD1306_NAME);
	if (IS_ERR(ssd1306dev->device))
	{
		return PTR_ERR(ssd1306dev->device);
	}

	ssd1306dev->private_data = client;

	return 0;
}

/*
 *@description: 驱动remove函数
 *param-client: i2c设备
 *return: 0 成功; 其它负值 失败
*/
static int ssd1306_remove(struct i2c_client *client)
{
	/* 删除设备 */
	cdev_del(&ssd1306dev->cdev);
	unregister_chrdev_region(ssd1306dev->devid, SSD1306_CNT);

	/* 注销类和设备 */
	device_destroy(ssd1306dev->class, ssd1306dev->devid);
	class_destroy(ssd1306dev->class);
	
	kfree(ssd1306dev);

	return 0;
}

/* 传统方式匹配ID列表 */
static const struct i2c_device_id ssd1306_id[] = {
	{"ssd1306", 0},
	{}
};

/* 设备树匹配列表 */
static const struct of_device_id ssd1306_of_match[] = {
	{.compatible = "ssd1306"},
	{}
};

/* i2c驱动结构体 */
static struct i2c_driver ssd1306_driver = {
	.probe = ssd1306_probe,
	.remove = ssd1306_remove,
	.driver = {
		.owner = THIS_MODULE,
		.name = "ssd1306",
		.of_match_table = ssd1306_of_match,
	},
	.id_table = ssd1306_id,
};

/*
 *@description: 驱动入口函数
 *@param: 无
 *return: 无
*/
static int __init ssd1306_init(void)
{
	int ret = 0;
	ret = i2c_add_driver(&ssd1306_driver);
	return ret;
}

/*
 *@description: 驱动出口函数
 *@param: 无
 *@return: 0
*/
static int __exit ssd1306_exit(void)
{
	i2c_del_driver(&ssd1306_driver);
	return 0;
}

module_init(ssd1306_init);
module_exit(ssd1306_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("dongyuqing");
/****************************************************************************************************/
/*                                       PUBLIC FUNCTIONS                                           */
/****************************************************************************************************/
