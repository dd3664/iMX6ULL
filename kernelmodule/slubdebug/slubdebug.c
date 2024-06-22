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
	unsigned int hold_size;
    struct list_head list;
    struct hlist_node hnode;
} ALLOCSTACK;

typedef struct __trace_obj {
    unsigned long addr;
	unsigned int size;
	ALLOCSTACK *allocstack;
    struct hlist_node hnode;
} TRACEOBJ;

typedef struct _dgbcfg {
	bool enable;
	bool save_allstack; /* 保存所有调用栈，即使关联的内存对象已经完全释放 */
	unsigned int thresh; /* 打印hold >= thresh的调用栈 */
} DBGCFG;
/****************************************************************************************************/
/*                                           VARIABLES                                              */
/****************************************************************************************************/
static struct proc_dir_entry *g_proc_root_dir;
DBGCFG g_config = {
	.enable = false,
	.save_allstack = false,
	.thresh = 10
};

DEFINE_SPINLOCK(g_lock);
DECLARE_HASHTABLE(g_traceobj_table, 12);
DECLARE_HASHTABLE(g_allocstack_table, 12);
static struct list_head g_allocstack_list;

/* alloccation caches for internal data */
static struct kmem_cache *allocstack_cache;
static struct kmem_cache *traceobj_cache;
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

static ALLOCSTACK *find_allocstack(unsigned long *trace, unsigned int trace_len)
{
	ALLOCSTACK *allocstack = NULL;
	unsigned long hkey = trace[trace_len - 1] + trace_len;
#if LINUX_VERSION_CODE <= KERNEL_VERSION(3,38,0)
	struct hlist_node *tmp_hnode;
#endif
#if LINUX_VERSION_CODE <= KERNEL_VERSION(3,38,0)
	hash_for_each_possible(g_allocstack_table, allocstack, tmp_hnode, hnode, hkey)
#else
    hash_for_each_possible(g_allocstack_table, allocstack, hnode, hkey)
#endif
    {
        if (0 == memcmp(allocstack->trace, trace, MAX_TRACE))
        {
			return allocstack;
        }
    }
	return NULL;
}

static TRACEOBJ *find_traceobj(const void *obj)
{
	TRACEOBJ *traceobj = NULL;
	unsigned long hkey = (unsigned long)obj;
#if LINUX_VERSION_CODE <= KERNEL_VERSION(3,38,0)
	struct hlist_node *tmp_hnode;
#endif
#if LINUX_VERSION_CODE <= KERNEL_VERSION(3,38,0)
	hash_for_each_possible(g_traceobj_table, traceobj, tmp_hnode, hnode, hkey)
#else
    hash_for_each_possible(g_traceobj_table, traceobj, hnode, hkey)
#endif
	{
        if (traceobj->addr == (unsigned long)obj)
        {
			return traceobj;
        }	
	}
	return NULL;
}

static void destroy_traceobj_table(void )
{
	TRACEOBJ *traceobj = NULL;
#if  LINUX_VERSION_CODE <= KERNEL_VERSION(3,38,0)
	TRACEOBJ *tmpobj = NULL;
#endif
	struct hlist_node *tmp_hnode = NULL;
	int bkt;

#if LINUX_VERSION_CODE <= KERNEL_VERSION(3,38,0)
	hash_for_each_safe(g_traceobj_table, bkt, tmp_hnode, tmpobj, traceobj, hnode)
#else
	hash_for_each_safe(g_traceobj_table, bkt, tmp_hnode, traceobj, hnode)
#endif
	{
		if (NULL != traceobj)
		{
			hash_del(&traceobj->hnode);
			kmem_cache_free(traceobj_cache, traceobj);
		}
	}
	return;
}

static void destroy_allocstack_table(void )
{
    ALLOCSTACK *allocstack = NULL;
#if LINUX_VERSION_CODE <= KERNEL_VERSION(3,38,0)
    ALLOCSTACK *tmpstack = NULL;
#endif
    struct hlist_node *tmp_hnode = NULL;
    int bkt;
#if LINUX_VERSION_CODE <= KERNEL_VERSION(3,38,0)
    hash_for_each_safe(g_allocstack_table, bkt, tmp_hnode, tmpstack, allocstack, hnode)
#else
    hash_for_each_safe(g_allocstack_table, bkt, tmp_hnode, allocstack, hnode)
#endif
    {
        if (NULL != allocstack)
        {
            hash_del(&allocstack->hnode);
			list_del(&allocstack->list);
            kmem_cache_free(allocstack_cache, allocstack);
        }
    }
	return;
}

static void start_trace_obj(void)
{
	unsigned long flags;

	spin_lock_irqsave(&g_lock, flags);
	destroy_traceobj_table();
	destroy_allocstack_table();
	hash_init(g_traceobj_table);
	hash_init(g_allocstack_table);
	INIT_LIST_HEAD(&g_allocstack_list);
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
	ALLOCSTACK *allocstack_a = list_entry(a, ALLOCSTACK, list);
	ALLOCSTACK *allocstack_b = list_entry(b, ALLOCSTACK, list);
	return (allocstack_b->hold - allocstack_a->hold);
}

static void __show_allocstack_info(ALLOCSTACK *allocstack)
{
	struct stack_trace trace;

	trace.nr_entries = allocstack->trace_len;
	trace.entries = allocstack->trace;
	printk("------------------------------------------------------------------------------------------------------------------\n");
	printk("hold counts=%u, hold size=%uB, total alloc counts=%u\n", allocstack->hold, allocstack->hold_size,  allocstack->total_alloc);
	print_stack_trace(&trace, 4);
	printk("\n");
	return;
}

static void show_allocstack_info(void)
{
	ALLOCSTACK *allocstack = NULL;
	ALLOCSTACK *tmpstack = NULL;
	unsigned long flags;
	
	spin_lock_irqsave(&g_lock, flags);
	list_sort(NULL, &g_allocstack_list, descending_by_hold);
	printk("=====================================================ALLOC STACK=====================================================\n");
	list_for_each_entry_safe(allocstack, tmpstack, &g_allocstack_list, list)
	{
		if (NULL != allocstack && allocstack->hold >= g_config.thresh)
		{
			__show_allocstack_info(allocstack);
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

static ssize_t proc_save_allstack_write(struct file *filp, const char __user *buf, size_t size, loff_t *offt)
{
	char c;
	if (get_user(c, buf))
	{
		return -EFAULT;
	}
	switch (c)
	{
		case '1':
            g_config.save_allstack = true;
			break;
		case '0':
            g_config.save_allstack = false;
			break;
	}
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

static const struct file_operations proc_save_allstack = {
	.owner = THIS_MODULE,
	.write = proc_save_allstack_write
};

static void init_proc(void)
{
    g_proc_root_dir = proc_mkdir("slubdebug", NULL);
    proc_create("enable", 0, g_proc_root_dir, &proc_enable);
    proc_create("showinfo", 0, g_proc_root_dir, &proc_showinfo);
	proc_create("threshold", 0, g_proc_root_dir, &proc_thresh);
	proc_create("save_allstack", 0, g_proc_root_dir, &proc_save_allstack);
    return;
}

static void destroy_proc(void)
{
    remove_proc_entry("enable", g_proc_root_dir);
    remove_proc_entry("showinfo", g_proc_root_dir);
	remove_proc_entry("threshold", g_proc_root_dir);
	remove_proc_entry("save_allstack", g_proc_root_dir);
    remove_proc_entry("slubdebug", NULL);
    return;
}
/****************************************************************************************************/
/*                                       PUBLIC FUNCTIONS                                           */
/****************************************************************************************************/
void trace_slub_alloc(const void *obj, size_t size, unsigned long cache_flags)
{
    unsigned long trace[MAX_TRACE] = {0};
    unsigned int trace_len = 0;
    TRACEOBJ *traceobj = NULL;
    ALLOCSTACK *allocstack = NULL;
	unsigned long flags;
#if LINUX_VERSION_CODE <= KERNEL_VERSION(3,38,0)
	struct hlist_node *tmp_hnode;
#endif

	if (false == g_config.enable)
	{
		return;
	}

	if (cache_flags & SLAB_NOLEAKTRACE)
	{
		return;
	}

    traceobj = kmem_cache_alloc(traceobj_cache, GFP_ATOMIC);
	memset(traceobj, 0, sizeof(TRACEOBJ));
    traceobj->addr = (unsigned long)obj;
	traceobj->size = size;
    trace_len = __save_stack_trace(trace);

	spin_lock_irqsave(&g_lock, flags);
	allocstack = find_allocstack(trace, trace_len);
    if (NULL == allocstack)
    {
        allocstack = kmem_cache_alloc(allocstack_cache, GFP_ATOMIC);
		memset(allocstack, 0, sizeof(ALLOCSTACK));
        allocstack->trace_len = trace_len;
        memcpy(allocstack->trace, trace, sizeof(trace));
        hash_add(g_allocstack_table, &allocstack->hnode, trace[trace_len - 1] + trace_len);
		list_add_tail(&allocstack->list, &g_allocstack_list);
    }

	traceobj->allocstack = allocstack;
    allocstack->hold++;
	allocstack->total_alloc++;
	allocstack->hold_size += size;
	hash_add(g_traceobj_table, &traceobj->hnode, traceobj->addr);
	spin_unlock_irqrestore(&g_lock, flags);
    return;
}
EXPORT_SYMBOL(trace_slub_alloc);

void trace_slub_free(const void *obj, unsigned long cache_flags)
{
	TRACEOBJ *traceobj = NULL;
	ALLOCSTACK *allocstack = NULL;
	unsigned long flags;

	if (false == g_config.enable)
	{
		return;
	}

	if (cache_flags & SLAB_NOLEAKTRACE)
	{
		return;
	}

	traceobj = find_traceobj(obj);
	if (NULL == traceobj)
	{
		return;
	}
	allocstack = traceobj->allocstack;
	spin_lock_irqsave(&g_lock, flags);
	allocstack->hold--;
	allocstack->hold_size -= traceobj->size;
	hash_del(&traceobj->hnode);
	kmem_cache_free(traceobj_cache, traceobj);
 
	if (0 == allocstack->hold && false == g_config.save_allstack)
	{
		hash_del(&allocstack->hnode);
		list_del(&allocstack->list);
	}
	spin_unlock_irqrestore(&g_lock, flags);
    return;
}
EXPORT_SYMBOL(trace_slub_free);

int __init slubdebug_init(void)
{
    init_proc();
	allocstack_cache = KMEM_CACHE(__alloc_stack, SLAB_NOLEAKTRACE); //参考kmemleak，使用SLAB_NOLEAKTRACE标志，避免递归调用
	traceobj_cache = KMEM_CACHE(__trace_obj, SLAB_NOLEAKTRACE);
    return 0;
}

void __exit slubdebug_exit(void)
{
	stop_trace_obj();
    destroy_proc();
    return;
}

module_init(slubdebug_init);
module_exit(slubdebug_exit);
