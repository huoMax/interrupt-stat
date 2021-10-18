#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/irqdesc.h>
#include <linux/mutex.h>
#include <asm/desc.h>
#include <asm/mce.h>
#include <asm/apic.h>
#include <asm/io_apic.h>
#include <asm/trace/irq_vectors.h>

#define OVERFLOWIRQ -1
#define IRQERROR -2
#define irq_stats(x)	(&per_cpu(irq_stat, x))

unsigned int mce_exception_count=0;
unsigned int mce_poll_count=0;
static DEFINE_MUTEX(irq_mutex_lock);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("huoman");
MODULE_VERSION("0.1");
MODULE_DESCRIPTION("All CPU's interrupt info statistic");

/* 打印体系结构相关中断信息 */
static int show_arch_irq_stat(void)
{
	int j;
/* 不可屏蔽中断 */
	printk(KERN_CONT "%-12s ","NMI");
	for_each_online_cpu(j)
		printk(KERN_CONT "%-10u ", irq_stats(j)->__nmi_count);
	printk(KERN_CONT "  Non-maskable interrupts\n");
#ifdef CONFIG_X86_LOCAL_APIC
/* APIC Timer 产生的中断(APIC timer generated interrupts) */
	printk(KERN_CONT "%-12s ","LOC");
	for_each_online_cpu(j)
		printk(KERN_CONT "%-10u ", irq_stats(j)->apic_timer_irqs);
	printk(KERN_CONT "  Local timer interrupts\n");

/* 伪中断（spurious interrupt）。一类不希望被产生的硬件中断
 * #define SPURIOUS_APIC_VECTOR		0xff
*/
	printk(KERN_CONT "%-12s ", "SPU");
	for_each_online_cpu(j)
		printk(KERN_CONT "%-10u ", irq_stats(j)->irq_spurious_count);
	printk(KERN_CONT "  Spurious interrupts\n");

/* Performance Monitoring Counter 在 overflow 时产生的中断(Performance monitoring counter interrupts) */
	printk(KERN_CONT "%-12s ", "PMI");
	for_each_online_cpu(j)
		printk(KERN_CONT "%-10u ", irq_stats(j)->apic_perf_irqs);
	printk(KERN_CONT "  Performance monitoring interrupts\n");

/* 本地直连 IO 设备 (Locally connected I/O devices) 通过 LINT0 和 LINT1 引脚发来的中断 */
	printk(KERN_CONT  "%-12s ", "IWI");
	for_each_online_cpu(j)
		printk(KERN_CONT  "%-10u ", irq_stats(j)->apic_irq_work_irqs);
	printk(KERN_CONT "  IRQ work interrupts\n");

/* ICR(Interrupt Command Register) 用于发送 IPI */
	printk(KERN_CONT "%-12s ","RTR");
	for_each_online_cpu(j)
		printk(KERN_CONT  "%-10u ", irq_stats(j)->icr_read_retry_count);
	printk(KERN_CONT "  APIC ICR read retries\n");

/* 其他 CPU (甚至是自己，称为 self-interrupt)发来的 IPI(Inter-processor interrupts) */
	printk(KERN_CONT  "%-12s ", "PLT");
	for_each_online_cpu(j)
		printk(KERN_CONT  "%-10u ", irq_stats(j)->x86_platform_ipis);
	printk(KERN_CONT "  Platform interrupts\n");
#endif
#ifdef CONFIG_SMP
/* 调度器将工作从一个内核卸载到另一个休眠的内核时，调度器发送的IPI消息唤醒事件就是Rescheduling interrupts
 * #define RESCHEDULE_VECTOR		0xfd
 */
	printk(KERN_CONT  "%-12s ", "RES");
	for_each_online_cpu(j)
		printk(KERN_CONT  "%-10u ", irq_stats(j)->irq_resched_count);
	printk(KERN_CONT "  Rescheduling interrupts\n");

/* function call interrupts actually count software interrupts, executed by the INT <> instruction, depending on the CPU type
 * #define CALL_FUNCTION_VECTOR		0xfc
*/
	printk(KERN_CONT  "%-12s ", "CAL");
	for_each_online_cpu(j)
		printk(KERN_CONT  "%-10u ", irq_stats(j)->irq_call_count);
	printk(KERN_CONT "  Function call interrupts\n");

/* 多CPU共享一个页表时，某个CPU修改该页表的地址映射时, 必须通知其他的CPU修改自己的TLB, 这种IPI事件称为TLB shootdowns */
	printk(KERN_CONT  "%-12s ", "TLB");
	for_each_online_cpu(j)
		printk(KERN_CONT  "%-10u ", irq_stats(j)->irq_tlb_count);
	printk(KERN_CONT "  TLB shootdowns\n");
#endif
#ifdef CONFIG_X86_THERMAL_VECTOR
/* machine check 是一种用来报告内部错误的一种硬件的方式
 * APIC的热敏原件发来的温度中断
 */
	printk(KERN_CONT  "%-12s ", "TRM");
	for_each_online_cpu(j)
		printk(KERN_CONT  "%-10u ", irq_stats(j)->irq_thermal_count);
	printk(KERN_CONT "  Thermal event interrupts\n");
#endif
#ifdef CONFIG_X86_MCE_THRESHOLD
	printk(KERN_CONT  "%-12s ", "THR");
	for_each_online_cpu(j)
		printk(KERN_CONT  "%-10u ", irq_stats(j)->irq_threshold_count);
	printk(KERN_CONT "  Threshold APIC interrupts\n");
#endif
#ifdef CONFIG_X86_MCE_AMD
	printk(KERN_CONT  "%-12s ", "DFR");
	for_each_online_cpu(j)
		printk(KERN_CONT  "%-10u ", irq_stats(j)->irq_deferred_error_count);
	printk(KERN_CONT "  Deferred Error APIC interrupts\n");
#endif
#ifdef CONFIG_X86_MCE
/* machine check exceptions(MCEs) 是在硬件不能纠正内部错误的时候发生，
 * 在这种情况下，通常会中断 CPU 当前正在运行的程序，并且调用一个特殊的异常处理程序。
 * 这种情况通常需要软件来进行处理，即 machine check exception handler
 * 当硬件能够纠正内部错误的时候，这种情况通常称作 silent machine check。
*/
	printk(KERN_CONT  "%-12s ", "MCE");
	for_each_online_cpu(j)
		printk(KERN_CONT  "%-10u ", per_cpu(mce_exception_count, j));
	printk(KERN_CONT "  Machine check exceptions\n");
	printk(KERN_CONT  "%-12s ", "MCP");
	for_each_online_cpu(j)
		printk(KERN_CONT  "%-10u ", per_cpu(mce_poll_count, j));
	printk(KERN_CONT "  Machine check polls\n");
#endif
/*  HYPER V 硬件虚拟化 */
#if IS_ENABLED(CONFIG_HYPERV) || defined(CONFIG_XEN)
	printk(KERN_CONT  "%-12s ", "HYP");
	for_each_online_cpu(j)
		printk(KERN_CONT  "%-10u ",irq_stats(j)->irq_hv_callback_count);
	printk(KERN_CONT "  Hypervisor callback interrupts\n");
#endif
#if IS_ENABLED(CONFIG_HYPERV)
	printk(KERN_CONT  "%-12s ", "HRE");
	for_each_online_cpu(j)
		printk(KERN_CONT  "%-10u ",irq_stats(j)->irq_hv_reenlightenment_count);
	printk(KERN_CONT "  Hyper-V reenlightenment interrupts\n");
	printk(KERN_CONT  "%-12s ", "HVS");
	for_each_online_cpu(j)
		printk(KERN_CONT  "%-10u ",irq_stats(j)->hyperv_stimer0_count);
	printk(KERN_CONT "  Hyper-V stimer0 interrupts\n");
#endif
#ifdef CONFIG_HAVE_KVM
/* #define POSTED_INTR_VECTOR		0xf2*/
	printk(KERN_CONT  "%-12s ", "PIN");
	for_each_online_cpu(j)
		printk(KERN_CONT  "%-10u ", irq_stats(j)->kvm_posted_intr_ipis);
	printk(KERN_CONT "  Posted-interrupt notification event\n");

/* #define POSTED_INTR_NESTED_VECTOR	0xf0 */
	printk(KERN_CONT  "%-12s ", "NPI");
	for_each_online_cpu(j)
		printk(KERN_CONT  "%-10u ",
			   irq_stats(j)->kvm_posted_intr_nested_ipis);
	printk(KERN_CONT "  Nested posted-interrupt event\n");

/* #define POSTED_INTR_WAKEUP_VECTOR	0xf1 */
	printk(KERN_CONT  "%-12s ", "PIW");
	for_each_online_cpu(j)
		printk(KERN_CONT  "%-10u ",
			   irq_stats(j)->kvm_posted_intr_wakeup_ipis);
	printk(KERN_CONT "  Posted-interrupt wakeup event\n");
#endif
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
	if(irq == nr_irqs)
	{
		j = show_arch_irq_stat();
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
		printk(KERN_CONT "%-10d ",desc->kstat_irqs?*per_cpu_ptr(desc->kstat_irqs,j):0);

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
	int j = 0;
	printk(KERN_CONT "%-12s ", "interrupt");
	for_each_online_cpu(j)
		printk(KERN_CONT "CPU%-7d ", j);
	printk(KERN_CONT "%-8s ", "control");
	printk(KERN_CONT "%-12s ", "hwirq");
	printk(KERN_CONT "%-8s ", "descname");
	printk(KERN_CONT "\n");
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
