#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/hashtable.h>
#include <linux/slab.h>

typedef struct student {
    int id;
    char name[32];
    struct hlist_node hnode; /* 哈希表节点 */
} HASHTEST;

DEFINE_HASHTABLE(student_table, 8); /* 定义哈希表，8表示哈希表的大小 */

static int __init hash_test_init(void)
{
    HASHTEST *s;
    HASHTEST *found;
    int id_to_find = 42;

    printk(KERN_INFO "hash_test_init, size of the hashtable is: %d\n", HASH_SIZE(student_table));
    //创建学生记录
    s = kmalloc(sizeof(HASHTEST), GFP_KERNEL);
    if (!s)
    {
        printk(KERN_ERR "Failed to allocate memory for student\n");
        return -ENOMEM;
    }
    s->id = 42;
    strcpy(s->name, "dongdong");

    //添加学生记录到哈希表
    hash_add(student_table, &s->hnode, s->id);

    //查找学生记录
    hash_for_each_possible(student_table, found, hnode, id_to_find)
    {
        if (found->id == id_to_find)
        {
            printk(KERN_INFO "Student found: ID= %d, Name=%s\n", found->id, found->name);
            break;
        }
    }
    return 0;
}

static void __exit hash_test_exit(void)
{
    HASHTEST *s;
    HASHTEST *tmp;
    int bkt;
    //遍历哈希表并释放内存
    hash_for_each_safe(student_table, bkt, s, tmp, hnode)
    {
        if (s != NULL)
        {
            hash_del(&s->hnode);
            kfree(s);
        }
    }
}

module_init(hash_test_init);
module_exit(hash_test_exit);

MODULE_LICENSE("GPL");
