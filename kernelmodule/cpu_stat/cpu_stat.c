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
#include <linux/timer.h>
#include <linux/kallsyms.h>
#include <linux/kprobes.h>
/****************************************************************************************************/
/*                                           DEFINES                                                */
/****************************************************************************************************/

/****************************************************************************************************/
/*                                           VARIABLES                                              */
/****************************************************************************************************/
struct kprobe kp;
/****************************************************************************************************/
/*                                       STATIC FUNCTIONS                                           */
/****************************************************************************************************/
static int (*get_cmdline_fn)(struct task_struct *task, char *buffer, int buflen);

int handler_pre(struct kprobe *p, struct pt_regs *regs)
{
	struct task_struct *task = current;
    printk(KERN_INFO "Kprobe pre-handler: Fuction do_exit is about to be called, PID=%d, command=%s\n",task->pid, task->comm);
    return 0;
}


/*
 *@description: 驱动入口函数
 *@param: 无
 *return: 无
*/
static int __init cpu_stat_init(void)
{
	struct task_struct *task;
	char cmd[64] = {0};
	
	//初始化kprobe结构
    kp.pre_handler = handler_pre;
    kp.symbol_name = "do_exit"; // 要跟踪的内核函数名

    //注册kprobe
    register_kprobe(&kp); 

	get_cmdline_fn = (int (*)(struct task_struct *, char *, int)) kallsyms_lookup_name("get_cmdline");
	for_each_process(task)
	{
		struct thread_group_cputimer *cputimer = &task->signal->cputimer;
		//memset(cmd, 0, sizeof(cmd));
		//get_cmdline_fn(task, cmd, sizeof(cmd));
		printk("PID=%d, Command=%s %s, utime=%llu, stime=%llu, cputimer->cputime.utime=%llu, cputimer->cputime.stime=%llu\n", task->pid, task->comm, (unsigned long long)task->utime, (unsigned long long)task->stime, (unsigned long long)cputimer->cputime.utime, (unsigned long long)cputimer->cputime.stime);
	}
	return 0;
}

/*
 *@description: 驱动出口函数
 *@param: 无
 *@return: 0
*/
static int __exit cpu_stat_exit(void)
{
	printk("Module unloaded\n");
	unregister_kprobe(&kp);
	return 0;
}

module_init(cpu_stat_init);
module_exit(cpu_stat_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("dongyuqing");
/****************************************************************************************************/
/*                                       PUBLIC FUNCTIONS                                           */
/****************************************************************************************************/
