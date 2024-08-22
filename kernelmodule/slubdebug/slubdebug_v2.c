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
#include <linux/timekeeping.h>
#include <linux/rtc.h>
#include <linux/ktime.h>
/****************************************************************************************************/
/*                                           DEFINES                                                */
/****************************************************************************************************/
#define MAX_TRACE                     16
#define MAX_PROC_BUFF_LEN             8
#define MAX_FILE_BUFF_LEN			  512
#define TRACEOBJ_HTABLE_SIZE          16   /* 1 << TRACEOBJ_HTABLE_SIZE */
#define ALLOCSTACK_HTABLE_SIZE        12   /* 1 << ALLOCSTACK_HTABLE_SIZE */
#define EACH_FREESTACK_HTABLE_SIZE    3    /* 1 << EACH_FREESTACK_HTABLE_SIZE */
#define DATA_FILE_PATH				  "/tmp/kmem_tracer_data"
#define MAX_TIME_STR_LEN			  24

typedef struct __alloc_stack {
    unsigned long trace[MAX_TRACE];
    unsigned int trace_len;
    unsigned int hold;
	unsigned int total_alloc;
	unsigned int hold_size;
    struct list_head list;
    struct hlist_node hnode;
	DECLARE_HASHTABLE(freestack, EACH_FREESTACK_HTABLE_SIZE);
	atomic_t ref_cnt;
} ALLOCSTACK;

typedef struct __free_stack {
	unsigned long trace[MAX_TRACE];
	unsigned int trace_len;
	unsigned int total_free;
	struct hlist_node hnode;
} FREESTACK;

typedef struct __trace_obj {
    unsigned long addr;
	unsigned int size;
	ALLOCSTACK *allocstack;
    struct hlist_node hnode;
} TRACEOBJ;

typedef struct __dgbcfg {
	bool enable;
	bool save_allstack; /* 保存所有调用栈，即使关联的内存对象已经完全释放 */
	bool save_freestack; /* 保存释放内存的调用栈 */
	unsigned int thresh; /* 打印hold >= thresh的调用栈 */
	unsigned int max_show; /* 打印的allocstack最大数量 */
} DBGCFG;

typedef struct __allocstack_queue {
	struct list_head list;
	unsigned int qlen;
} ALLOCSTACK_QUEUE;
/****************************************************************************************************/
/*                                           VARIABLES                                              */
/****************************************************************************************************/
static struct proc_dir_entry *g_proc_root_dir;
static DBGCFG g_config = {
	.enable = true,
	.save_allstack = false,
	.save_freestack = true,
	.thresh = 10,
	.max_show = 5000
};

DEFINE_SPINLOCK(g_lock);
DECLARE_HASHTABLE(g_traceobj_table, TRACEOBJ_HTABLE_SIZE);
DECLARE_HASHTABLE(g_allocstack_table, ALLOCSTACK_HTABLE_SIZE);
static ALLOCSTACK_QUEUE g_allocstack_quene;
bool g_initialized_flag = false;
static unsigned int g_hold_allocstack = 0;
static unsigned int g_hold_object = 0;
static unsigned long g_hold_object_size = 0;
/* alloccation caches for internal data */
static struct kmem_cache *allocstack_cache;
static struct kmem_cache *freestack_cache;
static struct kmem_cache *traceobj_cache;
/****************************************************************************************************/
/*                                       STATIC FUNCTIONS                                           */
/****************************************************************************************************/
static int kfprintf(struct file *fp, const char *fmt, ...)
{
	char buf[MAX_FILE_BUFF_LEN] = {0};
	ssize_t size;
	va_list args;

	if (NULL == fp)
	{
		printk("fp is null\n");
		return -1;
	}
	
	va_start(args, fmt);
	vsnprintf(buf, MAX_FILE_BUFF_LEN, fmt, args);
	va_end(args);

	size = kernel_write(fp, buf, strlen(buf), fp->f_pos);
	if (size < 0)
	{
		printk("%s", buf);
		return -1;
	}
	return 0;
}

static void get_current_time(char *time_s, int len)
{
    struct timespec64 ts;
    struct rtc_time tm;

    ktime_get_real_ts64(&ts);

    rtc_time64_to_tm(ts.tv_sec, &tm);

    snprintf(time_s, len, "%04d-%02d-%02d %02d:%02d:%02d",
            tm.tm_year + 1900,   // 年份从 1900 开始
            tm.tm_mon + 1,        // 月份从 0 开始，故需要加 1
            tm.tm_mday,           // 日期
            tm.tm_hour,           // 小时
            tm.tm_min,            // 分钟
            tm.tm_sec);           // 秒
	return;
}

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

static unsigned long cal_stacktrace_hkey(unsigned long *trace, unsigned int trace_len)
{
	unsigned long hkey;

	if (0 == trace_len)
	{
		hkey = 0;	
	}
	else if (1 == trace_len)
	{
		hkey = trace[0];
	}
	else if (2 == trace_len)
	{
		hkey = trace[0] + trace[1];
	}
	else
	{
		hkey = trace[1] + trace[trace_len - 1] + trace[trace_len / 2] + trace_len; /* trace[0]通常均为kmem_cache_alloc, 从trace[1]算起 */
	}
	return hkey;
}

static ALLOCSTACK *find_allocstack(unsigned long *trace, unsigned long hkey)
{
	ALLOCSTACK *allocstack = NULL;
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

#if  LINUX_VERSION_CODE <= KERNEL_VERSION(3,38,0)
#define find_freestack(htable, trace, hkey, freestack) \
do \
{ \
	FREESTACK *tmpstack = NULL; \
	struct hlist_node *tmp_hnode; \
    hash_for_each_possible(htable, tmpstack, tmp_hnode, hnode, hkey) \
	{ \
        if (0 == memcmp(tmpstack->trace, trace, MAX_TRACE)) \
        { \
			break; \
        } \
	} \
	freestack = tmpstack; \
} while (0);

#define destroy_freestack_table(htable) \
do \
{ \
	FREESTACK *freestack = NULL; \
	FREESTACK *tmpstack = NULL; \
	struct hlist_node *tmp_hnode = NULL; \
	int bkt; \
	hash_for_each_safe(htable, bkt, tmp_hnode, tmpstack, freestack, hnode) \
	{ \
		if (NULL != freestack) \
		{ \
			hash_del(&freestack->hnode); \
			kmem_cache_free(freestack_cache, freestack); \
		} \
	} \
} while (0);
#else
#define find_freestack(htable, trace, hkey, freestack) \
do \
{ \
	FREESTACK *tmpstack = NULL; \
    hash_for_each_possible(htable, tmpstack, hnode, hkey) \
	{ \
        if (0 == memcmp(tmpstack->trace, trace, MAX_TRACE)) \
        { \
			break; \
        } \
	} \
	freestack = tmpstack; \
} while (0);

#define destroy_freestack_table(htable) \
do \
{ \
	FREESTACK *freestack = NULL; \
	struct hlist_node *tmp_hnode = NULL; \
	int bkt; \
	hash_for_each_safe(htable, bkt, tmp_hnode, freestack, hnode) \
	{ \
		if (NULL != freestack) \
		{ \
			hash_del(&freestack->hnode); \
			kmem_cache_free(freestack_cache, freestack); \
		} \
	} \
} while (0);
#endif

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
			g_hold_object--;
			g_hold_object_size -= traceobj->size;
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
			destroy_freestack_table(allocstack->freestack);
            kmem_cache_free(allocstack_cache, allocstack);
			g_hold_allocstack--;
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

static void show_stack_trace(struct file *fp, struct stack_trace *trace, int spaces)
{
	int i;

	if (0 == trace->entries)
	{
		return;
	}

	for (i = 0; i < trace->nr_entries; i++)
	{
		kfprintf(fp, "%*c", 1 + spaces, ' ');
		kfprintf(fp, "[<%p>] %pS\n", (void *) trace->entries[i], (void *) trace->entries[i]);
	}
}

static void __show_freestack_info(struct file *fp, FREESTACK *freestack)
{
	struct stack_trace trace;

	trace.nr_entries = freestack->trace_len;
	trace.entries = freestack->trace;
	kfprintf(fp, "free counts=%u\n", freestack->total_free);
	show_stack_trace(fp, &trace, 4);
	kfprintf(fp, "\n");
	return;
}

static void __show_allocstack_info(struct file *fp, ALLOCSTACK *allocstack)
{
	FREESTACK *freestack = NULL;
#if LINUX_VERSION_CODE <= KERNEL_VERSION(3,38,0)
    FREESTACK *tmp_freestack = NULL;
#endif
	struct hlist_node *tmp_hnode = NULL;
	int bkt;
	struct stack_trace trace;

	trace.nr_entries = allocstack->trace_len;
	trace.entries = allocstack->trace;
	kfprintf(fp, "------------------------------------------------------------------------------------------------------------------\n");
	kfprintf(fp, "hold counts=%u, hold size=%uB, total alloc counts=%u\n", allocstack->hold, allocstack->hold_size,  allocstack->total_alloc);
	show_stack_trace(fp, &trace, 4);
	kfprintf(fp, "\n");
	if (g_config.save_freestack)
	{
#if LINUX_VERSION_CODE <= KERNEL_VERSION(3,38,0)
		hash_for_each_safe(allocstack->freestack, bkt, tmp_hnode, tmp_freestack, freestack, hnode)
#else
		hash_for_each_safe(allocstack->freestack, bkt, tmp_hnode, freestack, hnode)
#endif
		{
			if (NULL != freestack)
			{
				__show_freestack_info(fp, freestack);
			}
		}
	}
	return;
}

static void show_allocstack_info(void)
{
	ALLOCSTACK *allocstack = NULL;
	ALLOCSTACK *tmpstack = NULL;
    struct hlist_node *tmp_hnode = NULL;
    int bkt;
	struct file *fp;
	char time_s[MAX_TIME_STR_LEN] = {0};

	get_current_time(time_s, MAX_TIME_STR_LEN);

	fp = filp_open(DATA_FILE_PATH, O_WRONLY | O_CREAT | O_APPEND, 0644);
	if (IS_ERR(fp))
	{
		printk("Failed to open file:%s\n", DATA_FILE_PATH);
		return;
	}

	/* 写到文件中，不能使用spin_lock_irqsave，添加引用计数机制 */
	INIT_LIST_HEAD(&g_allocstack_quene.list);
#if LINUX_VERSION_CODE <= KERNEL_VERSION(3,38,0)
    hash_for_each_safe(g_allocstack_table, bkt, tmp_hnode, tmpstack, allocstack, hnode)
#else
    hash_for_each_safe(g_allocstack_table, bkt, tmp_hnode, allocstack, hnode)
#endif
    {
        if (NULL != allocstack && allocstack->hold >= g_config.thresh && g_allocstack_quene.qlen < g_config.max_show)
        {
			atomic_inc(&allocstack->ref_cnt);
			list_add_tail(&allocstack->list, &g_allocstack_quene.list);
			g_allocstack_quene.qlen++;
        }
    }
	list_sort(NULL, &g_allocstack_quene.list, descending_by_hold);

	kfprintf(fp, "=====================================================ALLOC STACK %s=====================================================\n", time_s);
	kfprintf(fp, "hold object=%u, hold object size=%luB, hold allocstack=%u\n", g_hold_object, g_hold_object_size, g_hold_allocstack);
	list_for_each_entry_safe(allocstack, tmpstack, &g_allocstack_quene.list, list)
	{
		if (NULL != allocstack)
		{
			__show_allocstack_info(fp, allocstack);
			list_del(&allocstack->list);
			g_allocstack_quene.qlen--;
			atomic_dec(&allocstack->ref_cnt);
			if (0 == atomic_read(&allocstack->ref_cnt))
			{
				if (g_config.save_freestack)
				{
					destroy_freestack_table(allocstack->freestack);
					kmem_cache_free(allocstack_cache, allocstack);
				}
			}
		}
	}

	filp_close(fp, NULL);
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
	char kbuf[MAX_PROC_BUFF_LEN];
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

static ssize_t proc_save_freestack_write(struct file *filp, const char __user *buf, size_t size, loff_t *offt)
{
	char c;
	if (get_user(c, buf))
	{
		return -EFAULT;
	}
	switch (c)
	{
		case '1':
            g_config.save_freestack = true;
			break;
		case '0':
            g_config.save_freestack = false;
			break;
	}
	return size;
}

static ssize_t proc_max_show_write(struct file *filp, const char __user *buf, size_t size, loff_t *offt)
{
	char kbuf[MAX_PROC_BUFF_LEN];
	char *pos;

	memset(kbuf, 0, sizeof(kbuf));
	if (0 != copy_from_user(kbuf, buf, size))
	{
		printk("copy from user failed.\n");
		return -1;
	}
	kbuf[size - 1] = '\0';
	g_config.max_show = simple_strtol(kbuf, &pos, 0);
	printk("g_config.max_show=%u\n", g_config.max_show);

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

static const struct file_operations proc_save_freestack = {
	.owner = THIS_MODULE,
	.write = proc_save_freestack_write
};

static const struct file_operations proc_max_show = {
	.owner = THIS_MODULE,
	.write = proc_max_show_write
};

static void init_proc(void)
{
    g_proc_root_dir = proc_mkdir("slubdebug", NULL);
    proc_create("enable", 0, g_proc_root_dir, &proc_enable);
    proc_create("showinfo", 0, g_proc_root_dir, &proc_showinfo);
	proc_create("threshold", 0, g_proc_root_dir, &proc_thresh);
	proc_create("save_allstack", 0, g_proc_root_dir, &proc_save_allstack);
	proc_create("save_freestack", 0, g_proc_root_dir, &proc_save_freestack);
	proc_create("max_show", 0, g_proc_root_dir, &proc_max_show);
    return;
}

static void destroy_proc(void)
{
    remove_proc_entry("enable", g_proc_root_dir);
    remove_proc_entry("showinfo", g_proc_root_dir);
	remove_proc_entry("threshold", g_proc_root_dir);
	remove_proc_entry("save_allstack", g_proc_root_dir);
	remove_proc_entry("save_freestack", g_proc_root_dir);
	remove_proc_entry("max_show", g_proc_root_dir);
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
	unsigned long st_hkey = 0;

	if (false == g_config.enable || false == g_initialized_flag)
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
	st_hkey = cal_stacktrace_hkey(trace, trace_len);

	spin_lock_irqsave(&g_lock, flags);
	allocstack = find_allocstack(trace, st_hkey);
    if (NULL == allocstack)
    {
        allocstack = kmem_cache_alloc(allocstack_cache, GFP_ATOMIC);
		memset(allocstack, 0, sizeof(ALLOCSTACK));
        allocstack->trace_len = trace_len;
        memcpy(allocstack->trace, trace, sizeof(trace));
        hash_add(g_allocstack_table, &allocstack->hnode, st_hkey);
		hash_init(allocstack->freestack);
		atomic_set(&allocstack->ref_cnt, 0);
		atomic_inc(&allocstack->ref_cnt);
		g_hold_allocstack++;
    }

	traceobj->allocstack = allocstack;
    allocstack->hold++;
	allocstack->total_alloc++;
	allocstack->hold_size += size;
	g_hold_object++;
	g_hold_object_size += size;
	hash_add(g_traceobj_table, &traceobj->hnode, traceobj->addr);
	spin_unlock_irqrestore(&g_lock, flags);
    return;
}
EXPORT_SYMBOL(trace_slub_alloc);

void trace_slub_free(const void *obj, unsigned long cache_flags)
{
	TRACEOBJ *traceobj = NULL;
	ALLOCSTACK *allocstack = NULL;
	FREESTACK *freestack = NULL;
	unsigned long trace[MAX_TRACE] = {0};
	unsigned int trace_len = 0;
	unsigned long flags;
	unsigned long st_hkey;

	if (false == g_config.enable || false == g_initialized_flag)
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
	g_hold_object--;
	g_hold_object_size -=  traceobj->size;
	hash_del(&traceobj->hnode);
	kmem_cache_free(traceobj_cache, traceobj);
 
	if (0 == allocstack->hold && false == g_config.save_allstack)
	{
		hash_del(&allocstack->hnode);
		g_hold_allocstack--;
		atomic_dec(&allocstack->ref_cnt);
		if (0 == atomic_read(&allocstack->ref_cnt))
		{
			if (g_config.save_freestack)
			{
				destroy_freestack_table(allocstack->freestack);
			}
			kmem_cache_free(allocstack_cache, allocstack);
		}
	}
	else if (g_config.save_freestack)
	{
		trace_len = __save_stack_trace(trace);
		st_hkey = cal_stacktrace_hkey(trace, trace_len);
		find_freestack(allocstack->freestack, trace, st_hkey, freestack);
		if (NULL == freestack)
		{	
			freestack = kmem_cache_alloc(freestack_cache, GFP_ATOMIC);
			memset(freestack, 0, sizeof(FREESTACK));
			freestack->trace_len = trace_len;
			memcpy(freestack->trace, trace, sizeof(trace));
			hash_add(allocstack->freestack, &freestack->hnode, st_hkey);
		}
		freestack->total_free++;
	}
	spin_unlock_irqrestore(&g_lock, flags);
    return;
}
EXPORT_SYMBOL(trace_slub_free);

int __init slubdebug_init(void)
{
    init_proc();
	allocstack_cache = KMEM_CACHE(__alloc_stack, SLAB_NOLEAKTRACE); //参考kmemleak，使用SLAB_NOLEAKTRACE标志，避免递归调用
	freestack_cache = KMEM_CACHE(__free_stack, SLAB_NOLEAKTRACE);
	traceobj_cache = KMEM_CACHE(__trace_obj, SLAB_NOLEAKTRACE);
	if (true == g_config.enable)
	{
		start_trace_obj();
	}
	g_initialized_flag = true;
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
