#include <linux/init.h>
#include <linux/module.h>
#include <linux/irq.h>
#include <linux/irqdesc.h>
#include <linux/interrupt.h>
#include <linux/mutex.h>
#include <linux/timer.h>
#include <linux/jiffies.h>
#include <linux/cpu.h>
#include <asm/mce.h>
#include <asm/apic.h>
#include <asm/io_apic.h>

MODULE_LICENSE("GPL");

#define IRQ_NUMBER 100
#define irq_stats(x)  (&per_cpu(irq_stat, x))


// 统计irq中的数据
struct int_stat
{
	int irq; // irqn
	char name[4];
	int tota_int;
	char dev[20];
	char func[20];
	char desc[30];
};

static int index;
static int turn = 2;
static struct int_stat print_int_stat[IRQ_NUMBER];
struct timer_list my_timer;

static int show_interrupts_inkernel(int i)
{
	int j = 0;
	unsigned long flags, any_count = 0, total_flags = 0;
	struct irqaction *action;
	struct irq_desc *desc;
	
	
	rcu_read_lock();
	
	// 获取 desc 中断描述符
	desc = irq_to_desc(i);
	if (!desc)
	{
		goto outparse; 
	}
	
	// 过滤掉未挂载中断函数的中断
	if (desc->kstat_irqs)
	{
		for_each_online_cpu(j)
		{
			any_count |= *per_cpu_ptr(desc->kstat_irqs, j);
		}
	}
	if (!desc->action && !any_count)
	{
		goto outparse;
	}
	
	// 打印逻辑中断号
	printk(KERN_CONT "%-8d", i); 
	print_int_stat[index].irq = i;
	
	// 打印每个cpu上的中断次数
	for_each_online_cpu(j)
	{
		printk(KERN_CONT "%-8d", desc->kstat_irqs ? *per_cpu_ptr(desc->kstat_irqs, j) : 0);
		if (desc->kstat_irqs ? *per_cpu_ptr(desc->kstat_irqs, j) : 0)
		{
			total_flags = 1;
			print_int_stat[index].tota_int +=  desc->kstat_irqs ? *per_cpu_ptr(desc->kstat_irqs, j) : 0;
		}
	}
	
	raw_spin_lock_irqsave(&desc->lock,flags);
	// 打印中断寄存器
	if (desc->irq_data.chip)
	{
		printk(KERN_CONT "%8s", desc->irq_data.chip->name);
		sprintf(print_int_stat[index].dev, desc->irq_data.chip->name);
	}
	else
	{
		printk(KERN_CONT "%8s", "none");
	}
	// 打印硬件中断号
	if (desc->irq_data.domain)
	{
		printk(KERN_CONT "%8d", (int)desc->irq_data.hwirq);
	}
	else
	{
		printk(KERN_CONT "%8s", "");
	}
	// 打印中断描述符名称
	if (desc->name)
	{
		printk(KERN_CONT "-%-8s", desc->name);
	//	sprintf(print_int_stat[index].func, desc->irq_data.chip->name);
	}
	action = desc->action;
	if (action)
	{
		printk(KERN_CONT "  %s", action->name);
		sprintf(print_int_stat[index].func, action->name);
		while ((action = action->next) != NULL)
		{
			printk(KERN_CONT ",  %s", action->name);
		}
	}
	printk(KERN_CONT "\n");

	raw_spin_unlock_irqrestore(&desc->lock, flags);
	if (total_flags)
	{
		index++;
	}
	
outparse:
	rcu_read_unlock();
	return 0;
}

static void arch_show_x86_interrupts_inkernel(void)
{
	int j;
	

	printk(KERN_CONT "%-8s", "NMI");
	for_each_online_cpu(j)
	{
		printk(KERN_CONT "%-8u ", irq_stats(j)->__nmi_count);
		print_int_stat[index].tota_int += (int)irq_stats(j)->__nmi_count;
	}
	printk(KERN_CONT " Non-maskable interrupts\n");	
	if (print_int_stat[index].tota_int!= 0)
	{
		sprintf(print_int_stat[index].name, "NMI");
		sprintf(print_int_stat[index].desc, "Non-maskable interrupts");
		index++;
	}

	printk(KERN_CONT "%-8s", "LOC");
	for_each_online_cpu(j)
	{
		printk(KERN_CONT "%-8u ", irq_stats(j)->apic_timer_irqs);
		print_int_stat[index].tota_int += (int)irq_stats(j)->apic_timer_irqs;
	}
	printk(KERN_CONT " Local timer interrupts\n");	
	if (print_int_stat[index].tota_int!= 0)
	{
		sprintf(print_int_stat[index].name, "LOC");
		sprintf(print_int_stat[index].desc, "Local timer interrupts");
		index++;
	}
	
	printk(KERN_CONT "%-8s", "SPU");
	for_each_online_cpu(j)
	{
		printk(KERN_CONT "%-8u ", irq_stats(j)->irq_spurious_count);
		print_int_stat[index].tota_int += irq_stats(j)->irq_spurious_count;
	}
	printk(KERN_CONT " Spurious interrupts\n");
	if (print_int_stat[index].tota_int!= 0)
	{
		sprintf(print_int_stat[index].name, "SPU");
		sprintf(print_int_stat[index].desc, "Spurious interrupts");
		index++;
	}
	
	printk(KERN_CONT "%-8s", "PMI");
	for_each_online_cpu(j)
	{
		printk(KERN_CONT "%-8u ", irq_stats(j)->apic_perf_irqs);
		print_int_stat[index].tota_int += irq_stats(j)->apic_perf_irqs;
	}
	printk(KERN_CONT " Performance monitoring interrupts\n");	
	if (print_int_stat[index].tota_int!= 0)
	{
		sprintf(print_int_stat[index].name, "PMI");
		sprintf(print_int_stat[index].desc, "Performance monitoring interrupts");
		index++;
	}
	
	printk(KERN_CONT "%-8s", "IWI");
	for_each_online_cpu(j)
	{
		printk(KERN_CONT "%-8u ", irq_stats(j)->apic_irq_work_irqs);
		print_int_stat[index].tota_int += irq_stats(j)->apic_irq_work_irqs;
	}
	printk(KERN_CONT " IRQ work interrupts\n");	
	if (print_int_stat[index].tota_int!= 0)
	{
		sprintf(print_int_stat[index].name, "PMI");
		sprintf(print_int_stat[index].desc, "IRQ work interrupts");
		index++;
	}
	
	printk(KERN_CONT "%-8s", "RTR");
	for_each_online_cpu(j)
	{
		printk(KERN_CONT "%-8u ", irq_stats(j)->icr_read_retry_count);
		print_int_stat[index].tota_int += irq_stats(j)->icr_read_retry_count;
	}
	printk(KERN_CONT " APIC ICR read retries\n");
	if (print_int_stat[index].tota_int!= 0)
	{
		sprintf(print_int_stat[index].name, "PMI");
		sprintf(print_int_stat[index].desc, "APIC ICR read retries");
		index++;
	}
	
	printk(KERN_CONT "%-8s", "RES");
	for_each_online_cpu(j)
	{

		printk(KERN_CONT "%-8u ", irq_stats(j)->irq_resched_count);
		print_int_stat[index].tota_int += irq_stats(j)->irq_resched_count;
	}
	printk(KERN_CONT " Rescheduling interrupts\n");	
	if (print_int_stat[index].tota_int!= 0)
	{
		sprintf(print_int_stat[index].name, "PMI");
		sprintf(print_int_stat[index].desc, "Rescheduling interrupts");
		index++;
	}
	
	printk(KERN_CONT "%-8s", "CAL");
	for_each_online_cpu(j)
	{
		printk(KERN_CONT "%-8u ", irq_stats(j)->irq_call_count);
		print_int_stat[index].tota_int += irq_stats(j)->irq_call_count;
	}
	printk(KERN_CONT " Function call interrupts\n");
	if (print_int_stat[index].tota_int!= 0)
	{
		sprintf(print_int_stat[index].name, "PMI");
		sprintf(print_int_stat[index].desc, "Function call interrupts");
		index++;
	}
	
	printk(KERN_CONT "%-8s", "TLB");
	for_each_online_cpu(j)
	{
		printk(KERN_CONT "%-8u ", irq_stats(j)->irq_tlb_count);
		print_int_stat[index].tota_int += irq_stats(j)->irq_tlb_count;
	}
	printk(KERN_CONT " TLB shootdowns\n");
	if (print_int_stat[index].tota_int!= 0)
	{
		sprintf(print_int_stat[index].name, "PMI");
		sprintf(print_int_stat[index].desc, "TLB shootdowns");
		index++;
	}
	
	printk(KERN_CONT "%-8s", "TRM");
	for_each_online_cpu(j)
	{
		printk(KERN_CONT "%-8u ", irq_stats(j)->irq_thermal_count);
		print_int_stat[index].tota_int += irq_stats(j)->irq_thermal_count;
	}
	printk(KERN_CONT " Thermal event interrupts\n");
	if (print_int_stat[index].tota_int!= 0)
	{
		sprintf(print_int_stat[index].name, "PMI");
		sprintf(print_int_stat[index].desc, "Thermal event interrupts");
		index++;
	}
	
	printk(KERN_CONT "%-8s", "THR");
	for_each_online_cpu(j)
	{
		printk(KERN_CONT "%-8u ", irq_stats(j)->irq_threshold_count);
		print_int_stat[index].tota_int += irq_stats(j)->irq_threshold_count;
	}
	printk(KERN_CONT " Threshold APIC interrupts\n");
	if (print_int_stat[index].tota_int!= 0)
	{
		sprintf(print_int_stat[index].name, "PMI");

		sprintf(print_int_stat[index].desc, "Threshold APIC interrupts");
		index++;
	}
	
	printk(KERN_CONT "%-8s", "DFR");
	for_each_online_cpu(j)
	{
		printk(KERN_CONT "%-8u ", irq_stats(j)->irq_deferred_error_count);
		print_int_stat[index].tota_int += irq_stats(j)->irq_deferred_error_count;
	} 
	printk(KERN_CONT " Deferred Error APIC interrupts\n");
	if (print_int_stat[index].tota_int!= 0)
	{
		sprintf(print_int_stat[index].name, "PMI");
		sprintf(print_int_stat[index].desc, "Deferred Error APIC interrupts");
		index++;
	}
	
	/*printk(KERN_CONT "%-8s", "MCE");
	for_each_online_cpu(j)
	{
		printk(KERN_CONT "%-8u ", per_cpu(mce_exception_count, j));
	}
	printk(KERN_CONT " Machine check exceptions\n");	
	printk(KERN_CONT "%-8s", "MCP");
	for_each_online_cpu(j)
	{
		printk(KERN_CONT "%-8u ", per_cpu(mce_poll_count, j));
	}
	printk(KERN_CONT " Machine check polls\n");
	*/
	printk(KERN_CONT "%-8s", "PIN");
	for_each_online_cpu(j)
	{
		printk(KERN_CONT "%-8u ", irq_stats(j)->kvm_posted_intr_ipis);
		print_int_stat[index].tota_int += irq_stats(j)->kvm_posted_intr_ipis;
	}
	printk(KERN_CONT " TPosted-interrupt notification event\n");
	if (print_int_stat[index].tota_int!= 0)
	{
		sprintf(print_int_stat[index].name, "PMI");
		sprintf(print_int_stat[index].desc, "TPosted-interrupt notification event");
		index++;
	}
	
	printk(KERN_CONT "%-8s", "NPI");
	for_each_online_cpu(j)
	{
		printk(KERN_CONT "%-8u ", irq_stats(j)->kvm_posted_intr_nested_ipis);
		print_int_stat[index].tota_int += irq_stats(j)->kvm_posted_intr_nested_ipis;
	}
	printk(KERN_CONT " Nested posted-interrupt event\n");
	if (print_int_stat[index].tota_int!= 0)
	{
		sprintf(print_int_stat[index].name, "PMI");
		sprintf(print_int_stat[index].desc, "Nested posted-interrupt event");
		index++;
	}
	
	printk(KERN_CONT "%-8s", "PIW");
	for_each_online_cpu(j)
	{
		printk(KERN_CONT "%-8u ", irq_stats(j)->kvm_posted_intr_wakeup_ipis);
		print_int_stat[index].tota_int += irq_stats(j)->kvm_posted_intr_wakeup_ipis;
	}
	printk(KERN_CONT " Posted-interrupt wakeup event\n");
	if (print_int_stat[index].tota_int!= 0)
	{
		sprintf(print_int_stat[index].name, "PMI");
		sprintf(print_int_stat[index].desc, "Posted-interrupt wakeup event");
		index++;
	}
	
}

static void show_all_interruts(void)
{	
	int i;
	for (i = 0; i < index; i++)
	{
		if (print_int_stat[i].name[0] == '\0')
		{
			printk(KERN_CONT "%-8d", print_int_stat[i].irq);
			printk(KERN_CONT "%-8d", print_int_stat[i].tota_int);
			printk(KERN_CONT "%-8s", print_int_stat[i].dev);
			printk(KERN_CONT "%-8s", print_int_stat[i].func);
		}
		else
		{
			printk(KERN_CONT "%-8s", print_int_stat[i].name);
			printk(KERN_CONT "%-8d", print_int_stat[i].tota_int);
			printk(KERN_CONT "%-s", print_int_stat[i].desc);
		}

		printk(KERN_CONT "\n");
	}
}

static void time_print(struct timer_list *unused)
{
	int i;

	printk("\nround turn %d\n", turn);
	turn++;
	for (i = 0; i < NR_IRQS; i++)
	{
		show_interrupts_inkernel(i);
	}
	
	arch_show_x86_interrupts_inkernel();
	printk("start print total data\n");
	show_all_interruts();
}

static int __init show_irq_init(void)
{
	int i , j;
	// 初始化定时器

	
	// 打印 cpu 头信息
	printk("***********************************************************************\n");
	printk(KERN_CONT "%*s", 8, "");
	for_each_online_cpu(j)
	{
		printk(KERN_CONT "CPU%-d    ", j);
	}
	printk(KERN_CONT "\n");

	for (i = 0; i < 256; i++)
	{
		show_interrupts_inkernel(i);
	}
	arch_show_x86_interrupts_inkernel();
	
	printk("start print total data\n\n");
	show_all_interruts();
	
	timer_setup(&my_timer, time_print, 0);
	my_timer.expires = jiffies + (300*HZ);
	add_timer(&my_timer);
	
	return 0;
}

static void __exit show_irq_exit(void)
{
	printk("modules irq exit\n");
	del_timer(&my_timer);
}

module_init(show_irq_init);
module_exit(show_irq_exit);
