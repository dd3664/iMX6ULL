/****************************************************************************************************/
/*                                           INCLUDE                                                */
/****************************************************************************************************/
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/sched.h>
/****************************************************************************************************/
/*                                           DEFINES                                                */
/****************************************************************************************************/
typedef struct _my_timer{
	struct timer_list timer;
	int interval_msecs;
	int count;
} MY_TIMER;
/****************************************************************************************************/
/*                                           VARIABLES                                              */
/****************************************************************************************************/
MY_TIMER l_timer;
static struct task_struct *my_thread;
DECLARE_WAIT_QUEUE_HEAD(my_wait_queue); //定义等待队列头

static bool l_wake_up = false; //等待条件
/****************************************************************************************************/
/*                                       STATIC FUNCTIONS                                           */
/****************************************************************************************************/
static int my_thread_func(void *data)
{
	while (!kthread_should_stop()) //检查是否有停止请求
	{
		//wait_event(my_wait_queue, l_wake_up); //进程等待条件的发生
		wait_event_interruptible(my_wait_queue, l_wake_up); //进程等待条件的发生，可以被信号中断
		printk("My kernel thread is running...\n");
		l_wake_up = false;
	}
	printk("My kernel thread is stoping...\n");
	return 0;
}

static void timer_callback(MY_TIMER *mytimer)
{
	//定时器到期执行的操作
	printk("Timer expired!\n");

	l_wake_up = true;
	//wake_up(&my_wait_queue);
	wake_up_interruptible(&my_wait_queue);

	//重新设置定时器
	if (0 != --mytimer->count)
	{
		mod_timer(&mytimer->timer, jiffies + msecs_to_jiffies(mytimer->interval_msecs));
	}
}

static void init_timer_with_parameters(MY_TIMER *mytimer, int interval_msecs, int count, void (*callback)(MY_TIMER *mytimer))
{
	mytimer->count = count;
	mytimer->interval_msecs = interval_msecs;
	init_timer(&mytimer->timer);
	mytimer->timer.function = callback;
	mytimer->timer.expires = jiffies + msecs_to_jiffies(interval_msecs);
	mytimer->timer.data = mytimer;

	add_timer(&mytimer->timer);
}
/****************************************************************************************************/
/*                                       PUBLIC FUNCTIONS                                           */
/****************************************************************************************************/
int __init thread_test_init(void)
{
	my_thread = kthread_run(my_thread_func, NULL, "my_thread");
	if (NULL == my_thread)
	{
		printk("Failed to create kernel thread.\n");
	}
	init_timer_with_parameters(&l_timer, 1000, 5, timer_callback);
	return 0;	
}

int __exit thread_test_exit(void)
{
	del_timer_sync(&l_timer.timer);
	if (my_thread)
	{
		l_wake_up = true;
		kthread_stop(my_thread);
		my_thread = NULL;
	}
	return 0;
}

module_init(thread_test_init);
module_exit(thread_test_exit);
MODULE_LICENSE("GPL");
