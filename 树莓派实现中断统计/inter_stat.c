#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/irqdesc.h>
#include <linux/mutex.h>
#include <asm/fiq.h>
#include <linux/kallsyms.h>
#include <linux/timer.h>
#include <linux/jiffies.h>

#define irq_stats(x)	(&per_cpu(irq_stat, x))
static DEFINE_MUTEX(irq_mutex_lock);
struct timer_list my_timer;
/* ipi types 
 * kallsyms_lookup_name can't export the "ipi_types" address
 * so I defined it on there
 */
static char *ipi_types[NR_IPI] = {
	[0] = "CPU wakeup interrupts",
	[1] = "Timer broadcast interrupts",
	[2] = "Rescheduling interrupts",
	[3] = "Function call interrupts",
	[4] = "CPU stop interrupts",
	[5] = "IRQ work interrupts",
	[6] = "completion interrupts"
};

/* struct for statistic interrupts infomations */
typedef struct inter_print{
	int irq;
	int count_percpu[4];
	char *chipname;
	char *actionsname[10];
	int hwirq;
};
static struct inter_print inter_count[256];

/* struct for statistic ipi interrupts infomations */
typedef struct inter_ipi_print{
	int ipi;
	int count_percpu[4]; // "4" is the raspberry's cpu number
	char *name;
};
static struct inter_ipi_print inter_ipi_count[NR_IPI+1];

MODULE_LICENSE("GPL");
MODULE_AUTHOR("huoman");
MODULE_VERSION("0.1");
MODULE_DESCRIPTION("All CPU's interrupt info statistic");
/*
 * output interrupts info in arm32
*/
static int show_arm32_interrupts_info(void)
{
	unsigned long * irq_err_count;
	unsigned int cpu, i;
	
#ifdef CONFIG_SMP
	for (i = 0; i < NR_IPI; i++) {
		/* if it's first statistic interrutps infomation, the inter_ipi_count[i].ipi == -1 */
		for_each_online_cpu(cpu){
			inter_ipi_count[i].count_percpu[cpu] = (inter_ipi_count[i].ipi == -1) ? (per_cpu(irq_stat.ipi_irqs[i],cpu)):(per_cpu(irq_stat.ipi_irqs[i],cpu) - inter_ipi_count[i].count_percpu[cpu]);
		}
		/* modify the inter_ipi_count[i].ipi, avoid second statistic get error interrrupts count */
		inter_ipi_count[i].ipi = i + 1;
		inter_ipi_count[i].name = ipi_types[i];
	}
#endif
	irq_err_count = (unsigned long *)kallsyms_lookup_name("irq_err_count");
	for(i = 0; i < 4; i++){
		inter_ipi_count[NR_IPI].count_percpu[i] = (inter_ipi_count[NR_IPI].ipi == -1) ? (*irq_err_count) : ((*irq_err_count) - inter_ipi_count[NR_IPI].count_percpu[i]);
	}
	inter_ipi_count[NR_IPI].ipi = 8;
	inter_ipi_count[NR_IPI].name = "ERR";
	return 0;

}

static void irq_lock(void)
{
	mutex_lock(&irq_mutex_lock);
}

static void irq_unlock(void)
{
	mutex_unlock(&irq_mutex_lock);
}

/* output the follow infomation:
 * 1. interrupt frequency from kernel starting
 * 2. interrupts controller'name for interrupts
 * 3. hw irq
 * 4. interrupt name in irq_desc
*/
int show_interrupts_info(int irq)
{
	struct irq_desc *desc;
	unsigned long flag,count=0;
	int j;
	struct irqaction *action;

	/* 打印体系结构相关中断信息 */
	if(irq == nr_irqs)
	{
		j = show_arm32_interrupts_info();
	}

	/* 获取irq_lock */
	irq_lock();

	/* 获取irq_desc */
	desc = irq_to_desc(irq);
	if(!desc)
	{
		irq_unlock();
		return -2;
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
	for_each_online_cpu(j){
		count = desc->kstat_irqs?*per_cpu_ptr(desc->kstat_irqs,j):0;
		inter_count[irq].count_percpu[j] = (inter_count[irq].irq == -1) ? (count) : (count - inter_count[irq].count_percpu[j]);
	}
	/* 获取interrupt controller名称 */
	if(desc->irq_data.chip && desc->irq_data.chip->name && inter_count[irq].chipname == "")
	{
		inter_count[irq].chipname = desc->irq_data.chip->name;
	}

	/* 获取hw irq */
	if(desc->irq_data.domain && inter_count[irq].hwirq == -1)
	{
		inter_count[irq].hwirq = desc->irq_data.hwirq;
	}
	
	j = 0;
	action = desc->action;
	if(action && inter_count[irq].irq == -1)
	{
		while(action != NULL)
		{
			inter_count[irq].actionsname[j++] = action->name;
			action = action->next;
		}
	}

	/* 释放spin lock */
	raw_spin_unlock_irqrestore(&desc->lock,flag);

	/* 释放irq_lock */
	irq_unlock();
	
	inter_count[irq].irq = irq;
	return 0;
}

static void inter_stat(void)
{
	int i = 0;

// 依次访问中断号的统计信息
	while(i <= nr_irqs)
	{
		if(!show_interrupts_info(i))
		{
			i++;
			continue;
		}
		i++;
	}
}

static void delay_print(struct timer_list *unused)
{
	int i,j;
	inter_stat(); // get interrupt info after 5 minutes 
	
	/* print the interrupts infomation */
	for(i = 0;i < nr_irqs;i++)
	{
		if(inter_count[i].irq != -1)
		{
			printk(KERN_CONT "%d|", inter_count[i].irq);
			for(j = 0; j < 4;j++){
				printk(KERN_CONT "%d|", inter_count[i].count_percpu[j]);
			}
			printk(KERN_CONT "%s|", inter_count[i].chipname);
			printk(KERN_CONT "%d|", inter_count[i].hwirq);
			for(j = 0; j < 10;j++)
			{
				if(inter_count[i].actionsname[j] != ""){
				printk(KERN_CONT "%s|", inter_count[i].actionsname[j]);
				}
			}
			printk(KERN_CONT "\n");
		}
	}
	
	/* print the ipi interrupts infomation */
	for(i = 0;i < NR_IPI+1; i++)
	{
		printk(KERN_CONT "IPI%d|", inter_ipi_count[i].ipi);
		for(j = 0; j< 4; j++)
		{
			printk(KERN_CONT "%d|", inter_ipi_count[i].count_percpu[j]);
		}
		printk(KERN_CONT "%s\n",inter_ipi_count[i].name);
	}
}

static int __init inter_stat_init(void)
{

	int i, j;
	printk("nr_irqs:%d",nr_irqs);
	/* initial inter_count */
	for(i = 0;i<nr_irqs;i++)
	{
		inter_count[i].irq = -1;
		inter_count[i].chipname = "";
		inter_count[i].hwirq = -1;
		for(j = 0;j < 10;j++){
			inter_count[i].actionsname[j] = "";
		}
	}
	/* initial inter_ipi_count */
	for(i = 0;i < NR_IPI;i++)
	{
		inter_ipi_count[i].ipi = -1;
	}
	
	
	inter_stat(); // get interrupt info before 5 minutes
	timer_setup(&my_timer, delay_print, 0);
	my_timer.expires = jiffies + (300 * HZ);
	add_timer(&my_timer);

	printk("endoutput!\n");
	return 0;
}

static void __exit inter_stat_exit(void)
{
	printk("This module is removed!\n");
	del_timer(&my_timer);
}

module_init(inter_stat_init);
module_exit(inter_stat_exit);
