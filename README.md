# 中断统计

# 步骤

* 统计linux系统5分钟之内的中断
  * 测试不同情况、环境下获取到的中断统计数据
* 将统计代码移植到在树莓派上完成实验
* 了解Linux时间子系统
  * 打印每一个时钟中断的发生时间
  * 打印每一tick事件的发生时间
  * 打印tick事件发生间隔内发生的其他中断

## 方向
* 了解/proc/interrupts的实现
* 了解linux时间子系统
* 了解idle进程处理
* 了解进程调度时机 
