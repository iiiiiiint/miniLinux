//arch/riscv/kernel/proc.c
#include "proc.h"
#include "mm.h"
#include "defs.h"
#include "rand.h"
#include "printk.h"
#include "test.h"
#include "string.h"
#include "elf.h"

//arch/riscv/kernel/proc.c
extern void __dummy();
extern void __switch_to(struct task_struct* prev, struct task_struct* next);
extern unsigned long  swapper_pg_dir[512] __attribute__((__aligned__(0x1000)));
extern void create_mapping(uint64 *pgtbl, uint64 va, uint64 pa, uint64 sz, uint64 perm);
struct task_struct* idle;           // idle process
struct task_struct* current;        // 指向当前运行线程的 `task_struct`
struct task_struct* task[NR_TASKS]; // 线程数组, 所有的线程都保存在此

uint64 TASK_CNT;
/**
 * new content for unit test of 2023 OS lab2
*/
extern uint64 task_test_priority[]; // test_init 后, 用于初始化 task[i].priority 的数组
extern uint64 task_test_counter[];  // test_init 后, 用于初始化 task[i].counter  的数组
extern char _sramdisk[];
extern char _eramdisk[];
void do_mmap(struct task_struct *task, uint64_t addr, uint64_t length, uint64_t flags,
    uint64_t vm_content_offset_in_file, uint64_t vm_content_size_in_file);
struct vm_area_struct *find_vma(struct task_struct *task, uint64_t addr);

static uint64_t load_program(struct task_struct* task) {
    Elf64_Ehdr* ehdr = (Elf64_Ehdr*)_sramdisk;

    uint64_t phdr_start = (uint64_t)ehdr + ehdr->e_phoff;
    int phdr_cnt = ehdr->e_phnum;

    Elf64_Phdr* phdr;
    int load_phdr_cnt = 0;
    for (int i = 0; i < phdr_cnt; i++) {
        phdr = (Elf64_Phdr*)(phdr_start + sizeof(Elf64_Phdr) * i);
        if (phdr->p_type == PT_LOAD) {
            // uint64 seg_start = ((uint64)_sramdisk + phdr->p_offset);
            // uint64 offset = phdr->p_vaddr % PGSIZE;
            // uint64 pagenums = phdr->p_memsz + offset;
            // if(pagenums % PGSIZE == 0) pagenums /= PGSIZE;
            // else pagenums = pagenums / PGSIZE + 1 ;
            // char *temp = (char *)alloc_pages(pagenums);
            // memcpy((void*)((uint64)temp+offset), (void*)seg_start, phdr->p_memsz);
            // if(phdr->p_memsz>phdr->p_filesz)
            //     memset((void*)((uint64)temp+offset+phdr->p_filesz),0x0,phdr->p_memsz-phdr->p_filesz);
            // create_mapping(task->pgd, phdr->p_vaddr, (uint64)temp-PA2VA_OFFSET,pagenums*PGSIZE, 0x1f);

            uint64 p = 0UL;
            if(phdr->p_flags & (1UL) == 1) p = p | VM_X_MASK;
            if((phdr->p_flags & (1UL << 1)) >> 1 == 1) p = p | VM_W_MASK;
            if((phdr->p_flags & (1UL << 2)) >> 2 == 1) p = p | VM_R_MASK;
            do_mmap(task, phdr->p_vaddr, phdr->p_memsz, p, phdr->p_offset, phdr->p_filesz);

            load_phdr_cnt++;
        }
    }

    // allocate user stack and do mapping
    // code...
    // create_mapping(task->pgd, USER_END-PGSIZE, (uint64)(alloc_page())-PA2VA_OFFSET,PGSIZE,0x17);
    do_mmap(task, USER_END-PGSIZE, PGSIZE, VM_ANONYM | VM_W_MASK | VM_R_MASK, 0, 0);
    // following code has been written for you
    // set user stack
    
    // pc for the user program
    task->thread.sepc = ehdr->e_entry;
    // sstatus bits set
    task->thread.sstatus = csr_read(sstatus);
    task->thread.sstatus = task->thread.sstatus & ~(1 << 8);
    task->thread.sstatus = task->thread.sstatus | (1 << 5);
    task->thread.sstatus = task->thread.sstatus | (1 << 18);
    // user stack for user program
    task->thread.sscratch = USER_END;
    return load_phdr_cnt;
}
void task_init() {
    test_init(NR_TASKS);

    idle = (struct task_struct *)kalloc();
    idle->state = TASK_RUNNING;
    idle->counter = 0;
    idle->priority = 0;
    idle->pid = 0;
    current = idle;
    task[0] = idle;
    task[0]->vma_cnt = 0;
    TASK_CNT = 0;

    for(uint64 i = 1;i<NR_TASKS;i++){
        if(i > 1){
            task[i] = NULL;
        }else{
            task[i] = (struct task_struct *)kalloc();
            TASK_CNT ++;
            task[i]->state = TASK_RUNNING;
            task[i]->counter  = task_test_counter[i];
            task[i]->priority = task_test_priority[i];
            task[i]->pid = i;
            task[i]->thread.ra = (uint64)__dummy ;
            task[i]->thread.sp = (uint64)task[i] + PGSIZE ;
            // task[i]->thread.sepc = USER_START;
            // task[i]->thread.sstatus = csr_read(sstatus);
            // task[i]->thread.sstatus = task[i]->thread.sstatus & ~(1 << 8);
            // task[i]->thread.sstatus = task[i]->thread.sstatus | (1 << 5);
            // task[i]->thread.sstatus = task[i]->thread.sstatus | (1 << 18);
            // task[i]->thread.sscratch = USER_END ;
            task[i]->pgd = (pagetable_t)kalloc();
            memcpy(task[i]->pgd,swapper_pg_dir,PGSIZE);
            task[i]->vma_cnt = 0;
            // task[i]->vmas = (struct vm_area_struct *)kalloc();


            load_program(task[i]);

            // uint64 pagenums = ((uint64)_eramdisk-(uint64)_sramdisk);
            // if(pagenums % PGSIZE == 0) pagenums /= PGSIZE;
            // else pagenums = pagenums / PGSIZE + 1;

            // char *temp = (char *)alloc_pages(pagenums);
            // for(uint64 j = 0;j<pagenums*PGSIZE;j++){
            //     temp[j] = _sramdisk[j];
            // }
            // create_mapping(task[i]->pgd,USER_START,(uint64)temp-PA2VA_OFFSET,pagenums*PGSIZE,0x1f);
            // task[i]->thread_info.kernel_sp = (uint64)task[i] + PGSIZE;
            // task[i]->thread_info.user_sp = USER_END;
            // create_mapping(task[i]->pgd, USER_END-PGSIZE, (uint64)(alloc_page())-PA2VA_OFFSET,PGSIZE,0x17);
        }
    }
    printk("...proc_init done!\n");
}

// arch/riscv/kernel/proc.c
void dummy() {
    schedule_test();
    uint64 MOD = 1000000007;
    uint64 auto_inc_local_var = 0;
    int last_counter = -1;
    while(1) {
        if ((last_counter == -1 || current->counter != last_counter) && current->counter > 0) {
            if(current->counter == 1){
                --(current->counter);   // forced the counter to be zero if this thread is going to be scheduled
            }                           // in case that the new counter is also 1, leading the information not printed.
            last_counter = current->counter;
            auto_inc_local_var = (auto_inc_local_var + 1) % MOD;
            printk("[PID = %d] is running. auto_inc_local_var = %d\n", current->pid, auto_inc_local_var);
            printk("[COUNTER = %d]\n",current->counter);
        }
    }
}

void switch_to(struct task_struct* next) {
    // printk("in switch_to()\n");
    if((uint64)current != (uint64)next){
        struct task_struct *prev = current;
        current = next;
        // printk("Next  = %lx\n", (uint64)next);
    #ifdef SJF
        printk("\n");
        printk("switch to [PID = %d COUNTER = %d]\n\n", current->pid, current->counter);
    #endif 

    #ifdef PRIORITY
        printk("\n");
        printk("switch to [PID = %d PRIORITY = %d COUNTER = %d]\n\n", current->pid, current->priority, current->counter);
    #endif
    // printk("before __switch_to()\n");
        __switch_to(prev, next);
    }
    // printk("leave switch_to()\n");
}

void do_timer(void) {
    // 1. 如果当前线程是 idle 线程 直接进行调度
    // 2. 如果当前线程不是 idle 对当前线程的运行剩余时间减1 若剩余时间仍然大于0 则直接返回 否则进行调度

    if(current == idle){
        schedule();
    }else{
        if((signed long)(--(current->counter)) > 0 ){
            // printk("in do_timer COUNTER = %d\n",current->counter);
            return ;
        }else{
            schedule();
        }
    }

}

void schedule(void) {
    // printk("in schedule()\n");
#ifdef SJF
    uint64 judge = 1;
    for(uint64 i = 1;i<TASK_CNT + 1;i++){
        if(task[i]&&task[i]->counter != 0){
            judge = 0;
        }
    }
    if(judge){
        // printk("in schedule() task[i]->counter are all 0!\n");
        for(uint64 i=1;i<TASK_CNT + 1;i++){
            if(task[i]) task[i]->counter = rand();
        }
    }
    uint64 min = -1;
    // printk("MIN == %d\n",min);
    uint64 index = 0;
    for(uint64 i = 1;i < TASK_CNT + 1;i++){
        if(task[i]&&task[i]->counter < min && task[i]->state == TASK_RUNNING && task[i]->counter != 0){
            min = task[i]->counter;
            index = i;
        }
    }
    // printk("MIN ' == %d\n",min);
    // printk("index = %d\n",index);
    switch_to(task[index]);

#endif

#ifdef PRIORITY
        uint64 max = task[1]->counter;
        uint64 index = 1;
    while(1){
        for(uint64 i=1;i<TASK_CNT + 1;i++){
            if(task[i] && task[i]->counter >= max && task[i]->state == TASK_RUNNING){
                max = task[i]->counter;
                index = i;
            }
        }
        if(max) break;
        for(uint64 i = 1;i<TASK_CNT + 1;i++){
            if(task[i]){
                task[i] ->counter = (task[i]->counter >> 1) + task[i]->priority;
            }
        }
    }
    // printk("index == %d\n",index);
    switch_to(task[index]);
#endif
// printk("leave schedule()\n");
}



void do_mmap(struct task_struct *task, uint64 addr, uint64 length, uint64 flags,
    uint64 vm_content_offset_in_file, uint64 vm_content_size_in_file){
        struct vm_area_struct temp;
        temp.vm_start = addr;
        temp.vm_end = addr + length;
        temp.vm_flags = flags;
        temp.vm_content_offset_in_file = vm_content_offset_in_file;
        temp.vm_content_size_in_file = vm_content_size_in_file;
        task->vmas[task->vma_cnt ++] = temp;
    }