#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/irq.h>
#include <linux/irqdesc.h>
#include <linux/interrupt.h>
#include <linux/seq_file.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Zhou Zhiqi");
MODULE_DESCRIPTION("my proc interupt");

static void *int_seq_start(struct seq_file *f, loff_t *pos)
{
    return (*pos <= NR_IRQS) ? pos : NULL;
}

static void *int_seq_next(struct seq_file *f, void *v, loff_t *pos)
{
    (*pos)++;
    
    if (*pos > NR_IRQS)
    {
        return NULL;
    }

    return pos;
}

static void int_seq_stop(struct seq_file *f, void *v)
{
    /* nothing to do*/
}

static int arch_show_my_interrupts(struct seq_file *p, int prec)
{
    return 0;
}

static int show_my_interrupts(struct seq_file *p, void *v)
{
    static int prec;

    unsigned long flags;
    int i = *(loff_t *) v, j;
    struct irqaction *action;
    struct irq_desc *desc;

    if (i > NR_IRQS)
        return 0;

    if (i == NR_IRQS)
        return arch_show_my_interrupts(p, prec);

    // 打印cpu头信息
    if (i == 0) 
    {
        for (prec = 3, j = 1000; prec < 10 && j <= nr_irqs; ++prec)
        {
            j *= 10;
        }

        seq_printf(p, "%*s", prec + 8, "");
        for_each_online_cpu(j)
        {
            seq_printf(p, "CPU%-8d", j);
        }
        seq_printf(p, "%12s", "dev name");
        seq_printf(p, "%8s", "hwid");
        seq_printf(p, "%12s", "INT func");
        seq_putc(p, '\n');
    }

    rcu_read_lock();
    desc = irq_to_desc(i); // 获取中断描述符
    if (!desc)
        goto outsparse;

    seq_printf(p, "%*d: ", prec, i); // 打印逻辑中断号
    for_each_online_cpu(j)
    {
        seq_printf(p, "%10u ", desc->kstat_irqs ? *per_cpu_ptr(desc->kstat_irqs, j) : 0); // 打印中断次数
    }

    raw_spin_lock_irqsave(&desc->lock, flags);
    // 判断是否存在中断控制器
    if (desc->irq_data.chip) 
    {
        if (desc->irq_data.chip->irq_print_chip)
        {
            desc->irq_data.chip->irq_print_chip(&desc->irq_data, p);
        }
        else if (desc->irq_data.chip->name)
        {
            seq_printf(p, " %8s", desc->irq_data.chip->name); // 打印中断名称
        }
        else
        {
            seq_printf(p, " %8s", "-");
        }
    } 
    else 
    {
        seq_printf(p, " %8s", "None");
    }
    // 根据中断域找到逻辑中断号对应的硬件中断号并打印
    if (desc->irq_data.domain)
    {
        seq_printf(p, " %*d", prec, (int) desc->irq_data.hwirq); 
    }
    else
    {
        seq_printf(p, " %*s", prec, "");
    }

    // 打印中断触发类型
    // seq_printf(p, " %-8s", irqd_is_level_type(&desc->irq_data) ? "Level" : "Edge"); 

    // intterupt hardware name
    if (desc->name)
        seq_printf(p, "-%-8s", desc->name);

    action = desc->action;
    if (action) 
    {
        seq_printf(p, "  %s", action->name); // 打印中断处理函数
        while ((action = action->next) != NULL)
        {
            seq_printf(p, ", %s", action->name);
        }

    }
    seq_putc(p, '\n');
    raw_spin_unlock_irqrestore(&desc->lock, flags);

outsparse:
    rcu_read_unlock();
    return 0;
}

static const struct seq_operations int_seq_ops = {
    .start = int_seq_start,
    .next = int_seq_next,
    .stop = int_seq_stop,
    .show = show_my_interrupts
};

static int __init proc_my_interrupts_init(void)
{
    proc_create_seq("my_interrupts", 0, NULL, &int_seq_ops);    
    return 0;
}

static void __exit proc_my_interrupt_exit(void)
{
    printk("my_interrupts modules exit");
}

module_init(proc_my_interrupts_init);
module_exit(proc_my_interrupt_exit);
