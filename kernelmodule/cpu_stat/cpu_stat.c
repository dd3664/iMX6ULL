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
/****************************************************************************************************/
/*                                           DEFINES                                                */
/****************************************************************************************************/
#define PARM_LENTH 128
#define MAX_CMD_LEN 64

#ifndef arch_idle_time
#define arch_idle_time(cpu) 0
#endif

typedef int (*pre_func)(struct kprobe *p, struct pt_regs *regs);

typedef enum {
	PROCESS_RUNNING,
	PROCESS_EXITED,
} PROCESS_STATE;

typedef enum {
	STAT_START,
	STAT_STOP,
	PROCESS_EXIT,
} CALLD_FROM;

typedef struct _stat_config {
	int monitor_duration; /* 监控时长 */
	int show_period; /* 显示周期，sec */
	int show_threshold; /* 显示过滤阈值 */
	pid_t monitor_task_pid; /* 指定要监控的进程pid */
	char monitor_task_comm[TASK_COMM_LEN]; /* 指定要监控的进程名称 */
	int res_to_console; /* 输出到串口 */
	int res_to_file; /* 输出到文件 */
} STAT_CONFIG;

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
typedef struct _cpu_info {
	KERNEL_CPUSTAT start_stat;
	KERNEL_CPUSTAT end_stat;
	KERNEL_CPUSTAT delta;
	u64 sum;
} CPUINFO;
typedef struct _kernel_process_stat {
	u64 utime;
	u64 stime;
} KERNEL_PROCESS_STAT;
typedef struct _process_cpu_info {
	KERNEL_PROCESS_STAT start_stat;	
	KERNEL_PROCESS_STAT end_stat;
	KERNEL_PROCESS_STAT delta;
	int occupy_percent; /* CPU占用百分比 */
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
	struct _process_info *parent; /* 父进程信息 */
	struct list_head child; /* 子进程信息链表 */
	struct list_head list_for_parent;
	struct list_head list_for_global;
	PROCESS_CPU_INFO process_cpu_info; /* 进程CPU信息 */
	PROCESS_MM_INFO process_mm_info; /* 进程内存信息 */
	u64 hash_key;
	struct hlist_node hnode; /* 哈希表节点 */
} PROCESSINFO;
/****************************************************************************************************/
/*                                           VARIABLES                                              */
/****************************************************************************************************/
struct kprobe kp;

DEFINE_HASHTABLE(l_process_info_table, 16); /* 进程信息全局哈希表， 大小为2<<16，用于快速查找 */
struct list_head l_process_info_list; /* 进程信息全局链表，用于排序 */
CPUINFO l_cpu_info;
STAT_CONFIG l_stat_config;

/****************************************************************************************************/
/*                                       STATIC FUNCTIONS                                           */
/****************************************************************************************************/
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

static int get_cmd_line(struct task_struct *task, char * buffer)
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

    res = _access_process_vm(task, mm, mm->arg_start, buffer, len, 0);

    // If the nul at the end of args has been overwritten, then
    // assume application is using setproctitle(3).
    if (res > 0 && buffer[res-1] != '\0' && len < PAGE_SIZE) {
        len = strnlen(buffer, res);
        if (len < res) {
            res = len;
        } else {
            len = mm->env_end - mm->env_start;
            if (len > PAGE_SIZE - res)
                len = PAGE_SIZE - res;
            res += _access_process_vm(task, mm, mm->env_start, buffer+res, len, 0);
            res = strnlen(buffer, res);
        }
    }
	deal_raw_cmdline(buffer, len);
out_mm:
    mmput(mm);
out:
    return res;
}


static int kprobe_certain_func(const char *func_name, pre_func handler_pre)
{
	int ret = 0;
	//初始化kprobe结构
	memset(&kp, 0, sizeof(struct kprobe));
    kp.pre_handler = handler_pre;
    kp.symbol_name = func_name;

    //注册kprobe
    ret = register_kprobe(&kp);
    printk("register_kprobe for %s ret=%d\n", func_name, ret);
    if (ret < 0)
    {
        printk("Failed to register Kprobe.\n");
        return -1;
    }
    return 0;
}

static int unkprobe_certain_func(const char *func_name)
{
	//卸载kprobe
	if (0 == strcmp(kp.symbol_name, func_name))
	{
		printk("kp.symbol_name==func_name=%s, unregister kprobe for %s\n", kp.symbol_name, kp.symbol_name);
		unregister_kprobe(&kp);
	}
	return 0;
}

static int do_exit_handler_pre_debug(struct kprobe *p, struct pt_regs *regs)
{
	char params[PARM_LENTH];
	char parent_params[PARM_LENTH];
	char group_params[PARM_LENTH];
	struct task_struct *task = current;
	struct task_struct *parent_task = task->real_parent;
	struct task_struct *group_task = task->group_leader;
	memset(params, 0, sizeof(params));
	memset(parent_params, 0, sizeof(parent_params));
	memset(group_params, 0, sizeof(group_params));
	get_cmd_line(task, params);
	get_cmd_line(parent_task, parent_params);
	get_cmd_line(group_task, group_params);
	printk("=======================================================================================================================\n");
    printk("current->PID=%d, current->Command=%s, current->Params=%s\n", task->pid, task->comm, params);
	printk("parent->PID=%d, parent->Command=%s, parent->Params=%s\n", parent_task->pid, parent_task->comm, parent_params);
	printk("group->PID=%d, group->Command=%s, group->Params=%s\n\n", group_task->pid, group_task->comm, group_params);
    return 0;
}

static int cpu_stat_show(struct seq_file *seq, void *v)
{
	seq_puts(seq, "This is a test\n");
	return 0;
}

static int cpu_stat_open(struct inode *inode, struct file *filp)
{
	char *buf;
	struct seq_file *m;
	int ret;

	buf = kmalloc(1024, GFP_KERNEL);
	if (NULL == buf)
	{
		return -1;
	}
	ret = single_open(filp, cpu_stat_show, NULL);
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

static int calculate_cpu_delta(CPUINFO *cpu_info)
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
	return 0;
}

static u64 generate_hash_key(pid_t pid, u64 start_time)
{
	return (u64)(pid + start_time);
}

static int get_process_cpu_info(struct task_struct *task, KERNEL_PROCESS_STAT *process_stat)
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

	hash_key = generate_hash_key(task->pid, task->real_start_time);
#if LINUX_VERSION_CODE <= KERNEL_VERSION(3,38,0)
	struct hlist_node *tmp_hnode;
	hash_for_each_possible(l_process_info_table, matched_process_info, tmp_hnode, hnode, hash_key)
#else
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

static int insert_process_info(struct task_struct *task, CALLD_FROM calld_from)
{
	PROCESSINFO *process_info;
	PROCESSINFO *parent_process_info;
	process_info = kmalloc(sizeof(PROCESSINFO), GFP_KERNEL);
	if (NULL == process_info)
	{
		printk("Allocate memory for process info failed\n");
		return -1;
	}
	memset(process_info, 0, sizeof(PROCESSINFO));
	process_info->pid = task->pid;
	strncpy(process_info->comm, task->comm, TASK_COMM_LEN);
	get_cmd_line(task, process_info->params);
	process_info->start_time = task->real_start_time;

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
		process_info->state = PROCESS_EXITED;
	}

	process_info->hash_key = generate_hash_key(process_info->pid, process_info->start_time);
	hash_add(l_process_info_table, &process_info->hnode, process_info->hash_key);

	INIT_LIST_HEAD(&process_info->child);
	parent_process_info = get_matched_process_info(task->real_parent);
	if (parent_process_info != NULL)
	{
		process_info->parent = parent_process_info;
		list_add_tail(&process_info->list_for_parent, &parent_process_info->child);
	}

	list_add_tail(&process_info->list_for_global, &l_process_info_list);

	return 0;
}


static int do_update_process_info(struct task_struct *task, PROCESSINFO *process_info, CALLD_FROM calld_from)
{
	get_process_cpu_info(task, &process_info->process_cpu_info.end_stat);
	if (PROCESS_EXIT == calld_from)
	{
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

static int descending_by_cpu_occupy_percent(void *priv, struct list_head *a, struct list_head *b)
{
	PROCESSINFO *process_info_a = list_entry(a, PROCESSINFO, list_for_global);
	PROCESSINFO *process_info_b = list_entry(b, PROCESSINFO, list_for_global);
	return process_info_b->process_cpu_info.occupy_percent - process_info_a->process_cpu_info.occupy_percent;
}

static int show_process_info(void)
{
	PROCESSINFO *process_info;
	PROCESSINFO *tmp;
	int bkt;
#if LINUX_VERSION_CODE <= KERNEL_VERSION(3,38,0)
	struct hlist_node *tmp_hnode;
	hash_for_each_safe(l_process_info_table, bkt, tmp_hnode,  process_info, tmp, hnode)
#else 
	hash_for_each_safe(l_process_info_table, bkt, tmp, process_info, hnode)
#endif
	{
		if (NULL != process_info)
		{
			printk("pid=%d, comm=%s, params=%s, process_state=%d, delta_utime=%llu, delta_stime=%llu, delta_irq=%llu\n", process_info->pid, process_info->comm, process_info->params, process_info->state, 
						process_info->process_cpu_info.end_stat.utime - process_info->process_cpu_info.start_stat.utime, 
						process_info->process_cpu_info.end_stat.stime - process_info->process_cpu_info.start_stat.stime);
		}
	}
	return 0;
}

static int calculate_single_process_cpu_info_delta(PROCESSINFO *process_info)
{
	int process_occupy_time;
	int cpu_non_idle_time;
	process_info->process_cpu_info.delta.utime = process_info->process_cpu_info.end_stat.utime - process_info->process_cpu_info.start_stat.utime;
	process_info->process_cpu_info.delta.stime = process_info->process_cpu_info.end_stat.stime - process_info->process_cpu_info.start_stat.stime;
	process_occupy_time = process_info->process_cpu_info.delta.utime + process_info->process_cpu_info.delta.stime;
	cpu_non_idle_time = l_cpu_info.sum - l_cpu_info.delta.idle;
	process_info->process_cpu_info.occupy_percent = process_occupy_time * 100 / cpu_non_idle_time;
	//process_info->process_cpu_info.occupy_percent = (process_info->process_cpu_info.delta.utime + process_info->process_cpu_info.delta.stime) * 100 / (l_cpu_info.sum - l_cpu_info.delta.idle);
	return 0;
}

static int calculate_all_process_cpu_info_delta(void)
{
	PROCESSINFO *process_info;
	PROCESSINFO *tmp;

	list_for_each_entry_safe(process_info, tmp, &l_process_info_list, list_for_global)
	{
		if (NULL != process_info)
		{
			calculate_single_process_cpu_info_delta(process_info);
		}
	}
}

static int show_process_info_by_list(void)
{
	PROCESSINFO *process_info;
	PROCESSINFO *tmp;

	list_sort(NULL, &l_process_info_list, descending_by_cpu_occupy_percent);

	list_for_each_entry_safe(process_info, tmp, &l_process_info_list, list_for_global)
	{
		if (NULL != process_info)
		{
			printk("pid=%d, comm=%s, params=%s, process_state=%d, delta_utime=%llu, delta_stime=%llu, occupy_percent=%d\%\n", process_info->pid, process_info->comm, process_info->params, process_info->state, 
						process_info->process_cpu_info.end_stat.utime - process_info->process_cpu_info.start_stat.utime, 
						process_info->process_cpu_info.end_stat.stime - process_info->process_cpu_info.start_stat.stime,
						process_info->process_cpu_info.occupy_percent);
		}
	}
}

static int show_cpu_info(void)
{
	printk("start    user=%llu, system=%llu, idle=%llu, irq=%llu\n", l_cpu_info.start_stat.user, l_cpu_info.start_stat.system, l_cpu_info.start_stat.idle, l_cpu_info.start_stat.irq);
	printk("end      user=%llu, system=%llu, idle=%llu, irq=%llu\n", l_cpu_info.end_stat.user, l_cpu_info.end_stat.system, l_cpu_info.end_stat.idle, l_cpu_info.start_stat.irq);
	printk("delta    user=%llu, system=%llu, idle=%llu, irq=%llu, sum=%llu\n", l_cpu_info.delta.user, l_cpu_info.delta.system, l_cpu_info.delta.idle, l_cpu_info.start_stat.irq, l_cpu_info.sum);
	return 0;
}

static int destroy_process_info(void)
{
	PROCESSINFO *process_info;
	PROCESSINFO *tmp;
	int bkt;
#if LINUX_VERSION_CODE <= KERNEL_VERSION(3,38,0)
	struct hlist_node *tmp_hnode;
	hash_for_each_safe(l_process_info_table, bkt, tmp_hnode,  process_info, tmp, hnode)
#else 
	hash_for_each_safe(l_process_info_table, bkt, tmp, process_info, hnode)
#endif
	{
		if (NULL != process_info)
		{
			hash_del(&process_info->hnode);
			kfree(process_info);
		}
	}
	return 0;
}

static int init_process_info(void)
{
	struct task_struct *task;

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

static int do_exit_handler_pre(struct kprobe *p, struct pt_regs *regs)
{
	struct task_struct *task = current;
	update_process_info(task, PROCESS_EXIT);
	return 0;
}

static int start_to_watch(void)
{
	memset(&l_cpu_info, 0, sizeof(CPUINFO));
	get_cpu_stat(&l_cpu_info.start_stat);
	init_process_info();
	kprobe_certain_func("do_exit", do_exit_handler_pre);
	return 0;
}

static int stop_to_watch(void)
{
	unkprobe_certain_func("do_exit");
	stop_process_info();
	get_cpu_stat(&l_cpu_info.end_stat);
	calculate_cpu_delta(&l_cpu_info);
	calculate_all_process_cpu_info_delta();
	show_cpu_info();
	show_process_info_by_list();
	destroy_process_info();
	return 0;
}

static size_t cpu_stat_wirte(struct file *filp, char __user *buf, size_t size, loff_t *offt)
{
	char kbuf[MAX_CMD_LEN];
	memset(kbuf, 0, sizeof(kbuf));
	if (0 != copy_from_user(kbuf, buf, size))
	{
		printk("copy from user failed.\n");
		return -1;
	}
	kbuf[size - 1] = '\0';

	if (0 == strcmp(kbuf, "start"))
	{
		start_to_watch();
	}
	else if (0 == strcmp(kbuf, "stop"))
	{
		stop_to_watch();
	}
	
	return size;
}
static const struct file_operations proc_cpu_stat_oprations = {
	.owner = THIS_MODULE,
	.open = cpu_stat_open,
	.write = cpu_stat_wirte,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int init_func_pointer_by_kallsyms(void)
{
	/*get_idle_time_fp = (int (*)(int cpu))kallsyms_lookup_name("get_idle_time");
	if (NULL == get_idle_time_fp)
	{
		printk("kallsyms_lookup_name get_idle_time failed\n");
		return -1;
	}
	get_iowait_time_fp = (int (*)(int cpu))kallsyms_lookup_name("get_iowait_time");
	if (NULL == get_iowait_time_fp)
	{
		printk("kallsyms_lookup_name get_iowait_time failed\n");
		return -1;
	}*/
	return 0;
}
/****************************************************************************************************/
/*                                       PUBLIC FUNCTIONS                                           */
/****************************************************************************************************/
int __init cpu_stat_init(void)
{
	if (0 != init_func_pointer_by_kallsyms())
	{
		return -1;
	}
	proc_create("cpustat", 0, NULL, &proc_cpu_stat_oprations);
    printk("cpu_stat module loaded.\n");
	return 0;	
}

int __exit cpu_stat_exit(void)
{
	remove_proc_entry("cpustat", NULL);
    printk("cpu_stat module unloaded.\n");
	return 0;
}

module_init(cpu_stat_init);
module_exit(cpu_stat_exit);
MODULE_LICENSE("GPL");
