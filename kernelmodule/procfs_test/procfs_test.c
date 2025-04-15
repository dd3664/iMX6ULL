/****************************************************************************************************/
/*                                           INCLUDE                                                */
/****************************************************************************************************/
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/string.h>
#include <asm/uaccess.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>
/****************************************************************************************************/
/*                                           DEFINES                                                */
/****************************************************************************************************/

/****************************************************************************************************/
/*                                           VARIABLES                                              */
/****************************************************************************************************/


/****************************************************************************************************/
/*                                       STATIC FUNCTIONS                                           */
/****************************************************************************************************/
static void procfs_test_cb(void)
{
	unsigned long i, j, result;
	for (i = 0; i < 60000; i++) 
	{
		for (j = 0; j < 60000; j++)
		{
			result = i + j;
			// 防止编译器优化，避免循环被优化掉
			asm volatile("" ::: "memory");
		}
	}
	return;
}


static ssize_t procfs_test_write(struct file *filp, const char __user *buf, size_t size, loff_t *offt)
{
	char c;
	if (get_user(c, buf))
	{
		return -EFAULT;
	}
	switch (c)
	{
		case '0':
			break;
		case '1':
			procfs_test_cb();
			break;
		default:
			break;
	}
	return size;
}


static const struct file_operations procfs_test = {
	.write = procfs_test_write,
};

static void procfs_create_procfs(void)
{
    proc_create("procfs_test", 0, NULL, &procfs_test);
}
static void procfs_destroy_procfs(void)
{
    remove_proc_entry("procfs_test", NULL);
}

static int __init procfs_init(void)
{
    procfs_create_procfs();
    return 0;
}

static void __exit procfs_exit(void)
{
  procfs_destroy_procfs();
}
/****************************************************************************************************/
/*                                       PUBLIC FUNCTIONS                                           */
/****************************************************************************************************/
module_init(procfs_init);
module_exit(procfs_exit);

MODULE_LICENSE("GPL");
