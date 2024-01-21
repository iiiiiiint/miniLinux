#include "syscall.h"
#include "printk.h"
#include "string.h"
#include "defs.h"
#include "mm.h"


extern void __ret_from_fork();
extern struct task_struct* current;
extern unsigned long  swapper_pg_dir[512] __attribute__((__aligned__(0x1000)));
extern struct task_struct* task[NR_TASKS]; // 线程数组, 所有的线程都保存在此
extern uint64 TASK_CNT;
extern void create_mapping(uint64 *pgtbl, uint64 va, uint64 pa, uint64 sz, uint64 perm);

uint64 sys_clone(struct pt_regs *regs){

    if(TASK_CNT>=NR_TASKS-1) return -1;

    task[++TASK_CNT] = (struct task_struct *)kalloc();
    memcpy((void *)task[TASK_CNT], (void *)current, PGSIZE);
    task[TASK_CNT]->thread.ra = (uint64)__ret_from_fork;
    task[TASK_CNT]->pid = TASK_CNT;
    // printk("NEW_TASK = %lx\n", (uint64)task[TASK_CNT]);

    uint64 offset = (uint64)regs - (uint64)current;

    struct pt_regs *child_pt_regs = (struct pt_regs *)((uint64)task[TASK_CNT] + offset);
    task[TASK_CNT]->thread.sp = (uint64)child_pt_regs;

    // printk("offset = %lx\n", (uint64)child_pt_regs);
    // while(1);

    child_pt_regs->reg[10] = 0;
    child_pt_regs->reg[2] = (uint64)child_pt_regs;
    // printk("The sepc = %lx\n", task[TASK_CNT]->thread.sepc);
    // while(1);
    // child_pt_regs->reg[32] = task[TASK_CNT]->thread.sepc + 4;
    child_pt_regs->reg[32] = regs->sepc;
    task[TASK_CNT]->pgd = (pagetable_t)kalloc();
    memcpy((void*)(task[TASK_CNT]->pgd), (void*)swapper_pg_dir, PGSIZE);


    for(uint64 i = 0; i < current->vma_cnt; i++){
        uint64 PG_start = (current->vmas[i].vm_start / PGSIZE) * PGSIZE;
        uint64 last_PG = (current->vmas[i].vm_end / PGSIZE) * PGSIZE;
        // printk("vma_start / PGSIZE = %lx\n", PG_start);
        // printk("vma_end / PGSIZE = %lx\n", last_PG);
        for(uint64 j = PG_start;j <= last_PG; j += PGSIZE){
            uint64 vpn2 = (j >> 30) & 0x1ff;
            uint64 vpn1 = (j >> 21) & 0x1ff;
            uint64 vpn0 = (j >> 12) & 0x1ff;
            if(current->pgd[vpn2] & 0x1){
                uint64 *second = (uint64 *)(((current->pgd[vpn2] >> 10) << 12) + PA2VA_OFFSET);
                if(second[vpn1] & 0x1){
                    uint64 *third = (uint64 *)(((second[vpn1] >> 10) << 12) + PA2VA_OFFSET);
                    if(third[vpn0] & 0x1){
                        // printk("The PG_start = %lx\n", j);
                        uint64 flag = (uint64)(third[vpn0]) & 0x3ff;
                        uint64 *pa = (uint64 *)(((third[vpn0] >> 10) << 12) + PA2VA_OFFSET);
                        char *temp = (char *)alloc_page();
                        memcpy((void *)temp, (void *)pa, PGSIZE);
                        create_mapping(task[TASK_CNT]->pgd, (uint64)j, (uint64)temp-PA2VA_OFFSET, PGSIZE, flag);
                    }
                }
            }
        }
    }
    // while(1);
    return TASK_CNT;

}

void syscall(struct pt_regs *regs){
    // printk("in syscall :: %lx.\n", regs->reg[17]);
    regs->sepc += 4;
    if (regs->reg[17] == SYS_WRITE){
        if(regs->reg[10] == 1){
            regs->reg[10] = printk((char *)(regs->reg[11]));
        }
    }else if(regs->reg[17] == SYS_GETPID){
        regs->reg[10] = current->pid;
    }else if(regs->reg[17] == SYS_CLONE){
        regs->reg[10] = sys_clone(regs);
    }
}