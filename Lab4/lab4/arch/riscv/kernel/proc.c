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

/**
 * new content for unit test of 2023 OS lab2
*/
extern uint64 task_test_priority[]; // test_init 后, 用于初始化 task[i].priority 的数组
extern uint64 task_test_counter[];  // test_init 后, 用于初始化 task[i].counter  的数组
extern char _sramdisk[];
extern char _eramdisk[];

static uint64_t load_program(struct task_struct* task) {
    Elf64_Ehdr* ehdr = (Elf64_Ehdr*)_sramdisk;

    uint64_t phdr_start = (uint64_t)ehdr + ehdr->e_phoff;
    int phdr_cnt = ehdr->e_phnum;

    Elf64_Phdr* phdr;
    int load_phdr_cnt = 0;
    for (int i = 0; i < phdr_cnt; i++) {
        phdr = (Elf64_Phdr*)(phdr_start + sizeof(Elf64_Phdr) * i);
        if (phdr->p_type == PT_LOAD) {
            // alloc space and copy content
            // do mapping
            // code...
            uint64 seg_start = ((uint64)_sramdisk + phdr->p_offset);
            uint64 offset = phdr->p_vaddr % PGSIZE;
            uint64 pagenums = phdr->p_memsz + offset;
            if(pagenums % PGSIZE == 0) pagenums /= PGSIZE;
            else pagenums = pagenums / PGSIZE + 1 ;
            char *temp = (char *)alloc_pages(pagenums);
            memcpy((void*)((uint64)temp+offset), (void*)seg_start, phdr->p_memsz);
            if(phdr->p_memsz>phdr->p_filesz)
                memset((void*)((uint64)temp+offset+phdr->p_filesz),0x0,phdr->p_memsz-phdr->p_filesz);
            create_mapping(task->pgd, phdr->p_vaddr, (uint64)temp-PA2VA_OFFSET,pagenums*PGSIZE, 0x1f);

            load_phdr_cnt++;
        }
    }

    // allocate user stack and do mapping
    // code...
    task->thread_info.kernel_sp = (uint64)task + PGSIZE;
    task->thread_info.user_sp = USER_END;
    create_mapping(task->pgd, USER_END-PGSIZE, (uint64)(alloc_page())-PA2VA_OFFSET,PGSIZE,0x17);
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
    return ;
}
void task_init() {
    test_init(NR_TASKS);
    // 1. 调用 kalloc() 为 idle 分配一个物理页
    // 2. 设置 state 为 TASK_RUNNING;
    // 3. 由于 idle 不参与调度 可以将其 counter / priority 设置为 0
    // 4. 设置 idle 的 pid 为 0
    // 5. 将 current 和 task[0] 指向 idle

    idle = (struct task_struct *)kalloc();
    idle->state = TASK_RUNNING;
    idle->counter = 0;
    idle->priority = 0;
    idle->pid = 0;
    current = idle;
    task[0] = idle;

    // 1. 参考 idle 的设置, 为 task[1] ~ task[NR_TASKS - 1] 进行初始化
    // 2. 其中每个线程的 state 为 TASK_RUNNING, 此外，为了单元测试的需要，counter 和 priority 进行如下赋值：
    //      task[i].counter  = task_test_counter[i];
    //      task[i].priority = task_test_priority[i];
    // 3. 为 task[1] ~ task[NR_TASKS - 1] 设置 `thread_struct` 中的 `ra` 和 `sp`,
    // 4. 其中 `ra` 设置为 __dummy （见 4.3.2）的地址,  `sp` 设置为 该线程申请的物理页的高地址

    for(uint64 i = 1;i<NR_TASKS;i++){
        task[i] = (struct task_struct *)kalloc();
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
    for(uint64 i = 1;i<NR_TASKS;i++){
        if(task[i]&&task[i]->counter != 0){
            judge = 0;
        }
    }
    if(judge){
        // printk("in schedule() task[i]->counter are all 0!\n");
        for(uint64 i=1;i<NR_TASKS;i++){
            if(task[i]) task[i]->counter = rand();
        }
    }
    uint64 min = -1;
    // printk("MIN == %d\n",min);
    uint64 index = 0;
    for(uint64 i = 1;i < NR_TASKS;i++){
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
        for(uint64 i=1;i<NR_TASKS;i++){
            if(task[i] && task[i]->counter >= max && task[i]->state == TASK_RUNNING){
                max = task[i]->counter;
                index = i;
            }
        }
        if(max) break;
        for(uint64 i = 1;i<NR_TASKS;i++){
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