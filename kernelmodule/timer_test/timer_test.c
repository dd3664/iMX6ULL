/****************************************************************************************************/
/*                                           INCLUDE                                                */
/****************************************************************************************************/
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/tick.h>
#include <linux/spinlock.h>
/****************************************************************************************************/
/*                                           DEFINES                                                */
/****************************************************************************************************/
typedef struct {
	struct timer_list timer;
	spinlock_t timer_lock;
	int interval_msecs;
	int count;
} MY_TIMER;
/****************************************************************************************************/
/*                                           VARIABLES                                              */
/****************************************************************************************************/
MY_TIMER l_timer;

/****************************************************************************************************/
/*                                       STATIC FUNCTIONS                                           */
/****************************************************************************************************/
static void timer_callback(MY_TIMER *mytimer)
{
	//获取自旋锁并禁用本地中断
	spin_lock_irq(&mytimer->timer_lock);

	//定时器到期执行的操作
	printk("Timer expired!\n");

	//重新设置定时器
	if (0 != --mytimer->count)
	{
		mod_timer(&mytimer->timer, jiffies + msecs_to_jiffies(mytimer->interval_msecs));
	}

	//释放自旋锁并重新启用本地中断
	spin_unlock_irq(&mytimer->timer_lock);
}

static void init_timer_with_parameters(MY_TIMER *mytimer, int interval_msecs, int count, void (*callback)(MY_TIMER *mytimer))
{
	mytimer->count = count;
	mytimer->interval_msecs = interval_msecs;

	//初始化自旋锁
	spin_lock_init(&mytimer->timer_lock);

	//初始化定时器
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
	init_timer_with_parameters(&l_timer, 200, 5, timer_callback);

	return 0;	
}

int __exit timer_test_exit(void)
{
	del_timer_sync(&l_timer.timer);
	return 0;
}

module_init(timer_test_init);
module_exit(timer_test_exit);
MODULE_LICENSE("GPL");
