/****************************************************************************************************/
/*                                           INCLUDE                                                */
/****************************************************************************************************/
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/file.h>
#include <linux/fs_struct.h>
/****************************************************************************************************/
/*                                           DEFINES                                                */
/****************************************************************************************************/
#define FILE_PATH "/tmp/kernel_rw_test"
#define MAX_BUF_LEN (128)
/****************************************************************************************************/
/*                                           VARIABLES                                              */
/****************************************************************************************************/


/****************************************************************************************************/
/*                                       STATIC FUNCTIONS                                           */
/****************************************************************************************************/
ssize_t kernel_read_file(const char *filename, char *buf, size_t len)
{
	struct file *file;
	mm_segment_t old_fs;
	ssize_t ret;

	//打开文件
	file = filp_open(filename, O_RDONLY, 0);
	if (IS_ERR(file))
	{
		printk("Failed to open file:%s\n", filename);
		return PTR_ERR(file);
	}

	//切换地址空间	
	old_fs = get_fs();
	set_fs(KERNEL_DS);

	//读取文件内容
	ret = vfs_read(file, buf, len, &file->f_pos);

	//回复地址空间
	set_fs(old_fs);

	//关闭文件
	filp_close(file, NULL);

	return ret;
}

ssize_t kernel_write_file(const char *filename, const char *buf, size_t len)
{
	struct file *file;
	mm_segment_t old_fs;
	ssize_t ret;

	//打开文件
	file = filp_open(filename, O_WRONLY | O_CREAT | O_APPEND, 0644);
	if (IS_ERR(file))
	{
		printk("Failed to open file:%s\n", filename);
		return PTR_ERR(file);
	}

	//切换地址空间	
	old_fs = get_fs();
	set_fs(KERNEL_DS);

	//写入文件内容
	ret = vfs_write(file, buf, len, &file->f_pos);

	//回复地址空间
	set_fs(old_fs);

	//关闭文件
	filp_close(file, NULL);

	return ret;
}

/****************************************************************************************************/
/*                                       PUBLIC FUNCTIONS                                           */
/****************************************************************************************************/
int __init rw_test_init(void)
{
	char buf_write[MAX_BUF_LEN] = "This is a kernel read/write test!\n";
	char buf_read[MAX_BUF_LEN] = {0};
	kernel_write_file(FILE_PATH, buf_write, MAX_BUF_LEN);
	kernel_read_file(FILE_PATH, buf_read, MAX_BUF_LEN);
	printk("buf_write: %s", buf_write);
	printk("buf_read: %s", buf_read);

	return 0;	
}

int __exit rw_test_exit(void)
{
	return 0;
}

module_init(rw_test_init);
module_exit(rw_test_exit);
MODULE_LICENSE("GPL");
