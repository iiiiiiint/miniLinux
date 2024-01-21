#include "syscall.h"
#include "printk.h"
extern struct task_struct* current;

void syscall(struct pt_regs *regs){
    // printk("in syscall :: %lx.\n", regs->reg[17]);
    regs->sepc += 4;
    if (regs->reg[17] == SYS_WRITE){
        if(regs->reg[10] == 1){
            regs->reg[10] = printk((char *)(regs->reg[11]));
        }
    }else if(regs->reg[17] == SYS_GETPID){
        regs->reg[10] = current->pid;
    }
}