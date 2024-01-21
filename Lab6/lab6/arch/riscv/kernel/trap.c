#include "printk.h"
#include "clock.h"
#include "proc.h"
#include "syscall.h"
#include "defs.h"
#include "string.h"
#include "elf.h"
#include "mm.h"

extern char _sramdisk[];
extern struct task_struct* current;
extern void create_mapping(uint64 *pgtbl, uint64 va, uint64 pa, uint64 sz, uint64 perm);
extern struct task_struct* task[NR_TASKS]; // 线程数组, 所有的线程都保存在此

struct vm_area_struct *find_vma(struct task_struct *task, uint64 addr){
    uint64 num = task->vma_cnt;
    // printk("%lx\n", num);
    // while(1);
    for(uint64 i = 0; i < num; i++){
        // printk("%lx\n", task->vmas[i].vm_start);
        if(task->vmas[i].vm_start <= addr && task->vmas[i].vm_end >= addr){
            return &(task->vmas[i]);
        }
    }

    return NULL;
}

void do_page_fault(struct pt_regs *regs) {
    /*
     1. 通过 stval 获得访问出错的虚拟内存地址（Bad Address）
     2. 通过 find_vma() 查找 Bad Address 是否在某个 vma 中
     3. 分配一个页，将这个页映射到对应的用户地址空间
     4. 通过 (vma->vm_flags & VM_ANONYM) 获得当前的 VMA 是否是匿名空间
     5. 根据 VMA 匿名与否决定将新的页清零或是拷贝 uapp 中的内容
    */
        
    uint64 stvall = regs->stval;
    struct vm_area_struct *vma_temp = find_vma(current, stvall);
    if(vma_temp != NULL){
            uint64 seg_start = ((uint64)_sramdisk + vma_temp->vm_content_offset_in_file);
            uint64 PG_start = (stvall / PGSIZE) * PGSIZE;
            uint64 PG_end = PG_start + PGSIZE;
            char *temp = (char *)alloc_pages(1);
            memset((void*)temp, 0x0, PGSIZE);
            // printk("type:0.\n");
            // while (1);
            if(!(vma_temp->vm_flags & VM_ANONYM)){
                if(PG_start <= vma_temp->vm_start){
                    uint64 pre_offset = vma_temp->vm_start - PG_start;
                    if(PG_end < vma_temp->vm_start + vma_temp->vm_content_size_in_file){
                        // printk("type:1.\n");
                        // while (1);
                        memcpy((void *)((uint64)temp + pre_offset),(void*)(seg_start), PGSIZE - pre_offset);
                    }else{
                        // printk("type:2.\n");
                        // printk("PG_start = %lx, ",PG_start);
                        // printk("vm_start = %lx.\n", vma_temp->vm_start);
                        // printk("PG_END = %lx, ", PG_end);
                        // printk("end = %lx\n", vma_temp->vm_start + vma_temp->vm_content_size_in_file);
                        uint64 la_offset = PG_end - (vma_temp->vm_start + vma_temp->vm_content_size_in_file);
                        // printk("pre_offset = %lx, ", pre_offset);
                        // printk("la_offset = %lx\n", la_offset);
                        // printk("size = %lx\n", PGSIZE - pre_offset - la_offset);
                        // while (1);
                        memcpy((void *)((uint64)temp + pre_offset), (void*)(seg_start), PGSIZE - pre_offset - la_offset);
                    }
                }else{
                    uint64 offset = PG_start - vma_temp->vm_start;
                    if(PG_end < vma_temp->vm_start + vma_temp->vm_content_size_in_file){
                        // printk("type:3.\n");
                        // while (1);
                        memcpy((void*)(temp), (void*)(seg_start + offset), PGSIZE);
                    }else{
                        // printk("type:4.\n");
                        // while (1);
                        uint64 la_offset = PG_end - (vma_temp->vm_start + vma_temp->vm_content_size_in_file);
                        // printk("SSIZE = %lx\n",vma_temp->vm_start + vma_temp->vm_content_size_in_file);
                        // printk("SSIZE = %lx\n",PG_start);
                        // printk("SSIZE = %lx\n",PG_end);
                        if(PG_start<vma_temp->vm_start + vma_temp->vm_content_size_in_file)
                            memcpy((void*)(temp), (void*)(seg_start + offset), PGSIZE - la_offset);
                    }
                }
            }
            // printk("flag is %lx\n", vma_temp->vm_flags | 1UL | 1UL << 4);
            create_mapping(current->pgd, (uint64)PG_start, (uint64)temp-PA2VA_OFFSET, PGSIZE, vma_temp->vm_flags | 1UL | 1UL << 4);
    }else{
        // printk("type:-1.\n");
            // while (1);
    }
}

void trap_handler(unsigned long scause, unsigned long sepc, struct pt_regs *regs) {
    // 通过 `scause` 判断trap类型
    // 如果是interrupt 判断是否是timer interrupt
    // 如果是timer interrupt 则打印输出相关信息, 并通过 `clock_set_next_event()` 设置下一次时钟中断
    // `clock_set_next_event()` 见 4.3.4 节
    // 其他interrupt / exception 可以直接忽略

    // YOUR CODE HERE
    	// printk("in trap.c\n");
    	unsigned long temp = 1;
	if(scause == 0x8000000000000005){
	    clock_set_next_event();

        do_timer();
        
        printk("[S] Supervisor Timer Interrupt.\n");
	}else if(scause == 8UL){
        syscall(regs);
        // printk("[U] User Environment Call.\n");
    }else if(scause == 0xc | scause == 0xd | scause == 0xf){
        printk("[S] Supervisor Page Falut, ");
        printk("scause: %lx, ", scause);
        printk("stval: %lx, ", regs->stval);
        printk("sepc: %lx\n", regs->sepc);
        do_page_fault(regs);
    }else{
        printk("[S] Unhandled trap, ");
        printk("scause: %lx, ", scause);
        printk("stval: %lx, ", regs->stval);
        printk("sepc: %lx\n", regs->sepc);
        while (1);
    }

}
