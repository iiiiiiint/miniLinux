#include "defs.h"
#include "string.h"
#include "mm.h"
#include "printk.h"
// arch/riscv/kernel/vm.c
extern char _stext[];
extern char _etext[];
extern char _srodata[];
extern char _erodata[];
extern char _sdata[];
extern char _edata[];
/* early_pgtbl: 用于 setup_vm 进行 1GB 的 映射。 */
unsigned long  early_pgtbl[512] __attribute__((__aligned__(0x1000)));

void setup_vm() {

//    printk("Setup_vm...\n");
   memset(early_pgtbl, 0x0, PGSIZE);
   uint64 vpn2 = (PHY_START & 0x0000007fc0000000) >> 30;
//    early_pgtbl[vpn2] = ((PHY_START >> 30) << 28) | 0xf;
   vpn2 = (VM_START & 0x0000007fc0000000) >> 30;
   early_pgtbl[vpn2] = ((PHY_START >> 30) << 28) | 0xf;
//    printk("leaving setup_vm...\n");
   return ;

}

// arch/riscv/kernel/vm.c 
void create_mapping(uint64 *pgtbl, uint64 va, uint64 pa, uint64 sz, uint64 perm);
/* swapper_pg_dir: kernel pagetable 根目录， 在 setup_vm_final 进行映射。 */
unsigned long  swapper_pg_dir[512] __attribute__((__aligned__(0x1000)));

void setup_vm_final() {
    // printk("Setup_vm_final...\n");
    memset(swapper_pg_dir, 0x0, PGSIZE);
    // No OpenSBI mapping required

    // mapping kernel text X|-|R|V
    uint64 va = VM_START + OPENSBI_SIZE;
    uint64 pa = PHY_START + OPENSBI_SIZE;
    create_mapping(swapper_pg_dir, va, pa, (uint64)_srodata-(uint64)_stext, 0xb);

    // mapping kernel rodata -|-|R|V
    va += (uint64)_srodata-(uint64)_stext;
    pa += (uint64)_srodata-(uint64)_stext;
    create_mapping(swapper_pg_dir, va, pa, (uint64)_sdata-(uint64)_srodata, 0x3);

    // mapping other memory -|W|R|V
    va += (uint64)_sdata-(uint64)_srodata;
    pa += (uint64)_sdata-(uint64)_srodata;
    create_mapping(swapper_pg_dir, va, pa, PHY_SIZE - ((uint64)_sdata - (uint64)_stext), 0x7);

    // set satp with swapper_pg_dir
    uint64 table = (((uint64)swapper_pg_dir - PA2VA_OFFSET) >> 12) | (8UL << 60);
    // printk("before asm...\n");
    __asm__ volatile (
        "csrw satp, %[table]\n"
        :
        : [table] "r" (table)
        :"memory"
    );

    // flush TLB
    asm volatile("sfence.vma zero, zero\n");

    // flush icache
    asm volatile("fence.i\n");
    printk("leaving setup_vm_final...\n");
    return ;
}


/**** 创建多级页表映射关系 *****/
/* 不要修改该接口的参数和返回值 */
void create_mapping(uint64 *pgtbl, uint64 va, uint64 pa, uint64 sz, uint64 perm) {
    // printk("create_mapping...\n");
    uint64 *second;
    uint64 *third;

    for(uint64 i = va; i < va + sz; i = i + PGSIZE){
        uint64 vpn2 = (i >> 30) & 0x1ff;
        uint64 vpn1 = (i >> 21) & 0x1ff;
        uint64 vpn0 = (i >> 12) & 0x1ff;
        if(!(pgtbl[vpn2] & 0x1)){
            second = (uint64 *)kalloc();
            pgtbl[vpn2] = ((((uint64)second - PA2VA_OFFSET) >> 12) << 10) | 0x1;
            second = (uint64 *)(((pgtbl[vpn2] >> 10) << 12) + PA2VA_OFFSET);
            memset(second, 0x0, PGSIZE);
        }else{
            second = (uint64 *)(((pgtbl[vpn2] >> 10) << 12) + PA2VA_OFFSET);
        }
        if(!(second[vpn1] & 0x1)){
            third = (uint64 *)kalloc();
            second[vpn1] = ((((uint64)third - PA2VA_OFFSET) >> 12) << 10 ) | 0x1;
            third = (uint64 *)(((second[vpn1] >> 10) << 12) + PA2VA_OFFSET);
            memset(third, 0x0, PGSIZE);
        }else{
            third = (uint64 *)(((second[vpn1] >> 10) << 12) + PA2VA_OFFSET);
        }
        third[vpn0] = ((pa >> 12) << 10) | perm;
        pa += PGSIZE;
    }
    
    return ;
}