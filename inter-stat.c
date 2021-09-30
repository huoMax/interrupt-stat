#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/irqdesc.h>
#include <linux/mutex.h>

#define OVERFLOWIRQ -1
#define IRQERROR -2

static DEFINE_MUTEX(irq_mutex_lock);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("huoman");
MODULE_VERSION("0.1");
MODULE_DESCRIPTION("All CPU's interrupt info statistic");

/* 打印体系结构相关中断信息 */
static int show_arch_irq_stat(void)
{
	printk("I know the Raspberry pi is arm64 so it is trouble,because my vmware is x86_64!");
	return 0;
}
/* define mutex function */
static void irq_lock(void)
{
	mutex_lock(&irq_mutex_lock);
}

static void irq_unlock(void)
{
	mutex_unlock(&irq_mutex_lock);
}

/* 遍历irq，获取:
 * 该irq在每个CPU上的统计信息
 * irq 所属的interrupt controller信息
 * irq 对应的hw irq
 * 驱动程序注册时使用的irq名称
*/
int show_irq_stat(int irq)
{
	struct irq_desc *desc;
	unsigned long flag,count=0;
	int j;

	/* 打印体系结构相关中断信息 */
	if(irq == NR_IRQS)
	{
		show_arch_irq_stat();
	}

	/* 获取irq_lock */
	irq_lock();

	/* 获取irq_desc */
	desc = irq_to_desc(irq);
	if(!desc)
	{
		irq_unlock();
		return IRQERROR;
	}

	/* 获取spin lock */
	raw_spin_lock_irqsave(&desc->lock,flag);

	/* 遍历cpu,获取中断信息 */
	for_each_online_cpu(j)
		count |= desc->kstat_irqs?*per_cpu_ptr(desc->kstat_irqs,j):0;
	if(!count)
	{
		irq_unlock();
		raw_spin_unlock_irqrestore(&desc->lock,flag);
		return 0;
	}
	printk(KERN_CONT "%-12d ",irq);
	for_each_online_cpu(j)
		printk(KERN_CONT "%-10ld ",count);

	/* 获取interrupt controller名称 */
	if(desc->irq_data.chip && desc->irq_data.chip->name)
	{
		printk(KERN_CONT "%-8s ",desc->irq_data.chip->name);
	}
	else
	{
		printk(KERN_CONT "%-8s ","-");
	}

	/* 获取hw irq */
	if(desc->irq_data.domain)
	{
		printk(KERN_CONT "%-12ld ",desc->irq_data.hwirq);
	}

	/* 获取irq 名称 */
	if(desc->name)
	{
		printk(KERN_CONT "%-8s\n",desc->name);
	}

	/* 释放spin lock */
	raw_spin_unlock_irqrestore(&desc->lock,flag);

	/* 释放irq_lock */
	irq_unlock();

	printk(KERN_CONT "\n");
	return 0;
}

static int __init inter_stat_init(void)
{
	int i = 0;
	while(i <= nr_irqs)
	{
		if(!show_irq_stat(i))
		{
/*			printk("There is a error!\n"); */
			i++;
			continue;
		}
		i++;
	}
	return 0;
}

static void __exit inter_stat_exit(void)
{
	printk("This module is uninstalled!\n");
}

module_init(inter_stat_init);
module_exit(inter_stat_exit);
