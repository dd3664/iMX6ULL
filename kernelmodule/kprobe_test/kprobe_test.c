/****************************************************************************************************/
/*                                           INCLUDE                                                */
/****************************************************************************************************/
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kprobes.h>
#include <linux/slab.h>

/****************************************************************************************************/
/*                                           DEFINES                                                */
/****************************************************************************************************/

/****************************************************************************************************/
/*                                           VARIABLES                                              */
/****************************************************************************************************/
struct kprobe kp;

/****************************************************************************************************/
/*                                       STATIC FUNCTIONS                                           */
/****************************************************************************************************/

/****************************************************************************************************/
/*                                       PUBLIC FUNCTIONS                                           */
/****************************************************************************************************/
int handler_pre(struct kprobe *p, struct pt_regs *regs)
{
    printk(KERN_INFO "Kprobe pre-handler: Fuction __kmalloc is about to be called.\n");
    return 0;
}

int __init kprobe_example_init(void)
{
    int ret = 0;
    printk(KERN_INFO "Kprobe example module loaded.\n");

    //初始化kprobe结构
    kp.pre_handler = handler_pre;
    kp.symbol_name = "__kmalloc"; // 要跟踪的内核函数名

    //注册kprobe
    ret = register_kprobe(&kp);
    printk(KERN_INFO "register_kprobe ret: %d\n", ret);
    if (ret < 0)
    {
        printk(KERN_ERR "Failed to register Kprobe.\n");
        return -1;
    }
    return 0;
}

void __exit kprobe_example_exit(void)
{
    printk(KERN_INFO "Kprobe example module unloaded.\n");
    
    //卸载kprobe
    unregister_kprobe(&kp);
}

module_init(kprobe_example_init);
module_exit(kprobe_example_exit);
MODULE_LICENSE("GPL");