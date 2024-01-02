/****************************************************************************************************/
/*                                           INCLUDE                                                */
/****************************************************************************************************/
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kprobes.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/pagemap.h>
#include <asm-generic/cacheflush.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/string.h>
#include <linux/list.h>
#include <linux/list_sort.h>
#include <linux/hashtable.h>
#include <linux/kallsyms.h>
#include <linux/kernel_stat.h>
#include <linux/tick.h>
#include <linux/version.h>
#include <linux/spinlock.h>
/****************************************************************************************************/
/*                                           DEFINES                                                */
/****************************************************************************************************/
typedef struct {
	struct timer_list timer;
	spinlock_t timer_lock;
	int count;
} TIMER;
/****************************************************************************************************/
/*                                           VARIABLES                                              */
/****************************************************************************************************/
TIMER l_timer;

/****************************************************************************************************/
/*                                       STATIC FUNCTIONS                                           */
/****************************************************************************************************/
static void timer_callback(TIMER *timer)
{
	//获取自旋锁并禁用本地中断
	spin_lock_irq(&timer->timer_lock);

	//定时器到期执行的操作
	printk("Timer expired!\n");

	//重新设置定时器
	if (0 != --timer->count)
	{
		mod_timer(&timer->timer, jiffies + msecs_to_jiffies(1000));
	}

	//释放自旋锁并重新启用本地中断
	spin_unlock_irq(&timer->timer_lock);
}

static void init_timer_with_parameters(TIMER *timer, int interval_msecs, int count, void (*callback)(TIMER *timer))
{
	timer->count = count;

	//初始化自旋锁
	spin_lock_init(&timer->timer_lock);

	//初始化定时器
	init_timer(&timer->timer);

	timer->timer.function = callback;
	timer->timer.expires = jiffies + msecs_to_jiffies(interval_msecs);
	timer->timer.data = timer;

	add_timer(timer);
}


/****************************************************************************************************/
/*                                       PUBLIC FUNCTIONS                                           */
/****************************************************************************************************/
int __init timer_test_init(void)
{
	init_timer_with_parameters(&l_timer, 1000, 10, timer_callback);

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
