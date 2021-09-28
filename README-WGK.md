# 中断统计

# 要求

- 统计linux系统5分钟之内的中断
- 测试不同情况、环境下获取到的中断统计数据
- 在树莓派上完成实验

# 思路方向

[procfs](%E4%B8%AD%E6%96%AD%E7%BB%9F%E8%AE%A1%20bbf7887dad1541e5b9a6d5e10a3cf56a/procfs%207dfde666467b42b191d9ad577a041bc0.md)

- 借鉴/proc/interrupts 了解其中原理，自己编写一个工具
    - [ ]  了解怎么编写一个/proc
    - [ ]  了解/proc/interrupts里面涉及到了哪些数据结构、调用了哪些方法

# Linux kernel interrupt

## high level handler

## irq number

## irq domain

## interrupt controller

## HW interrupt ID

## irq chip

# 引用

1. [Blog《/proc/interrupts 的数值是如何获得的？》肥叉烧](https://feichashao.com/proc-interrupts/)
2. [蜗窝科技《中断子系统系列》linuxer](http://www.wowotech.net/sort/irq_subsystem)
3. [csdn《desc的High level irq event handler和action》tom-wei](https://blog.csdn.net/chengbeng1745/article/details/91127844)
4. [csdn《linux kernel的中断子系统之（四）：High level irq event handler》jiazhi_lian](https://blog.csdn.net/G_linuxer_/article/details/51011703?utm_medium=distribute.pc_relevant.none-task-blog-2~default~baidujs_title~default-0.no_search_link&spm=1001.2101.3001.4242)
5. [csdn《Linux 中断 —— GIC (中断号映射)》爱洋葱](https://blog.csdn.net/zhoutaopower/article/details/90613988)