/****************************************************************************************************/
/*                                           INCLUDE                                                */
/****************************************************************************************************/
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
/****************************************************************************************************/
/*                                           DEFINES                                                */
/****************************************************************************************************/
typedef struct _my_timer{
	struct timer_list timer;
	int interval_msecs;
	int count;
} MY_TIMER;
typedef struct _my_tasklet_data {
	int data;
} MY_TASKLET_DATA;
/****************************************************************************************************/
/*                                           VARIABLES                                              */
/****************************************************************************************************/
MY_TIMER l_timer;
MY_TASKLET_DATA l_tasklet_data;

/****************************************************************************************************/
/*                                       STATIC FUNCTIONS                                           */
/****************************************************************************************************/
static void my_tasklet_handler(unsigned long data)
{
	MY_TASKLET_DATA *param = (MY_TASKLET_DATA*)data;
	printk("tasklet executed with data: %d\n", param->data);
}

DECLARE_TASKLET(my_tasklet, my_tasklet_handler, &l_tasklet_data); //声明tasklet，并将l_tasklet_data结构体指针作为参数传递给tasklet处理函数

static void kill_my_tasklet(void)
{
	tasklet_kill(&my_tasklet); //销毁tasklet
}

static void timer_callback(MY_TIMER *mytimer)
{
	//定时器到期执行的操作
	printk("Timer expired!\n");

	l_tasklet_data.data++;
	tasklet_schedule(&my_tasklet);

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
int __init timer_test_init(void)
{
	init_timer_with_parameters(&l_timer, 1000, 5, timer_callback);

	return 0;	
}

int __exit timer_test_exit(void)
{
	del_timer_sync(&l_timer.timer);
	kill_my_tasklet();
	return 0;
}

module_init(timer_test_init);
module_exit(timer_test_exit);
MODULE_LICENSE("GPL");
