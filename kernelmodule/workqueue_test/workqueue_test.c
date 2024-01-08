/****************************************************************************************************/
/*                                           INCLUDE                                                */
/****************************************************************************************************/
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/tick.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
/****************************************************************************************************/
/*                                           DEFINES                                                */
/****************************************************************************************************/
typedef struct _my_timer{
	struct timer_list timer;
	int interval_msecs;
	int count;
} MY_TIMER;

typedef struct _my_workqueue{
	struct workqueue_struct *workqueue; //工作队列
	struct work_struct work; //工作项
	int parameter; //传递参数示例
} MY_WORKQUEUE;
/****************************************************************************************************/
/*                                           VARIABLES                                              */
/****************************************************************************************************/
MY_TIMER l_timer;
MY_WORKQUEUE l_workqueue;

/****************************************************************************************************/
/*                                       STATIC FUNCTIONS                                           */
/****************************************************************************************************/
static void work_queue_callback(struct work_struct *work)
{
	MY_WORKQUEUE *my_workqueue = container_of(work, MY_WORKQUEUE, work);
	printk("work queue callback executed, the parameter=%d\n", my_workqueue->parameter);
	msleep(1000);
}

static void timer_callback(MY_TIMER *mytimer)
{
	//定时器到期执行的操作
	printk("Timer expired!\n");

	l_workqueue.parameter = 10; //workqueue传参示例
	queue_work(l_workqueue.workqueue, &l_workqueue.work); //将工作项添加到自定义工作队列中
	//schedule_work(&l_workqueue.work); //添加工作项到系统默认工作队列中，与queue_work择其一即可

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

static void init_workqueue(MY_WORKQUEUE *my_workqueue)
{
	my_workqueue->workqueue = create_workqueue("my_workqueue"); //创建工作队列
	INIT_WORK(&my_workqueue->work, work_queue_callback);  //初始化工作项
}

static void uninit_workqueue(MY_WORKQUEUE *my_workqueue)
{
	flush_workqueue(my_workqueue->workqueue); //等待工作队列中的所有工作项完成
	destroy_workqueue(my_workqueue->workqueue); //销毁工作队列
}

/****************************************************************************************************/
/*                                       PUBLIC FUNCTIONS                                           */
/****************************************************************************************************/
int __init timer_test_init(void)
{
	init_workqueue(&l_workqueue);
	init_timer_with_parameters(&l_timer, 1000, 5, timer_callback);

	return 0;	
}

int __exit timer_test_exit(void)
{
	del_timer_sync(&l_timer.timer);
	uninit_workqueue(&l_workqueue);
	return 0;
}

module_init(timer_test_init);
module_exit(timer_test_exit);
MODULE_LICENSE("GPL");
