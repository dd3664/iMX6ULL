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

typedef struct __alloc_stack {
    unsigned long trace[MAX_TRACE];
    unsigned int trace_len;
    unsigned int hold;
	unsigned int total_alloc;
    struct list_head list_for_obj;
    struct list_head list_for_global;
    struct hlist_node hnode;
	DECLARE_HASHTABLE(freestack, 3);
} ALLOCSTACK;

typedef struct __free_stack {
	unsigned long trace[MAX_TRACE];
	unsigned int trace_len;
	unsigned int total_free;
	struct hlist_node hnode;
} FREESTACK;

typedef struct __trace_obj {
    unsigned long addr;
	ALLOCSTACK *allocstack;
    struct list_head list_for_stack;
    struct hlist_node hnode;
} TRACEOBJ;

typedef struct _dgbcfg {
	bool enable;
	bool save_all_stack; /* 保存所有调用栈，即使已经完全释放 */
	unsigned int thresh; /* 打印hold >= thresh的调用栈 */
} DBGCFG;
/****************************************************************************************************/
/*                                           VARIABLES                                              */
/****************************************************************************************************/
static struct proc_dir_entry *g_proc_root_dir;
static bool g_flag_inner = false;
DBGCFG g_config = {
	.enable = false,
	.save_all_stack = true,
	.thresh = 10
};

DEFINE_SPINLOCK(g_lock);
DECLARE_HASHTABLE(g_trace_obj_table, 12);
DECLARE_HASHTABLE(g_alloc_stack_table, 12);
DECLARE_HASHTABLE(g_free_stack_table, 12);

static struct list_head g_allocstack_list;
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

static void init_allocstack_table(void)
{
	hash_init(g_alloc_stack_table);
	INIT_LIST_HEAD(&g_allocstack_list);
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

static void destroy_allocstack_table(void)
{
    ALLOCSTACK *allocstack = NULL;
	FREESTACK *freestack = NULL;
#if LINUX_VERSION_CODE <= KERNEL_VERSION(3,38,0)
    ALLOCSTACK *tmptrace = NULL;
	FREESTACK *tmp_freestack = NULL;
#endif
    struct hlist_node *tmp_hnode = NULL;
    int bkt;

#if LINUX_VERSION_CODE <= KERNEL_VERSION(3,38,0)
    hash_for_each_safe(g_alloc_stack_table, bkt, tmp_hnode, tmptrace, allocstack, hnode)
#else
    hash_for_each_safe(g_alloc_stack_table, bkt, tmp_hnode, allocstack, hnode)
#endif
    {
        if (NULL != allocstack)
        {
            hash_del(&allocstack->hnode);
			list_del(&allocstack->list_for_global);
#if LINUX_VERSION_CODE <= KERNEL_VERSION(3,38,0)
			hash_for_each_safe(allocstack->freestack, bkt, tmp_hnode, tmp_freestack, freestack, hnode)
#else
			hash_for_each_safe(allocstack->freestack, bkt, tmp_hnode, freestack, hnode)
#endif
			{
				if (NULL != freestack)
				{
					hash_del(&freestack->hnode);
					kfree(freestack);
				}
			}

            kfree(allocstack);
        }
    }
    return;
}

static void start_trace_obj(void)
{
	unsigned long flags;
	spin_lock_irqsave(&g_lock, flags);
	init_traceobj_table();
	init_allocstack_table();
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
	destroy_allocstack_table();
	spin_unlock_irqrestore(&g_lock, flags);
    return;
}

static int descending_by_hold(void *priv, struct list_head *a, struct list_head *b)
{
	ALLOCSTACK *allocstack_a = list_entry(a, ALLOCSTACK, list_for_global);
	ALLOCSTACK *allocstack_b = list_entry(b, ALLOCSTACK, list_for_global);
	return (allocstack_b->hold - allocstack_a->hold);
}

static void __show_allocstack_info(ALLOCSTACK *allocstack)
{
	struct stack_trace trace;

	trace.nr_entries = allocstack->trace_len;
	trace.entries = allocstack->trace;
	printk("===========================================================================================================================\n");
	printk("total_alloc=%u, hold=%u\n", allocstack->total_alloc, allocstack->hold);
	print_stack_trace(&trace, 4);
	printk("\n");
	return;
}

static void __show_freestack_info(FREESTACK *freestack)
{
	struct stack_trace trace;

	trace.nr_entries = freestack->trace_len;
	trace.entries = freestack->trace;
	printk("total_free=%u\n", freestack->total_free);
	print_stack_trace(&trace, 4);
	printk("\n");
	return;
}

static void show_allocstack_info(void)
{
	ALLOCSTACK *allocstack = NULL;
	ALLOCSTACK *tmptrace = NULL;
	FREESTACK *freestack = NULL;
#if LINUX_VERSION_CODE <= KERNEL_VERSION(3,38,0)
    FREESTACK *tmp_freestack = NULL;
#endif
	struct hlist_node *tmp_hnode = NULL;
	int bkt;
	unsigned long flags;
	
	spin_lock_irqsave(&g_lock, flags);
	list_sort(NULL, &g_allocstack_list, descending_by_hold);
	printk("---------------------------------------------------------STACK TRACE---------------------------------------------------------\n");
	list_for_each_entry_safe(allocstack, tmptrace, &g_allocstack_list, list_for_global)
	{
		if (NULL != allocstack && allocstack->hold >= g_config.thresh)
		{
			__show_allocstack_info(allocstack);
#if LINUX_VERSION_CODE <= KERNEL_VERSION(3,38,0)
			hash_for_each_safe(allocstack->freestack, bkt, tmp_hnode, tmp_freestack, freestack, hnode)
#else
			hash_for_each_safe(allocstack->freestack, bkt, tmp_hnode, freestack, hnode)
#endif
			{
				if (NULL != freestack)
				{
					__show_freestack_info(freestack);
				}
			}
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
            show_allocstack_info();
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
    unsigned long allocstack_hashkey = 0;
    unsigned int trace_len = 0;
    TRACEOBJ *traceobj = NULL;
    ALLOCSTACK *allocstack = NULL;
	ALLOCSTACK *tmp = NULL;
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
    allocstack_hashkey = trace[trace_len - 1] + trace_len;
	
#if LINUX_VERSION_CODE <= KERNEL_VERSION(3,38,0)
	hash_for_each_possible(g_alloc_stack_table, tmp, tmp_hnode, hnode, allocstack_hashkey)
#else
    hash_for_each_possible(g_alloc_stack_table, tmp, hnode, allocstack_hashkey)
#endif
    {
        if (0 == memcmp(tmp->trace, trace, sizeof(trace)))
        {
			allocstack = tmp;
            break;
        }
    }

    if (NULL == allocstack)
    {
        allocstack = kmalloc(sizeof(ALLOCSTACK), GFP_ATOMIC);
		memset(allocstack, 0, sizeof(ALLOCSTACK));
        allocstack->trace_len = trace_len;
        memcpy(allocstack->trace, trace, sizeof(trace));
        hash_add(g_alloc_stack_table, &allocstack->hnode, allocstack_hashkey);
		list_add_tail(&allocstack->list_for_global, &g_allocstack_list);
        INIT_LIST_HEAD(&allocstack->list_for_obj);
		hash_init(allocstack->freestack);
    }

	traceobj->allocstack = allocstack;
    allocstack->hold++;
	allocstack->total_alloc++;
    list_add_tail(&traceobj->list_for_stack, &allocstack->list_for_obj);
	
	g_flag_inner = false;
	spin_unlock_irqrestore(&g_lock, flags);
    return;
}
EXPORT_SYMBOL(trace_slub_alloc);

void trace_slub_free(const void *obj)
{
	TRACEOBJ *traceobj = NULL;
	TRACEOBJ *tmpobj = NULL;
	ALLOCSTACK *allocstack = NULL;
	FREESTACK *freestack = NULL;
	FREESTACK *tmp = NULL;
	unsigned long trace[MAX_TRACE] = {0};
	unsigned int trace_len = 0;
	unsigned long freestack_hashkey = 0;
	struct hlist_node *tmp_hnode;
	int bkt;
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
			allocstack = traceobj->allocstack;
			hash_del(&traceobj->hnode);
			list_del(&traceobj->list_for_stack);
			kfree(traceobj);
			

			allocstack->hold--;
			if (0 == allocstack->hold)
			{
				hash_del(&allocstack->hnode);	
				list_del(&allocstack->list_for_global);
#if LINUX_VERSION_CODE <= KERNEL_VERSION(3,38,0)
				hash_for_each_safe(allocstack->freestack, bkt, tmp_hnode, tmp, freestack, hnode)
#else
				hash_for_each_safe(allocstack->freestack, bkt, tmp_hnode, freestack, hnode)
#endif
				{
					if (NULL != freestack)
					{
						hash_del(&freestack->hnode);
						kfree(freestack);
					}
				}

				kfree(allocstack);
			}
			else
			{
				trace_len = __save_stack_trace(trace);
				freestack_hashkey = trace[trace_len - 1] + trace_len;
#if LINUX_VERSION_CODE <= KERNEL_VERSION(3,38,0)
				hash_for_each_possible(allocstack->freestack, tmp, tmp_hnode, hnode, freestack_hashkey)
#else
				hash_for_each_possible(allocstack->freestack, tmp, hnode, freestack_hashkey)
#endif
				{	
					if (0 == memcmp(tmp->trace, trace, sizeof(trace)))
					{
						freestack = tmp;
						break;
					}
				}
			
				if (NULL == freestack)
				{	
					freestack = kmalloc(sizeof(FREESTACK), GFP_ATOMIC);
					memset(freestack, 0, sizeof(FREESTACK));
					freestack->trace_len = trace_len;
					memcpy(freestack->trace, trace, sizeof(trace));
					hash_add(allocstack->freestack, &freestack->hnode, freestack_hashkey);
				}
				freestack->total_free++;

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
