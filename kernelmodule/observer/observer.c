/****************************************************************************************************/
/*                                           INCLUDE                                                */
/****************************************************************************************************/
#include <linux/module.h>
#include <linux/kernel.h>
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
#include <linux/math64.h>
#include <linux/workqueue.h>
/****************************************************************************************************/
/*                                           DEFINES                                                */
/****************************************************************************************************/
#define PARM_LENTH 128
#define MAX_CMD_LEN 64

#ifndef arch_idle_time
#define arch_idle_time(cpu) 0
#endif

typedef enum {
	PROCESS_RUNNING,
	PROCESS_EXITED,
} PROCESS_STATE;

typedef enum {
	STAT_START,
	STAT_STOP,
	PROCESS_EXIT,
} CALLD_FROM;

typedef struct _monitor_timer {
	struct timer_list timer;
	int interval_msecs;
	int count;
	struct workqueue_struct *wq;
	struct work_struct wk;
} MONITOR_TIMER;

typedef struct _stat_config {
	int monitor_counts; /* 监控次数 */
	int monitor_period; /* 监控周期 */
	int threshold_of_cpu_occupy; /* 显示过滤阈值 */
	pid_t monitor_pid; /* 指定要监控的进程pid */
	char monitor_process_name[TASK_COMM_LEN]; /* 指定要监控的进程名称 */
	int print_to_console; /* 输出到串口 */
	int print_to_file; /* 输出到文件 */
} STAT_CONFIG;

typedef struct _percentage {
	u8 integer; /* 整数部分 */
	u8 frac; /* 小数部分 */
} PERCENTAGE;
typedef struct _kernel_cpustat {
	u64 user;
	u64 nice;
	u64 system;
	u64 idle;
	u64 iowait;
	u64 irq;
	u64 softirq;
	u64 steal;
	u64 guest;
	u64 guest_nice;
} KERNEL_CPUSTAT;
typedef struct _cpu_percentage {
	PERCENTAGE user;
	PERCENTAGE nice;
	PERCENTAGE system;
	PERCENTAGE idle;
	PERCENTAGE iowait;
	PERCENTAGE irq;
	PERCENTAGE softirq;
	PERCENTAGE steal;
	PERCENTAGE guest;
	PERCENTAGE guest_nice;
} CPU_PERCENTAGE;
typedef struct _cpu_info {
	KERNEL_CPUSTAT start_stat;
	KERNEL_CPUSTAT end_stat;
	KERNEL_CPUSTAT delta;
	CPU_PERCENTAGE percent_info;
	u64 sum;
} CPUINFO;
typedef struct _kernel_process_stat {
	u64 utime;
	u64 stime;
} KERNE_PROCESS_STAT;
typedef struct _process_cpu_info {
	KERNE_PROCESS_STAT start_stat;	
	KERNE_PROCESS_STAT end_stat;
	KERNE_PROCESS_STAT delta;
	PERCENTAGE occupy_percent_of_non_idle; /* CPU占用率，(stime + utime) / non-idletime */
	PERCENTAGE occupy_percent_of_sum; /* CPU占用率， (stime + utime) / sumtime */
} PROCESS_CPU_INFO;
typedef struct _process_mm_info {
	u32 rss;
} PROCESS_MM_INFO;
typedef struct _process_info {
	pid_t pid;
	char comm[TASK_COMM_LEN];
	char params[PARM_LENTH];
	u64 start_time;
	u64 exit_time;
	PROCESS_STATE state; /* 进程是否已退出 */
	u64 parent_task_hash_key;
	struct _process_info *parent; /* 父进程信息 */
	struct list_head child; /* 子进程信息链表 */
	struct list_head list_for_parent;
	struct list_head list_for_global;
	PROCESS_CPU_INFO process_cpu_info; /* 进程CPU信息 */
	PROCESS_MM_INFO process_mm_info; /* 进程内存信息 */
	u64 hash_key;
	struct hlist_node hnode; /* 哈希表节点 */
} PROCESSINFO;
typedef struct _exception_process_info {
	char comm[TASK_COMM_LEN];
	char params[PARM_LENTH];
	u32 except_counts; /* 异常次数 */
	struct hlist_node hnode; /* 哈希表节点 */
} EXCETION_PROCESS_INFO;
/****************************************************************************************************/
/*                                           VARIABLES                                              */
/****************************************************************************************************/
struct proc_dir_entry *l_proc_root_dir;

DECLARE_HASHTABLE(l_process_info_table, 16); /* 采样期间进程信息全局哈希表， 大小为2<<16，用于快速查找 */
DECLARE_HASHTABLE(l_exception_process_info_table, 8); /* 存储异常进程信息 */
struct list_head l_process_info_list; /* 采样期间进程信息全局链表，用于排序 */
CPUINFO l_cpu_info;
STAT_CONFIG l_stat_config;
MONITOR_TIMER l_monitor_timer;

extern int (*do_exit_observer_hook)(struct task_struct *task); 
/****************************************************************************************************/
/*                                       STATIC FUNCTIONS                                           */
/****************************************************************************************************/
static int start_to_observe(void);
static int stop_to_observe(void);

static void deal_raw_cmdline(char *buffer, unsigned int length)
{
        int i = 0;
		if (buffer[0] == '\0')
		{
			return;
		}
        for (i = 0; i < length; i ++) {
                if (buffer[i] == '\0') {
                        buffer[i] = ' ';
                }
        }
		return;
}

static int _access_process_vm(struct task_struct *tsk, struct mm_struct *mm,
        unsigned long addr, void *buf, int len, int write)
{
    struct vm_area_struct *vma;
    void *old_buf = buf;
    down_read(&mm->mmap_sem);
    /* ignore errors, just check how much was successfully transferred */
    while (len) {
        int bytes, ret, offset;
        void *maddr;
        struct page *page = NULL;
        ret = get_user_pages(tsk, mm, addr, 1,
                write, 1, &page, &vma);
        if (ret <= 0) {
            /*
             * Check if this is a VM_IO | VM_PFNMAP VMA, which
             * we can access using slightly different code.
             */
#ifdef CONFIG_HAVE_IOREMAP_PROT
            vma = find_vma(mm, addr);
            if (!vma || vma->vm_start > addr)
                break;
            if (vma->vm_ops && vma->vm_ops->access)
                ret = vma->vm_ops->access(vma, addr, buf,
                              len, write);
            if (ret <= 0)
#endif
                break;
            bytes = ret;
        } else {
            bytes = len;
            offset = addr & (PAGE_SIZE-1);
            if (bytes > PAGE_SIZE-offset)
                bytes = PAGE_SIZE-offset;
            maddr = kmap(page);
            if (write) {
                copy_to_user_page(vma, page, addr,
                          maddr + offset, buf, bytes);
                set_page_dirty_lock(page);
            } else {
                copy_from_user_page(vma, page, addr,
                            buf, maddr + offset, bytes);
            }
            kunmap(page);
            page_cache_release(page);
        }
        len -= bytes;
        buf += bytes;
        addr += bytes;
    }
    up_read(&mm->mmap_sem);

	return buf - old_buf;
}

static int get_cmd_line(struct task_struct *task, char * buffer, int size)
{
    int res = 0;
    unsigned int len;
    struct mm_struct *mm = get_task_mm(task);
    if (!mm)
        goto out;
    if (!mm->arg_end)
        goto out_mm;    /* Shh! No looking before we're done */

    len = mm->arg_end - mm->arg_start;

    if (len > PAGE_SIZE)
        len = PAGE_SIZE;
	if (len >= size)
		len = size - 1;
    res = _access_process_vm(task, mm, mm->arg_start, buffer, len, 0);
	
	deal_raw_cmdline(buffer, len);
	buffer[size - 1] = '\0';
out_mm:
    mmput(mm);
out:
	return res;
}

static u64 get_idle_time(int cpu)
{
    u64 idle, idle_time = get_cpu_idle_time_us(cpu, NULL);

    if (idle_time == -1ULL) {
        /* !NO_HZ so we can rely on cpustat.idle */
        idle = kcpustat_cpu(cpu).cpustat[CPUTIME_IDLE];
        idle += arch_idle_time(cpu);
    } else
        idle = usecs_to_cputime64(idle_time);

    return idle;
}

static u64 get_iowait_time(int cpu)
{
    u64 iowait, iowait_time = get_cpu_iowait_time_us(cpu, NULL);

    if (iowait_time == -1ULL)
        /* !NO_HZ so we can rely on cpustat.iowait */
        iowait = kcpustat_cpu(cpu).cpustat[CPUTIME_IOWAIT];
    else
        iowait = usecs_to_cputime64(iowait_time);

    return iowait;
}

static u64 generate_hash_key_from_pid(pid_t pid, u64 start_time)
{
	return (u64)(pid + start_time);
}

static int generate_hash_key_form_name(char *comm, char *params)
{
	int ret = 0;
	int i;
	char name[TASK_COMM_LEN + PARM_LENTH];

	snprintf(name, TASK_COMM_LEN, "%s", comm);
	snprintf(name + strlen(name), PARM_LENTH, "%s", params);
	for (i = 0; i < strlen(name); i++)
	{
		ret += name[i];
	}

	return ret;
}

static int get_process_cpu_info(struct task_struct *task, KERNE_PROCESS_STAT *process_stat)
{
	process_stat->utime = task->utime;
	process_stat->stime = task->stime;
	return 0;
}

static PROCESSINFO* get_matched_process_info(struct task_struct *task)
{
	u64 hash_key;
	PROCESSINFO *matched_process_info;

	if (NULL == task)
	{
		return NULL;
	}

#if LINUX_VERSION_CODE <= KERNEL_VERSION(3,38,0)
	hash_key = generate_hash_key_from_pid(task->pid, task->real_start_time.tv_sec);
	struct hlist_node *tmp_hnode;
	hash_for_each_possible(l_process_info_table, matched_process_info, tmp_hnode, hnode, hash_key)
#else
	hash_key = generate_hash_key_from_pid(task->pid, task->real_start_time);
	hash_for_each_possible(l_process_info_table, matched_process_info, hnode, hash_key)
#endif
	{
		if(matched_process_info->hash_key == hash_key)
		{
			return matched_process_info;
		}
	}
	return NULL;
}

static int filter_task(struct task_struct *task)
{
	PROCESSINFO *matched_process_info;

	if ( 0 != l_stat_config.monitor_pid) /* 指定了监控的pid */
	{
		if (task->pid == l_stat_config.monitor_pid)
		{
			return 0;
		}
		else
		{
			matched_process_info = get_matched_process_info(task->real_parent);
			if (NULL != matched_process_info) /* 亲属进程在监控列表之中 */
			{
				return 0;
			}
			else
			{
				return 1;
			}
		}
	}

	else if (0 != strlen(l_stat_config.monitor_process_name)) /* 指定了监控进程名称 */
	{
		if (0 == strcmp(l_stat_config.monitor_process_name, task->comm))
		{
			return 0;
		}
		else
		{
			matched_process_info = get_matched_process_info(task->real_parent);
			if (NULL != matched_process_info) /* 亲属进程在监控列表之中 */
			{
				return 0;
			}
			else
			{
				return 1;
			}
		}
	}

	return 0;
}

static int insert_process_info(struct task_struct *task, CALLD_FROM calld_from)
{
	PROCESSINFO *process_info;

	if (1 == filter_task(task))
	{
		return -1;
	}
	
	process_info = kmalloc(sizeof(PROCESSINFO), GFP_KERNEL);
	if (NULL == process_info)
	{
		printk("Allocate memory for process info failed\n");
		return -1;
	}
	memset(process_info, 0, sizeof(PROCESSINFO));
	process_info->pid = task->pid;
	strncpy(process_info->comm, task->comm, TASK_COMM_LEN);
	get_cmd_line(task, process_info->params, PARM_LENTH);
#if LINUX_VERSION_CODE <= KERNEL_VERSION(3,38,0)
	process_info->start_time = task->real_start_time.tv_sec; //start_time 记录的是进程从内核调度器的角度来看开始运行的时间; real_start_time 记录的是进程创建的实际时间，即从用户空间发起创建请求的时间
#else
	process_info->start_time = task->real_start_time;
#endif
	
	if (STAT_START == calld_from)
	{
		get_process_cpu_info(task, &process_info->process_cpu_info.start_stat);
	}
	else
	{
		get_process_cpu_info(task, &process_info->process_cpu_info.end_stat);
	}

	if (PROCESS_EXIT != calld_from)
	{
		process_info->state = PROCESS_RUNNING;
	}
	else
	{
		//printk("TPT insert_process_info with exited, comm=%s, params=%s\n", process_info->comm, process_info->params);
		process_info->state = PROCESS_EXITED;
	}

	process_info->hash_key = generate_hash_key_from_pid(process_info->pid, process_info->start_time);
	hash_add(l_process_info_table, &process_info->hnode, process_info->hash_key);

	INIT_LIST_HEAD(&process_info->child);
	if (NULL != task->real_parent)
	{
#if LINUX_VERSION_CODE <= KERNEL_VERSION(3,38,0)
		process_info->parent_task_hash_key = generate_hash_key_from_pid(task->real_parent->group_leader->pid, task->real_parent->group_leader->real_start_time.tv_sec); /* 先记录父进程hash_key，统计完成后再进行链表操作，避免子进程已结束而父进程未结束，导致子进程找不到父进程信息*/
#else
		process_info->parent_task_hash_key = generate_hash_key_from_pid(task->real_parent->group_leader->pid, task->real_parent->group_leader->real_start_time);
#endif
		//printk("process_info->pid=%d, process_info->params=%s, task->real_parent->pid=%d, task->parent->pid=%d, task->parent->group_leader->pid=%d\n",process_info->pid, process_info->params, task->real_parent->pid, task->parent->pid, task->parent->group_leader->pid);

	}

	list_add_tail(&process_info->list_for_global, &l_process_info_list);

	return 0;
}

static int do_update_process_info(struct task_struct *task, PROCESSINFO *process_info, CALLD_FROM calld_from)
{
	get_process_cpu_info(task, &process_info->process_cpu_info.end_stat);
	if (PROCESS_EXIT == calld_from)
	{
		//printk("TPT do_update_process_info, comm=%s, params=%s\n", process_info->comm, process_info->params);
		process_info->state = PROCESS_EXITED;
	}
	return 0;
}

static int update_process_info(struct task_struct *task, CALLD_FROM calld_from)
{	
	PROCESSINFO *matched_process_info;

	matched_process_info= get_matched_process_info(task);
	if (NULL != matched_process_info)
	{
		do_update_process_info(task, matched_process_info, calld_from);
	}
	else
	{
		insert_process_info(task, calld_from);
	}
	return 0;
}

static int descending_by_cpu_occupy(void *priv, struct list_head *a, struct list_head *b)
{
	PROCESSINFO *process_info_a = list_entry(a, PROCESSINFO, list_for_global);
	PROCESSINFO *process_info_b = list_entry(b, PROCESSINFO, list_for_global);
	return ((process_info_b->process_cpu_info.delta.utime + process_info_b->process_cpu_info.delta.stime) - 
				(process_info_a->process_cpu_info.delta.utime + process_info_a->process_cpu_info.delta.stime));
}

static int insert_to_exception(PROCESSINFO *process_info)
{
	int hash_key;
	EXCETION_PROCESS_INFO *excetion_process_info;

	hash_key = generate_hash_key_form_name(process_info->comm, process_info->params);
#if LINUX_VERSION_CODE <= KERNEL_VERSION(3,38,0)
	struct hlist_node *tmp_hnode;
	hash_for_each_possible(l_exception_process_info_table, excetion_process_info, tmp_hnode, hnode, hash_key)
#else
	hash_for_each_possible(l_exception_process_info_table, excetion_process_info, hnode, hash_key)
#endif
	{
		if (0 == strncmp(excetion_process_info->comm, process_info->comm, TASK_COMM_LEN) && 0 == strncmp(excetion_process_info->params, process_info->params, PARM_LENTH))
		{
			excetion_process_info->except_counts++;
			return 0;
		}
	}

	excetion_process_info = kmalloc(sizeof(EXCETION_PROCESS_INFO), GFP_KERNEL);
	if (excetion_process_info == NULL)
	{
		printk("kmalloc for excetion_process_info failed\n");
		return -1;
	}
	memset(excetion_process_info, 0, sizeof(EXCETION_PROCESS_INFO));
	strncpy(excetion_process_info->comm, process_info->comm, TASK_COMM_LEN);
	strncpy(excetion_process_info->params, process_info->params, PARM_LENTH);
	excetion_process_info->except_counts++;
	hash_add(l_exception_process_info_table, &excetion_process_info->hnode, hash_key);
	return 0;
}

static int link_to_parent(PROCESSINFO *process_info)
{
	//printk("TPT link_to_parent, pid=%d, comm=%s, params=%s\n", process_info->pid, process_info->comm, process_info->params);
	PROCESSINFO *parent_process_info;

#if LINUX_VERSION_CODE <= KERNEL_VERSION(3,38,0)
	struct hlist_node *tmp_hnode;
	hash_for_each_possible(l_process_info_table, parent_process_info, tmp_hnode, hnode, process_info->parent_task_hash_key)
#else
	hash_for_each_possible(l_process_info_table, parent_process_info, hnode, process_info->parent_task_hash_key)
#endif
	{
		if(parent_process_info->hash_key == process_info->parent_task_hash_key)
		{
			process_info->parent = parent_process_info;
			list_add_tail(&process_info->list_for_parent, &parent_process_info->child);
			break;
		}
	}
	return 0;
}

static void calculate_percentage(u64 dividend, u64 divisor, u8 *interger, u8 *frac)
{
	u64 tmp;
	if (0 == divisor || 0 == dividend)
	{
		*interger = 0;
		*frac = 0;
	}
	else
	{
		tmp = div64_u64(dividend * 10000, divisor);
		*frac = do_div(tmp, 100);
		*interger = tmp;
	}
	return;
}

static int calculate_single_process_cpu_info(PROCESSINFO *process_info)
{
	u64 process_occupy_time;
	u64 cpu_non_idle_time;
	
	process_info->process_cpu_info.delta.utime = process_info->process_cpu_info.end_stat.utime - process_info->process_cpu_info.start_stat.utime;
	process_info->process_cpu_info.delta.stime = process_info->process_cpu_info.end_stat.stime - process_info->process_cpu_info.start_stat.stime;
	process_occupy_time = process_info->process_cpu_info.delta.utime + process_info->process_cpu_info.delta.stime;
	cpu_non_idle_time = l_cpu_info.sum - l_cpu_info.delta.idle;
	calculate_percentage(process_occupy_time, cpu_non_idle_time, &process_info->process_cpu_info.occupy_percent_of_non_idle.integer, &process_info->process_cpu_info.occupy_percent_of_non_idle.frac);
	calculate_percentage(process_occupy_time, l_cpu_info.sum, &process_info->process_cpu_info.occupy_percent_of_sum.integer, &process_info->process_cpu_info.occupy_percent_of_sum.frac);

	
	if (process_info->process_cpu_info.occupy_percent_of_non_idle.integer >= l_stat_config.threshold_of_cpu_occupy)
	{
		insert_to_exception(process_info);
	}
	link_to_parent(process_info);
	return 0;
}

static int calulate_all_process_info(void)
{
	PROCESSINFO *process_info;
	PROCESSINFO *tmp;
	list_for_each_entry_safe(process_info, tmp, &l_process_info_list, list_for_global)
	{
		if (NULL != process_info)
		{
			calculate_single_process_cpu_info(process_info);
		}
	}
	return 0;
}

static int get_cpu_stat(KERNEL_CPUSTAT *cpu_stat)
{
	int i;
	for_each_possible_cpu(i) {
		cpu_stat->user += kcpustat_cpu(i).cpustat[CPUTIME_USER];
		cpu_stat->nice += kcpustat_cpu(i).cpustat[CPUTIME_NICE];
		cpu_stat->system += kcpustat_cpu(i).cpustat[CPUTIME_SYSTEM];
		cpu_stat->idle += get_idle_time(i);
		cpu_stat->iowait += get_iowait_time(i);
		cpu_stat->irq += kcpustat_cpu(i).cpustat[CPUTIME_IRQ];
		cpu_stat->softirq += kcpustat_cpu(i).cpustat[CPUTIME_SOFTIRQ];
		cpu_stat->steal += kcpustat_cpu(i).cpustat[CPUTIME_STEAL];
		cpu_stat->guest += kcpustat_cpu(i).cpustat[CPUTIME_GUEST];
		cpu_stat->guest_nice += kcpustat_cpu(i).cpustat[CPUTIME_GUEST_NICE];
	}
	return 0;
}

static int calculate_cpu_info(CPUINFO *cpu_info)
{
	cpu_info->delta.user = cpu_info->end_stat.user - cpu_info->start_stat.user;
	cpu_info->delta.nice = cpu_info->end_stat.nice - cpu_info->start_stat.nice;
	cpu_info->delta.system = cpu_info->end_stat.system - cpu_info->start_stat.system;
	cpu_info->delta.idle = cpu_info->end_stat.idle - cpu_info->start_stat.idle;
	cpu_info->delta.iowait = cpu_info->end_stat.iowait -cpu_info->start_stat.iowait;
	cpu_info->delta.irq = cpu_info->end_stat.irq - cpu_info->start_stat.irq;
	cpu_info->delta.softirq = cpu_info->end_stat.softirq - cpu_info->start_stat.softirq;
	cpu_info->delta.steal = cpu_info->end_stat.steal - cpu_info->start_stat.steal;
	cpu_info->delta.guest = cpu_info->end_stat.guest - cpu_info->start_stat.guest;
	cpu_info->delta.guest_nice = cpu_info->end_stat.guest_nice - cpu_info->start_stat.guest_nice;
	cpu_info->sum = cpu_info->delta.user + cpu_info->delta.nice + cpu_info->delta.system + cpu_info->delta.idle + cpu_info->delta.iowait + cpu_info->delta.irq + cpu_info->delta.softirq + cpu_info->delta.steal + cpu_info->delta.guest +  cpu_info->delta.guest_nice;
	calculate_percentage(cpu_info->delta.user, cpu_info->sum, &cpu_info->percent_info.user.integer, &cpu_info->percent_info.user.frac);
	calculate_percentage(cpu_info->delta.nice, cpu_info->sum, &cpu_info->percent_info.nice.integer, &cpu_info->percent_info.nice.frac);
	calculate_percentage(cpu_info->delta.system , cpu_info->sum, &cpu_info->percent_info.system.integer, &cpu_info->percent_info.system.frac);
	calculate_percentage(cpu_info->delta.idle, cpu_info->sum, &cpu_info->percent_info.idle.integer, &cpu_info->percent_info.idle.frac);
	calculate_percentage(cpu_info->delta.iowait, cpu_info->sum, &cpu_info->percent_info.iowait.integer, &cpu_info->percent_info.iowait.frac);
	calculate_percentage(cpu_info->delta.irq, cpu_info->sum, &cpu_info->percent_info.irq.integer, &cpu_info->percent_info.irq.frac);
	calculate_percentage(cpu_info->delta.softirq, cpu_info->sum, &cpu_info->percent_info.softirq.integer, &cpu_info->percent_info.softirq.frac);
	calculate_percentage(cpu_info->delta.steal, cpu_info->sum, &cpu_info->percent_info.steal.integer, &cpu_info->percent_info.steal.frac);
	calculate_percentage(cpu_info->delta.guest, cpu_info->sum, &cpu_info->percent_info.guest.integer, &cpu_info->percent_info.guest.frac);
	calculate_percentage(cpu_info->delta.guest_nice, cpu_info->sum, &cpu_info->percent_info.guest_nice.integer, &cpu_info->percent_info.guest_nice.frac);
	
	return 0;
}

static int show_sigle_process_info_to_console(PROCESSINFO *process_info)
{
	printk("         %-12d %-12d %-12llu %-12llu %3d.%02d%-6s %3d.%02d%-6s %-32s\n",
				process_info->pid,
				process_info->parent == NULL ? -1 : process_info->parent->pid,
				process_info->process_cpu_info.end_stat.utime - process_info->process_cpu_info.start_stat.utime,
				process_info->process_cpu_info.end_stat.stime - process_info->process_cpu_info.start_stat.stime,
				process_info->process_cpu_info.occupy_percent_of_non_idle.integer, process_info->process_cpu_info.occupy_percent_of_non_idle.frac, "",
				process_info->process_cpu_info.occupy_percent_of_sum.integer, process_info->process_cpu_info.occupy_percent_of_sum.frac, "",
				strlen(process_info->params) == 0 ? process_info->comm : process_info->params);
	return 0;
}

static int show_all_process_info_to_console(void)
{
	PROCESSINFO *process_info;
	PROCESSINFO *tmp;

	list_sort(NULL, &l_process_info_list, descending_by_cpu_occupy);

	printk("---------------------------------------------------------PROCESS INFO---------------------------------------------------------\n");
	printk("Item:    %-12s %-12s %-12s %-12s %-12s %-12s %-32s\n", "pid", "ppid", "user", "system", "\%CPU-nonIdle", "\%CPU-sum", "command");
	list_for_each_entry_safe(process_info, tmp, &l_process_info_list, list_for_global)
	{
		if (NULL != process_info)
		{
			if (process_info->process_cpu_info.occupy_percent_of_non_idle.integer >= l_stat_config.threshold_of_cpu_occupy)
			{	
				show_sigle_process_info_to_console(process_info);
			}
		}
	}
	return 0;
}

static int show_all_process_info(void)
{
	if (1 == l_stat_config.print_to_console)
	{
		show_all_process_info_to_console();
	}
	return 0;
}

static int show_child_process_info_to_console(PROCESSINFO *process_info)
{
	PROCESSINFO *child_process_info;
	PROCESSINFO *tmp;

	list_for_each_entry_safe(child_process_info, tmp, &process_info->child, list_for_parent)
	{
		show_sigle_process_info_to_console(child_process_info);
		show_child_process_info_to_console(child_process_info);
	}
	return 0;
}

static int show_given_pid_process_info_to_console(pid_t pid)
{
	PROCESSINFO *process_info;
	PROCESSINFO *tmp;
	printk("---------------------------------------------------------PROCESS INFO---------------------------------------------------------\n");
	printk("Item:    %-12s %-12s %-12s %-12s %-12s %-12s %-32s\n", "pid", "ppid", "user", "system", "\%CPU-nonIdle", "\%CPU-sum", "command");
	list_for_each_entry_safe(process_info, tmp, &l_process_info_list, list_for_global)
	{
		if (NULL != process_info)
		{
			if (process_info->pid == pid)
			{
				show_sigle_process_info_to_console(process_info);
				show_child_process_info_to_console(process_info);
			}
		}
	}
	return 0;
}

static int show_cpu_info_to_console(void)
{
	printk("===========================================================CPU INFO===========================================================\n");
	printk("Item:    %-12s %-12s %-12s %-12s %-12s %-12s %-12s %-12s %-12s %-12s %-12s\n", "softirq", "irq", "user", "system", "idle", "iowait", "nice", "steal", "guest", "guest_nice", "sum");
	printk("Jiffies: %-12llu %-12llu %-12llu %-12llu %-12llu %-12llu %-12llu %-12llu %-12llu %-12llu %-12llu\n",
				l_cpu_info.delta.softirq,
				l_cpu_info.delta.irq,
				l_cpu_info.delta.user,
				l_cpu_info.delta.system,
				l_cpu_info.delta.idle,
				l_cpu_info.delta.iowait,
				l_cpu_info.delta.nice,
				l_cpu_info.delta.steal,
				l_cpu_info.delta.guest,
				l_cpu_info.delta.guest_nice,
				l_cpu_info.sum);
	printk("Percent: %3d.%02d%-6s %3d.%02d%-6s %3d.%02d%-6s %3d.%02d%-6s %3d.%02d%-6s %3d.%02d%-6s %3d.%02d%-6s %3d.%02d%-6s %3d.%02d%-6s %3d.%02d%-6s\n",
				l_cpu_info.percent_info.softirq.integer, l_cpu_info.percent_info.softirq.frac, "",
				l_cpu_info.percent_info.irq.integer, l_cpu_info.percent_info.irq.frac, "",
				l_cpu_info.percent_info.user.integer, l_cpu_info.percent_info.user.frac, "",
				l_cpu_info.percent_info.system.integer, l_cpu_info.percent_info.system.frac, "",
				l_cpu_info.percent_info.idle.integer, l_cpu_info.percent_info.idle.frac, "",
				l_cpu_info.percent_info.iowait.integer, l_cpu_info.percent_info.iowait.frac, "",
				l_cpu_info.percent_info.nice.integer, l_cpu_info.percent_info.nice.frac, "",
				l_cpu_info.percent_info.steal.integer, l_cpu_info.percent_info.steal.frac, "",
				l_cpu_info.percent_info.guest.integer, l_cpu_info.percent_info.guest.frac, "",
				l_cpu_info.percent_info.guest_nice.integer, l_cpu_info.percent_info.guest_nice.frac, "");
	return 0;
}

static void show_exception_info_to_console(void)
{
	EXCETION_PROCESS_INFO *excetion_process_info;
#if LINUX_VERSION_CODE <= KERNEL_VERSION(3,38,0)	
	EXCETION_PROCESS_INFO *tmp;
#endif
	int bkt;
	struct hlist_node *tmp_hnode;
	printk("---------------------------------------------------------EXCEPTION INFO---------------------------------------------------------\n");
	printk("Item:    %-64s %-12s\n", "command", "counts");
#if LINUX_VERSION_CODE <= KERNEL_VERSION(3,38,0)
	hash_for_each_safe(l_exception_process_info_table, bkt, tmp_hnode, tmp, excetion_process_info, hnode)
#else 
	hash_for_each_safe(l_exception_process_info_table, bkt, tmp_hnode, excetion_process_info, hnode)
#endif
	{
		if (NULL != excetion_process_info)
		{
			printk("         %-64s %-12d\n", strlen(excetion_process_info->params) == 0 ? excetion_process_info->comm : excetion_process_info->params, excetion_process_info->except_counts);
		}
	}
	return;
}

static void show_cpu_info(void)
{
	if (1 == l_stat_config.print_to_console)
	{
		show_cpu_info_to_console();
	}
	return;
}

static int destroy_process_info(void)
{
	PROCESSINFO *process_info;
	PROCESSINFO *tmp;

	if (l_process_info_list.next == NULL && l_process_info_list.prev == NULL)
	{
		return 0;
	}

	list_for_each_entry_safe(process_info, tmp, &l_process_info_list, list_for_global)
	{
		if (NULL != process_info)
		{
			list_del(&process_info->list_for_global);
			kfree(process_info);
		}
	}
	return 0;
}

static int destroy_exception_process_info(void)
{
	EXCETION_PROCESS_INFO *excetion_process_info;
#if LINUX_VERSION_CODE <= KERNEL_VERSION(3,38,0)
	EXCETION_PROCESS_INFO *tmp;
#endif
	struct hlist_node *tmp_hnode;
	int bkt;
#if LINUX_VERSION_CODE <= KERNEL_VERSION(3,38,0)
	hash_for_each_safe(l_exception_process_info_table, bkt, tmp_hnode, tmp, excetion_process_info, hnode)
#else 
	hash_for_each_safe(l_exception_process_info_table, bkt, tmp_hnode, excetion_process_info, hnode)
#endif
	{
		if (NULL != excetion_process_info)
		{
			hash_del(&excetion_process_info->hnode);
			kfree(excetion_process_info);
		}
	}
	return 0;
}

static int init_process_info(void)
{
	struct task_struct *task;
	hash_init(l_process_info_table);
	INIT_LIST_HEAD(&l_process_info_list);
	for_each_process(task){
		insert_process_info(task, STAT_START);
	}
	return 0;
}

static int stop_process_info(void)
{
	struct task_struct *task;
	for_each_process(task)
	{
		update_process_info(task, STAT_STOP);
	}
	return 0;
}

static int do_exit_handler(struct task_struct *task)
{
	update_process_info(task, PROCESS_EXIT);
	return 0;
}

static void monitor_workqueue_callback(struct work_struct *wk)
{
	MONITOR_TIMER *monitor_timer = container_of(wk, MONITOR_TIMER, wk);
	stop_to_observe();
	if (0 != --monitor_timer->count)
	{
		start_to_observe();
		mod_timer(&monitor_timer->timer, jiffies + msecs_to_jiffies(monitor_timer->interval_msecs));
	}
	else
	{
		printk("Periodically to stat complete!\n");
	}
	return;
}

static void monitor_timer_callback(unsigned long data)
{
	MONITOR_TIMER *monitor_timer = (MONITOR_TIMER *)data;	
	queue_work(monitor_timer->wq, &monitor_timer->wk);
	return;
}

static void init_monitoer_timer(MONITOR_TIMER *monitor_timer)
{
	init_timer(&monitor_timer->timer);
	monitor_timer->wq = create_workqueue("monitor_workqueue");
	INIT_WORK(&monitor_timer->wk, monitor_workqueue_callback);
	return;
}

static void destroy_monitor_timer(MONITOR_TIMER *monitor_timer)
{
	flush_workqueue(monitor_timer->wq);
	destroy_workqueue(monitor_timer->wq);
	return;
}

static void add_monitor_timer(MONITOR_TIMER *monitor_timer, int interval_msecs, int count, void (*callback)(unsigned long))
{
	monitor_timer->count = count;
	monitor_timer->interval_msecs = interval_msecs;

	monitor_timer->timer.function = callback;
	monitor_timer->timer.expires = jiffies + msecs_to_jiffies(interval_msecs);
	monitor_timer->timer.data = (unsigned long)monitor_timer;

	add_timer(&monitor_timer->timer);
	return;
}

static void cancel_monitor_timer(MONITOR_TIMER *monitor_timer)
{
	del_timer_sync(&monitor_timer->timer);
	return;
}

static int start_to_observe(void)
{
	memset(&l_cpu_info, 0, sizeof(CPUINFO));
	get_cpu_stat(&l_cpu_info.start_stat);
	destroy_process_info();
	init_process_info();
	BUG_ON(do_exit_observer_hook != NULL);
	RCU_INIT_POINTER(do_exit_observer_hook, do_exit_handler);

	return 0;
}

static int stop_to_observe(void)
{
	RCU_INIT_POINTER(do_exit_observer_hook, NULL);
	stop_process_info();
	get_cpu_stat(&l_cpu_info.end_stat);
	calculate_cpu_info(&l_cpu_info);
	calulate_all_process_info();
	show_cpu_info();
	show_all_process_info();
	return 0;
}

static int observer_show(struct seq_file *seq, void *v)
{
	seq_printf(seq, "==========Current config==========\n");
	seq_printf(seq, "monitor_period: %d (msec)\n", l_stat_config.monitor_period);
	seq_printf(seq, "monitor_counts: %d\n", l_stat_config.monitor_counts);
	seq_printf(seq, "monitor_pid: %d\n", l_stat_config.monitor_pid);
	seq_printf(seq, "monitor_process_name: %s\n", l_stat_config.monitor_process_name);
	seq_printf(seq, "threshhold_of_cpu_occupy: %d\n", l_stat_config.threshold_of_cpu_occupy);
	seq_printf(seq, "print_to_console: %d\n", l_stat_config.print_to_console);
	return 0;
}

static int observer_open(struct inode *inode, struct file *filp)
{
	char *buf;
	struct seq_file *m;
	int ret;

	buf = kmalloc(1024, GFP_KERNEL);
	if (NULL == buf)
	{
		return -1;
	}
	ret = single_open(filp, observer_show, NULL);
	if (0 == ret)
	{
		m = filp->private_data;
		m->buf = buf;
		m->size = 1024;
	}
	else
	{
		kfree(buf);
	}
	return ret;
}

static ssize_t proc_start_stop_wirte(struct file *filp, const char __user *buf, size_t size, loff_t *offt)
{
	char c;
	if (get_user(c, buf))
	{
		return -EFAULT;
	}
	switch (c)
	{
		case '1':
			start_to_observe();
			if (0 != l_stat_config.monitor_period)
			{
				add_monitor_timer(&l_monitor_timer, l_stat_config.monitor_period, l_stat_config.monitor_counts, monitor_timer_callback);
			}
			break;
		case '0':
			stop_to_observe();
			if (0 != l_stat_config.monitor_period)
			{
				cancel_monitor_timer(&l_monitor_timer);
			}
			break;
	}
	return size;
}

static ssize_t proc_monitor_period_wirte(struct file *filp, const char __user *buf, size_t size, loff_t *offt)
{
	char kbuf[MAX_CMD_LEN];
	char *pos;

	memset(kbuf, 0, sizeof(kbuf));
	if (0 != copy_from_user(kbuf, buf, size))
	{
		printk("copy from user failed.\n");
		return -1;
	}
	kbuf[size - 1] = '\0';
	l_stat_config.monitor_period = simple_strtol(kbuf, &pos, 0);
	return size;
}

static ssize_t proc_monitor_counts_wirte(struct file *filp, const char __user *buf, size_t size, loff_t *offt)
{
	char kbuf[MAX_CMD_LEN];
	char *pos;

	memset(kbuf, 0, sizeof(kbuf));
	if (0 != copy_from_user(kbuf, buf, size))
	{
		printk("copy from user failed.\n");
		return -1;
	}
	kbuf[size - 1] = '\0';
	l_stat_config.monitor_counts = simple_strtol(kbuf, &pos, 0);
	return size;
}

static ssize_t proc_monitor_pid_wirte(struct file *filp, const char __user *buf, size_t size, loff_t *offt)
{
	char kbuf[MAX_CMD_LEN];
	char *pos;

	memset(kbuf, 0, sizeof(kbuf));
	if (0 != copy_from_user(kbuf, buf, size))
	{
		printk("copy from user failed.\n");
		return -1;
	}
	kbuf[size - 1] = '\0';
	l_stat_config.monitor_pid = simple_strtol(kbuf, &pos, 0);
	return size;
}

static ssize_t proc_monitor_process_name_wirte(struct file *filp, const char __user *buf, size_t size, loff_t *offt)
{
	char kbuf[MAX_CMD_LEN];

	memset(kbuf, 0, sizeof(kbuf));
	if (0 != copy_from_user(kbuf, buf, size))
	{
		printk("copy from user failed.\n");
		return -1;
	}
	kbuf[size - 1] = '\0';
	strncpy(l_stat_config.monitor_process_name, kbuf, TASK_COMM_LEN);
	return size;
}

static ssize_t proc_threshold_of_cpu_occupy_wirte(struct file *filp, const char __user *buf, size_t size, loff_t *offt)
{
	char kbuf[MAX_CMD_LEN];
	char *pos;

	memset(kbuf, 0, sizeof(kbuf));
	if (0 != copy_from_user(kbuf, buf, size))
	{
		printk("copy from user failed.\n");
		return -1;
	}
	kbuf[size - 1] = '\0';
	l_stat_config.threshold_of_cpu_occupy = simple_strtol(kbuf, &pos, 0);
	return size;
}

static ssize_t proc_show_process_info_wirte(struct file *filp, const char __user *buf, size_t size, loff_t *offt)
{
	pid_t show_pid;
	char kbuf[MAX_CMD_LEN];
	char *pos;

	memset(kbuf, 0, sizeof(kbuf));
	if (0 != copy_from_user(kbuf, buf, size))
	{
		printk("copy from user failed.\n");
		return -1;
	}
	kbuf[size - 1] = '\0';
	show_pid = simple_strtol(kbuf, &pos, 0);

	show_cpu_info_to_console();
	if (0 == show_pid)
	{
		show_all_process_info_to_console();
	}
	else
	{
		show_given_pid_process_info_to_console(show_pid);
	}
	return size;
}

static ssize_t proc_print_to_console_write(struct file *filp, const char __user *buf, size_t size, loff_t *offt)
{
	char c;
	if (get_user(c, buf))
	{
		return -EFAULT;
	}
	switch (c)
	{
		case '0':
			l_stat_config.print_to_console = 0;
			break;
		case '1':
			l_stat_config.print_to_console = 1;
			break;
	}
	return size;
}

static ssize_t proc_show_exception_info_write(struct file *filp, const char __user *buf, size_t size, loff_t *offt)
{
	char c;
	if (get_user(c, buf))
	{
		return -EFAULT;
	}
	switch (c)
	{
		case '0':
			break;
		case '1':
			show_exception_info_to_console();
			break;
	}
	return size;
}

static const struct file_operations proc_start_stop = {
	.owner = THIS_MODULE,
	.open = observer_open,
	.write = proc_start_stop_wirte,
	.read = seq_read,
};

static const struct file_operations proc_monitor_period = {
	.owner = THIS_MODULE,
	.open = observer_open,
	.write = proc_monitor_period_wirte,
	.read = seq_read,
};

static const struct file_operations proc_monitor_counts = {
	.owner = THIS_MODULE,
	.open = observer_open,
	.write = proc_monitor_counts_wirte,
	.read = seq_read,
};

static const struct file_operations proc_monitor_pid = {
	.owner = THIS_MODULE,
	.open = observer_open,
	.write = proc_monitor_pid_wirte,
	.read = seq_read,
};

static const struct file_operations proc_monitor_process_name = {
	.owner = THIS_MODULE,
	.open = observer_open,
	.write = proc_monitor_process_name_wirte,
	.read = seq_read,
};

static const struct file_operations proc_threshold_of_cpu_occupy = {
	.owner = THIS_MODULE,
	.open = observer_open,
	.write = proc_threshold_of_cpu_occupy_wirte,
	.read = seq_read,
};

static const struct file_operations proc_show_process_info = {
	.owner = THIS_MODULE,
	.open = observer_open,
	.write = proc_show_process_info_wirte,
	.read = seq_read,
};

static const struct file_operations proc_print_to_console = {
	.owner = THIS_MODULE,
	.open = observer_open,
	.write = proc_print_to_console_write,
	.read = seq_read,
};

static const struct file_operations proc_show_exception_info = {
	.owner = THIS_MODULE,
	.open = observer_open,
	.write = proc_show_exception_info_write,
	.read = seq_read,
};

static void init_proc(void)
{
	l_proc_root_dir = proc_mkdir("observer", NULL);
	proc_create("start_stop", 0, l_proc_root_dir, &proc_start_stop);
	proc_create("monitor_period", 0, l_proc_root_dir, &proc_monitor_period);
	proc_create("monitor_counts", 0, l_proc_root_dir, &proc_monitor_counts);
	proc_create("monitor_pid", 0, l_proc_root_dir, &proc_monitor_pid);
	proc_create("monitor_process_name", 0, l_proc_root_dir, &proc_monitor_process_name);
	proc_create("threshold_of_cpu_occupy", 0, l_proc_root_dir, &proc_threshold_of_cpu_occupy);
	proc_create("print_to_console", 0, l_proc_root_dir, &proc_print_to_console);
	proc_create("show_process_info", 0, l_proc_root_dir, &proc_show_process_info);
	proc_create("show_exception_info", 0, l_proc_root_dir, &proc_show_exception_info);
}

static void destroy_proc(void)
{
	remove_proc_entry("start_stop", l_proc_root_dir);
	remove_proc_entry("monitor_period", l_proc_root_dir);
	remove_proc_entry("monitor_counts", l_proc_root_dir);
	remove_proc_entry("monitor_pid", l_proc_root_dir);
	remove_proc_entry("monitor_process_name", l_proc_root_dir);
	remove_proc_entry("threshold_of_cpu_occupy", l_proc_root_dir);
	remove_proc_entry("print_to_console", l_proc_root_dir);
	remove_proc_entry("show_process_info", l_proc_root_dir);
	remove_proc_entry("show_exception_info", l_proc_root_dir);
	remove_proc_entry("observer", NULL);
}
/****************************************************************************************************/
/*                                       PUBLIC FUNCTIONS                                           */
/****************************************************************************************************/
int __init observer_init(void)
{
	init_proc();
	hash_init(l_exception_process_info_table);
	init_monitoer_timer(&l_monitor_timer);
	return 0;	
}

void __exit observer_exit(void)
{
	stop_to_observe();
	destroy_process_info();
	destroy_exception_process_info();
	destroy_monitor_timer(&l_monitor_timer);
	destroy_proc();
	return;
}

module_init(observer_init);
module_exit(observer_exit);
MODULE_LICENSE("GPL");
