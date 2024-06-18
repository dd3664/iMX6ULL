/****************************************************************************************************/
/*                                           INCLUDE                                                */
/****************************************************************************************************/
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/stacktrace.h>
#include <linux/list.h>
#include <linux/list_sort.h>
#include <linux/proc_fs.h>
#include <linux/hashtable.h>
#include <linux/string.h>
#include <linux/version.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
/****************************************************************************************************/
/*                                           DEFINES                                                */
/****************************************************************************************************/
#define MAX_TRACE        16
#define MAX_BUFER_LEN    8

typedef struct _stacktrace {
    unsigned long trace[MAX_TRACE];
    unsigned int trace_len;
    unsigned int hold;
	unsigned int total_alloc;
    struct list_head list_for_obj;
    struct list_head list_for_global;
    struct hlist_node hnode;
} STACKTRACE;

typedef struct _trace_obj {
    unsigned long addr;
	STACKTRACE *stacktrace;
    struct list_head list_for_stack;
    struct hlist_node hnode;
} TRACEOBJ;

typedef struct _dgbcfg {
	bool enable;
	unsigned int thresh; /* 打印hold >= thresh的调用栈 */
} DBGCFG;
/****************************************************************************************************/
/*                                           VARIABLES                                              */
/****************************************************************************************************/
static struct proc_dir_entry *g_proc_root_dir;
static bool g_flag_inner = false;
DBGCFG g_config = {
	.enable = false,
	.thresh = 10
};

DEFINE_SPINLOCK(g_lock);
DECLARE_HASHTABLE(g_trace_obj_table, 12);
DECLARE_HASHTABLE(g_stacktrace_table, 12);

static struct list_head g_stacktrace_list;
/****************************************************************************************************/
/*                                       STATIC FUNCTIONS                                           */
/****************************************************************************************************/
static int __save_stack_trace(unsigned long *trace)
{
    struct stack_trace stack_trace;

    stack_trace.max_entries = MAX_TRACE;
    stack_trace.nr_entries = 0;
    stack_trace.entries = trace;
    stack_trace.skip = 0;
    save_stack_trace(&stack_trace);
    return stack_trace.nr_entries;
}

static void init_traceobj_table(void)
{
	hash_init(g_trace_obj_table);
	return;
}

static void init_stacktrace_table(void)
{
	hash_init(g_stacktrace_table);
	INIT_LIST_HEAD(&g_stacktrace_list);
}


static void destroy_traceobj_table(void)
{
	TRACEOBJ *traceobj = NULL;
#if LINUX_VERSION_CODE <= KERNEL_VERSION(3,38,0)
	TRACEOBJ *tmpobj = NULL;
#endif
	struct hlist_node *tmp_hnode = NULL;
	int bkt;

#if LINUX_VERSION_CODE <= KERNEL_VERSION(3,38,0)
	hash_for_each_safe(g_trace_obj_table, bkt, tmp_hnode, tmpobj, traceobj, hnode)
#else
	hash_for_each_safe(g_trace_obj_table, bkt, tmp_hnode, traceobj, hnode)
#endif
	{
		if (NULL != traceobj)
		{
			hash_del(&traceobj->hnode);
			kfree(traceobj);
		}
	}
	return;
}

static void destroy_stacktrace_table(void)
{
    STACKTRACE *stacktrace = NULL;
#if LINUX_VERSION_CODE <= KERNEL_VERSION(3,38,0)
    STACKTRACE *tmptrace = NULL;
#endif
    struct hlist_node *tmp_hnode = NULL;
    int bkt;

#if LINUX_VERSION_CODE <= KERNEL_VERSION(3,38,0)
    hash_for_each_safe(g_stacktrace_table, bkt, tmp_hnode, tmptrace, stacktrace, hnode)
#else
    hash_for_each_safe(g_stacktrace_table, bkt, tmp_hnode, stacktrace, hnode)
#endif
    {
        if (NULL != stacktrace)
        {
            hash_del(&stacktrace->hnode);
			list_del(&stacktrace->list_for_global);
            kfree(stacktrace);
        }
    }
    return;
}

static void start_trace_obj(void)
{
	unsigned long flags;
	spin_lock_irqsave(&g_lock, flags);
	init_traceobj_table();
	init_stacktrace_table();
	g_config.enable = true;
	spin_unlock_irqrestore(&g_lock, flags);
    return;
}

static void stop_trace_obj(void)
{
	unsigned long flags;

	spin_lock_irqsave(&g_lock, flags);
	g_config.enable = false;
	destroy_traceobj_table();
	destroy_stacktrace_table();
	spin_unlock_irqrestore(&g_lock, flags);
    return;
}

static int descending_by_hold(void *priv, struct list_head *a, struct list_head *b)
{
	STACKTRACE *stacktrace_a = list_entry(a, STACKTRACE, list_for_global);
	STACKTRACE *stacktrace_b = list_entry(b, STACKTRACE, list_for_global);
	return (stacktrace_b->hold - stacktrace_a->hold);
}

static void __show_stacktrace_info(STACKTRACE *stacktrace)
{
	struct stack_trace trace;

	trace.nr_entries = stacktrace->trace_len;
	trace.entries = stacktrace->trace;
	if (stacktrace->hold >= g_config.thresh)
	{
		printk("===========================================================================================================================\n");
		printk("total_alloc=%u, hold=%u\n", stacktrace->total_alloc, stacktrace->hold);
		print_stack_trace(&trace, 4);
		printk("\n");
	}
}

static void show_stacktrace_info(void)
{
	STACKTRACE *stacktrace = NULL;
	STACKTRACE *tmptrace = NULL;
	unsigned long flags;
	
	spin_lock_irqsave(&g_lock, flags);
	list_sort(NULL, &g_stacktrace_list, descending_by_hold);
	printk("---------------------------------------------------------STACK TRACE---------------------------------------------------------\n");
	list_for_each_entry_safe(stacktrace, tmptrace, &g_stacktrace_list, list_for_global)
	{
		if (NULL != stacktrace)
		{
			__show_stacktrace_info(stacktrace);
		}
	}
	spin_unlock_irqrestore(&g_lock, flags);
	return;
}

static ssize_t proc_enable_write(struct file *filp, const char __user *buf, size_t size, loff_t *offt)
{
	char c;
	if (get_user(c, buf))
	{
		return -EFAULT;
	}
	switch (c)
	{
		case '1':
            start_trace_obj();
			break;
		case '0':
            stop_trace_obj();
			break;
	}
	return size;
}

static ssize_t proc_showinfo_write(struct file *filp, const char __user *buf, size_t size, loff_t *offt)
{
	char c;
	if (get_user(c, buf))
	{
		return -EFAULT;
	}
	switch (c)
	{
		case '1':
            show_stacktrace_info();
			break;
		case '0':
			break;
	}
	return size;
}

static ssize_t proc_thresh_write(struct file *filp, const char __user *buf, size_t size, loff_t *offt)
{
	char kbuf[MAX_BUFER_LEN];
	char *pos;

	memset(kbuf, 0, sizeof(kbuf));
	if (0 != copy_from_user(kbuf, buf, size))
	{
		printk("copy from user failed.\n");
		return -1;
	}
	kbuf[size - 1] = '\0';
	g_config.thresh = simple_strtol(kbuf, &pos, 0);
	printk("g_config.thresh=%u\n", g_config.thresh);

	return size;
}

static const struct file_operations proc_enable = {
	.owner = THIS_MODULE,
	.write = proc_enable_write
};

static const struct file_operations proc_showinfo = {
	.owner = THIS_MODULE,
	.write = proc_showinfo_write
};

static const struct file_operations proc_thresh = {
	.owner = THIS_MODULE,
	.write = proc_thresh_write
};

static void init_proc(void)
{
    g_proc_root_dir = proc_mkdir("slubdebug", NULL);
    proc_create("enable", 0, g_proc_root_dir, &proc_enable);
    proc_create("showinfo", 0, g_proc_root_dir, &proc_showinfo);
	proc_create("threshold", 0, g_proc_root_dir, &proc_thresh);
    return;
}

static void destroy_proc(void)
{
    remove_proc_entry("enable", g_proc_root_dir);
    remove_proc_entry("showinfo", g_proc_root_dir);
	remove_proc_entry("threshold", g_proc_root_dir);
    remove_proc_entry("slubdebug", NULL);
    return;
}
/****************************************************************************************************/
/*                                       PUBLIC FUNCTIONS                                           */
/****************************************************************************************************/
void trace_slub_alloc(const void *obj)
{
    unsigned long trace[MAX_TRACE] = {0};
    unsigned long stacktrace_hashkey = 0;
    unsigned int trace_len = 0;
    TRACEOBJ *traceobj = NULL;
    STACKTRACE *stacktrace = NULL;
	STACKTRACE *tmp = NULL;
#if LINUX_VERSION_CODE <= KERNEL_VERSION(3,38,0)
	struct hlist_node *tmp_hnode;
#endif
	unsigned long flags;

	if (false == g_config.enable)
	{
		return;
	}

    //此处需打破递归调用
	if (true == g_flag_inner)
	{
		return;
	}
	spin_lock_irqsave(&g_lock, flags);
	g_flag_inner = true;

    traceobj = kmalloc(sizeof(TRACEOBJ), GFP_ATOMIC);
	memset(traceobj, 0, sizeof(TRACEOBJ));
    traceobj->addr = (unsigned long)obj;
    hash_add(g_trace_obj_table, &traceobj->hnode, traceobj->addr);
    
    trace_len = __save_stack_trace(trace);
    stacktrace_hashkey = trace[trace_len - 1] + trace_len;
	
#if LINUX_VERSION_CODE <= KERNEL_VERSION(3,38,0)
	hash_for_each_possible(g_stacktrace_table, tmp, tmp_hnode, hnode, stacktrace_hashkey)
#else
    hash_for_each_possible(g_stacktrace_table, tmp, hnode, stacktrace_hashkey)
#endif
    {
        if (0 == memcmp(tmp->trace, trace, sizeof(trace)))
        {
			stacktrace = tmp;
            break;
        }
    }

    if (NULL == stacktrace)
    {
        stacktrace = kmalloc(sizeof(STACKTRACE), GFP_ATOMIC);
		memset(stacktrace, 0, sizeof(STACKTRACE));
        stacktrace->trace_len = trace_len;
        memcpy(stacktrace->trace, trace, sizeof(trace));
        hash_add(g_stacktrace_table, &stacktrace->hnode, stacktrace_hashkey);
		list_add_tail(&stacktrace->list_for_global, &g_stacktrace_list);
        INIT_LIST_HEAD(&stacktrace->list_for_obj);
    }

	traceobj->stacktrace = stacktrace;
    stacktrace->hold++;
	stacktrace->total_alloc++;
    list_add_tail(&traceobj->list_for_stack, &stacktrace->list_for_obj);
	
	g_flag_inner = false;
	spin_unlock_irqrestore(&g_lock, flags);
    return;
}
EXPORT_SYMBOL(trace_slub_alloc);

void trace_slub_free(const void *obj)
{
	TRACEOBJ *traceobj = NULL;
	TRACEOBJ *tmpobj = NULL;
	STACKTRACE *stacktrace = NULL;
#if LINUX_VERSION_CODE <= KERNEL_VERSION(3,38,0)
	struct hlist_node *tmp_hnode;
#endif 
	unsigned long flags;

	if (false == g_config.enable)
	{
		return;
	}

	if (true == g_flag_inner)
	{
		return;
	}
	spin_lock_irqsave(&g_lock, flags);
	g_flag_inner = true;

 #if LINUX_VERSION_CODE <= KERNEL_VERSION(3,38,0)
	hash_for_each_possible_safe(g_trace_obj_table, traceobj, tmp_hnode, tmpobj, hnode, (unsigned long)obj)
#else
	hash_for_each_possible_safe(g_trace_obj_table, traceobj, tmpobj, hnode, (unsigned long)obj)
#endif
	{
		if(traceobj->addr == (unsigned long)obj)
		{
			stacktrace = traceobj->stacktrace;
			hash_del(&traceobj->hnode);
			list_del(&traceobj->list_for_stack);
			kfree(traceobj);
			stacktrace->hold--;
			if (0 == stacktrace->hold)
			{
				hash_del(&stacktrace->hnode);	
				list_del(&stacktrace->list_for_global);
				kfree(stacktrace);
			}
			break;
		}
	}

	g_flag_inner = false;
	spin_unlock_irqrestore(&g_lock, flags);
    return;
}
EXPORT_SYMBOL(trace_slub_free);

int __init slubdebug_init(void)
{
    init_proc();
    return 0;
}

void __exit slubdebug_exit(void)
{
    destroy_proc();
    return;
}

module_init(slubdebug_init);
module_exit(slubdebug_exit);
