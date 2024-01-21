#include "printk.h"
#include "clock.h"
#include "proc.h"
#include "syscall.h"

void trap_handler(unsigned long scause, unsigned long sepc, struct pt_regs *regs) {
    // 通过 `scause` 判断trap类型
    // 如果是interrupt 判断是否是timer interrupt
    // 如果是timer interrupt 则打印输出相关信息, 并通过 `clock_set_next_event()` 设置下一次时钟中断
    // `clock_set_next_event()` 见 4.3.4 节
    // 其他interrupt / exception 可以直接忽略

    // YOUR CODE HERE
    	// printk("in trap.c\n");
    	unsigned long temp = 1;
	if((scause&(temp<<63))==(temp<<63) && (scause&5UL)==5UL){

	    clock_set_next_event();

        do_timer();
        
        // printk("[S] Supervisor Timer Interrupt.\n");
	}else if((scause)==8UL){
        syscall(regs);
        // printk("[U] User Environment Call.\n");
    }

}
